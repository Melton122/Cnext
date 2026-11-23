/*
 * optimizer_leak_harness.c
 *
 * Regression harness for Phase 1: prove that the optimizer does not leak
 * memory across repeated compile cycles.
 *
 * It links against the compiler objects (minus main.c) and runs the exact
 * pipeline used by the real driver for each input:
 *
 *     parse_program -> analyze_semantics -> optimize_ast -> free_ast
 *
 * Bombarding the process with N cycles. Heap usage is measured at the
 * process/allocator level (private commit bytes on Windows, mallinfo2 on
 * glibc) rather than by interposing malloc/free -- interposition is fragile
 * against CRT-internal allocations in this codebase and cheap pads are not
 * needed for a leak signal that grows linearly with cycles.
 *
 * The harness runs both pipelines (opt and control) in interleaved batches so
 * allocator caches / bin provisioning affect both equally. A leak shows up as
 * a slope difference: optimizer batches keep growing, control batches stay
 * flat. After each optimized cycle it also checks the ownership invariant:
 * every owned token buffer must be the token's active text, and no other AST
 * node may borrow that buffer.
 *
 * Usage:
 *     make leaktest
 *     ./leaktest [cycles] [batch]
 *     LEAK_PROGRAM=<index> ./leaktest [cycles] [batch]
 *
 * Exit code 0: no leaks detected within the slope margin and all ownership
 * checks passed. Non-zero: an ownership check failed or optimizer batches grew
 * beyond the control baseline.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>

#include "parser.h"
#include "semantics.h"
#include "optimizer.h"
#include "ast.h"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <malloc.h>
#endif

/* --- heap measurement (OS level, no interposition) --- */

static size_t heap_bytes(void) {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return (size_t)pmc.PagefileUsage;   /* private commit bytes */
    }
    return 0;
#elif defined(__linux__)
    struct mallinfo2 mi = mallinfo2();
    return (size_t)(mi.uordblks + mi.hblkhd);
#else
    return 0;   /* measurement not available; treated as baseline */
#endif
}

#define OWNED_TEXT_MAX 4096

static bool visit_ownership(ASTNode* node, void** owned, int* count, bool collect) {
    if (!node) return true;
    if (collect) {
        if (node->owned_token_text) {
            for (int i = 0; i < *count; i++) {
                if (owned[i] == node->owned_token_text) return false;
            }
            if (*count >= OWNED_TEXT_MAX) return false;
            owned[(*count)++] = node->owned_token_text;
        }
    } else {
        if (node->owned_token_text && node->token.start != node->owned_token_text) return false;
        if (node->token.start) {
            for (int i = 0; i < *count; i++) {
                if (owned[i] == node->token.start && node->owned_token_text != node->token.start) return false;
            }
        }
    }
    for (int i = 0; i < node->child_count; i++) {
        if (!visit_ownership(node->children[i], owned, count, collect)) return false;
    }
    for (int i = 0; i < node->type_param_count; i++) {
        if (!visit_ownership(node->type_params[i], owned, count, collect)) return false;
    }
    for (int i = 0; i < node->type_arg_count; i++) {
        if (!visit_ownership(node->type_args[i], owned, count, collect)) return false;
    }
    return visit_ownership(node->left, owned, count, collect) &&
           visit_ownership(node->right, owned, count, collect) &&
           visit_ownership(node->condition, owned, count, collect) &&
           visit_ownership(node->init, owned, count, collect) &&
           visit_ownership(node->increment, owned, count, collect) &&
           visit_ownership(node->default_value, owned, count, collect);
}

static bool check_ast_ownership(ASTNode* program) {
    void* owned[OWNED_TEXT_MAX];
    int count = 0;
    if (!visit_ownership(program, owned, &count, true)) return false;
    return visit_ownership(program, owned, &count, false);
}

/* --- compile-cycle driver (mirrors compile_file's pipeline) --- */

static void run_cycle(const char* source, bool with_optimizer) {
    ASTNode* program = parse_program(source);
    if (!program) {
        fprintf(stderr, "leaktest: parse failed\n");
        exit(2);
    }
    if (getenv("LEAK_DEBUG")) { fprintf(stderr, "[diag] parsed\n"); fflush(stderr); }
    reset_semantics_state();
    if (!analyze_semantics(program, true)) {
        free_ast(program);
        return;
    }
    if (getenv("LEAK_DEBUG")) { fprintf(stderr, "[diag] analyzed\n"); fflush(stderr); }
    if (with_optimizer) {
        optimize_ast(program);
        if (!check_ast_ownership(program)) {
            fprintf(stderr, "leaktest: optimizer token ownership invariant violated\n");
            exit(3);
        }
        if (getenv("LEAK_DEBUG")) { fprintf(stderr, "[diag] optimized\n"); fflush(stderr); }
    }
    free_ast(program);
    if (getenv("LEAK_DEBUG")) { fprintf(stderr, "[diag] freed\n"); fflush(stderr); }
}

/* --- exercise programs (as compiled by the real driver) --- */

