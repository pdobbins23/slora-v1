//! Multi-hop data forwarding.
//! When a node has a packet to send, it seals the payload with the next hop's
//! link key and queues it under the link's pending-TX list. When a node
//! receives a sealed packet, it opens it with the link key, and either delivers
//! the payload locally (if the destination is this node) or re-seals it under
//! the next hop's link key per the route table and forwards it.

const std = @import("std");
const types = @import("types.zig");
const slora = @import("slora.zig");
const crypto = @import("crypto.zig");
const link = @import("link.zig");
const echo = @import("echo.zig");

const header_len: usize = 1 + 16 + 16;
const max_plaintext: usize = 224;
pub const max_payload: usize = max_plaintext - header_len;

fn seal(node: *slora.Node, peer: types.Address, info: *slora.Node.LinkInfo, plaintext: []const u8) []const u8 {
    std.debug.assert(plaintext.len <= max_plaintext);

    const counter = info.tx_counter;
    info.tx_counter += 1;
    const nonce = crypto.linkNonce(node.addr, peer, counter);

    const start = node.tx_buf.items.len;
    node.tx_buf.appendSliceAssumeCapacity(&nonce);

    const ct_start = node.tx_buf.items.len;
    node.tx_buf.appendSliceAssumeCapacity(plaintext);

    var tag: [crypto.tag_len]u8 = undefined;
    crypto.seal(node.tx_buf.items[ct_start..], &tag, nonce, info.link_key);
    node.tx_buf.appendSliceAssumeCapacity(&tag);

    return node.tx_buf.items[start..];
}

pub fn buildDataFrame(node: *slora.Node, peer: types.Address, info: *slora.Node.LinkInfo, dest: types.Address, source: types.Address, payload: []const u8) ?[]const u8 {
    if (payload.len > max_payload) return null;

    var pt: [max_plaintext]u8 = undefined;
    pt[0] = @intFromEnum(types.Category.relay);
    const to_bytes: [16]u8 = @bitCast(dest);
    const from_bytes: [16]u8 = @bitCast(source);
    @memcpy(pt[1..17], &to_bytes);
    @memcpy(pt[17..33], &from_bytes);
    @memcpy(pt[33 .. 33 + payload.len], payload);

    return seal(node, peer, info, pt[0 .. header_len + payload.len]);
}

pub fn handleEncryptedRx(node: *slora.Node, frame: []const u8, now_ns: u64) void {
    if (frame.len < crypto.nonce_len + crypto.tag_len) return;

    const nonce: [crypto.nonce_len]u8 = frame[0..crypto.nonce_len].*;
    const ct_with_tag = frame[crypto.nonce_len..];
    const ct_len = ct_with_tag.len - crypto.tag_len;
    if (ct_len == 0 or ct_len > max_plaintext) return;
    const tag: [crypto.tag_len]u8 = ct_with_tag[ct_len..][0..crypto.tag_len].*;

    // The frame carries no link identifier, so the receiver tries every link key it holds.
    // Poly1305 makes the wrong-key path a constant-time reject, and `max_links_per_node` is small.
    var pt: [max_plaintext]u8 = undefined;
    var iter = node.links.iterator();
    while (iter.next()) |entry| {
        @memcpy(pt[0..ct_len], ct_with_tag[0..ct_len]);
        crypto.open(pt[0..ct_len], tag, nonce, entry.value_ptr.link_key) catch continue;
        dispatchDecoded(node, pt[0..ct_len], now_ns);
        return;
    }
}

fn dispatchDecoded(node: *slora.Node, pt: []const u8, now_ns: u64) void {
    if (pt.len < header_len) return;
    if (pt[0] != @intFromEnum(types.Category.relay)) return;

    const to: types.Address = @bitCast(pt[1..17].*);
    const source: types.Address = @bitCast(pt[17..33].*);
    const payload = pt[header_len..];

    if (to == node.addr) {
        deliverLocal(node, source, payload, now_ns);
        return;
    }

    forwardToward(node, to, source, payload, now_ns);
}

fn deliverLocal(node: *slora.Node, source: types.Address, payload: []const u8, now_ns: u64) void {
    if (payload.len > 0 and payload[0] == @intFromEnum(types.Category.echo)) {
        echo.handleDelivered(node, source, payload, now_ns);
        return;
    }
    const start = node.tx_buf.items.len;
    node.tx_buf.appendSliceAssumeCapacity(payload);
    node.actions.appendAssumeCapacity(.{ .deliver = .{
        .from = source,
        .payload = node.tx_buf.items[start..],
    } });
}

fn forwardToward(node: *slora.Node, dest: types.Address, source: types.Address, payload: []const u8, now_ns: u64) void {
    if (node.links.getPtr(dest)) |info| {
        if (!enqueue(node, info, dest, source, payload)) node.drop_queue_full += 1;
        return;
    }

    const r = node.routes.get(dest) orelse {
        if (node.neighbors.contains(dest)) _ = link.tryEstablish(node, dest, now_ns);
        node.drop_no_route += 1;
        return;
    };
    if (r.next_hop == source) return;

    if (node.links.getPtr(r.next_hop)) |info| {
        if (!enqueue(node, info, dest, source, payload)) node.drop_queue_full += 1;
        return;
    }
    _ = link.tryEstablish(node, r.next_hop, now_ns);
    node.drop_no_link += 1;
}

fn enqueue(node: *slora.Node, info: *slora.Node.LinkInfo, dest: types.Address, source: types.Address, payload: []const u8) bool {
    if (info.pending_tx.items.len >= slora.Node.pending_tx_max) return false;

    const dup = node.gpa.dupe(u8, payload) catch return false;
    info.pending_tx.append(node.gpa, .{ .dest = dest, .source = source, .payload = dup }) catch {
        node.gpa.free(dup);
        return false;
    };
    node.forwarded_count += 1;
    return true;
}
