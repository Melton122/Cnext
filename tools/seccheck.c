/* Focused unit test for the hardening added to main_utils.c / main_packages.c.
   Not part of the compiler build; run manually:
       gcc -std=gnu11 -Iinclude tools/seccheck.c -o tools/seccheck.exe */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* --- copy of the static helpers under test (kept in sync with sources) --- */
static bool is_safe_package_name(const char* name) {
    if (!name || name[0] == '\0') return false;
    if (name[0] == '-' || name[0] == '.' || name[0] == '/') return false;
    size_t len = strlen(name);
    if (name[len - 1] == '/') return false;
    if (strstr(name, "..") || strstr(name, "//")) return false;
    for (const char* p = name; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c == 0x7f) return false;
        if (c == '\\' || c == ':') return false;
        if (c == '"' || c == '\'' || c == '`' || c == '$' || c == ';' || c == '&' ||
            c == '|' || c == '<' || c == '>' || c == '(' || c == ')' || c == '{' ||
            c == '}' || c == '*' || c == '?' || c == '!') return false;
    }
    return true;
}

static void ps_quote(char* dst, size_t cap, const char* src) {
    size_t o = 0;
    if (o < cap - 1) dst[o++] = '\'';
    for (const char* p = src; *p; p++) {
        if (*p == '\'') {
            if (o < cap - 2) dst[o++] = '\'';
            if (o < cap - 1) dst[o++] = '\'';
        } else if (o < cap - 1) {
            dst[o++] = *p;
        }
    }
    if (o < cap - 1) dst[o++] = '\'';
    dst[o] = '\0';
}

static size_t append_quoted_arg(char* dst, size_t cap, size_t pos, const char* arg) {
    if (pos < cap - 1) dst[pos++] = '"';
    if (!*arg) {
        if (pos < cap - 1) dst[pos++] = '"';
        return pos;
    }
    int slashes = 0;
    for (const char* p = arg; *p; p++) {
        if (*p == '\\') { slashes++; continue; }
        int n = (*p == '"') ? slashes * 2 + 1 : slashes;
        for (int s = 0; s < n; s++)
            if (pos < cap - 1) dst[pos++] = '\\';
        slashes = 0;
        if (pos < cap - 1) dst[pos++] = *p;
    }
    for (int s = 0; s < slashes * 2; s++)
        if (pos < cap - 1) dst[pos++] = '\\';
    if (pos < cap - 1) dst[pos++] = '"';
    return pos;
}

/* Reference splitter implementing the CommandLineToArgvW rules, used to prove
   the emitted command line still round-trips to the original argv. */
static int split_cmdline(const char* line, char out[][256], int max) {
    int n = 0, i = 0;
    while (line[i] && n < max) {
        while (line[i] == ' ') i++;
        if (!line[i]) break;
        int o = 0;
        int in_quote = 0;
        if (line[i] == '"') { in_quote = 1; i++; }
        while (line[i] && (in_quote || line[i] != ' ')) {
            if (!in_quote && line[i] == '"') { in_quote = 1; i++; continue; }
            int sl = 0;
            while (line[i] == '\\') { sl++; i++; }
            if (line[i] == '"') {
                for (int s = 0; s < sl / 2; s++) out[n][o++] = '\\';
                if (sl % 2) { out[n][o++] = '"'; i++; }        /* escaped quote: data */
                else { in_quote = 0; i++; }                    /* closing quote */
            } else {
                for (int s = 0; s < sl; s++) out[n][o++] = '\\';
                if (line[i]) out[n][o++] = line[i++];
            }
        }
        out[n][o] = '\0';
        n++;
    }
    return n;
}

/* Print a string with backslashes made explicit so counts are unambiguous. */
static void show(const char* s) {
    for (const char* p = s; *p; p++) {
        if (*p == '\\') fputs("\\\\", stdout);
        else putchar(*p);
    }
}

static int fails = 0;

static void expect_bool(const char* what, const char* input, bool got, bool want) {
    if (got != want) { printf("  FAIL %s(%s): got %d want %d\n", what, input, got, want); fails++; }
    else printf("  ok   %s(%s) = %d\n", what, input, got);
}

