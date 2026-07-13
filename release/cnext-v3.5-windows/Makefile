CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -iquote include

SRCS = src/main.c src/lexer.c src/parser.c src/ast.c src/codegen.c src/semantics.c
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
