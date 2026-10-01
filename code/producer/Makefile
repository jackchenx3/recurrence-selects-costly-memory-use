# Build with the documented system compiler (GCC 8.5.0). GCC 8 needs
# -lstdc++fs for std::filesystem. Nothing here runs a binary.
# Compiler and flags are pinned with := and `override`, so neither the
# environment nor the make command line can replace them.
.RECIPEPREFIX := >
override CXX := g++
override CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
override CPPFLAGS := -Isrc
override LDLIBS := -lstdc++fs
BUILD ?= build
HEADERS := $(wildcard src/mm/*.hpp)

all: $(BUILD)/mmem_fixtures $(BUILD)/mmem_production $(BUILD)/mmem_regenerate

$(BUILD)/mmem_fixtures: src/fixtures_main.cpp $(HEADERS) | $(BUILD)
> $(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDLIBS)

$(BUILD)/mmem_production: src/production_main.cpp $(HEADERS) | $(BUILD)
> $(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDLIBS)

$(BUILD)/mmem_regenerate: src/regenerate_main.cpp $(HEADERS) | $(BUILD)
> $(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDLIBS)

$(BUILD):
> mkdir -p $(BUILD)

.PHONY: all
