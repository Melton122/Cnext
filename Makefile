CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -iquote include

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
EXEC = cnext.exe

.PHONY: all clean test

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXEC) temp_out.c _cnext_*.tmp

test: $(EXEC)
	./$(EXEC) test
