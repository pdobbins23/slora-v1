//! Entry point for the SLoRa stack.
//! The `Node` struct owns all the per-node state, and the per-layer modules are
//! re-exported as namespaces so callers can reach them through this one file.
//! When the host hands the node some bytes off the wire, the dispatch below
//! looks at the first byte (the category) and forwards the frame to the
//! announce, link, route, relay, tsch, or echo layer.
//! The node never does I/O itself. Every side effect (a transmission, a
//! frequency retune, a wakeup timer) gets queued as an `Action` value, and the
//! host pulls them out with `nextAction` and runs them on its radios.

const std = @import("std");
const types = @import("types.zig");

pub const announce = @import("announce.zig");
pub const link = @import("link.zig");
pub const crypto = @import("crypto.zig");
pub const route = @import("route.zig");
pub const relay = @import("relay.zig");
pub const echo = @import("echo.zig");
pub const schedule = @import("schedule.zig");
pub const tsch = @import("tsch.zig");

const announce_interval_default_ns: u64 = 5 * std.time.ns_per_s;
const announce_jitter_default_ns: u64 = 2 * std.time.ns_per_s;
const x_kp_context: [16]u8 = "slora.x25519.kpd".*;

pub const Node = struct {
    // Wakeup tokens the host fires back into the node:
    //   announce: time to emit the next Announce broadcast.
    //   slot: time to handle the next TSCH slot boundary.
    pub const Token = enum(u32) { announce = 0, slot = 1 };

    pub const Action = union(enum) {
        tx_broadcast: []const u8,
        tx_data: struct { bytes: []const u8, freq_hz: u32, bw_hz: u32 },
        tune_data: struct { freq_hz: u32, bw_hz: u32 },
        schedule: struct { token: Token, delay_ns: u64 },
        deliver: struct { from: types.Address, payload: []const u8 },
    };

    pub const NeighborInfo = struct {
        pubkey: types.PublicKey,
        last_link_req_ns: u64 = 0,
    };

    pub const PendingTx = struct {
        dest: types.Address,
        source: types.Address,
        payload: []u8,
    };

    pub const LinkInfo = struct {
        link_key: [32]u8,
        tx_counter: u64 = 0,
        pending_tx: std.ArrayList(PendingTx) = .empty,
    };

    pub const RouteEntry = struct {
        next_hop: types.Address,
        cost: u16,
        seq: u32,
    };

    pub const ReverseEntry = struct {
        dest: types.Address,
        prev_hop: types.Address,
        min_cost: u16,
    };

    pub const pending_tx_max: usize = 32;
    pub const pending_per_dest_max: usize = 8;

    gpa: types.Allocator,
    prng: std.Random.DefaultPrng,

    addr: types.Address,
    keypair: types.KeyPair,
    x_keypair: types.X25519.KeyPair,
    seq: u64 = 0,
    echo_seq: u32 = 0,

    asn_offset_ns: i128 = 0,
    time_source: ?types.Address = null,

    announce_interval_ns: u64 = announce_interval_default_ns,
    announce_jitter_ns: u64 = announce_jitter_default_ns,

    tx_buf: std.ArrayList(u8) = .empty,
    actions: std.ArrayList(Action) = .empty,
    broadcast_queue: std.ArrayList([]u8) = .empty,
    consumed_frames: std.ArrayList([]u8) = .empty,

    neighbors: std.AutoHashMapUnmanaged(types.Address, NeighborInfo) = .empty,
    links: std.AutoHashMapUnmanaged(types.Address, LinkInfo) = .empty,

    routes: std.AutoHashMapUnmanaged(types.Address, RouteEntry) = .empty,
    reverse_paths: std.AutoHashMapUnmanaged(u32, ReverseEntry) = .empty,
    pending_routes: std.AutoHashMapUnmanaged(types.Address, std.ArrayList([]u8)) = .empty,
    last_rreq_ns: std.AutoHashMapUnmanaged(types.Address, u64) = .empty,

    forwarded_count: u32 = 0,
    drop_no_route: u32 = 0,
    drop_no_link: u32 = 0,
    drop_queue_full: u32 = 0,
    target_peers: std.AutoHashMapUnmanaged(types.Address, void) = .empty,
    eb_pending: bool = false,

    pub fn init(gpa: types.Allocator, seed: [32]u8, keypair: types.KeyPair) !Node {
        var x_seed: [32]u8 = undefined;
        types.Blake256.hash(&seed, &x_seed, .{ .context = x_kp_context });
        const x_kp = types.X25519.KeyPair.generateDeterministic(x_seed) catch unreachable;

        var node: Node = .{
            .gpa = gpa,
            .prng = std.Random.DefaultPrng.init(std.mem.readInt(u64, seed[0..8], .little)),
            .addr = types.addressFromPubkey(keypair.public_key.bytes),
            .keypair = keypair,
            .x_keypair = x_kp,
        };

        try node.tx_buf.ensureTotalCapacity(gpa, 4096);
        try node.actions.ensureTotalCapacity(gpa, 256);
        try node.broadcast_queue.ensureTotalCapacity(gpa, 64);
        try node.consumed_frames.ensureTotalCapacity(gpa, 256);

        return node;
    }

    pub fn deinit(node: *Node) void {
        var link_iter = node.links.iterator();
        while (link_iter.next()) |entry| {
            for (entry.value_ptr.pending_tx.items) |pending| node.gpa.free(pending.payload);
            entry.value_ptr.pending_tx.deinit(node.gpa);
        }
        var pending_iter = node.pending_routes.iterator();
        while (pending_iter.next()) |entry| {
            for (entry.value_ptr.items) |buf| node.gpa.free(buf);
            entry.value_ptr.deinit(node.gpa);
        }
        for (node.broadcast_queue.items) |frame| node.gpa.free(frame);
        for (node.consumed_frames.items) |frame| node.gpa.free(frame);

        node.tx_buf.deinit(node.gpa);
        node.actions.deinit(node.gpa);
        node.broadcast_queue.deinit(node.gpa);
        node.consumed_frames.deinit(node.gpa);
        node.neighbors.deinit(node.gpa);
        node.links.deinit(node.gpa);
        node.routes.deinit(node.gpa);
        node.reverse_paths.deinit(node.gpa);
        node.pending_routes.deinit(node.gpa);
        node.last_rreq_ns.deinit(node.gpa);
        node.target_peers.deinit(node.gpa);
    }

    pub fn start(node: *Node, now_ns: u64) void {
        const jitter = node.prng.random().uintLessThan(u64, node.announce_interval_ns);
        node.actions.appendAssumeCapacity(.{ .schedule = .{ .token = .announce, .delay_ns = jitter } });
        tsch.scheduleNextSlot(node, now_ns);
    }

    pub fn onBroadcastRx(node: *Node, bytes: []const u8, now_ns: u64) void {
        if (bytes.len < 1) return;
        const cat = std.enums.fromInt(types.Category, bytes[0]) orelse return;
        switch (cat) {
            .announce => announce.handleRx(node, bytes, now_ns) catch {},
            .link => link.handleRx(node, bytes) catch {},
            .route => route.handleRx(node, bytes, now_ns) catch {},
            else => {},
        }
    }

    pub fn onDataRx(node: *Node, bytes: []const u8, _: u32, _: u32, now_ns: u64) void {
        relay.handleEncryptedRx(node, bytes, now_ns);
    }

    pub fn onWakeup(node: *Node, token: Token, now_ns: u64) void {
        switch (token) {
            .announce => scheduleNextAnnounce(node),
            .slot => tsch.handleSlotWakeup(node, now_ns),
        }
    }

    fn scheduleNextAnnounce(node: *Node) void {
        node.eb_pending = true;
        const jitter = node.prng.random().uintLessThan(u64, 2 * node.announce_jitter_ns);
        const delay = node.announce_interval_ns + jitter -| node.announce_jitter_ns;
        node.actions.appendAssumeCapacity(.{ .schedule = .{ .token = .announce, .delay_ns = delay } });
    }

    pub fn sendPayload(node: *Node, dest: types.Address, payload: []const u8, now_ns: u64) void {
        route.send(node, dest, node.addr, payload, now_ns);
    }

    pub fn sendEcho(node: *Node, dest: types.Address, payload: []const u8, now_ns: u64) void {
        echo.send(node, dest, payload, now_ns);
    }

    pub fn nextAction(node: *Node, out: *Action) bool {
        // Once the host has drained every queued Action, it is safe to free the broadcast
        // frames it consumed and reset the per-call scratch buffer used by `tx_data`.
        if (node.actions.items.len == 0) {
            for (node.consumed_frames.items) |frame| node.gpa.free(frame);
            node.consumed_frames.clearRetainingCapacity();
            node.tx_buf.clearRetainingCapacity();
            return false;
        }
        out.* = node.actions.orderedRemove(0);
        if (out.* == .tx_broadcast) {
            node.consumed_frames.appendAssumeCapacity(@constCast(out.tx_broadcast));
        }
        return true;
    }
};

test {
    _ = announce;
    _ = link;
    _ = crypto;
    _ = route;
    _ = relay;
    _ = echo;
    _ = schedule;
    _ = tsch;
    std.testing.refAllDecls(@This());
}
