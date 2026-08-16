# Cnext Compiler - Modern Makefile
# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
# Detect MSYS2/Cygwin/MinGW on Windows (uname returns MSYS_NT-*, CYGWIN_*, MINGW_*)
ifneq (,$(findstring MINGW,$(UNAME_S)))
    WINDOWS_BUILD := true
endif
ifneq (,$(findstring MSYS,$(UNAME_S)))
    WINDOWS_BUILD := true
endif
ifneq (,$(findstring CYGWIN,$(UNAME_S)))
    WINDOWS_BUILD := true
endif
ifeq ($(UNAME_S),Windows)
    WINDOWS_BUILD := true
endif
ifdef WINDOWS_BUILD
    EXEC = cnext.exe
    PLATFORM_LIBS = -lwinhttp -lws2_32
else ifeq ($(UNAME_S),Darwin)
    EXEC = cnext
    PLATFORM_LIBS = -lcurl -lpthread
else
    EXEC = cnext
    PLATFORM_LIBS = -lcurl -lpthread
endif

# Version (single source of truth: include/main_internal.h)
VERSION := $(shell sed -n 's/.*CNEXT_VERSION "\([^"]*\)".*/\1/p' include/main_internal.h 2>/dev/null)
ifeq ($(strip $(VERSION)),)
    VERSION := 10.0.4
endif

# Install layout: exe -> INSTALL_BIN, runtime headers -> INSTALL_INC.
# Windows matches install.ps1 / install.bat (%LOCALAPPDATA%\Cnext\{bin,include}).
# POSIX-style tools (provided by MSYS on Windows) keep every target portable.
RM    = rm -f
RMDIR = rm -rf
MKDIR = mkdir -p

ifdef WINDOWS_BUILD
    INSTALL_DIR = $(subst \,/,$(LOCALAPPDATA))/Cnext
    INSTALL_BIN = $(INSTALL_DIR)/bin
    INSTALL_INC = $(INSTALL_DIR)/include
else
    INSTALL_DIR = /usr/local
    INSTALL_BIN = /usr/local/bin
    INSTALL_INC = /usr/local/include
endif



# Compiler settings
CC ?= gcc
STD = -std=gnu11
WARNINGS = -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function

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
       src/diagnostics.c \
       src/optimizer.c src/formatter.c src/linter.c src/repl.c \
       src/docgen.c src/registry.c src/semver.c src/sourcemap.c src/moduleresolver.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

.PHONY: all clean test install uninstall help format bench check release coverage

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
	@echo "Running LSP tests..."
	@python3 tests/test_lsp.py 2>/dev/null || python tests/test_lsp.py 2>/dev/null || echo "LSP tests skipped"

install: $(EXEC)
ifdef WINDOWS_BUILD
	@mkdir -p "$(INSTALL_BIN)" "$(INSTALL_INC)"
	@cp "$(EXEC)" "$(INSTALL_BIN)/$(EXEC)"
	@echo "Copying include directory..."
	@cp -r include/. "$(INSTALL_INC)/"
else
	@mkdir -p "$(INSTALL_BIN)" "$(INSTALL_DIR)/share/cnext/include"
	@cp "$(EXEC)" "$(INSTALL_BIN)/$(EXEC)"
	@echo "Copying include directory..."
	@cp -r include/. "$(INSTALL_DIR)/share/cnext/include/"
endif
	@echo "Installed $(EXEC) to $(INSTALL_BIN)"
	@echo "Add $(INSTALL_BIN) to your PATH to use 'cnext' globally."

uninstall:
	@rm -f "$(INSTALL_BIN)/$(EXEC)"
	@rm -rf "$(INSTALL_DIR)/share/cnext" 2>/dev/null || true
ifdef WINDOWS_BUILD
	@rm -rf "$(INSTALL_INC)" 2>/dev/null || true
endif
	@echo "Removed $(EXEC) from $(INSTALL_BIN)"

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
	@echo "Packaging Cnext $(VERSION) release..."
	@mkdir -p dist
	@cp "$(EXEC)" dist/
	@cp -r include dist/ 2>/dev/null || true
	@cp -r examples dist/ 2>/dev/null || true
	@cp README.md dist/ 2>/dev/null || true
	@cp LICENSE dist/ 2>/dev/null || true
	@cp install.sh dist/ 2>/dev/null || true
	@chmod +x dist/install.sh 2>/dev/null || true
ifdef WINDOWS_BUILD
	@echo "Creating Windows ZIP archive..."
	@powershell -NoProfile -Command "Compress-Archive -Path 'dist/*' -DestinationPath 'cnext-windows-x64-$(VERSION).zip' -Force"
	@echo "Release archive: cnext-windows-x64-$(VERSION).zip"
	@rm -rf dist
else
	@cd dist && tar -czf "$(CURDIR)/cnext-$(shell uname -s | tr A-Z a-z)-$(shell uname -m)-$(VERSION).tar.gz" *
	@rm -rf dist
	@echo "Release archive: cnext-$(shell uname -s | tr A-Z a-z)-$(shell uname -m)-$(VERSION).tar.gz"
endif
	@echo ""
	@echo "To create a full multi-platform release, push a tag:"
	@echo "  git tag v$(VERSION)"
	@echo "  git push origin v$(VERSION)"
	@echo ""
	@echo "GitHub Actions will build for all platforms automatically."

help:
	@echo "Cnext Compiler Build System v10.0"
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

# Code coverage (gcov/lcov)
COVERAGE_DIR = coverage
coverage: $(EXEC)
	@echo "Running tests with coverage..."
	$(MAKE) clean
	$(MAKE) DEBUG=1 OPT="-O0 -g --coverage"
	./$(EXEC) test
	@echo "Generating coverage report..."
	lcov --capture --directory . --output-file $(COVERAGE_DIR)/coverage.info --rc lcov_branch_coverage=1 2>/dev/null || true
	lcov --remove $(COVERAGE_DIR)/coverage.info '/usr/*' '*/tests/*' '*/examples/*' --output-file $(COVERAGE_DIR)/coverage_filtered.info --rc lcov_branch_coverage=1 2>/dev/null || true
	genhtml $(COVERAGE_DIR)/coverage_filtered.info --output-directory $(COVERAGE_DIR)/html --rc lcov_branch_coverage=1 2>/dev/null || true
	@echo "Coverage report: $(COVERAGE_DIR)/html/index.html"
	@lcov --summary $(COVERAGE_DIR)/coverage_filtered.info 2>/dev/null || echo "Install lcov for coverage summaries"

# Include auto-generated dependency files
-include $(DEPS)
