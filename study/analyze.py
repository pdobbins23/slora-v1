#!/usr/bin/env python3
"""
Aggregate the study's OMNeT++ scalar outputs into a single CSV plus
heatmaps and topology renders.

Walks every `.sca` in /work/simulations/results/, pulls the per-protocol
scalars (delivered, sent, latencyMean, ...), pivots them into one row per
(level, traffic, layout, trial) with columns for each protocol, and
writes:
  /work/study/results.csv
  /work/study/plots/matrix_{delivered,ratio,latency}_{random,sparse_random}.png
  /work/study/topo_viz/{random,sparse_random}/Slora_*.png

Run inside the container.
"""

import csv
import json
import math
import re
from collections import defaultdict
from pathlib import Path
from statistics import mean, stdev

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

RESULTS_DIR = Path("/work/simulations/results")
STUDY_DIR = Path("/work/study")
PLOTS_DIR = STUDY_DIR / "plots"
TOPO_DIR = STUDY_DIR / "topo_viz"
LIST_FILE = STUDY_DIR / "config_list.txt"
TOPO_FILE = STUDY_DIR / "topology_specs.json"
CSV_FILE = STUDY_DIR / "results.csv"


SLORA_KEYS = {
    "delivered": r"^delivered$",
    "sent": r"^sent$",
    "latencyMean": r"^latencyMean$",
    "latencyMax": r"^latencyMax$",
    "throughputPps": r"^throughputPps$",
    "links": r"^links$",
    "forwarded": r"^forwarded$",
    "dropNoRoute": r"^dropNoRoute$",
    "dropNoLink": r"^dropNoLink$",
    "pendingTx": r"^pendingTx$",
}

FLOOD_KEYS = {
    "delivered": r"^pktsDeliveredToMe$",
    "sent": r"^pktsOriginated$",
    "latencyMean": r"^latencyMean$",
    "latencyMax": r"^latencyMax$",
    "throughputPps": r"^throughputPps$",
    "forwarded": r"^pktsForwarded$",
    "suppressed": r"^pktsSuppressed$",
    "lbtBackoff": r"^pktsLbtBackoff$",
}


_SCALAR_LINE = re.compile(r"^scalar\s+(\S+)\s+(\S+)\s+(\S+)\s*$")


def parse_sca(path, keyset):
    """Return summed metric values across all nodes in a single .sca file.

    Dedupes by (module, scalar_name) because OMNeT++ appends on rerun.
    """
    if not path.exists():
        return None
    per_scalar = {}
    for line in path.read_text().splitlines():
        m = _SCALAR_LINE.match(line)
        if not m:
            continue
        module, name, value = m.group(1), m.group(2), m.group(3)
        try:
            per_scalar[(module, name)] = float(value)
        except ValueError:
            pass

    totals = defaultdict(float)
    per_key_values = defaultdict(list)
    for (_module, name), value in per_scalar.items():
        for key, pat in keyset.items():
            if re.match(pat, name):
                totals[key] += value
                per_key_values[key].append(value)
                break

    out = dict(totals)
    for k in ("latencyMean", "latencyMax", "throughputPps"):
        nonzero = [v for v in per_key_values.get(k, []) if v > 0]
        out[k] = mean(nonzero) if nonzero else 0.0
    return out


def proto_keyset(proto):
    return SLORA_KEYS if proto == "Slora" else FLOOD_KEYS


def parse_all():
    """Parse every line of config_list.txt into rows.

    Manifest format:
      level  traffic  layout  trial  cfg1 ...
    """
    rows = []
    for line in LIST_FILE.read_text().strip().splitlines():
        parts = line.split("\t")
        if len(parts) < 5:
            continue
        level, traffic, layout, trial, *cfgs = parts
        row = {
            "level": level,
            "traffic": traffic,
            "layout": layout,
            "trial": int(trial),
        }
        for cfg in cfgs:
            proto = cfg.split("_")[0]
            sca = RESULTS_DIR / f"{cfg}-#0.sca"
            metrics = parse_sca(sca, proto_keyset(proto)) or {}
            prefix = proto.lower()
            for k, v in metrics.items():
                row[f"{prefix}_{k}"] = v
            row[f"{prefix}_present"] = bool(metrics)
        rows.append(row)
    return rows


def write_csv(rows):
    if not rows:
        return
    all_keys = set()
    for r in rows:
        all_keys.update(r.keys())
    head_cols = ["level", "traffic", "layout", "trial"]
    fieldnames = head_cols + sorted(k for k in all_keys if k not in set(head_cols))
    with CSV_FILE.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in rows:
            writer.writerow({k: r.get(k, "") for k in fieldnames})
    print(f"Wrote {CSV_FILE} ({len(rows)} rows)")