static void expect_str(const char* what, const char* got, const char* want) {
    if (strcmp(got, want) != 0) { printf("  FAIL %s: got [%s] want [%s]\n", what, got, want); fails++; }
    else printf("  ok   %s = [%s]\n", what, got);
}

static void roundtrip(const char* arg) {
    char line[512];
    size_t pos = 0;
    pos = append_quoted_arg(line, sizeof(line), pos, arg);
    line[pos] = '\0';
    char got[4][256];
    int n = split_cmdline(line, got, 4);
    if (n != 1 || strcmp(got[0], arg) != 0) {
        printf("  FAIL roundtrip ["); show(arg);
        printf("] (len %d) cmdline [", (int)strlen(arg)); show(line);
        printf("] -> %d parts [", n);
        if (n) { show(got[0]); printf("] (len %d)", (int)strlen(got[0])); }
        printf("\n");
        fails++;
    } else {
        printf("  ok   roundtrip ["); show(arg);
        printf("] via ["); show(line); printf("]\n");
    }
}

int main(void) {
    puts("== is_safe_package_name: must REJECT ==");
    expect_bool("is_safe_package_name", "../evil", is_safe_package_name("../evil"), false);
    expect_bool("is_safe_package_name", "..\\evil", is_safe_package_name("..\\evil"), false);
    expect_bool("is_safe_package_name", "a\\b", is_safe_package_name("a\\b"), false);
    expect_bool("is_safe_package_name", "c:/x", is_safe_package_name("c:/x"), false);
    expect_bool("is_safe_package_name", "/abs", is_safe_package_name("/abs"), false);
    expect_bool("is_safe_package_name", "a//b", is_safe_package_name("a//b"), false);
    expect_bool("is_safe_package_name", "a/../b", is_safe_package_name("a/../b"), false);
    expect_bool("is_safe_package_name", "a/", is_safe_package_name("a/"), false);
    expect_bool("is_safe_package_name", "bad'name", is_safe_package_name("bad'name"), false);
    expect_bool("is_safe_package_name", "a;calc", is_safe_package_name("a;calc"), false);
    expect_bool("is_safe_package_name", "a&b", is_safe_package_name("a&b"), false);
    expect_bool("is_safe_package_name", "a`b", is_safe_package_name("a`b"), false);
    expect_bool("is_safe_package_name", "$(x)", is_safe_package_name("$(x)"), false);
    expect_bool("is_safe_package_name", "-K", is_safe_package_name("-K"), false);
    expect_bool("is_safe_package_name", ".hidden", is_safe_package_name(".hidden"), false);
    expect_bool("is_safe_package_name", "a\nb", is_safe_package_name("a\nb"), false);

    puts("\n== is_safe_package_name: must ACCEPT ==");
    expect_bool("is_safe_package_name", "mylib", is_safe_package_name("mylib"), true);
    expect_bool("is_safe_package_name", "std/http", is_safe_package_name("std/http"), true);
    expect_bool("is_safe_package_name", "mylib-1.2.3", is_safe_package_name("mylib-1.2.3"), true);
    expect_bool("is_safe_package_name", "my_lib.v2", is_safe_package_name("my_lib.v2"), true);

    puts("\n== ps_quote ==");
    char q[256];
    ps_quote(q, sizeof(q), "https://ok.com/a.cn");
    expect_str("ps_quote plain", q, "'https://ok.com/a.cn'");
    ps_quote(q, sizeof(q), "a'b");
    expect_str("ps_quote embedded quote", q, "'a''b'");
    ps_quote(q, sizeof(q), "'; calc; '");
    expect_str("ps_quote breakout attempt", q, "'''; calc; '''");

    puts("\n== append_quoted_arg round-trips through CommandLineToArgvW rules ==");
    roundtrip("plain");
    roundtrip("");
    roundtrip("with space");
    roundtrip("with\"quote");
    roundtrip("trailing\\");
    roundtrip("back\\\\slash");
    roundtrip("back\\\"slash");
    roundtrip("a\\\"; calc; '");
    roundtrip("{\"http_code\":200}");

    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
