# Host-side Makefile that orchestrates the in-container build.
# `make zig` builds the Zig stack, `make makefiles` regenerates the OMNeT++ makefile, `make all` builds both.

INET_PROJ ?= /opt/inet4.4
FLORA_PROJ ?= /opt/flora
MODE ?= release
CONFIG ?= Smoke
INI ?= simulations/omnetpp.ini
UI ?= Cmdenv

# Binary name is `sim`, not `slora`, because the project has a `slora/` subpackage.
# opp_makemake would otherwise put the executable and the package's .o-dir
# at the same path and collide.
BUILD_DIR := build
BIN := $(BUILD_DIR)/gcc-$(MODE)/slora

.PHONY: all makefiles zig run clean cleanall

all: zig makefiles
	$(MAKE) -C src MODE=$(MODE)

zig:
	cd src/stack && zig build -Doptimize=ReleaseSafe

makefiles:
	cd src && opp_makemake -f --deep -o slora \
		-O ../$(BUILD_DIR) -M $(MODE) \
		-KINET4_4_PROJ=$(INET_PROJ) -KFLORA_PROJ=$(FLORA_PROJ) \
		-DINET_IMPORT \
		-I. -I$(INET_PROJ)/src -I$(FLORA_PROJ)/src -I./stack/include \
		-L$(INET_PROJ)/src -L$(FLORA_PROJ)/src -L./stack/zig-out/lib \
		-lINET -lflora -lsodium -lslora

run: all
	./$(BIN) -u $(UI) -c $(CONFIG) \
		-n src:$(INET_PROJ)/src:$(FLORA_PROJ)/src:$(FLORA_PROJ)/simulations \
		$(INI)

clean:
	cd src && $(MAKE) clean MODE=$(MODE) || true

cleanall:
	rm -rf $(BUILD_DIR) src/Makefile