static const char* PROGRAMS[] = {
    /* constant folding + integer literals */
    "main {\n"
    "    var a = 2 + 3 * 4\n"
    "    var b = 100 - 50\n"
    "    printin(a)\n"
    "    printin(b)\n"
    "}\n",

    /* float literals + mixed int/float folding */
    "main {\n"
    "    var f = 3.5 * 2.0 + 1.5\n"
    "    var g = f + 1\n"
    "    printin(f)\n"
    "    printin(g)\n"
    "}\n",

    /* copy propagation */
    "main {\n"
    "    var x = 42\n"
    "    var y = x\n"
    "    var z = y\n"
    "    printin(z)\n"
    "}\n",

    /* nested expressions */
    "main {\n"
    "    var r = ((1 + 2) * (3 + 4)) + ((10 - 5) * (2 * 2))\n"
    "    printin(r)\n"
    "}\n",

    /* loops + strength reduction (x * 2 -> x + x) */
    "main {\n"
    "    for var i = 0; i < 10; i++ {\n"
    "        var t = i * 2\n"
    "        printin(t)\n"
    "    }\n"
    "}\n",

    /* strength reduction in a while condition, where the discarded literal is
     * owned by the condition node rather than a statement expression */
    "main {\n"
    "    var i = 1\n"
    "    while i * 2 {\n"
    "        i = i - 1\n"
    "        if i == 0 { break }\n"
    "    }\n"
    "    printin(i)\n"
    "}\n",

    /* functions + call folding */
    "func add(int a, int b): int {\n"
    "    return a + b\n"
    "}\n"
    "func mul(int a, int b): int {\n"
    "    return a * b\n"
    "}\n"
    "main {\n"
    "    printin(mul(add(2, 3), 5))\n"
    "}\n",

    /* identity elimination + dead code + branch simplification */
    "main {\n"
    "    var v = 0\n"
    "    if 1 {\n"
    "        v = v + 0\n"
    "    } else {\n"
    "        v = v * 1\n"
    "    }\n"
    "    while 0 {\n"
    "        v = 999\n"
    "    }\n"
    "    printin(v)\n"
    "}\n",

    /* unary folding + comparisons (x == x) */
    "main {\n"
    "    var n = -5\n"
    "    var a = n < 0\n"
    "    var b = a == a\n"
    "    printin(b)\n"
    "    printin(-n)\n"
    "}\n"
};

#define PROG_COUNT (sizeof(PROGRAMS) / sizeof(PROGRAMS[0]))

int main(int argc, char** argv) {
    long cycles = (argc > 1) ? strtol(argv[1], NULL, 10) : 1200;
    long batch = (argc > 2) ? strtol(argv[2], NULL, 10) : 200;
    if (cycles < 1) cycles = 1;
    if (batch < 1) batch = 1;
    int only_program = getenv("LEAK_PROGRAM") ? (int)strtol(getenv("LEAK_PROGRAM"), NULL, 10) : -1;

    printf("optimizer leak harness\n");
    printf("programs: %llu  total cycles: %ld  batch: %ld\n\n",
           (unsigned long long)PROG_COUNT, cycles, batch);

    if (heap_bytes() == 0) {
        printf("WARNING: no heap measurement available on this platform; "
               "results are meaningless.\n");
    }

    /* Warm up: let loader/CRT one-time allocations and allocator bin
     * provisioning settle before we start measuring. */
    for (long i = 0; i < 2; i++) {
        for (size_t p = 0; p < PROG_COUNT; p++) {
            if (only_program >= 0 && (int)p != only_program) continue;
            if (getenv("LEAK_DEBUG")) { fprintf(stderr, "[diag] warmup prog %llu\n", (unsigned long long)p); fflush(stderr); }
            run_cycle(PROGRAMS[p], true);
        }
    }

    printf("%-12s %-16s %-16s %-16s %-16s\n",
           "batch#", "control+?B", "optimizer+?B", "control cum", "opt cum");
    printf("%-12s %-16s %-16s %-16s %-16s\n",
           "----------", "----------------", "----------------", "----------------", "----------------");
    fflush(stdout);

    long long opt_total = 0;
    long long ctrl_total = 0;
    long batches = (cycles + batch - 1) / batch;

    for (long b = 1; b <= batches; b++) {
        /* control batch */
        size_t cb = heap_bytes();
        for (long i = 0; i < batch; i++)
            for (size_t p = 0; p < PROG_COUNT; p++) {
                if (only_program >= 0 && (int)p != only_program) continue;
                run_cycle(PROGRAMS[p], false);
            }
        long long cd = (long long)heap_bytes() - (long long)cb;
        ctrl_total += cd;

        /* optimizer batch */
        size_t ob = heap_bytes();
        for (long i = 0; i < batch; i++)
            for (size_t p = 0; p < PROG_COUNT; p++) {
                if (only_program >= 0 && (int)p != only_program) continue;
                run_cycle(PROGRAMS[p], true);
            }
        long long od = (long long)heap_bytes() - (long long)ob;
        opt_total += od;

        printf("%-12ld %-16lld %-16lld %-16lld %-16lld\n",
               b, cd, od, ctrl_total, opt_total);
        fflush(stdout);
    }

    int64_t excess = opt_total - ctrl_total;

    /* Margin absorbs slow allocator-bin creep that affects both batches
     * unequally; a real leak dwarfs it (thousands of nodes freed wrongly
     * become megabytes of private bytes across 1000+ cycles). */
    const long long margin = 16 * 1024;

    printf("\ncontrol cumulative growth: %lld bytes\n", ctrl_total);
    printf("optimizer cumulative growth: %lld bytes\n", opt_total);
    printf("optimizer excess vs control: %lld bytes (margin %lld)\n", excess, margin);

    if (excess > margin) {
        printf("\nRESULT: FAIL  (optimizer heap grows faster than control)\n");
        return 1;
    }
    printf("\nRESULT: PASS  (optimizer and control heap slopes match)\n");
    return 0;
}