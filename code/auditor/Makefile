# Pinned out-of-tree build of the independent MMEM replay auditor.
# Usage: make -C AUDITOR_ROOT BUILD=/absolute/new/build/dir all
# Only this package's src/ directory is compiled; no include path to the producer exists.

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
REQUIRED_GCC_VERSION := 8.5.0

AUDITOR_ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
SRC_DIR := $(AUDITOR_ROOT)/src
SOURCES := $(SRC_DIR)/main.cpp
HEADERS := $(SRC_DIR)/sha256.hpp $(SRC_DIR)/philox.hpp $(SRC_DIR)/records.hpp $(SRC_DIR)/replay.hpp

ifndef BUILD
$(error BUILD must name a new out-of-tree directory, e.g. make BUILD=/path/to/new/build all)
endif
BUILD_ABS := $(abspath $(BUILD))
ifeq ($(BUILD_ABS),$(AUDITOR_ROOT))
$(error BUILD must be outside the auditor source tree)
endif
ifneq ($(filter $(AUDITOR_ROOT)/%,$(BUILD_ABS)),)
$(error BUILD must be outside the auditor source tree)
endif

TARGET := $(BUILD_ABS)/mmem_audit_replay

.PHONY: all toolchain-check

all: $(TARGET)

toolchain-check:
	@v="$$($(CXX) -dumpfullversion 2>/dev/null)"; \
	if [ "$$v" != "$(REQUIRED_GCC_VERSION)" ]; then \
	  echo "error: $(CXX) reports version '$$v'; the pinned compiler is GCC $(REQUIRED_GCC_VERSION)" >&2; \
	  exit 1; \
	fi

$(TARGET): $(SOURCES) $(HEADERS) | toolchain-check
	mkdir -p $(BUILD_ABS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@
