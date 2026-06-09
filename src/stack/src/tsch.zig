//! Runtime TSCH driver.
//! Once per slot the host wakes the node up. If the wakeup falls on slot 0 of
//! a slotframe, the driver emits an Enhanced Beacon so receivers can align
//! their clocks. It then drains any pending broadcast frames, transmits on
//! any link whose peer owns this slot, and arms the data radio to listen on
//! the node's own autonomous RX cell. The slot math itself lives in
//! `schedule.zig`.

const std = @import("std");
const types = @import("types.zig");
const schedule = @import("schedule.zig");
const slora = @import("slora.zig");
const relay = @import("relay.zig");
const announce = @import("announce.zig");

const frames_per_tx_cell: usize = 4;
const broadcasts_drained_per_slot: usize = 32;

pub fn currentAsn(node: *const slora.Node, now_ns: u64) u64 {
    const adjusted: i128 = @as(i128, now_ns) + node.asn_offset_ns;
    std.debug.assert(adjusted >= 0);
    return @intCast(@as(u128, @intCast(adjusted)) / schedule.slot_duration_ns);
}

pub fn scheduleNextSlot(node: *slora.Node, now_ns: u64) void {
    const asn = currentAsn(node, now_ns);
    const next_sim: i128 = @as(i128, @intCast(schedule.wallclockAtAsn(asn + 1))) - node.asn_offset_ns;
    const delay: u64 = if (next_sim > @as(i128, now_ns)) @intCast(next_sim - @as(i128, now_ns)) else schedule.slot_duration_ns;

    node.actions.appendAssumeCapacity(.{ .schedule = .{
        .token = .slot,
        .delay_ns = delay,
    } });
}

pub fn handleSlotWakeup(node: *slora.Node, now_ns: u64) void {
    defer scheduleNextSlot(node, now_ns);

    const asn = currentAsn(node, now_ns);
    const slot_off = schedule.slotOffset(asn);
    std.debug.assert(slot_off < schedule.slotframe_len);

    if (node.eb_pending and slot_off == 0) {
        announce.buildAndQueueEb(node, now_ns);
        node.eb_pending = false;
    }

    drainBroadcasts(node);

    if (slot_off == 0) return;

    if (maybeTxOnCell(node, asn, slot_off)) {
        tuneToOwnRx(node, asn);
        return;
    }

    const my = schedule.autonomousRxCell(node.addr);
    if (slot_off == my.slot_offset) {
        tuneToOwnRx(node, asn);
    }
}

fn drainBroadcasts(node: *slora.Node) void {
    var n: usize = 0;
    while (n < broadcasts_drained_per_slot and node.broadcast_queue.items.len > 0) : (n += 1) {
        const frame = node.broadcast_queue.orderedRemove(0);
        node.actions.appendAssumeCapacity(.{ .tx_broadcast = frame });
    }
}

fn maybeTxOnCell(node: *slora.Node, asn: u64, slot_off: u32) bool {
    std.debug.assert(slot_off > 0 and slot_off < schedule.slotframe_len);

    var iter = node.links.iterator();
    while (iter.next()) |entry| {
        const peer = entry.key_ptr.*;
        const info = entry.value_ptr;
        if (info.pending_tx.items.len == 0) continue;

        const peer_cell = schedule.autonomousRxCell(peer);
        if (peer_cell.slot_offset != slot_off) continue;

        const freq = schedule.frequencyHz(schedule.physicalChannel(asn, peer_cell.ch_offset));
        var n: usize = 0;
        while (n < frames_per_tx_cell and info.pending_tx.items.len > 0) : (n += 1) {
            const pending = info.pending_tx.items[0];
            std.debug.assert(pending.payload.len > 0);
            const frame = relay.buildDataFrame(node, peer, info, pending.dest, pending.source, pending.payload) orelse break;
            node.actions.appendAssumeCapacity(.{ .tx_data = .{
                .bytes = frame,
                .freq_hz = freq,
                .bw_hz = schedule.sub_channel_hz,
            } });
            const head = info.pending_tx.orderedRemove(0);
            node.gpa.free(head.payload);
        }
        return true;
    }
    return false;
}

fn tuneToOwnRx(node: *slora.Node, asn: u64) void {
    const my = schedule.autonomousRxCell(node.addr);
    const ch = schedule.physicalChannel(asn, my.ch_offset);
    node.actions.appendAssumeCapacity(.{ .tune_data = .{
        .freq_hz = schedule.frequencyHz(ch),
        .bw_hz = schedule.sub_channel_hz,
    } });
}

pub fn syncTimeFromEb(node: *slora.Node, source: types.Address, eb_asn: u64, now_ns: u64) void {
    // Lock onto the first EB source the node hears and stay with it.
    // Multiple time sources would cause the node to bounce between offsets and miss slot boundaries.
    if (node.time_source) |ts| {
        if (ts != source) return;
    } else {
        node.time_source = source;
    }
    const new_offset: i128 = @as(i128, @intCast(schedule.wallclockAtAsn(eb_asn))) - @as(i128, now_ns);
    if (new_offset == node.asn_offset_ns) return;
    node.asn_offset_ns = new_offset;
    scheduleNextSlot(node, now_ns);
}
