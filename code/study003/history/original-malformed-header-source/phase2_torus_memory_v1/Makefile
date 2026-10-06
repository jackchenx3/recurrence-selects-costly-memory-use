# PHASE2-TORUS-MEMORY-003 build (GCC 8.5+ / C++17). NOT YET COMPILED.
# Integer-only scientific code: no -ffast-math, no -march=native.
# Usage: make BUILD=<explicit build directory>

CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -pthread
CPPFLAGS += -Ithird_party/random123/include
LDFLAGS += -pthread

ifndef BUILD
$(error BUILD must be set explicitly, e.g. make BUILD=build)
endif

LIB_SRC := \
  src/torus/sha256.cpp \
  src/torus/util.cpp \
  src/torus/threefry_rng.cpp \
  src/torus/target_law.cpp \
  src/torus/tape.cpp \
  src/torus/candidates.cpp \
  src/torus/tournament.cpp \
  src/torus/transition.cpp \
  src/torus/step.cpp \
  src/torus/records.cpp \
  src/torus/path_runner.cpp \
  src/torus/chunk_io.cpp \
  src/torus/collision.cpp \
  src/torus/config_check.cpp \
  src/torus/manifest.cpp

LIB_OBJ := $(patsubst src/%.cpp,$(BUILD)/obj/%.o,$(LIB_SRC))
HEADERS := $(wildcard src/torus/*.hpp)

all: $(BUILD)/torus_produce $(BUILD)/torus_fixtures $(BUILD)/torus_regen

$(BUILD)/obj/%.o: src/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/torus_%: $(BUILD)/obj/apps/torus_%.o $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: all
.SECONDARY:
