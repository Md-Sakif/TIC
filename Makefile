# Builds the tests and demos. cterm itself needs no build step: it's one header.
#
#   make                build the demos
#   make test           build and run the unit tests
#   make test-all       the above with gcc, clang and C++, plus sanitizers
#   make e2e            run the tests that use a real pseudo-terminal (POSIX only)
#   make windows        cross-compile for Windows (needs MinGW)
#   make test-windows   run the Windows build's unit tests with Wine
#   make clean          delete the build directory
#
# Choose tools with variables, for example: make test CC=clang

CC     ?= cc
CXX    ?= c++
MINGW  ?= x86_64-w64-mingw32-gcc
WINE   ?= wine
PYTHON ?= python3

WARN     = -Wall -Wextra -Wpedantic -Wshadow -Werror
CFLAGS   = -std=c99 $(WARN) -g -I. -Itests
CXXFLAGS = -std=c++11 $(WARN) -g -I. -Itests -x c++
SANITIZE = -fsanitize=address,undefined -fno-sanitize-recover=all

BUILD = build
TESTS = test_unicode test_buffer test_draw test_render test_parser
DEMOS = keys dashboard
E2E_APPS = e2e_app

.PHONY: all demos test test-all test-gcc test-clang test-cpp test-asan e2e windows test-windows clean

all: demos

demos: $(DEMOS:%=$(BUILD)/%)

# Every target rebuilds if the header or test helpers change.
HEADERS = cterm.h tests/test.h tests/vt.h

$(BUILD)/%: demo/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/test_%: tests/test_%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: $(TESTS:%=$(BUILD)/%)
	@set -e; for t in $(TESTS); do printf '%-20s' $$t; $(BUILD)/$$t; done

# Each compiler gets its own run in a separate directory, so that their outputs
# don't get mixed up.
test-gcc:
	$(MAKE) --no-print-directory test CC=gcc BUILD=$(BUILD)/gcc

test-clang:
	$(MAKE) --no-print-directory test CC=clang BUILD=$(BUILD)/clang

test-asan:
	$(MAKE) --no-print-directory test CC=clang BUILD=$(BUILD)/asan CFLAGS="$(CFLAGS) $(SANITIZE)"

# Compiles the C sources as C++ to prove the header stays valid C++.
test-cpp:
	@mkdir -p $(BUILD)/cpp
	@set -e; for t in $(TESTS); do \
		$(CXX) $(CXXFLAGS) tests/$$t.c -o $(BUILD)/cpp/$$t; \
		printf '%-20s' $$t; $(BUILD)/cpp/$$t; \
	done

test-all: test-gcc test-clang test-cpp test-asan

$(BUILD)/e2e_app: tests/e2e_app.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

e2e: demos $(BUILD)/e2e_app
	$(PYTHON) tests/e2e.py $(BUILD)/e2e_app

windows:
	@mkdir -p $(BUILD)/windows
	@set -e; for t in $(TESTS); do \
		$(MINGW) $(CFLAGS) tests/$$t.c -o $(BUILD)/windows/$$t.exe; \
	done
	@set -e; for d in $(DEMOS); do \
		$(MINGW) $(CFLAGS) demo/$$d.c -o $(BUILD)/windows/$$d.exe; \
	done

test-windows: windows
	@set -e; for t in $(TESTS); do \
		printf '%-20s' $$t; $(WINE) $(BUILD)/windows/$$t.exe; \
	done

clean:
	rm -rf $(BUILD)
