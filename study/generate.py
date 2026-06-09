#!/usr/bin/env python3
"""
SLoRa vs Flood scaling study generator.

Sweeps two axes (network complexity and traffic intensity) across
two deployment-density layouts and four protocols. Output is an OMNeT++
study.ini section per (protocol, level, traffic, layout, trial) cell
plus a manifest that the runner consumes.

Axes:
  LEVEL    L1 (2 nodes) to L5 (90 nodes)
  TRAFFIC  T1 (0.1 pps, 10% active) to T5 (5.0 pps, 100% active)
  LAYOUTS  random (dense neighbourhood), sparse-random (city-scale)

Protocols (each cell runs all four):
  Slora        full stack (TSCH + AEAD + AODV-style routing)
  FloodSNR     Meshtastic-style managed flood
  FloodNaive   every node rebroadcasts every packet
  FloodMPR     OLSR-style MPR-elected rebroadcast

Sim duration: 300 s per cell. Trials per cell: 100 (override with
STUDY_TRIALS env var). Total: 5x5x2x100x4 = 20,000 simulation runs.

Output:
  /work/simulations/study.ini         omnetpp .ini sections
  /work/study/config_list.txt         tab-separated runner manifest
  /work/study/topology_specs.json     node positions per config
"""

import json
import math
import os
import random as _random
from pathlib import Path

OUT_INI = Path(os.environ.get("STUDY_INI", "/work/simulations/study.ini"))
OUT_LIST = Path(os.environ.get("STUDY_LIST", "/work/study/config_list.txt"))
OUT_TOPO = Path(os.environ.get("STUDY_TOPO", "/work/study/topology_specs.json"))

SIM_TIME_S = 300
TRIALS = int(os.environ.get("STUDY_TRIALS", "100"))

# Node count grows by hop-distance regime, not strictly geometric.
# L1-L2 are degenerate (pair and chain) to anchor the per-hop link budget.
# L3-L5 are dense meshes where routing matters.
LEVELS = [
    {"name": "L1", "label": "2 nodes / 1 direct unicast", "n_nodes": 2, "kind": "pair"},
    {"name": "L2", "label": "5 nodes / 1 chain (4 hops)", "n_nodes": 5, "kind": "chain"},
    {"name": "L3", "label": "15 nodes / 5 clusters", "n_nodes": 15, "kind": "clusters", "n_clusters": 5},
    {"name": "L4", "label": "45 nodes / 15 clusters", "n_nodes": 45, "kind": "clusters", "n_clusters": 15},
    {"name": "L5", "label": "90 nodes / 30 clusters", "n_nodes": 90, "kind": "clusters", "n_clusters": 30},
]


TRAFFIC = [
    {
        "name": "T1",
        "label": "very light  (0.1 pps, 10% active)",
        "rate_pps": 0.1,
        "active_frac": 0.10,
    },
    {
        "name": "T2",
        "label": "light       (0.3 pps, 25% active)",
        "rate_pps": 0.3,
        "active_frac": 0.25,
    },
    {
        "name": "T3",
        "label": "moderate    (1.0 pps, 50% active)",
        "rate_pps": 1.0,
        "active_frac": 0.50,
    },
    {
        "name": "T4",
        "label": "heavy       (2.0 pps, 75% active)",
        "rate_pps": 2.0,
        "active_frac": 0.75,
    },
    {
        "name": "T5",
        "label": "saturated   (5.0 pps, 100% active)",
        "rate_pps": 5.0,
        "active_frac": 1.00,
    },
]


PROTOCOLS = ["Slora", "FloodSNR", "FloodNaive", "FloodMPR"]

# Two layouts cover the two LoRa deployment regimes that matter.
# "random" is the dense-neighbourhood case where every node hears every other node.
# "sparse_random" widens the box by 1.8x linearly (3.24x area) so range becomes the bottleneck.
LAYOUTS = ["random", "sparse_random"]

LAYOUT_INFIX = {"random": "_rand", "sparse_random": "_sparse"}

# Random-layout bounding box per level.
# Sized to give ~1 cluster per km^2 at L4 and L5 (suburban-ish density).
RANDOM_AREA_M = {
    "L1": (800, 600),
    "L2": (1800, 600),
    "L3": (2000, 1300),
    "L4": (3000, 2000),
    "L5": (4000, 3000),
}

SPARSE_RANDOM_SCALE = 1.8
SPARSE_RANDOM_AREA_M = {
    name: (round(w * SPARSE_RANDOM_SCALE), round(h * SPARSE_RANDOM_SCALE))
    for name, (w, h) in RANDOM_AREA_M.items()
}


