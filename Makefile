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
    # The compiler itself talks to the registry through the curl command, not
    # libcurl, so it links no HTTP or socket library. Programs that import
    # net/http get their platform libraries from main.c when they are built.
    PLATFORM_LIBS =
else ifeq ($(UNAME_S),Darwin)
    EXEC = cnext
    PLATFORM_LIBS = -lpthread
else
    EXEC = cnext
    PLATFORM_LIBS = -lpthread
endif

# Version (single source of truth: include/main_internal.h)
VERSION := $(shell sed -n 's/.*CNEXT_VERSION "\([^"]*\)".*/\1/p' include/main_internal.h 2>/dev/null)
ifeq ($(strip $(VERSION)),)
    VERSION := 10.0.5
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

CFLAGS = $(STD) $(WARNINGS) $(OPT) -DWIN32_LEAN_AND_MEAN -DNOMINMAX -iquote include -MMD -MP
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
       src/semantics_predeclare.c src/semantics_analyze.c src/semantics_expr.c src/lifetimes.c \
       src/diagnostics.c src/origin_map.c src/private_names.c \
       src/optimizer.c src/formatter.c src/linter.c src/repl.c \
       src/docgen.c src/registry.c src/semver.c src/sourcemap.c src/moduleresolver.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

.PHONY: all clean test install uninstall help format bench check release coverage parity parser-parity bootstrap-test msvc-defer-test self-build

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(DEPS) $(EXEC) temp_out.c _cnext_*.tmp __cnext_run_tmp.c _p*.c _probe* tests/_test_out tests/_test_out.exe tests/_test_list.txt tests/_test_methods_temp.c tests/_cnext_test_* $(LEAKTEST) $(LEAKTEST_OBJ) 2>/dev/null || true
	$(RMDIR) $(PARITY_DIR) out_parity build/tmp 2>/dev/null || true

test: $(EXEC)
	./$(EXEC) test

# Full test suite (compiler + Python tests)
check: $(EXEC)
	./$(EXEC) test
	$(PARITY_PY) tests/test_native_linking.py
	$(PARITY_PY) tests/test_native_manifest.py
	$(PARITY_PY) tests/test_diagnostics.py
	$(PARITY_PY) tests/test_fuzz_invalid_input.py
	$(PARITY_PY) tests/test_optimizer_semantics.py
	$(PARITY_PY) tests/test_cross_platform.py
	$(PARITY_PY) tests/test_package_security.py
	$(PARITY_PY) tests/test_language_contracts.py
	$(PARITY_PY) tests/test_allocators.py
	$(PARITY_PY) tests/test_lifetimes.py
	$(MAKE) msvc-defer-test
	@echo "Running Python test suite..."
	$(PARITY_PY) tests/run_tests.py
	@echo "Running LSP tests..."
	$(PARITY_PY) tests/test_lsp.py

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

# Optimizer leak harness: links the compiler objects (minus main.c) so repeated
# parse -> analyze -> optimize -> free cycles can be compared against a control
# pipeline using OS heap accounting. The harness also enforces the optimizer's
# owned-token-text invariant after each optimized cycle.
LEAKTEST = tools/optimizer_leak_harness$(if $(WINDOWS_BUILD),.exe,)
LEAKTEST_OBJ = tools/optimizer_leak_harness.o

.PHONY: leaktest

leaktest: $(LEAKTEST)
	$(LEAKTEST)