def aggregate(rows, protocols, layout=None):
    """Group by (level, traffic), optionally filtered to one layout.

    Each cell averages over the 100 trials in that (level, traffic, layout).
    """
    if layout is not None:
        rows = [r for r in rows if r.get("layout") == layout]
    grouped = defaultdict(list)
    for r in rows:
        grouped[(r["level"], r["traffic"])].append(r)

    def stat(group, field):
        vals = [r[field] for r in group if r.get(field) is not None]
        if not vals:
            return (0.0, 0.0)
        if len(vals) == 1:
            return (vals[0], 0.0)
        return (mean(vals), stdev(vals))

    agg = {}
    for key, group in grouped.items():
        cell = {"n_trials": len(group)}
        for proto in protocols:
            p = proto.lower()
            cell[f"{p}_delivered"] = stat(group, f"{p}_delivered")
            cell[f"{p}_sent"] = stat(group, f"{p}_sent")
            cell[f"{p}_latency"] = stat(group, f"{p}_latencyMean")
            cell[f"{p}_throughput"] = stat(group, f"{p}_throughputPps")
        agg[key] = cell
    return agg


def matrix_from(agg, levels, traffics, field):
    """Build [n_levels, n_traffics] mean matrix for a given field name."""
    m = np.full((len(levels), len(traffics)), np.nan)
    for i, lv in enumerate(levels):
        for j, tr in enumerate(traffics):
            cell = agg.get((lv, tr), {})
            v = cell.get(field, (0.0, 0.0))
            m[i, j] = v[0]
    return m


def plot_heatmap_grid(agg, levels, traffics, protocols, fname):
    """One heatmap per protocol showing delivered (mean) per (L, T) cell."""
    n = len(protocols)
    fig, axes = plt.subplots(1, n, figsize=(4.6 * n, 4.4))
    if n == 1:
        axes = [axes]
    all_vals = []
    for proto in protocols:
        m = matrix_from(agg, levels, traffics, f"{proto.lower()}_delivered")
        all_vals.extend(m.flatten().tolist())
    vmax = max(v for v in all_vals if not np.isnan(v)) or 1.0

    for ax, proto in zip(axes, protocols):
        m = matrix_from(agg, levels, traffics, f"{proto.lower()}_delivered")
        im = ax.imshow(
            m, cmap="viridis", origin="lower", aspect="auto", vmin=0, vmax=vmax
        )
        ax.set_xticks(range(len(traffics)))
        ax.set_xticklabels(traffics)
        ax.set_yticks(range(len(levels)))
        ax.set_yticklabels(levels)
        ax.set_xlabel("Traffic intensity")
        ax.set_ylabel("Network complexity")
        ax.set_title(f"{proto} delivered (mean)")
        for i in range(m.shape[0]):
            for j in range(m.shape[1]):
                val = m[i, j]
                if not np.isnan(val):
                    ax.text(
                        j,
                        i,
                        f"{int(val)}",
                        ha="center",
                        va="center",
                        color="white" if val < vmax * 0.6 else "black",
                        fontsize=8,
                    )
        fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)

    plt.tight_layout()
    PLOTS_DIR.mkdir(parents=True, exist_ok=True)
    plt.savefig(PLOTS_DIR / fname, dpi=120, bbox_inches="tight")
    plt.close()
    print(f"Wrote {PLOTS_DIR / fname}")


def plot_ratio_heatmaps(agg, levels, traffics, protocols, fname):
    """SLoRa / Flood-variant ratio heatmaps."""
    flood_protos = [p for p in protocols if p != "Slora"]
    n = len(flood_protos)
    if n == 0:
        return
    fig, axes = plt.subplots(1, n, figsize=(4.6 * n, 4.4))
    if n == 1:
        axes = [axes]

    for ax, flood in zip(axes, flood_protos):
        slora = matrix_from(agg, levels, traffics, "slora_delivered")
        fm = matrix_from(agg, levels, traffics, f"{flood.lower()}_delivered")
        ratio = np.divide(slora, fm, out=np.zeros_like(slora), where=fm > 0)

        im = ax.imshow(
            ratio, cmap="RdYlGn", origin="lower", aspect="auto", vmin=0, vmax=2.0
        )
        ax.set_xticks(range(len(traffics)))
        ax.set_xticklabels(traffics)
        ax.set_yticks(range(len(levels)))
        ax.set_yticklabels(levels)
        ax.set_xlabel("Traffic intensity")
        ax.set_ylabel("Network complexity")
        ax.set_title(f"SLoRa / {flood}  (>1 = SLoRa wins)")
        for i in range(ratio.shape[0]):
            for j in range(ratio.shape[1]):
                ax.text(
                    j,
                    i,
                    f"{ratio[i, j]:.2f}",
                    ha="center",
                    va="center",
                    fontsize=8,
                    color="black",
                )
        fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)

    plt.tight_layout()
    plt.savefig(PLOTS_DIR / fname, dpi=120, bbox_inches="tight")
    plt.close()
    print(f"Wrote {PLOTS_DIR / fname}")


