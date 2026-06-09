//! TSCH schedule constants and helpers.
//! The slotframe has 16 slots of 100 ms each, and there are 16 physical
//! channels spaced across the US ISM band. The physical channel for a given
//! slot rotates through a fixed permutation, so two cells with the same
//! channel offset never collide on consecutive slots.
//! Each node has one autonomous RX cell whose `(slot_offset, ch_offset)` is
//! the BLAKE2b hash of its address, per RFC 9033 MSF. Any other node can
//! compute the same cell from the address alone, which is what lets a sender
//! transmit to a peer without any prior schedule negotiation.

const std = @import("std");
const types = @import("types.zig");

pub const slot_duration_ns: u64 = 100_000_000;
pub const slotframe_len: u32 = 16;
pub const num_channels: u8 = 16;

pub const base_freq_hz: u64 = 903_250_000;
pub const channel_spacing_hz: u32 = 1_600_000;
pub const sub_channel_hz: u32 = 500_000;

// Fixed permutation of the 16 physical channels.
// Two cells with the same ch_offset land on different channels at consecutive ASNs, which spreads interference.
pub const hop_sequence: [num_channels]u8 = .{ 0, 5, 10, 15, 1, 6, 11, 2, 7, 12, 3, 8, 13, 4, 9, 14 };

const msf_context: [16]u8 = "slora.msf.cell01".*;

pub const Cell = struct { slot_offset: u32, ch_offset: u8 };

pub fn slotOffset(asn: u64) u32 {
    return @intCast(asn % slotframe_len);
}

pub fn wallclockAtAsn(asn: u64) u64 {
    return asn * slot_duration_ns;
}

pub fn physicalChannel(asn: u64, ch_offset: u8) u8 {
    std.debug.assert(ch_offset < num_channels);
    const idx = (asn + ch_offset) % num_channels;
    return hop_sequence[idx];
}

pub fn frequencyHz(ch: u8) u32 {
    std.debug.assert(ch < num_channels);
    return @intCast(base_freq_hz + @as(u64, ch) * channel_spacing_hz);
}

pub fn autonomousRxCell(addr: types.Address) Cell {
    const bytes: [16]u8 = @bitCast(addr);
    var out: [16]u8 = undefined;
    types.Blake.hash(&bytes, &out, .{ .context = msf_context });

    // Slot 0 is reserved for Enhanced Beacons, so the hash range is 1..slotframe_len.
    const cell: Cell = .{
        .slot_offset = 1 + (std.mem.readInt(u32, out[0..4], .little) % (slotframe_len - 1)),
        .ch_offset = @intCast(std.mem.readInt(u32, out[4..8], .little) % num_channels),
    };

    std.debug.assert(cell.slot_offset >= 1 and cell.slot_offset < slotframe_len);
    std.debug.assert(cell.ch_offset < num_channels);
    return cell;
}
