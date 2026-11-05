# Builds the tests and demos. TIC itself needs no build step: it's one header.
#
#   make                build the demos
#   make test           build and run the unit tests
#   make test-all       the above with gcc, clang and C++, sanitizers, and link modes
#   make test-link      check every documented way of including the header
#   make check-readme   compile the examples in README.md
#   make check-reference check the API reference matches tic.h
#   make tutorial       build the tutorial's step programs
#   make check-tutorial check the tutorial's text matches its code
#   make e2e            run the tests that use a real pseudo-terminal (POSIX only)
#   make windows        cross-compile for Windows (needs MinGW)
#   make test-windows   run the Windows build's unit tests with Wine
#   make e2e-windows    run the ConPTY end-to-end test with Wine
#   make clean          delete the build directory
#
# Choose tools with variables, for example: make test CC=clang

CC     ?= cc
CXX    ?= c++
MINGW  ?= x86_64-w64-mingw32-gcc
WINE   ?= wine
PYTHON ?= python3

WARN     = -Wall -Wextra -Wpedantic -Wshadow -Werror
CFLAGS   = -std=c99 $(WARN) -g -I. -Itests -Itutorial/system-monitor
CXXFLAGS = -std=c++11 $(WARN) -g -I. -Itests -Itutorial/system-monitor -x c++
SANITIZE = -fsanitize=address,undefined -fno-sanitize-recover=all

BUILD = build
TESTS = test_unicode test_buffer test_draw test_render test_parser test_ui test_alloc test_tutorial_stats
DEMOS = hello keys dashboard widgets
TUTORIAL = step1 step2 step3 step4 step5 step6 step7 step8 step9

.PHONY: all demos tutorial check-tutorial check-reference test test-all test-gcc test-clang test-cpp test-asan test-link check-readme e2e e2e-windows windows test-windows clean

all: demos

demos: $(DEMOS:%=$(BUILD)/%)

tutorial: $(TUTORIAL:%=$(BUILD)/tutorial/%)

# Every target rebuilds if the header or test helpers change.
HEADERS = tic.h tests/test.h tests/vt.h

$(BUILD)/%: demo/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/test_%: tests/test_%.c $(HEADERS) tutorial/system-monitor/stats.h | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/tutorial/%: tutorial/system-monitor/%.c tic.h tutorial/system-monitor/stats.h
	@mkdir -p $(BUILD)/tutorial
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
	@set -e; for d in $(DEMOS); do \
		$(CXX) $(CXXFLAGS) demo/$$d.c -o $(BUILD)/cpp/$$d; \
	done
	$(CXX) $(CXXFLAGS) tests/e2e_app.c -o $(BUILD)/cpp/e2e_app

# Proves every documented way of including the header compiles and links.
LINK = tests/link
test-link:
	@mkdir -p $(BUILD)/link
	$(CC) $(CFLAGS) $(LINK)/main.c $(LINK)/other.c -o $(BUILD)/link/extern
	$(BUILD)/link/extern
	$(CC) $(CFLAGS) $(LINK)/static.c -o $(BUILD)/link/static
	$(BUILD)/link/static
	$(CC) $(CFLAGS) -DTIC_NO_UI $(LINK)/main.c $(LINK)/other.c -o $(BUILD)/link/no_ui
	$(BUILD)/link/no_ui
	$(CC) $(CFLAGS) -DTIC_MAX_MARKS=8 $(LINK)/main.c $(LINK)/other.c -o $(BUILD)/link/marks
	$(BUILD)/link/marks
	$(CC) $(CFLAGS) $(LINK)/header_only.c -c -o $(BUILD)/link/header_only.o
	$(CC) -std=gnu99 $(WARN) -I. $(LINK)/late_implementation.c -o $(BUILD)/link/late
	$(BUILD)/link/late
	$(CC) $(CFLAGS) -c $(LINK)/other.c -o $(BUILD)/link/other.o
	$(CXX) $(CXXFLAGS) $(LINK)/main.c -c -o $(BUILD)/link/cpp_main.o
	$(CXX) $(BUILD)/link/cpp_main.o $(BUILD)/link/other.o -o $(BUILD)/link/mixed 2>/dev/null || \
		(echo "C++ and C objects failed to link"; exit 1)
	$(BUILD)/link/mixed
	@echo "link modes: ok"

test-all: test-gcc test-clang test-cpp test-asan test-link

$(BUILD)/e2e_app: tests/e2e_app.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

e2e: demos tutorial $(BUILD)/e2e_app
	$(PYTHON) tests/e2e.py $(BUILD)/e2e_app
	$(PYTHON) tests/e2e_tutorial.py $(BUILD)/tutorial

windows:
	@mkdir -p $(BUILD)/windows
	@set -e; for t in $(TESTS); do \
		$(MINGW) $(CFLAGS) tests/$$t.c -o $(BUILD)/windows/$$t.exe; \
	done
	@set -e; for d in $(DEMOS); do \
		$(MINGW) $(CFLAGS) demo/$$d.c -o $(BUILD)/windows/$$d.exe; \
	done
	@set -e; for s in $(TUTORIAL); do \
		$(MINGW) $(CFLAGS) tutorial/system-monitor/$$s.c -o $(BUILD)/windows/$$s.exe; \
	done
	$(MINGW) $(CFLAGS) tests/e2e_app.c -o $(BUILD)/windows/e2e_app.exe
	$(MINGW) $(CFLAGS) tests/e2e_conpty.c -o $(BUILD)/windows/e2e_conpty.exe

test-windows: windows
	@set -e; for t in $(TESTS); do \
		printf '%-20s' $$t; $(WINE) $(BUILD)/windows/$$t.exe; \
	done

e2e-windows: windows
	cd $(BUILD)/windows && $(WINE) ./e2e_conpty.exe ./e2e_app.exe

check-readme:
	$(PYTHON) tests/check_readme.py $(CC)

check-reference:
	$(PYTHON) tests/check_reference.py $(CC)

check-tutorial:
	$(PYTHON) tests/check_tutorial.py

clean:
	rm -rf $(BUILD)
