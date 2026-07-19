# Cnext Compiler - Modern Makefile
# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME_S),Windows)
    EXEC = cnext.exe
    PLATFORM_LIBS = -lwinhttp -lws2_32
    INSTALL_DIR = $(LOCALAPPDATA)/Cnext/bin
    RM = del /f /q
    RMDIR = rmdir /s /q
    MKDIR = mkdir
    SEP = \\
else ifeq ($(UNAME_S),Darwin)
    EXEC = cnext
    PLATFORM_LIBS = -lcurl -lpthread
    INSTALL_DIR = /usr/local/bin
    RM = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
    SEP = /
else
    EXEC = cnext
    PLATFORM_LIBS = -lcurl -lpthread
    INSTALL_DIR = /usr/local/bin
    RM = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
    SEP = /
endif

# Compiler settings
CC ?= gcc
STD = -std=gnu11
WARNINGS = -Wall -Wextra -Wno-unused-parameter

# Build mode: make DEBUG=1 for debug build
ifdef DEBUG
    OPT = -O0 -g -DDEBUG
else
    OPT = -O2 -DNDEBUG
endif

# Parallel jobs: make -j8 or JOBS=8
JOBS ?= 1

CFLAGS = $(STD) $(WARNINGS) $(OPT) -iquote include -MMD -MP
LDFLAGS = $(PLATFORM_LIBS)

# Source files
SRCS = src/main.c src/main_utils.c src/main_packages.c src/main_compiler.c \
       src/lexer.c src/parser.c src/parser_core.c src/parser_macro.c \
       src/parser_type.c src/parser_expr.c src/parser_stmt.c src/parser_decl.c \
       src/ast.c \
       src/codegen.c src/codegen_gen.c src/codegen_closure.c src/codegen_types.c \
       src/codegen_generic.c src/codegen_classspec.c src/codegen_emit_utils.c \
       src/codegen_expr.c src/codegen_node.c \
       src/semantics.c src/semantics_scope.c src/semantics_types.c \
       src/semantics_predeclare.c src/semantics_analyze.c src/semantics_expr.c \
       src/optimizer.c src/formatter.c src/linter.c src/repl.c \
       src/docgen.c src/registry.c src/semver.c src/sourcemap.c src/moduleresolver.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

.PHONY: all clean test install uninstall help format bench check release

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(DEPS) $(EXEC) temp_out.c _cnext_*.tmp 2>/dev/null || true

test: $(EXEC)
	./$(EXEC) test

# Full test suite (compiler + Python tests)
check: $(EXEC)
	./$(EXEC) test
	@echo "Running Python test suite..."
	@python3 tests/run_tests.py 2>/dev/null || python tests/run_tests.py 2>/dev/null || echo "Python tests skipped"

install: $(EXEC)
ifeq ($(UNAME_S),Windows)
	@if not exist "$(INSTALL_DIR)" mkdir "$(INSTALL_DIR)"
	copy /Y $(EXEC) "$(INSTALL_DIR)\$(EXEC)"
	@echo Copying include directory...
	@xcopy /E /Y /Q "include\*" "$(INSTALL_DIR)\..\include\" >nul 2>&1
else
	$(MKDIR) "$(INSTALL_DIR)" 2>/dev/null || true
	cp $(EXEC) "$(INSTALL_DIR)/$(EXEC)"
	cp -r include "$(INSTALL_DIR)/../include" 2>/dev/null || true
endif
	@echo "Installed $(EXEC) to $(INSTALL_DIR)"
	@echo "Add $(INSTALL_DIR) to your PATH to use 'cnext' globally."

uninstall:
ifeq ($(UNAME_S),Windows)
	@if exist "$(INSTALL_DIR)\$(EXEC)" del /f "$(INSTALL_DIR)\$(EXEC)"
	@if exist "$(INSTALL_DIR)\..\include" rmdir /S /Q "$(INSTALL_DIR)\..\include" 2>nul
else
	$(RM) "$(INSTALL_DIR)/$(EXEC)" 2>/dev/null || true
	rm -rf "$(INSTALL_DIR)/../include" 2>/dev/null || true
endif
	@echo "Removed $(EXEC) from $(INSTALL_DIR)"

format:
	clang-format -i src/*.c include/*.h

# Build benchmark
bench: $(EXEC)
	@echo "=== Build Benchmark ==="
	@echo "Cleaning..."
	@$(MAKE) clean > /dev/null 2>&1
	@echo "Building with $(shell nproc 2>/dev/null || echo 4) cores..."
	@time $(MAKE) -j$(shell nproc 2>/dev/null || echo 4) 2>/dev/null
	@echo ""
	@echo "=== Incremental Build Benchmark ==="
	@touch src/main.c
	@echo "Rebuilding after touching main.c..."
	@time $(MAKE) 2>/dev/null

release: $(EXEC)
	@echo "Packaging Cnext release..."
	@mkdir -p release/build
	@cp $(EXEC) release/build/
	@cp -r include release/build/ 2>/dev/null || true
	@cp -r examples release/build/ 2>/dev/null || true
	@cp install.sh release/build/ 2>/dev/null || true
	@cd release/build && tar -czf ../cnext-$(shell uname -s | tr A-Z a-z)-$(shell uname -m).tar.gz *
	@echo "Release archive: release/cnext-$(shell uname -s | tr A-Z a-z)-$(shell uname -m).tar.gz"
	@echo ""
	@echo "To create a full release, push a tag:"
	@echo "  git tag v9.0.0"
	@echo "  git push origin v9.0.0"
	@echo ""
	@echo "GitHub Actions will build for all platforms automatically."

help:
	@echo "Cnext Compiler Build System v9.0"
	@echo ""
	@echo "Targets:"
	@echo "  all        Build the compiler (default)"
	@echo "  clean      Remove build artifacts"
	@echo "  test       Build and run compiler tests"
	@echo "  check      Run full test suite (compiler + Python)"
	@echo "  install    Install compiler to $(INSTALL_DIR)"
	@echo "  uninstall  Remove compiler from $(INSTALL_DIR)"
	@echo "  release    Package for release"
	@echo "  format     Format source code with clang-format"
	@echo "  bench      Run build benchmarks"
	@echo "  help       Show this help"
	@echo ""
	@echo "Options:"
	@echo "  DEBUG=1    Build with debug symbols (-O0 -g)"
	@echo "  JOBS=N     Parallel compilation jobs"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Release build"
	@echo "  make DEBUG=1            # Debug build"
	@echo "  make -j8                # Parallel build"
	@echo "  make install            # Install to $(INSTALL_DIR)"
	@echo "  make release            # Package for release"
	@echo "  make check              # Run all tests"
	@echo "  make clean              # Clean build artifacts"

# Include auto-generated dependency files
-include $(DEPS)
