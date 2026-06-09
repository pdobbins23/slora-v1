//! ChaCha20-Poly1305 sealing and link-key derivation.
//! Two neighbours derive a shared link key by running X25519 over their
//! long-term keypairs and then hashing the result with BLAKE2b-256.
//! Both addresses are mixed into the hash in sorted order so the two
//! endpoints compute the same key without caring who initiated.

const std = @import("std");
const types = @import("types.zig");

pub const AEAD = std.crypto.aead.chacha_poly.ChaCha20Poly1305;
pub const Ed25519 = std.crypto.sign.Ed25519;

pub const nonce_len: usize = AEAD.nonce_length;
pub const tag_len: usize = AEAD.tag_length;
pub const sig_len: usize = Ed25519.Signature.encoded_length;

const link_key_context: [16]u8 = "slora.link.key01".*;

pub fn deriveLinkKey(shared: [32]u8, a: types.Address, b: types.Address) [32]u8 {
    var out: [32]u8 = undefined;
    var h = types.Blake256.init(.{ .context = link_key_context });
    h.update(&shared);
    const lo: [16]u8 = @bitCast(@min(a, b));
    const hi: [16]u8 = @bitCast(@max(a, b));
    h.update(&lo);
    h.update(&hi);
    h.final(&out);
    return out;
}

pub fn linkNonce(my: types.Address, peer: types.Address, counter: u64) [nonce_len]u8 {
    var n: [nonce_len]u8 = @splat(0);
    std.mem.writeInt(u64, n[0..8], counter, .little);
    // Top bit of the nonce is the direction flag, so the two endpoints can share a counter without colliding.
    if (my > peer) n[nonce_len - 1] |= 0x80;
    return n;
}

pub fn seal(pt_inout: []u8, tag_out: *[tag_len]u8, nonce: [nonce_len]u8, key: [32]u8) void {
    AEAD.encrypt(pt_inout, tag_out, pt_inout, &.{}, nonce, key);
}

pub fn open(ct_inout: []u8, tag: [tag_len]u8, nonce: [nonce_len]u8, key: [32]u8) !void {
    try AEAD.decrypt(ct_inout, ct_inout, tag, &.{}, nonce, key);
}
