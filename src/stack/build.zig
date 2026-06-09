//! Builds the SLoRa stack as a static library with a C ABI.
//! `zig build` produces libslora.a and slora.h for the OMNeT++ harness to
//! link against. `zig build test` runs the per-module unit tests.

const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const test_mod = b.createModule(.{
        .root_source_file = b.path("src/slora.zig"),
        .target = target,
        .optimize = optimize,
    });
    const tests = b.addTest(.{ .root_module = test_mod });
    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&b.addRunArtifact(tests).step);

    const lib_mod = b.createModule(.{
        .root_source_file = b.path("src/c_abi.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .stack_check = false,
    });
    const lib = b.addLibrary(.{
        .name = "slora",
        .root_module = lib_mod,
        .linkage = .static,
    });
    lib.installHeader(b.path("include/slora.h"), "slora.h");
    b.installArtifact(lib);
}
