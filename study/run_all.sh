#!/bin/bash
# Runs every config listed in config_list.txt. Designed to run inside the
# podman container (paths assume /work mount). Continues past failed runs.
# Resumable: skips configs whose .sca already exists.
set -u

LIST="${STUDY_LIST:-/work/study/config_list.txt}"
INI="${STUDY_INI:-simulations/omnetpp.ini}"
BIN="${STUDY_BIN:-./build/gcc-release/slora}"
NED_PATH="src:/opt/inet4.4/src:/opt/flora/src:/opt/flora/simulations"

if [[ ! -f "$LIST" ]]; then
    echo "Config list missing: $LIST" >&2
    exit 1
fi

cd /work

total_cells=$(wc -l < "$LIST")
n_protos=$(awk -F'\t' 'NR==1 { print NF - 4; exit }' "$LIST")
total_sims=$((total_cells * n_protos))
idx=0
fail=0

while IFS=$'\t' read -r level traffic layout trial cfgs; do
    IFS=$'\t' read -ra cfgs_arr <<< "$cfgs"
    for cfg in "${cfgs_arr[@]}"; do
        idx=$((idx + 1))
        sca="simulations/results/${cfg}-#0.sca"
        if [[ -s "$sca" ]]; then
            echo "[${idx}/${total_sims}] skip $cfg (already present)"
            continue
        fi
        echo "[${idx}/${total_sims}] run $cfg"
        if ! "$BIN" -u Cmdenv -c "$cfg" -n "$NED_PATH" "$INI" > /tmp/last_run.log 2>&1; then
            echo "  FAILED, last log:" >&2
            tail -5 /tmp/last_run.log >&2
            fail=$((fail + 1))
        fi
    done
done < "$LIST"

echo
echo "Done. Failures: $fail / ${total_sims}"