def plot_latency_heatmaps(agg, levels, traffics, protocols, fname):
    n = len(protocols)
    fig, axes = plt.subplots(1, n, figsize=(4.6 * n, 4.4))
    if n == 1:
        axes = [axes]
    mats = [
        matrix_from(agg, levels, traffics, f"{p.lower()}_latency") for p in protocols
    ]
    vmax = max(np.nanmax(m) for m in mats if m.size and not np.all(np.isnan(m)))
    for ax, proto, m in zip(axes, protocols, mats):
        im = ax.imshow(
            m, cmap="magma", origin="lower", aspect="auto", vmin=0, vmax=vmax
        )
        ax.set_xticks(range(len(traffics)))
        ax.set_xticklabels(traffics)
        ax.set_yticks(range(len(levels)))
        ax.set_yticklabels(levels)
        ax.set_xlabel("Traffic intensity")
        ax.set_ylabel("Network complexity")
        ax.set_title(f"{proto} mean latency (s)")
        for i in range(m.shape[0]):
            for j in range(m.shape[1]):
                v = m[i, j]
                ax.text(
                    j,
                    i,
                    f"{v:.1f}",
                    ha="center",
                    va="center",
                    fontsize=7,
                    color="white" if v < vmax * 0.5 else "black",
                )
        fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    plt.tight_layout()
    plt.savefig(PLOTS_DIR / fname, dpi=120, bbox_inches="tight")
    plt.close()
    print(f"Wrote {PLOTS_DIR / fname}")


LORA_RANGE_M = 1500  # SF7/500kHz at -30 dBm free-space ~ 1.5 km
PATH_COLORS = plt.get_cmap("tab20").colors + plt.get_cmap("tab20b").colors


def plot_topology(spec_name, spec):
    fig, ax = plt.subplots(figsize=(9, 7))
    positions = spec["positions"]
    n = len(positions)
    sources = list(spec["sources"])
    dest_offsets = list(spec["dest_offsets"])
    dests = [(s + dest_offsets[i]) % n for i, s in enumerate(sources)]

    src_set = set(sources)
    dst_set = set(dests)

    for ai in range(n):
        xa, ya = positions[ai]
        for bi in range(ai + 1, n):
            xb, yb = positions[bi]
            if math.hypot(xa - xb, ya - yb) <= LORA_RANGE_M:
                ax.plot(
                    [xa, xb],
                    [ya, yb],
                    color="#bbbbbb",
                    linewidth=0.4,
                    alpha=0.35,
                    zorder=0,
                )

    legend_seen = set()
    for pair_idx, (s, d) in enumerate(zip(sources, dests)):
        color = PATH_COLORS[pair_idx % len(PATH_COLORS)]
        xs_, ys_ = positions[s]
        xd_, yd_ = positions[d]
        ax.annotate(
            "",
            xy=(xd_, yd_),
            xytext=(xs_, ys_),
            arrowprops=dict(arrowstyle="->", color=color, lw=1.8, alpha=0.85),
            zorder=2,
        )
        ax.text(
            (xs_ + xd_) / 2,
            (ys_ + yd_) / 2,
            f"#{pair_idx}",
            color=color,
            fontsize=8,
            weight="bold",
            ha="center",
            va="center",
            bbox=dict(
                boxstyle="round,pad=0.15", fc="white", ec=color, lw=0.6, alpha=0.9
            ),
        )

    for idx, (x, y) in enumerate(positions):
        if idx in src_set:
            ax.scatter(
                x,
                y,
                s=140,
                c="#1f77b4",
                marker="^",
                edgecolor="black",
                linewidth=0.6,
                label="source" if "source" not in legend_seen else "",
                zorder=3,
            )
            legend_seen.add("source")
        elif idx in dst_set:
            ax.scatter(
                x,
                y,
                s=140,
                c="#d62728",
                marker="v",
                edgecolor="black",
                linewidth=0.6,
                label="dest" if "dest" not in legend_seen else "",
                zorder=3,
            )
            legend_seen.add("dest")
        else:
            ax.scatter(
                x,
                y,
                s=45,
                c="#888",
                marker="o",
                label="relay" if "relay" not in legend_seen else "",
                zorder=3,
            )
            legend_seen.add("relay")
        ax.annotate(
            str(idx), (x, y), fontsize=6, xytext=(4, 4), textcoords="offset points"
        )

    xs_pos = [p[0] for p in positions]
    ys_pos = [p[1] for p in positions]
    x_span = max(xs_pos) - min(xs_pos) if xs_pos else 1.0
    y_span = max(ys_pos) - min(ys_pos) if ys_pos else 0.0
    if y_span < x_span * 0.1:
        pad = max(x_span * 0.1, 200)
        ax.set_ylim(min(ys_pos) - pad, max(ys_pos) + pad)
    ax.set_aspect("equal", adjustable="datalim")
    ax.set_xlabel("X (m)")
    ax.set_ylabel("Y (m)")
    ax.set_title(
        f"{spec_name}\n{spec['label']}  |  {spec.get('traffic_label', '')}\n"
        f"{len(sources)} src→dst pairs  |  faint lines = within {LORA_RANGE_M}m range",
        fontsize=10,
    )
    handles, labels_ = ax.get_legend_handles_labels()
    if handles:
        seen = {}
        for h, lbl in zip(handles, labels_):
            if lbl and lbl not in seen:
                seen[lbl] = h
        ax.legend(seen.values(), seen.keys(), loc="upper right", fontsize=9)
    ax.grid(alpha=0.3)
    plt.tight_layout()
    sub = TOPO_DIR / spec["layout"]
    sub.mkdir(parents=True, exist_ok=True)
    out = sub / f"{spec_name}.png"
    plt.savefig(out, dpi=110, bbox_inches="tight")
    plt.close()


