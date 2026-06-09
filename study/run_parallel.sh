#!/bin/bash
# Parallel runner for the study. Runs N sims concurrently using xargs -P.
# Default N = nproc - 2 to leave headroom; override with WORKERS=N.
#
# Each sim uses ~1 core and ~300-500 MB of RAM, so on a 7700X (8c/16t) with
# 32 GB the safe upper bound is ~12 workers (~6 GB RAM, leaves headroom for
# the container's other processes).
#
# Resumable: skips configs whose .sca already exists.

set -u

LIST="${STUDY_LIST:-/work/study/config_list.txt}"
INI="${STUDY_INI:-simulations/omnetpp.ini}"
BIN="${STUDY_BIN:-./build/gcc-release/slora}"
NED_PATH="src:/opt/inet4.4/src:/opt/flora/src:/opt/flora/simulations"
WORKERS="${WORKERS:-$(( $(nproc) - 2 ))}"
[ "$WORKERS" -lt 1 ] && WORKERS=1

if [[ ! -f "$LIST" ]]; then
    echo "Config list missing: $LIST" >&2
    exit 1
fi

cd /work

# Extract every config name from the manifest (columns 5+). Skip if already
# done. Print one per line to feed xargs.
tmp_jobs="$(mktemp)"
trap 'rm -f "$tmp_jobs"' EXIT

awk -F'\t' '{
    for (i = 5; i <= NF; i++) print $i
}' "$LIST" | while read -r cfg; do
    sca="simulations/results/${cfg}-#0.sca"
    [ -s "$sca" ] || echo "$cfg"
done > "$tmp_jobs"

total_pending=$(wc -l < "$tmp_jobs")
total_all=$(awk -F'\t' '{ for (i=5;i<=NF;i++) print $i }' "$LIST" | wc -l)
echo "Manifest: $total_all configs total; $total_pending pending; running with $WORKERS workers"

if [ "$total_pending" -eq 0 ]; then
    echo "Nothing to do."
    exit 0
fi

start_ts=$(date +%s)

# Each worker runs one sim, redirecting its log to /tmp/run_<pid>.log so they
# don't clobber each other. Printed line is "[i/N] OK cfg" or "[i/N] FAIL cfg".
counter_file="$(mktemp)"
trap 'rm -f "$tmp_jobs" "$counter_file"' EXIT
echo 0 > "$counter_file"

run_one() {
    local cfg="$1"
    local log="/tmp/run_${BASHPID}.log"
    if "$BIN" -u Cmdenv -c "$cfg" -n "$NED_PATH" "$INI" > "$log" 2>&1; then
        # Atomically bump and read the counter for stable progress line.
        ( flock 9
          n=$(< "$counter_file"); n=$((n + 1)); echo "$n" > "$counter_file"
          printf '[%d/%d] OK   %s\n' "$n" "$total_pending" "$cfg"
        ) 9>>"$counter_file"
    else
        ( flock 9
          n=$(< "$counter_file"); n=$((n + 1)); echo "$n" > "$counter_file"
          printf '[%d/%d] FAIL %s\n  log tail:\n%s\n' "$n" "$total_pending" "$cfg" "$(tail -3 "$log")"
        ) 9>>"$counter_file"
    fi
}
export -f run_one
export BIN NED_PATH INI total_pending counter_file

xargs -P "$WORKERS" -I {} bash -c 'run_one "$@"' _ {} < "$tmp_jobs"

end_ts=$(date +%s)
echo
echo "Done in $((end_ts - start_ts))s. Use 'grep FAIL' on the log to find failures."