$(LEAKTEST): $(filter-out src/main.o,$(OBJS)) $(LEAKTEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $(LEAKTEST_OBJ) $(filter-out src/main.o,$(OBJS)) $(LDFLAGS) $(if $(WINDOWS_BUILD),-lpsapi,)

$(LEAKTEST_OBJ): tools/optimizer_leak_harness.c
	$(CC) $(CFLAGS) -c $< -o $@

format:
	clang-format -i src/*.c include/*.h

# Self-hosting parity gate. Builds the Cnext lexer (src2/lexer.cn) with the
# compiler under test, then diffs its token stream against the hand-written C
# lexer over the whole corpus. The reference dumper is built through this
# Makefile so the parity step inherits the same toolchain as the main build.
PARITY_DIR = build/selfhost
PARITY_CAND = $(PARITY_DIR)/lexer2$(if $(WINDOWS_BUILD),.exe,)
PARITY_REF = $(PARITY_DIR)/lexdump$(if $(WINDOWS_BUILD),.exe,)
PARITY_PY = $(shell if command -v python3 >/dev/null 2>&1; then echo python3; else echo python; fi)
STAGE1_EXE = $(PARITY_DIR)/stage1_exe$(if $(WINDOWS_BUILD),.exe,)
STAGE2_EXE = $(PARITY_DIR)/stage2_exe$(if $(WINDOWS_BUILD),.exe,)
MSVC_DEFER_OBJ = $(PARITY_DIR)/codegen_node_msvc_defer.o
MSVC_DEFER_EXE = $(PARITY_DIR)/cnext_msvc_defer$(if $(WINDOWS_BUILD),.exe,)
# Bootstrap builds favor fast, lower-memory compilation. Override with
# BOOTSTRAP_OPT=-O2 when measuring the self-hosted compiler's runtime speed.
BOOTSTRAP_OPT ?= -O0
BOOTSTRAP_CFLAGS = $(filter-out -O%,$(CFLAGS)) $(BOOTSTRAP_OPT)

.PHONY: parity

parity: $(EXEC) $(PARITY_CAND) $(PARITY_REF)
	@echo "=== Self-hosting lexer parity ==="
	$(PARITY_PY) tests/parity_lexer.py \
		--ref-binary "$(PARITY_REF)" \
		--cand-cmd "$(PARITY_CAND) {file}" \
		--paths "tests/**/*.cn" "examples/*.cn" "src2/*.cn"

parser-parity: $(EXEC)
	@echo "=== Self-hosting parser parity ==="
	$(PARITY_PY) tools/parser_parity.py

bootstrap-test: $(EXEC)
	python tests/test_bootstrap_codegen.py --stage0 "./$(EXEC)"

$(MSVC_DEFER_OBJ): src/codegen_node.c | $(PARITY_DIR)
	$(CC) $(CFLAGS) -DCNEXT_FORCE_MSVC_DEFER -c $< -o $@

$(MSVC_DEFER_EXE): $(filter-out src/codegen_node.o,$(OBJS)) $(MSVC_DEFER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

msvc-defer-test: $(MSVC_DEFER_EXE)
	@echo "=== MSVC defer lowering ==="
	$(PARITY_PY) tests/test_msvc_defer_codegen.py --compiler "$(MSVC_DEFER_EXE)"
	$(PARITY_PY) tests/test_msvc_defer_codegen.py --compiler "$(MSVC_DEFER_EXE)" --source tests/control/test_defer_captures.cn

$(PARITY_CAND): src2/lexer.cn $(EXEC) | $(PARITY_DIR)
	@echo "Building self-hosted lexer: $@"
	$(if $(WINDOWS_BUILD),./$(EXEC) build $< -o $@,./$(EXEC) build $< -o $@)

$(PARITY_REF): tools/lexdump.c src/lexer.c | $(PARITY_DIR)
	@echo "Building reference lexer dumper: $@"
	$(CC) $(STD) -Iinclude tools/lexdump.c src/lexer.c -o $@

$(PARITY_DIR):
	$(MKDIR) $(PARITY_DIR)

.PHONY: self-build
self-build: $(EXEC)
	@echo "=== Starting Self-Hosting Bootstrap ==="
	@$(MKDIR) $(PARITY_DIR)
	@echo "Stage 0 -> Stage 1: Bundling src2..."
	@python tools/build_bundle.py codegen -o $(PARITY_DIR)/stage1_bundle.cn
	@echo "Stage 0 -> Stage 1: Compiling bundle to C..."
	./$(EXEC) build $(PARITY_DIR)/stage1_bundle.cn --emit-c $(PARITY_DIR)/stage1_src.c
	@echo "Stage 0 -> Stage 1: Building binary with GCC..."
	$(CC) $(BOOTSTRAP_CFLAGS) -o $(STAGE1_EXE) $(PARITY_DIR)/stage1_src.c $(LDFLAGS)
	@echo "Stage 1 binary built: $(STAGE1_EXE)"
	@echo ""
	@echo "Stage 1 -> Stage 2: Compiling bundle with Stage 1..."
	$(STAGE1_EXE) $(PARITY_DIR)/stage1_bundle.cn -o $(PARITY_DIR)/stage2_src.c
	@echo "Stage 1 -> Stage 2: Building binary with GCC..."
	$(CC) $(BOOTSTRAP_CFLAGS) -o $(STAGE2_EXE) $(PARITY_DIR)/stage2_src.c $(LDFLAGS)
	@echo "Stage 2 binary built: $(STAGE2_EXE)"
	@echo ""
	@echo "=== Self-Hosting Verification ==="
	$(PARITY_PY) tools/verify_bootstrap.py --directory "$(PARITY_DIR)"

bench: $(EXEC)
	$(PARITY_PY) tools/bench.py --make "$(MAKE)" --compiler "$(EXEC)" --runs 3

.PHONY: selfhost-check
selfhost-check: self-build
	$(PARITY_PY) tests/test_optimizer_semantics.py --compiler "$(STAGE1_EXE)"
	$(PARITY_PY) tests/test_selfhost_optimizer.py --compiler "$(STAGE1_EXE)"
	$(PARITY_PY) tests/test_selfhost_test_runner.py --compiler "$(STAGE1_EXE)"
	$(PARITY_PY) tests/test_optimizer_semantics.py --compiler "$(STAGE2_EXE)"
	$(PARITY_PY) tests/test_selfhost_optimizer.py --compiler "$(STAGE2_EXE)"
	$(PARITY_PY) tests/test_selfhost_test_runner.py --compiler "$(STAGE2_EXE)"
	$(PARITY_PY) tests/test_selfhost_cli.py --compiler "$(STAGE2_EXE)"
	$(PARITY_PY) tests/test_compiler_parity.py --native "./$(EXEC)" --selfhost "$(STAGE2_EXE)"

release: $(EXEC)
	@echo "Packaging Cnext $(VERSION) release..."
	@mkdir -p dist
	@cp "$(EXEC)" dist/
	@cp -r include dist/ 2>/dev/null || true
	@cp -r examples dist/ 2>/dev/null || true
	@cp README.md dist/ 2>/dev/null || true
	@cp logo.png dist/ 2>/dev/null || true
	@cp LICENSE dist/ 2>/dev/null || true
	@cp install.sh dist/ 2>/dev/null || true
	@chmod +x dist/install.sh 2>/dev/null || true
ifdef WINDOWS_BUILD
	@echo "Creating Windows ZIP archive..."
	@powershell -NoProfile -Command "Compress-Archive -Path 'dist/*' -DestinationPath 'cnext-windows-x64-v$(VERSION).zip' -Force"
	@echo "Release archive: cnext-windows-x64-v$(VERSION).zip"
	@rm -rf dist
else
	@cd dist && tar -czf "$(CURDIR)/cnext-$(shell uname -s | tr A-Z a-z)-$(shell uname -m)-v$(VERSION).tar.gz" *
	@rm -rf dist
	@echo "Release archive: cnext-$(shell uname -s | tr A-Z a-z)-$(shell uname -m)-v$(VERSION).tar.gz"
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
	@echo "  parity     Self-hosting lexer parity gate (src2/ vs src/)"
	@echo "  parser-parity  Self-hosting parser AST parity gate"
	@echo "  bootstrap-test  Compile and run bootstrap tests through Stage 1"
	@echo "  msvc-defer-test  Validate MSVC defer lowering without MSVC"
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
	@echo "  BOOTSTRAP_OPT=-O2  Optimize self-build binaries (default: -O0)"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Release build"
	@echo "  make DEBUG=1            # Debug build"
	@echo "  make -j8                # Parallel build"
	@echo "  make install            # Install to $(INSTALL_DIR)"
	@echo "  make release            # Package for release"
	@echo "  make check              # Run all tests"
	@echo "  make bootstrap-test    # Verify Stage 1 code generation"
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
