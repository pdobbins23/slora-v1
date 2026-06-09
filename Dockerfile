# syntax=docker/dockerfile:1.7

ARG UBUNTU_TAG=26.04
FROM docker.io/library/ubuntu:${UBUNTU_TAG} AS base

ARG OMNETPP_VERSION=6.3.0
ARG INET_VERSION=4.5.4
ARG FLORA_VERSION=1.2.0
ARG ZIG_VERSION=0.16.0

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    TZ=Etc/UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential gcc g++ bison flex perl pkg-config swig \
        libsodium-dev \
        python3 python3-dev python3-pip python3-venv \
        qt6-base-dev qt6-tools-dev qt6-tools-dev-tools \
        libqt6opengl6-dev libqt6svg6-dev \
        libxml2-dev zlib1g-dev libdwarf-dev \
        doxygen graphviz xdg-utils \
        ca-certificates curl wget git xz-utils \
        libgl1 libglu1-mesa libxkbcommon-x11-0 libxcb-xinerama0 \
        libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
        libxcb-render-util0 libxcb-shape0 libxcb-xfixes0 libxcb-cursor0 \
        libfontconfig1 libdbus-1-3 \
        adwaita-icon-theme hicolor-icon-theme dbus-x11 \
        sudo gosu locales tzdata less vim-tiny \
    && rm -rf /var/lib/apt/lists/*

ENV OMNETPP_ROOT=/opt/omnetpp \
    INET_ROOT=/opt/inet \
    FLORA_ROOT=/opt/flora

RUN mkdir -p /opt && cd /opt \
    && curl -fsSL "https://github.com/omnetpp/omnetpp/releases/download/omnetpp-${OMNETPP_VERSION}/omnetpp-${OMNETPP_VERSION}-linux-x86_64.tgz" \
        -o omnetpp.tgz \
    && tar xf omnetpp.tgz \
    && mv "omnetpp-${OMNETPP_VERSION}" omnetpp \
    && rm omnetpp.tgz \
    && python3 -m venv /opt/omnetpp/.venv \
    && /opt/omnetpp/.venv/bin/pip install --no-cache-dir --upgrade pip \
    && /opt/omnetpp/.venv/bin/pip install --no-cache-dir -r /opt/omnetpp/python/requirements.txt

ENV PATH=/opt/omnetpp/.venv/bin:/opt/omnetpp/bin:$PATH \
    HOME=/root

RUN cd /opt/omnetpp \
    && bash -c "source .venv/bin/activate && source setenv && \
        ./configure WITH_OSG=no WITH_OSGEARTH=no PREFER_CLANG=no && \
        make -j\$(nproc) MODE=release base"

RUN cd /opt \
    && git clone --depth 1 --branch "v${INET_VERSION}" https://github.com/inet-framework/inet.git inet \
    && cd inet \
    && bash -c "source /opt/omnetpp/setenv && source ./setenv && \
        make makefiles && make -j\$(nproc) MODE=release"

RUN cd /opt \
    && curl -fsSL "https://github.com/florasim/flora/archive/refs/tags/v${FLORA_VERSION}.tar.gz" \
        -o flora.tgz \
    && tar xf flora.tgz \
    && mv "flora-${FLORA_VERSION}" flora \
    && rm flora.tgz \
    && ln -s /opt/inet /opt/inet4.4 \
    && cd flora \
    && bash -c "source /opt/omnetpp/setenv && \
        cd src && opp_makemake -f --deep --make-so -o flora \
            -KINET4_4_PROJ=/opt/inet4.4 \
            -DINET_IMPORT -I. -I/opt/inet4.4/src \
            -L/opt/inet4.4/src -lINET && \
        make -j\$(nproc) MODE=release"

ENV VIRTUAL_ENV=/opt/omnetpp/.venv

ARG ZIG_ARCHIVE_ARCH=x86_64
RUN cd /opt \
    && curl -fsSL "https://ziglang.org/download/${ZIG_VERSION}/zig-${ZIG_ARCHIVE_ARCH}-linux-${ZIG_VERSION}.tar.xz" \
        -o zig.tar.xz \
    && tar xf zig.tar.xz \
    && mv "zig-${ZIG_ARCHIVE_ARCH}-linux-${ZIG_VERSION}" zig \
    && rm zig.tar.xz
ENV PATH=/opt/zig:$PATH

WORKDIR /work
ENV SLORA_WORKSPACE=/work \
    OMNETPP_ROOT=/opt/omnetpp \
    INET_PROJ=/opt/inet4.4 \
    FLORA_PROJ=/opt/flora \
    LD_LIBRARY_PATH=/opt/omnetpp/lib \
    HOSTNAME=slora-sim

CMD ["bash", "-li"]
