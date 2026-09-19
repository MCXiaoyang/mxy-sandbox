# ============================================================
#  mxy-sandbox
# ============================================================

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

CORE_SRC := core/src/framebuffer.cpp \
            core/src/vfs.cpp \
            core/src/cpu.cpp \
            core/src/machine.cpp \
            core/src/programs.cpp \
            core/src/bridge.cpp \
            core/src/main.cpp

UNAME := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(OS),Windows_NT)
    CORE_BIN := core/sandbox-core.exe
    LDLIBS   := -lws2_32
    RM       := del /q
    RMDIR    := rmdir /s /q
    MKDIR    := if not exist
    RUN_CORE := core\\sandbox-core.exe
else
    CORE_BIN := core/sandbox-core
    LDLIBS   := -pthread
    RM       := rm -f
    RMDIR    := rm -rf
    MKDIR    := mkdir -p
    RUN_CORE := ./core/sandbox-core
endif

.PHONY: all core tty gui run-tty run-gui clean rebuild help

all: core

# ---- build -------------------------------------------------

core: $(CORE_BIN)

$(CORE_BIN): $(CORE_SRC) $(wildcard core/src/*.h)
	$(CXX) $(CXXFLAGS) -o $@ $(CORE_SRC) $(LDLIBS)
	@echo "[ok] $@ built"

# ---- run ---------------------------------------------------

tty: core
	@echo "start core in a new window, then run 'make tty-run' in another terminal"
	@echo "or just double-click run-tty.bat on Windows"

run-tty: core
	@echo "starting core..."
	@start "mxy-sandbox core" cmd /k "cd /d $(CURDIR)\\core && sandbox-core.exe" 2>nul || \
	 ( $(RUN_CORE) & )
	@sleep 1
	cd shell && python tty_main.py

run-gui: core
	cd shell && python main.py

# ---- deps --------------------------------------------------

deps:
	pip install -r shell/requirements.txt

# ---- clean -------------------------------------------------

clean:
	-$(RM) core/sandbox-core core/sandbox-core.exe
	-$(RMDIR) shell/__pycache__
	-$(RMDIR) core/build

rebuild: clean core

# ---- help --------------------------------------------------

help:
	@echo "targets:"
	@echo "  make            build the C++ core"
	@echo "  make core       same as above"
	@echo "  make deps       install python dependencies"
	@echo "  make run-tty    start core + TTY shell (Windows)"
	@echo "  make run-gui    start GUI shell (needs core running separately)"
	@echo "  make clean      remove build artifacts"
	@echo "  make rebuild    clean + build"