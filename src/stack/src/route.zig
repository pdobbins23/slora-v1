//! AODV-style reactive routing over the broadcast radio.
//! When a node needs a route to a destination it does not have, it floods a
//! RREQ on the broadcast radio. Each intermediate that rebroadcasts the RREQ
//! adds its own hop cost into the cost field. The hop cost is `1 + the number
//! of active links` the intermediate has, so paths through congested relays
//! end up more expensive and the routing prefers quieter ones.
//! When the destination receives the RREQ, it sends a RREP back along the
//! lowest-cost reverse path the RREQ traversed.

const std = @import("std");
const types = @import("types.zig");
const slora = @import("slora.zig");
const link = @import("link.zig");
const crypto = @import("crypto.zig");

pub const Op = enum(u8) { rreq, rrep };

pub const RreqPacket = extern struct {
    op: Op align(1),
    env: types.Envelope align(1),
    pubkey: [32]u8 align(1),
    orig: types.Address align(1),
    id: u32 align(1),
    orig_seq: u32 align(1),
    cost: u16 align(1),
    radius: u8 align(1),
};

pub const RrepPacket = extern struct {
    op: Op align(1),
    env: types.Envelope align(1),
    pubkey: [32]u8 align(1),
    orig: types.Address align(1),
    id: u32 align(1),
    dst_seq: u32 align(1),
    cost: u16 align(1),
};

pub const route_radius: u8 = 10;
pub const max_payload: usize = 192;

const rreq_retry_ns: u64 = 20 * std.time.ns_per_s;
const rreq_body_len: usize = @sizeOf(RreqPacket);
const rrep_body_len: usize = @sizeOf(RrepPacket);
const rreq_frame_len: usize = 1 + rreq_body_len + crypto.sig_len;
const rrep_frame_len: usize = 1 + rrep_body_len + crypto.sig_len;
const max_signed_frame: usize = 1 + @max(rreq_body_len, rrep_body_len) + crypto.sig_len;

fn hopCost(node: *const slora.Node) u16 {
    return 1 + @as(u16, @intCast(node.links.count()));
}

fn signQueue(node: *slora.Node, body: []const u8) void {
    const total = 1 + body.len + crypto.sig_len;
    std.debug.assert(total <= max_signed_frame);

    var scratch: [max_signed_frame]u8 = undefined;
    scratch[0] = @intFromEnum(types.Category.route);
    @memcpy(scratch[1 .. 1 + body.len], body);

    const sig = node.keypair.sign(scratch[0 .. 1 + body.len], null) catch unreachable;
    @memcpy(scratch[1 + body.len ..][0..crypto.sig_len], &sig.toBytes());

    const owned = node.gpa.dupe(u8, scratch[0..total]) catch return;
    node.broadcast_queue.append(node.gpa, owned) catch node.gpa.free(owned);
}

fn verifySig(node: *slora.Node, frame: []const u8, from: types.Address, pubkey: [32]u8) bool {
    if (types.addressFromPubkey(pubkey) != from) return false;

    const pk = crypto.Ed25519.PublicKey.fromBytes(pubkey) catch return false;
    const sig = crypto.Ed25519.Signature.fromBytes(frame[frame.len - crypto.sig_len ..][0..crypto.sig_len].*);
    sig.verify(frame[0 .. frame.len - crypto.sig_len], pk) catch return false;

    const gop = node.neighbors.getOrPut(node.gpa, from) catch return true;
    if (!gop.found_existing or gop.value_ptr.pubkey == 0) {
        gop.value_ptr.* = .{ .pubkey = @bitCast(pubkey) };
    }
    return true;
}

fn upsertRoute(node: *slora.Node, dest: types.Address, next_hop: types.Address, cost: u16, seq: u32) void {
    const gop = node.routes.getOrPut(node.gpa, dest) catch return;
    if (gop.found_existing) {
        const cur = gop.value_ptr.*;
        const newer_seq = seq > cur.seq;
        const better_cost = seq == cur.seq and cost < cur.cost;
        if (!newer_seq and !better_cost) return;
    }
    gop.value_ptr.* = .{ .next_hop = next_hop, .cost = cost, .seq = seq };
}

pub fn handleRx(node: *slora.Node, frame: []const u8, now_ns: u64) !void {
    if (frame.len < 2 + crypto.sig_len) return;
    switch (std.enums.fromInt(Op, frame[1]) orelse return) {
        .rreq => try handleRreq(node, frame),
        .rrep => try handleRrep(node, frame, now_ns),
    }
}

fn handleRreq(node: *slora.Node, frame: []const u8) !void {
    if (frame.len != rreq_frame_len) return;

    var body: RreqPacket = undefined;
    @memcpy(std.mem.asBytes(&body), frame[1 .. 1 + rreq_body_len]);

    std.debug.assert(body.op == .rreq);
    if (body.env.from == node.addr or body.orig == node.addr) return;
    if (!verifySig(node, frame, body.env.from, body.pubkey)) return;

    const own_hop = hopCost(node);
    std.debug.assert(own_hop >= 1);
    const reverse_cost: u16 = body.cost +| own_hop;

    if (node.reverse_paths.getPtr(body.id)) |existing| {
        if (reverse_cost >= existing.min_cost) return;
        existing.prev_hop = body.env.from;
        existing.min_cost = reverse_cost;
    } else {
        try node.reverse_paths.put(node.gpa, body.id, .{
            .dest = body.env.to,
            .prev_hop = body.env.from,
            .min_cost = reverse_cost,
        });
    }

    upsertRoute(node, body.orig, body.env.from, reverse_cost, body.orig_seq);

    if (body.env.to == node.addr) {
        try emitRrep(node, body.env.from, body.orig, body.id, @truncate(node.seq), own_hop);
        return;
    }

    if (body.radius == 0) return;
    body.radius -= 1;
    body.cost = reverse_cost;
    body.env.from = node.addr;
    body.pubkey = node.keypair.public_key.bytes;
    signQueue(node, std.mem.asBytes(&body));
}

