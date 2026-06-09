//! C-callable ABI for the OMNeT++ simulator and any future firmware host.
//! The surface area is small on purpose. The host creates a node, feeds it
//! bytes off the wire and wakeup tokens from its timers, and pulls `Action`
//! values out one at a time to run on its radios.

const std = @import("std");
const slora = @import("slora.zig");
const types = @import("types.zig");

pub const NodeHandle = opaque {};

pub const tag_tx_broadcast: u8 = 0;
pub const tag_tx_data: u8 = 1;
pub const tag_tune_data: u8 = 2;
pub const tag_schedule: u8 = 3;
pub const tag_deliver: u8 = 5;

pub const Action = extern struct {
    tag: u8,
    payload: extern union {
        tx_broadcast: extern struct { bytes: [*]const u8, bytes_len: usize },
        tx_data: extern struct { bytes: [*]const u8, bytes_len: usize, freq_hz: u32, bw_hz: u32 },
        tune_data: extern struct { freq_hz: u32, bw_hz: u32 },
        schedule: extern struct { token: u32, delay_ns: u64 },
        deliver: extern struct { from: [16]u8, payload: [*]const u8, payload_len: usize },
    },
};

fn nodeFrom(handle: *NodeHandle) *slora.Node {
    return @ptrCast(@alignCast(handle));
}

export fn slora_node_init(seed: *const [32]u8) ?*NodeHandle {
    const kp = types.KeyPair.generateDeterministic(seed.*) catch return null;
    const node = std.heap.c_allocator.create(slora.Node) catch return null;
    node.* = slora.Node.init(std.heap.c_allocator, seed.*, kp) catch {
        std.heap.c_allocator.destroy(node);
        return null;
    };
    return @ptrCast(node);
}

export fn slora_node_deinit(handle: *NodeHandle) void {
    const node = nodeFrom(handle);
    node.deinit();
    std.heap.c_allocator.destroy(node);
}

export fn slora_node_addr(handle: *NodeHandle, out: *[16]u8) void {
    out.* = @bitCast(nodeFrom(handle).addr);
}

export fn slora_node_link_count(handle: *NodeHandle) u32 {
    return @intCast(nodeFrom(handle).links.count());
}

export fn slora_node_neighbor_count(handle: *NodeHandle) u32 {
    return @intCast(nodeFrom(handle).neighbors.count());
}

export fn slora_node_route_count(handle: *NodeHandle) u32 {
    return @intCast(nodeFrom(handle).routes.count());
}

export fn slora_node_pending_tx_total(handle: *NodeHandle) u32 {
    var n: u32 = 0;
    var iter = nodeFrom(handle).links.iterator();
    while (iter.next()) |entry| n += @intCast(entry.value_ptr.pending_tx.items.len);
    return n;
}

export fn slora_node_forwarded_count(handle: *NodeHandle) u32 {
    return nodeFrom(handle).forwarded_count;
}

export fn slora_node_drop_no_route(handle: *NodeHandle) u32 {
    return nodeFrom(handle).drop_no_route;
}
export fn slora_node_drop_no_link(handle: *NodeHandle) u32 {
    return nodeFrom(handle).drop_no_link;
}
export fn slora_node_drop_link_down(_: *NodeHandle) u32 {
    return 0;
}
export fn slora_node_drop_queue_full(handle: *NodeHandle) u32 {
    return nodeFrom(handle).drop_queue_full;
}

export fn slora_node_add_target_peer(handle: *NodeHandle, peer_addr: *const [16]u8) void {
    const node = nodeFrom(handle);
    node.target_peers.put(node.gpa, @bitCast(peer_addr.*), {}) catch {};
}

export fn slora_node_set_announce_interval(handle: *NodeHandle, interval_ns: u64, jitter_ns: u64) void {
    const node = nodeFrom(handle);
    node.announce_interval_ns = interval_ns;
    node.announce_jitter_ns = jitter_ns;
}

export fn slora_node_start(handle: *NodeHandle, now_ns: u64) void {
    nodeFrom(handle).start(now_ns);
}

export fn slora_node_on_broadcast_rx(handle: *NodeHandle, bytes: [*]const u8, bytes_len: usize, now_ns: u64) void {
    nodeFrom(handle).onBroadcastRx(bytes[0..bytes_len], now_ns);
}

export fn slora_node_on_data_rx(handle: *NodeHandle, bytes: [*]const u8, bytes_len: usize, freq_hz: u32, bw_hz: u32, now_ns: u64) void {
    nodeFrom(handle).onDataRx(bytes[0..bytes_len], freq_hz, bw_hz, now_ns);
}

export fn slora_node_on_wakeup(handle: *NodeHandle, token: u32, now_ns: u64) void {
    const t = std.enums.fromInt(slora.Node.Token, token) orelse return;
    nodeFrom(handle).onWakeup(t, now_ns);
}

export fn slora_node_send_payload(handle: *NodeHandle, dest: *const [16]u8, payload: [*]const u8, payload_len: usize, now_ns: u64) void {
    nodeFrom(handle).sendPayload(@bitCast(dest.*), payload[0..payload_len], now_ns);
}

export fn slora_node_send_echo(handle: *NodeHandle, dest: *const [16]u8, payload: [*]const u8, payload_len: usize, now_ns: u64) void {
    nodeFrom(handle).sendEcho(@bitCast(dest.*), payload[0..payload_len], now_ns);
}

export fn slora_node_next_action(handle: *NodeHandle, out: *Action) bool {
    var z: slora.Node.Action = undefined;
    if (!nodeFrom(handle).nextAction(&z)) return false;
    switch (z) {
        .tx_broadcast => |b| {
            out.tag = tag_tx_broadcast;
            out.payload = .{ .tx_broadcast = .{ .bytes = b.ptr, .bytes_len = b.len } };
        },
        .tx_data => |t| {
            out.tag = tag_tx_data;
            out.payload = .{ .tx_data = .{ .bytes = t.bytes.ptr, .bytes_len = t.bytes.len, .freq_hz = t.freq_hz, .bw_hz = t.bw_hz } };
        },
        .tune_data => |f| {
            out.tag = tag_tune_data;
            out.payload = .{ .tune_data = .{ .freq_hz = f.freq_hz, .bw_hz = f.bw_hz } };
        },
        .schedule => |s| {
            out.tag = tag_schedule;
            out.payload = .{ .schedule = .{ .token = @intFromEnum(s.token), .delay_ns = s.delay_ns } };
        },
        .deliver => |d| {
            out.tag = tag_deliver;
            out.payload = .{ .deliver = .{ .from = @bitCast(d.from), .payload = d.payload.ptr, .payload_len = d.payload.len } };
        },
    }
    return true;
}
