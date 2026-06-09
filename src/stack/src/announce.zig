//! Identity broadcasts.
//! Each node emits a signed Announce frame on the broadcast radio every few
//! seconds. The frame carries the node's Ed25519 public key, which a receiver
//! hashes to recover the sender's address and verifies the signature against.
//! Slot 0 of every slotframe is reserved for an Enhanced Beacon that carries
//! the absolute slot number, so a fresh node can hear one beacon and align its
//! TSCH clock to the rest of the network.

const std = @import("std");
const types = @import("types.zig");
const slora = @import("slora.zig");
const tsch = @import("tsch.zig");
const link = @import("link.zig");
const crypto = @import("crypto.zig");

pub const asn_offset: usize = 1;
pub const asn_size: usize = 8;
pub const signed_body_offset: usize = asn_offset + asn_size;
pub const signed_body_size: usize = 32;
pub const sig_offset: usize = signed_body_offset + signed_body_size;
pub const frame_len: usize = sig_offset + crypto.sig_len;

pub fn buildAndQueueEb(node: *slora.Node, now_ns: u64) void {
    _ = now_ns;

    var scratch: [frame_len]u8 = @splat(0);
    scratch[0] = @intFromEnum(types.Category.announce);
    @memcpy(scratch[signed_body_offset..sig_offset], &node.keypair.public_key.bytes);

    var signed_buf: [1 + signed_body_size]u8 = undefined;
    signed_buf[0] = scratch[0];
    @memcpy(signed_buf[1..], scratch[signed_body_offset..sig_offset]);

    const sig = node.keypair.sign(&signed_buf, null) catch unreachable;
    @memcpy(scratch[sig_offset..], &sig.toBytes());

    const owned = node.gpa.dupe(u8, &scratch) catch return;
    node.broadcast_queue.append(node.gpa, owned) catch node.gpa.free(owned);
}

pub fn handleRx(node: *slora.Node, frame: []const u8, now_ns: u64) !void {
    if (frame.len != frame_len) return;

    const asn = std.mem.readInt(u64, frame[asn_offset..sig_offset][0..asn_size], .little);
    const pubkey: [32]u8 = frame[signed_body_offset..sig_offset][0..signed_body_size].*;

    var signed_buf: [1 + signed_body_size]u8 = undefined;
    signed_buf[0] = frame[0];
    @memcpy(signed_buf[1..], &pubkey);

    const pk = crypto.Ed25519.PublicKey.fromBytes(pubkey) catch return;
    const sig = crypto.Ed25519.Signature.fromBytes(frame[sig_offset..][0..crypto.sig_len].*);
    sig.verify(&signed_buf, pk) catch return;

    const peer = types.addressFromPubkey(pubkey);
    if (peer == node.addr) return;

    const gop = try node.neighbors.getOrPut(node.gpa, peer);
    if (!gop.found_existing) {
        gop.value_ptr.* = .{ .pubkey = @bitCast(pubkey) };
    } else {
        gop.value_ptr.pubkey = @bitCast(pubkey);
    }

    tsch.syncTimeFromEb(node, peer, asn, now_ns);

    if (node.target_peers.count() > 0 and node.target_peers.contains(peer)) {
        _ = link.tryEstablish(node, peer, now_ns);
    }
}
