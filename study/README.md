# study/

Reproduction harness for the 20,000-run scaling study.

## Scripts

```
generate.py       writes simulations/study.ini + config_list.txt + topology_specs.json
run_all.sh        runs every config sequentially; resumable
run_parallel.sh   parallel variant (one worker per CPU by default)
analyze.py        parses .sca into results.csv + heatmaps + topology PNGs
```

## Default parameters

5 levels (L1-L5) x 5 traffic intensities (T1-T5) x 2 layouts (random,
sparse-random) x 100 trials x 4 protocols = 20,000 simulation runs.

Override the trial count via env var:

```sh
STUDY_TRIALS=10 python3 study/generate.py   # 2,000-run smoke study
```

## Reproduce end-to-end

All commands run inside the container; from the host use
`just shell bash -c "..."` or open a shell with `just shell`.

```sh
python3 /work/study/generate.py             # rewrite study.ini
bash /work/study/run_parallel.sh            # ~24h on a 32-core machine
python3 /work/study/analyze.py              # → results.csv + plots
```

The runner skips any `.sca` already present in
`/work/simulations/results/`, so it's safe to interrupt and resume.
OMNeT++ *appends* to existing `.sca` files instead of overwriting, so
delete a config's `.sca` before re-running that specific config.

## Outputs

After `analyze.py`:

```
results.csv               one row per (level, traffic, layout, trial),
                          36 metric columns (9 per protocol x 4 protocols)
plots/matrix_*.png        heatmaps for delivered, ratio, latency,
                          one per layout (_random, _sparse_random)
topo_viz/<layout>/*.png   per-cell topology + source-dest paths
```

## Default per-PHY parameters

| parameter | value |
|---|---|
| spreading factor | SF7 |
| bandwidth | 500 kHz |
| TX power | -30 dBm |
| centre frequency | 902.25 MHz (flood) / 16-channel hop (slora) |
| sim duration | 300 s |
| seed formula | `1000 + 17 * trial` |
