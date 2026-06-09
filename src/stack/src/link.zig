//! Per-neighbour link setup.
//! The initiator sends a Link/REQUEST that carries its X25519 public key, and
//! the responder replies with a Link/RESPONSE carrying its own X25519 public
//! key. Each side then runs ECDH over its private key and the peer's public
//! key, derives the shared ChaCha20-Poly1305 link key, and the link is up.
//! Once the link exists, the relay layer hands it data packets to seal and the
//! TSCH layer schedules transmissions on the peer's autonomous cell.

const std = @import("std");
const types = @import("types.zig");
const slora = @import("slora.zig");
const crypto = @import("crypto.zig");
const route = @import("route.zig");

pub const Op = enum(u8) { request, response };

pub const Packet = extern struct {
    op: Op align(1),
    env: types.Envelope align(1),
    pubkey: [32]u8 align(1),
    xchg_key: [32]u8 align(1),
};

const body_len: usize = @sizeOf(Packet);
pub const frame_len: usize = 1 + body_len + crypto.sig_len;

const retry_interval_ns: u64 = 5 * std.time.ns_per_s;
pub const max_links_per_node: u32 = 8;

fn queueFrame(node: *slora.Node, op: Op, peer: types.Address) void {
    std.debug.assert(peer != node.addr);

    const body: Packet = .{
        .op = op,
        .env = .{ .to = peer, .from = node.addr },
        .pubkey = node.keypair.public_key.bytes,
        .xchg_key = node.x_keypair.public_key,
    };

    var scratch: [frame_len]u8 = undefined;
    scratch[0] = @intFromEnum(types.Category.link);
    @memcpy(scratch[1 .. 1 + body_len], std.mem.asBytes(&body));

    const sig = node.keypair.sign(scratch[0 .. 1 + body_len], null) catch unreachable;
    @memcpy(scratch[1 + body_len ..][0..crypto.sig_len], &sig.toBytes());

    const owned = node.gpa.dupe(u8, &scratch) catch return;
    node.broadcast_queue.append(node.gpa, owned) catch node.gpa.free(owned);
}

pub fn tryEstablish(node: *slora.Node, peer: types.Address, now_ns: u64) bool {
    if (peer == node.addr) return false;
    if (node.links.contains(peer)) return true;
    if (node.links.count() >= max_links_per_node) return false;

    const gop = node.neighbors.getOrPut(node.gpa, peer) catch return false;
    if (!gop.found_existing) gop.value_ptr.* = .{ .pubkey = 0 };
    if (gop.value_ptr.last_link_req_ns != 0 and now_ns < gop.value_ptr.last_link_req_ns + retry_interval_ns) return true;
    gop.value_ptr.last_link_req_ns = now_ns;

    queueFrame(node, .request, peer);
    return true;
}

pub fn handleRx(node: *slora.Node, frame: []const u8) !void {
    if (frame.len != frame_len) return;
    const op = std.enums.fromInt(Op, frame[1]) orelse return;

    var body: Packet = undefined;
    @memcpy(std.mem.asBytes(&body), frame[1 .. 1 + body_len]);

    if (body.env.to != node.addr or body.env.from == node.addr) return;
    if (types.addressFromPubkey(body.pubkey) != body.env.from) return;

    const pk = crypto.Ed25519.PublicKey.fromBytes(body.pubkey) catch return;
    const sig = crypto.Ed25519.Signature.fromBytes(frame[frame.len - crypto.sig_len ..][0..crypto.sig_len].*);
    sig.verify(frame[0 .. frame.len - crypto.sig_len], pk) catch return;

    const ngop = try node.neighbors.getOrPut(node.gpa, body.env.from);
    if (!ngop.found_existing) {
        ngop.value_ptr.* = .{ .pubkey = @bitCast(body.pubkey) };
    } else {
        ngop.value_ptr.pubkey = @bitCast(body.pubkey);
    }

    const shared = try types.X25519.scalarmult(node.x_keypair.secret_key, body.xchg_key);
    const link_key = crypto.deriveLinkKey(shared, node.addr, body.env.from);

    const gop = try node.links.getOrPut(node.gpa, body.env.from);
    if (!gop.found_existing) {
        gop.value_ptr.* = .{ .link_key = link_key };
    } else {
        gop.value_ptr.link_key = link_key;
    }

    route.flushPendingFor(node, body.env.from);

    if (op == .request) queueFrame(node, .response, body.env.from);
}