def main():
    rows = parse_all()
    write_csv(rows)

    levels = sorted({r["level"] for r in rows})
    traffics = sorted({r["traffic"] for r in rows})
    protocols = []
    for r in rows:
        for k in r:
            if k.endswith("_delivered"):
                p = k.replace("_delivered", "")
                p = p[0].upper() + p[1:]
                if p == "Floodsnr":
                    p = "FloodSNR"
                elif p == "Floodnaive":
                    p = "FloodNaive"
                elif p == "Floodmpr":
                    p = "FloodMPR"
                if p not in protocols:
                    protocols.append(p)

    if not protocols:
        protocols = ["Slora", "FloodSNR", "FloodNaive"]

    layouts_present = sorted({r["layout"] for r in rows})

    for layout in layouts_present:
        agg = aggregate(rows, protocols, layout=layout)
        suffix = f"_{layout}"
        plot_heatmap_grid(
            agg, levels, traffics, protocols, f"matrix_delivered{suffix}.png"
        )
        plot_ratio_heatmaps(
            agg, levels, traffics, protocols, f"matrix_ratio{suffix}.png"
        )
        plot_latency_heatmaps(
            agg, levels, traffics, protocols, f"matrix_latency{suffix}.png"
        )

    if TOPO_FILE.exists():
        specs = json.loads(TOPO_FILE.read_text())
        drawn = set()
        for name, spec in specs.items():
            if not name.startswith("Slora_"):
                continue
            # Random and sparse-random vary positions per trial, so draw every (L, T, layout, trial).
            key = (spec["level"], spec.get("traffic"), spec["layout"], spec.get("trial"))
            if key in drawn:
                continue
            drawn.add(key)
            plot_topology(name, spec)
        print(f"Drew {len(drawn)} topology figures into {TOPO_DIR}")

    print()
    print("=== Aggregated summary ===")
    for layout in layouts_present:
        print(f"--- layout: {layout} ---")
        agg = aggregate(rows, protocols, layout=layout)
        for lv in levels:
            for tr in traffics:
                stats = agg.get((lv, tr))
                if not stats:
                    continue
                summary_parts = []
                for proto in protocols:
                    p = proto.lower()
                    d = stats.get(f"{p}_delivered", (0, 0))
                    lat = stats.get(f"{p}_latency", (0, 0))
                    summary_parts.append(
                        f"{proto}={d[0]:.0f}±{d[1]:.0f} ({lat[0]:.1f}s)"
                    )
                print(f"  {lv} {tr}: " + "  |  ".join(summary_parts))


if __name__ == "__main__":
    main()
