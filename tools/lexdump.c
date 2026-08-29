/* Reference lexer token dumper for self-host parity testing.
 * Builds against the hand-written C lexer (src/lexer.c) and prints a
 * deterministic token stream; the Cnext port (src2/lexer.cn) must produce
 * byte-identical output.
 *
 * Line format: <line>:<column> <type-int> <escaped-text>
 */
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>

static void print_escaped(const char* s, int len) {
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '\n': fputs("\\n", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '"':  fputs("\\\"", stdout); break;
            default:
                if (c >= 32 && c <= 126) {
                    putchar(c);
                } else {
                    printf("\\x%02x", c);
                }
        }
    }
}

static void dump_lex(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* src = (char*)malloc((size_t)size + 1);
    if (!src) {
        fprintf(stderr, "OOM reading %s\n", path);
        exit(2);
    }
    size_t got = fread(src, 1, (size_t)size, f);
    fclose(f);
    src[got] = '\0';

    printf("%s\n", path);
    init_lexer(src);
    for (;;) {
        Token t = next_token();
        printf("%d:%d %d ", t.line, t.column, (int)t.type);
        print_escaped(t.start, t.length);
        putchar('\n');
        if (t.type == TOKEN_EOF) break;
    }
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) dump_lex(argv[i]);
    return 0;
}