def random_positions(level, trial, sparse=False):
    # The salt keeps the sparse and dense topologies from sharing per-trial RNG sequences.
    salt = 0x5959 if sparse else 0
    rng = _random.Random(0xC10DA1A1 ^ (trial * 1009) ^ hash(level["name"]) ^ salt)
    area = SPARSE_RANDOM_AREA_M if sparse else RANDOM_AREA_M
    area_w, area_h = area[level["name"]]
    return [
        (rng.uniform(0, area_w), rng.uniform(0, area_h))
        for _ in range(level["n_nodes"])
    ]


def random_traffic_pairs(level, traffic, trial):
    """Pick (source_idx, dest_idx) pairs for this cell.

    Pair count scales with traffic active_frac up to a per-level cap.
    Source and destination are drawn without replacement, so each node
    participates in at most one role per trial.
    """
    rng = _random.Random(
        0xDE57DE57 ^ (trial * 7919) ^ hash(level["name"]) ^ (hash(traffic["name"]) * 31)
    )
    n = level["n_nodes"]
    if level["kind"] in ("pair", "chain"):
        n_pairs_max = 1
    else:
        n_pairs_max = level["n_clusters"]
    n_pairs = max(1, math.ceil(traffic["active_frac"] * n_pairs_max))

    nodes = list(range(n))
    rng.shuffle(nodes)
    pairs = []
    for i in range(n_pairs):
        src = nodes[2 * i]
        dst = nodes[2 * i + 1]
        pairs.append((src, dst))
    return pairs


def positions_for(level, layout, trial):
    return random_positions(level, trial, sparse=(layout == "sparse_random"))


def positions_block(positions):
    out = []
    for idx, (x, y) in enumerate(positions):
        out.append(f"**.nodes[{idx}].mobility.initialX = {x:.2f}m")
        out.append(f"**.nodes[{idx}].mobility.initialY = {y:.2f}m")
    return out


def cfg_name(proto, level, traffic, layout, trial):
    infix = LAYOUT_INFIX[layout]
    return f"{proto}_{level['name']}_{traffic['name']}{infix}_T{trial}"


def traffic_pairs(level, traffic, layout, trial):
    """Return list of (source_idx, dest_offset) tuples for this cell."""
    pairs = random_traffic_pairs(level, traffic, trial)
    return [(s, (d - s) % level["n_nodes"]) for (s, d) in pairs]


def render_slora(level, traffic, layout, trial):
    name = cfg_name("Slora", level, traffic, layout, trial)
    seed = 1000 + trial * 17
    n = level["n_nodes"]
    pairs = traffic_pairs(level, traffic, layout, trial)
    # Random and sparse-random layouts have no fixed cluster targets.
    # Each node falls back to nearest-N neighbours up to MAX_LINKS_PER_NODE.
    target_peers = {}

    lines = [
        f"[Config {name}]",
        "extends = Slora",
        f"sim-time-limit = {SIM_TIME_S}s",
        f"seed-set = {seed}",
        "",
        f"**.numNodes = {n}",
    ]
    lines.extend(positions_block(positions_for(level, layout, trial)))
    lines.extend(
        [
            "",
            "**.nodes[*].app.broadcastTxPowerDbm = -30dBm",
            "**.nodes[*].app.dataTxPowerDbm = -30dBm",
            "**.nodes[*].app.announceInterval = 5s",
            "**.nodes[*].app.announceJitter   = 1s",
            "",
            '**.nodes[*].app.trafficMode = "unicast"',
            "**.nodes[*].app.trafficPayloadBytes = 32",
            f"**.nodes[*].app.trafficInterval = {1.0 / traffic['rate_pps']}s",
            "",
        ]
    )
    for src, offset in pairs:
        lines.append(f"**.nodes[{src}].app.trafficEnabled = true")
        lines.append(f"**.nodes[{src}].app.trafficDestOffset = {offset}")
    lines.append("**.nodes[*].app.trafficEnabled = false")
    for idx, peers in target_peers.items():
        lines.append(f'**.nodes[{idx}].app.targetPeerOffsets = "{peers}"')
    lines.append("")
    return "\n".join(lines) + "\n", name


