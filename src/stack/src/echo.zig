//! Tiny request/response app used to measure delivery and latency.
//! The traffic generator in the simulator sends echo requests at a fixed rate
//! and counts replies, which exercises every layer of the stack from the
//! application down to the radio.

const std = @import("std");
const types = @import("types.zig");
const slora = @import("slora.zig");
const route = @import("route.zig");

pub const Op = enum(u8) { req, res };

pub const Packet = extern struct {
    op: Op align(1),
    seq: u32 align(1),
};

pub const header_len: usize = 1 + @sizeOf(Packet);
pub const max_payload: usize = route.max_payload - header_len;
const scratch_size: usize = route.max_payload;

fn emit(node: *slora.Node, dest: types.Address, op: Op, seq: u32, payload: []const u8, now_ns: u64) void {
    if (payload.len > max_payload) return;

    var scratch: [scratch_size]u8 = undefined;
    scratch[0] = @intFromEnum(types.Category.echo);

    const body: Packet = .{ .op = op, .seq = seq };
    @memcpy(scratch[1..header_len], std.mem.asBytes(&body));
    @memcpy(scratch[header_len .. header_len + payload.len], payload);

    route.send(node, dest, node.addr, scratch[0 .. header_len + payload.len], now_ns);
}

pub fn send(node: *slora.Node, dest: types.Address, payload: []const u8, now_ns: u64) void {
    const seq = node.echo_seq;
    node.echo_seq +%= 1;
    emit(node, dest, .req, seq, payload, now_ns);
}

pub fn handleDelivered(node: *slora.Node, source: types.Address, frame: []const u8, now_ns: u64) void {
    if (frame.len < header_len) return;

    var body: Packet = undefined;
    @memcpy(std.mem.asBytes(&body), frame[1..header_len]);
    const payload = frame[header_len..];

    switch (body.op) {
        .req => emit(node, source, .res, body.seq, payload, now_ns),
        .res => {
            const start = node.tx_buf.items.len;
            node.tx_buf.appendSliceAssumeCapacity(payload);
            node.actions.appendAssumeCapacity(.{ .deliver = .{
                .from = source,
                .payload = node.tx_buf.items[start..],
            } });
        },
    }
}
