# Out-of-tree build for the independent PHASE2-TORUS-MEMORY-003 auditor.
#
# NOT RUN during construction.  Future, separately authorized use only:
#   make -f <package>/Makefile BUILD_DIR=/absolute/path/outside/package
#
# Pinned toolchain: GCC 8.5.x, C++17, C++ standard library only (GCC 8 needs
# -lstdc++fs for <filesystem>).  Flags are fixed here; do not override CXX,
# the flag variables or the recipe on the command line.  No object or binary
# is ever written inside the source tree.

.RECIPEPREFIX := >
.SUFFIXES:
.DELETE_ON_ERROR:

SRC_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must name a directory outside $(SRC_DIR))
endif
BUILD_ABS := $(abspath $(BUILD_DIR))
ifneq ($(filter $(SRC_DIR) $(SRC_DIR)/%,$(BUILD_ABS)),)
$(error BUILD_DIR must lie outside the source tree $(SRC_DIR))
endif

CXX := g++
REQUIRED_GCC_PREFIX := 8.5.
PINNED_STD := -std=c++17
PINNED_OPT := -O2 -fno-strict-aliasing -fno-fast-math -ffp-contract=off
PINNED_WARN := -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Werror=return-type
PINNED_CPP := -I$(SRC_DIR)/src
PINNED_LIBS := -lstdc++fs

HEADERS := $(addprefix $(SRC_DIR)/src/,records.hpp replay.hpp sha256.hpp threefry.hpp)
BIN := $(BUILD_ABS)/torus_audit

.PHONY: all toolchain-check

all: $(BIN)

toolchain-check:
> @v=$$($(CXX) -dumpfullversion 2>/dev/null || $(CXX) -dumpversion); \
>  case "$$v" in \
>    $(REQUIRED_GCC_PREFIX)*) echo "toolchain: g++ $$v";; \
>    *) echo "refused: g++ $$v is not GCC $(REQUIRED_GCC_PREFIX)x" >&2; exit 2;; \
>  esac

$(BUILD_ABS):
> mkdir -p $@

$(BIN): $(SRC_DIR)/src/main.cpp $(HEADERS) $(SRC_DIR)/Makefile | toolchain-check $(BUILD_ABS)
> $(CXX) $(PINNED_STD) $(PINNED_OPT) $(PINNED_WARN) $(PINNED_CPP) -o $@ $(SRC_DIR)/src/main.cpp $(PINNED_LIBS)