fn emitRrep(node: *slora.Node, to_hop: types.Address, orig: types.Address, id: u32, dst_seq: u32, cost: u16) !void {
    const body: RrepPacket = .{
        .op = .rrep,
        .env = .{ .to = to_hop, .from = node.addr },
        .pubkey = node.keypair.public_key.bytes,
        .orig = orig,
        .id = id,
        .dst_seq = dst_seq,
        .cost = cost,
    };
    signQueue(node, std.mem.asBytes(&body));
}

fn handleRrep(node: *slora.Node, frame: []const u8, now_ns: u64) !void {
    if (frame.len != rrep_frame_len) return;

    var body: RrepPacket = undefined;
    @memcpy(std.mem.asBytes(&body), frame[1 .. 1 + rrep_body_len]);

    std.debug.assert(body.op == .rrep);
    if (body.env.from == node.addr) return;
    if (!verifySig(node, frame, body.env.from, body.pubkey)) return;

    const rev = node.reverse_paths.get(body.id) orelse return;
    const dst = rev.dest;
    if (dst == node.addr) return;

    const via_cost: u16 = body.cost +| hopCost(node);
    upsertRoute(node, dst, body.env.from, via_cost, body.dst_seq);

    if (body.env.to != node.addr) return;

    _ = link.tryEstablish(node, body.env.from, now_ns);

    if (body.orig == node.addr) {
        flushPendingFor(node, body.env.from);
        return;
    }

    try emitRrep(node, rev.prev_hop, body.orig, body.id, body.dst_seq, via_cost);
}

fn enqueueOnLink(node: *slora.Node, info: *slora.Node.LinkInfo, dest: types.Address, source: types.Address, payload: []const u8) bool {
    if (info.pending_tx.items.len >= slora.Node.pending_tx_max) return false;

    const dup = node.gpa.dupe(u8, payload) catch return false;
    info.pending_tx.append(node.gpa, .{ .dest = dest, .source = source, .payload = dup }) catch {
        node.gpa.free(dup);
        return false;
    };
    return true;
}

pub fn flushPendingFor(node: *slora.Node, peer: types.Address) void {
    const info = node.links.getPtr(peer) orelse return;

    var to_remove: std.ArrayList(types.Address) = .empty;
    defer to_remove.deinit(node.gpa);

    var iter = node.pending_routes.iterator();
    while (iter.next()) |kv| {
        const dst = kv.key_ptr.*;
        const matches_direct = dst == peer;
        const matches_via = if (node.routes.get(dst)) |r| r.next_hop == peer else false;
        if (!matches_direct and !matches_via) continue;

        for (kv.value_ptr.items) |payload| {
            _ = enqueueOnLink(node, info, dst, node.addr, payload);
            node.gpa.free(payload);
        }
        kv.value_ptr.clearAndFree(node.gpa);
        to_remove.append(node.gpa, dst) catch return;
    }

    for (to_remove.items) |dst| {
        if (node.pending_routes.fetchRemove(dst)) |kv| {
            var queue = kv.value;
            queue.deinit(node.gpa);
        }
    }
}

pub fn send(node: *slora.Node, dest: types.Address, source: types.Address, payload: []const u8, now_ns: u64) void {
    if (payload.len > max_payload) return;
    if (dest == node.addr) return;

    if (node.links.getPtr(dest)) |info| {
        if (!enqueueOnLink(node, info, dest, source, payload)) node.drop_queue_full += 1;
        return;
    }

    if (node.routes.get(dest)) |r| {
        if (node.links.getPtr(r.next_hop)) |info| {
            if (!enqueueOnLink(node, info, dest, source, payload)) node.drop_queue_full += 1;
            return;
        }
        _ = link.tryEstablish(node, r.next_hop, now_ns);
    }

    if (node.neighbors.contains(dest)) _ = link.tryEstablish(node, dest, now_ns);

    if (source != node.addr) {
        node.drop_no_link += 1;
        return;
    }
    initiate(node, dest, payload, now_ns);
}

fn initiate(node: *slora.Node, dest: types.Address, payload: []const u8, now_ns: u64) void {
    std.debug.assert(dest != node.addr);
    const dup = node.gpa.dupe(u8, payload) catch return;

    const gop = node.pending_routes.getOrPut(node.gpa, dest) catch {
        node.gpa.free(dup);
        return;
    };
    if (!gop.found_existing) gop.value_ptr.* = .empty;
    if (gop.value_ptr.items.len >= slora.Node.pending_per_dest_max) {
        const old = gop.value_ptr.orderedRemove(0);
        node.gpa.free(old);
    }
    gop.value_ptr.append(node.gpa, dup) catch {
        node.gpa.free(dup);
        return;
    };

    if (node.last_rreq_ns.get(dest)) |last| {
        if (now_ns - last < rreq_retry_ns) return;
    }
    node.last_rreq_ns.put(node.gpa, dest, now_ns) catch return;

    node.seq +%= 1;
    const id = node.prng.random().int(u32);
    node.reverse_paths.put(node.gpa, id, .{
        .dest = dest,
        .prev_hop = node.addr,
        .min_cost = 0,
    }) catch return;

    const body: RreqPacket = .{
        .op = .rreq,
        .env = .{ .to = dest, .from = node.addr },
        .pubkey = node.keypair.public_key.bytes,
        .orig = node.addr,
        .id = id,
        .orig_seq = @truncate(node.seq),
        .cost = 0,
        .radius = route_radius,
    };
    signQueue(node, std.mem.asBytes(&body));
}
