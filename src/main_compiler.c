#include "main_internal.h"
#include "semantics_internal.h"
#include "optimizer.h"

int compile_file(const char* input_path, const char* output_c_path, bool test_mode, bool run_optimizer, bool emit_debug_lines) {
    char* raw_source = read_file(input_path);
    if (!raw_source) return 74;

    if (emit_debug_lines) {
        codegen_set_debug_source(input_path);
    }

    // `import "./util.cn"` merges local files first so that the package
    // expander sees the whole program, then packages are prepended as before.
    char source_dir[CNEXT_PATH_MAX];
    const char* dir = dirname_from_path(input_path, source_dir, sizeof(source_dir))
        ? source_dir : ".";
    OriginMap* origins = origin_map_create();
    char* expanded = build_source_with_local_imports(raw_source, dir, input_path, origins);
    if (!expanded) {
        origin_map_free(origins);
        free(raw_source);
        return 65;
    }

    int expanded_lines = origin_map_count_lines(expanded);
    char* source = build_source_with_packages(expanded, ".");
    free(expanded);
    if (!source) {
        origin_map_free(origins);
        free(raw_source);
        return 74;
    }
    /* The package expander prepends its own sources, which moves every line
       again; package code keeps falling back to plain line numbers. */
    origin_map_shift(origins, origin_map_count_lines(source) - expanded_lines);
    free(raw_source);

    if (origins->count > 0) diag_set_origin_map(origins);

    semantics_source = source;
    ASTNode* program = parse_program(source);
    if (!program) {
        diag_set_origin_map(NULL);
        origin_map_free(origins);
        free(source);
        return 65;
    }

    int status = 0;
    if (!analyze_semantics(program, !test_mode)) {
        fprintf(stderr, "Semantic analysis failed.\n");
        status = 65;
    } else {
        if (run_optimizer) {
            optimize_ast(program);
        }
        if (!generate_c_code(program, output_c_path, test_mode)) {
            status = 74;
        }
    }

    free_ast(program);
    diag_set_origin_map(NULL);
    origin_map_free(origins);
    free(source);
    return status;
}