def render_flood(level, traffic, layout, trial, mode):
    name = cfg_name(mode, level, traffic, layout, trial)
    seed = 1000 + trial * 17
    n = level["n_nodes"]
    pairs = traffic_pairs(level, traffic, layout, trial)

    flood_mode = {"FloodNaive": "naive", "FloodSNR": "snr", "FloodMPR": "mpr"}[mode]

    lines = [
        f"[Config {name}]",
        "network = flood.FloodMesh",
        f"sim-time-limit = {SIM_TIME_S}s",
        f"seed-set = {seed}",
        "",
        f"**.numNodes = {n}",
    ]
    lines.extend(positions_block(positions_for(level, layout, trial)))
    lines.extend(
        [
            "",
            "**.nodes[*].numApps = 1",
            '**.nodes[*].app[0].typename = "flood.FloodApp"',
            f'**.nodes[*].app[0].mode = "{flood_mode}"',
            "**.nodes[*].app[0].initialLoRaSF = 7",
            "**.nodes[*].app[0].initialLoRaBW = 500kHz",
            "**.nodes[*].app[0].initialLoRaCR = 5",
            "**.nodes[*].app[0].initialLoRaCF = 902.25MHz",
            "**.nodes[*].app[0].initialLoRaTP = -30dBm",
            "",
            "**.nodes[*].app[0].payloadSizeBytes = 32B",
            f"**.nodes[*].app[0].sendInterval = {1.0 / traffic['rate_pps']}s",
            "**.nodes[*].app[0].initialHopLimit = 5",
            "",
            "**.nodes[*].app[0].cwMin = 2",
            "**.nodes[*].app[0].cwMax = 8",
            "**.nodes[*].app[0].slotTime = 100ms",
            "**.nodes[*].app[0].snrGoodDb = -5",
            "**.nodes[*].app[0].snrBadDb = -15",
            "",
        ]
    )
    for src, offset in pairs:
        lines.append(f"**.nodes[{src}].app[0].isSource = true")
        lines.append(f"**.nodes[{src}].app[0].trafficDestOffset = {offset}")
    lines.append("**.nodes[*].app[0].isSource = false")
    lines.append("")
    return "\n".join(lines) + "\n", name


def main():
    chunks = ["# Auto-generated by generate.py. Do not edit.\n\n"]
    manifest = []
    topo_specs = {}

    for level in LEVELS:
        for traffic in TRAFFIC:
            for layout in LAYOUTS:
                for trial in range(TRIALS):
                    row_names = []
                    slora_text, slora_name = render_slora(level, traffic, layout, trial)
                    chunks.append(slora_text)
                    row_names.append(slora_name)
                    for proto in ("FloodSNR", "FloodNaive", "FloodMPR"):
                        text, name = render_flood(level, traffic, layout, trial, proto)
                        chunks.append(text)
                        row_names.append(name)

                    pairs = traffic_pairs(level, traffic, layout, trial)
                    sources = [s for s, _ in pairs]
                    dest_offsets = [o for _, o in pairs]
                    spec = {
                        "level": level["name"],
                        "label": level["label"],
                        "traffic": traffic["name"],
                        "traffic_label": traffic["label"],
                        "layout": layout,
                        "trial": trial,
                        "positions": positions_for(level, layout, trial),
                        "sources": sources,
                        "dest_offsets": dest_offsets,
                        "n_nodes": level["n_nodes"],
                    }
                    for n in row_names:
                        topo_specs[n] = spec

                    manifest.append(
                        (level["name"], traffic["name"], layout, trial, *row_names)
                    )

    OUT_INI.parent.mkdir(parents=True, exist_ok=True)
    OUT_INI.write_text("".join(chunks))
    OUT_LIST.parent.mkdir(parents=True, exist_ok=True)
    rows = ["\t".join(map(str, row)) for row in manifest]
    OUT_LIST.write_text("\n".join(rows) + "\n")
    OUT_TOPO.write_text(json.dumps(topo_specs, indent=2))

    n_cells = len(LEVELS) * len(TRAFFIC) * len(LAYOUTS) * TRIALS
    n_sims = n_cells * len(PROTOCOLS)
    print(f"Wrote {OUT_INI}")
    print(
        f"  {n_sims} configs ({len(LEVELS)} L x {len(TRAFFIC)} T x {len(LAYOUTS)} layouts x {TRIALS} trials x {len(PROTOCOLS)} protos)"
    )
    print(f"Wrote {OUT_LIST} ({len(manifest)} cells)")
    print(f"Wrote {OUT_TOPO}")
    print()
    print("Levels:")
    for lv in LEVELS:
        print(f"  {lv['name']}: {lv['label']}")
    print("Traffic levels:")
    for tr in TRAFFIC:
        print(f"  {tr['name']}: {tr['label']}")
    print("Protocols:", ", ".join(PROTOCOLS))


if __name__ == "__main__":
    main()
