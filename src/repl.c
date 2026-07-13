#include "repl.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "semantics.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define CNEXT_TEMP_REPL_C "cnext_repl_temp.c"
#define CNEXT_TEMP_REPL_EXE "cnext_repl_temp.exe"
#else
#define CNEXT_TEMP_REPL_C "/tmp/cnext_repl_temp.c"
#define CNEXT_TEMP_REPL_EXE "/tmp/cnext_repl_temp"
#endif

static char last_generated_c[65536] = "";

static void print_banner(void) {
    printf("Cnext REPL v6.0\n");
    printf("Type .help for commands, .exit to quit\n\n");
}

static void print_help(void) {
    printf("Commands:\n");
    printf("  .help     Show this help message\n");
    printf("  .exit     Exit the REPL\n");
    printf("  .c        Show last generated C code\n");
    printf("\n");
}

static int run_process_repl(const char* program, char* const args[]) {
#ifdef _WIN32
    intptr_t result = _spawnvp(_P_WAIT, program, (const char* const*)args);
    if (result == -1) {
        fprintf(stderr, "Could not start %s: %s\n", program, strerror(errno));
        return 127;
    }
    return (int)result;
#else
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Could not start %s: %s\n", program, strerror(errno));
        return 127;
    }
    if (pid == 0) {
        execvp(program, args);
        fprintf(stderr, "Could not start %s: %s\n", program, strerror(errno));
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "Could not wait for %s: %s\n", program, strerror(errno));
        return 127;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 127;
#endif
}

static bool compile_and_run(const char* c_code) {
    FILE* f = fopen(CNEXT_TEMP_REPL_C, "w");
    if (!f) {
        fprintf(stderr, "Could not create temp file.\n");
        return false;
    }
    fwrite(c_code, 1, strlen(c_code), f);
    fclose(f);

    char* gcc_args[] = {"gcc", "-std=gnu11", "-o", CNEXT_TEMP_REPL_EXE, CNEXT_TEMP_REPL_C, NULL};
    int compile_status = run_process_repl("gcc", gcc_args);
    if (compile_status != 0) {
        fprintf(stderr, "Compilation failed.\n");
        remove(CNEXT_TEMP_REPL_C);
        return false;
    }

    char* run_args[] = {CNEXT_TEMP_REPL_EXE, NULL};
    run_process_repl(CNEXT_TEMP_REPL_EXE, run_args);

    remove(CNEXT_TEMP_REPL_C);
    remove(CNEXT_TEMP_REPL_EXE);

    return true;
}

bool run_repl(void) {
    print_banner();

    char line[4096];

    for (;;) {
        printf(">> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        // Remove trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) continue;

        // Handle REPL commands
        if (line[0] == '.') {
            if (strcmp(line, ".exit") == 0 || strcmp(line, ".quit") == 0) {
                break;
            } else if (strcmp(line, ".help") == 0) {
                print_help();
            } else if (strcmp(line, ".c") == 0) {
                if (last_generated_c[0]) {
                    printf("%s\n", last_generated_c);
                } else {
                    printf("No code generated yet.\n");
                }
            } else {
                printf("Unknown command: %s\n", line);
                printf("Type .help for commands.\n");
            }
            continue;
        }

        // Wrap bare expressions in a main() with println
        char expr_buf[8192];
        if (line[0] != '{' && strstr(line, "func ") != line &&
            strstr(line, "class ") == NULL && strstr(line, "struct ") == NULL &&
            strstr(line, "enum ") == NULL) {
            snprintf(expr_buf, sizeof(expr_buf),
                "import \"io\"\n\nfunc main() {\n    println(%s);\n}\n", line);
        } else {
            snprintf(expr_buf, sizeof(expr_buf), "%s\n", line);
        }

        // Parse
        ASTNode* program = parse_program(expr_buf);
        if (!program) {
            fprintf(stderr, "Parse error.\n");
            continue;
        }

        // Type check
        if (!analyze_semantics(program, true)) {
            fprintf(stderr, "Type error.\n");
            free_ast(program);
            continue;
        }

        // Generate C
        if (!generate_c_code(program, CNEXT_TEMP_REPL_C, true)) {
            fprintf(stderr, "Code generation failed.\n");
            free_ast(program);
            continue;
        }
        free_ast(program);

        // Read generated C
        FILE* f = fopen(CNEXT_TEMP_REPL_C, "rb");
        if (!f) {
            fprintf(stderr, "Could not read generated code.\n");
            continue;
        }
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        rewind(f);
        char* gen = (char*)malloc(fsize + 1);
        fread(gen, 1, fsize, f);
        gen[fsize] = '\0';
        fclose(f);

        // Save for .c command
        strncpy(last_generated_c, gen, sizeof(last_generated_c) - 1);
        last_generated_c[sizeof(last_generated_c) - 1] = '\0';

        // Compile and run
        compile_and_run(gen);
        free(gen);
    }

    printf("Goodbye.\n");
    return true;
}
