#include "main_internal.h"

int compile_file(const char* input_path, const char* output_c_path, bool test_mode) {
    char* raw_source = read_file(input_path);
    if (!raw_source) return 74;

    char* source = build_source_with_packages(raw_source, ".");
    if (!source) {
        free(raw_source);
        return 74;
    }
    free(raw_source);

    ASTNode* program = parse_program(source);
    if (!program) {
        free(source);
        return 65;
    }

    int status = 0;
    if (!analyze_semantics(program, !test_mode)) {
        fprintf(stderr, "Semantic analysis failed.\n");
        status = 65;
    } else if (!generate_c_code(program, output_c_path, test_mode)) {
        status = 74;
    }

    free_ast(program);
    free(source);
    return status;
}
