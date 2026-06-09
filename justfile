# Host-side commands.
# `just build` bakes the podman image.
# `just shell` and `just run` execute inside the container with the project bind-mounted at /work.

image := env_var_or_default("SLORA_IMAGE", "localhost/slora-sim:dev")
project_root := justfile_directory()

default:
    @just --list

build *args:
    cd {{project_root}} && podman build \
        --tag {{image}} \
        --file Dockerfile \
        {{args}} \
        .

_ensure-image:
    #!/usr/bin/env bash
    if ! podman image exists {{image}}; then
        echo "[slora] image '{{image}}' not built; running 'just build' first..." >&2
        just build
    fi

shell *args: _ensure-image
    podman run --rm -it \
        --hostname slora-sim \
        --workdir /work \
        -v {{project_root}}:/work \
        -e HOST_UID="$(id -u)" \
        -e HOST_GID="$(id -g)" \
        -e TERM="${TERM:-xterm-256color}" \
        {{image}} {{args}}

run config="Smoke": _ensure-image
    podman run --rm -it \
        --workdir /work \
        -v {{project_root}}:/work \
        -e HOST_UID="$(id -u)" \
        -e HOST_GID="$(id -g)" \
        {{image}} \
        bash -lc 'make CONFIG={{config}} run'

gui config="Smoke": _ensure-image
    #!/usr/bin/env bash
    set -euo pipefail
    : "${DISPLAY:?DISPLAY not set; start an X server / XWayland first}"
    xhost +SI:localuser:"$(id -un)" >/dev/null 2>&1 || true
    # NixOS+Wayland often has no XAUTHORITY; rely on xhost line above.
    xauth_args=()
    if [[ -n "${XAUTHORITY:-}" && -e "${XAUTHORITY}" ]]; then
        xauth_args=(-v "$XAUTHORITY:/tmp/.Xauthority:ro" -e XAUTHORITY=/tmp/.Xauthority)
    fi
    podman run --rm -it \
        --workdir /work \
        -v {{project_root}}:/work \
        -v /tmp/.X11-unix:/tmp/.X11-unix \
        -e DISPLAY="$DISPLAY" \
        "${xauth_args[@]}" \
        -e HOST_UID="$(id -u)" \
        -e HOST_GID="$(id -g)" \
        {{image}} \
        bash -lc 'make UI=Qtenv CONFIG={{config}} run'

ide: _ensure-image
    #!/usr/bin/env bash
    set -euo pipefail
    : "${DISPLAY:?DISPLAY not set; start an X server / XWayland first}"
    xhost +SI:localuser:"$(id -un)" >/dev/null 2>&1 || true
    xauth_args=()
    if [[ -n "${XAUTHORITY:-}" && -e "${XAUTHORITY}" ]]; then
        xauth_args=(-v "$XAUTHORITY:/tmp/.Xauthority:ro" -e XAUTHORITY=/tmp/.Xauthority)
    fi
    podman run --rm -it \
        --workdir /work \
        -v {{project_root}}:/work \
        -v /tmp/.X11-unix:/tmp/.X11-unix \
        -e DISPLAY="$DISPLAY" \
        "${xauth_args[@]}" \
        -e HOST_UID="$(id -u)" \
        -e HOST_GID="$(id -g)" \
        {{image}} \
        bash -lc "omnetpp -data /work/.omnetpp-workspace"

clean: _ensure-image
    podman run --rm \
        --workdir /work \
        -v {{project_root}}:/work \
        {{image}} \
        bash -lc "make cleanall"

nuke:
    -podman image rm {{image}}
