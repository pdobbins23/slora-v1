//! Shared type aliases and constants for the rest of the stack.
//! An `Address` is the BLAKE2b-128 hash of an Ed25519 public key, so any node
//! can verify a sender's address from its public key without a PKI.
//! The personalization-context constants below keep the different BLAKE2 uses
//! in the protocol separate so they cannot collide.

const std = @import("std");

pub const Allocator = std.mem.Allocator;
pub const Blake = std.crypto.hash.blake2.Blake2b128;
pub const Blake256 = std.crypto.hash.blake2.Blake2b256;
pub const KeyPair = std.crypto.sign.Ed25519.KeyPair;
pub const X25519 = std.crypto.dh.X25519;

pub const Address = u128;
pub const PublicKey = u256;

pub const Envelope = extern struct {
    to: Address align(1),
    from: Address align(1),
};

pub const Category = enum(u8) { announce, link, route, relay, echo };

const addr_context: [16]u8 = "slora.address.hs".*;

pub fn addressFromPubkey(pubkey: [32]u8) Address {
    var out: [16]u8 = undefined;
    Blake.hash(&pubkey, &out, .{ .context = addr_context });
    return @bitCast(out);
}
