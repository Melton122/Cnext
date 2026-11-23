#include "codegen_internal.h"
#include "diagnostics.h"

#ifdef _WIN32
#include <process.h>
#include <io.h>
#else
#include <unistd.h>
#endif

// Codegen buffers its output streams in temporary files, then stitches them
// together in a fixed order. tmpfile() is the natural choice because the
// OS deletes the file on close, but on Windows it resolves the temp directory
// from TMP/TEMP and fails outright when that directory denies the process
// create access (a locked-down or redirected profile does this). A compiler
// that cannot write a temporary file cannot compile anything, so fall back to
// an explicitly named file in TMPDIR/TMP/TEMP, then in the working directory,
// and remove it when the stream is closed.
#define CODEGEN_TEMP_SLOTS 4
static char codegen_temp_paths[CODEGEN_TEMP_SLOTS][1024];

static int codegen_pid(void)
{
#ifdef _WIN32
    return _getpid();
#else
    return (int)getpid();
#endif
}

static FILE* codegen_open_temp_path(char* temp_path, int slot)
{
    temp_path[0] = '\0';
    FILE* file = tmpfile();
    if (file) return file;

    const char* dirs[4];
    int dir_count = 0;
    const char* env = getenv("TMPDIR");
    if (env && *env) dirs[dir_count++] = env;
    env = getenv("TMP");
    if (env && *env) dirs[dir_count++] = env;
    env = getenv("TEMP");
    if (env && *env) dirs[dir_count++] = env;
    dirs[dir_count++] = ".";

    for (int d = 0; d < dir_count; d++) {
        for (int attempt = 0; attempt < 100; attempt++) {
            char path[1024];
            int wrote = snprintf(path, sizeof(path), "%s%c_cnext_codegen_%d_%d_%d.tmp",
                                 dirs[d], '/', codegen_pid(), slot, attempt);
            if (wrote <= 0 || wrote >= (int)sizeof(path)) continue;
            file = fopen(path, "w+b");
            if (file) {
                snprintf(temp_path, 1024, "%s", path);
                return file;
            }
        }
    }
    return NULL;
}

static FILE* codegen_open_temp(int slot) {
    return codegen_open_temp_path(codegen_temp_paths[slot], slot);
}

FILE* codegen_open_lambda_temp(char* path) {
    static int serial = CODEGEN_TEMP_SLOTS;
    return codegen_open_temp_path(path, serial++);
}

static void codegen_close_temp(FILE* file, int slot)
{
    if (file) fclose(file);
    if (slot >= 0 && slot < CODEGEN_TEMP_SLOTS && codegen_temp_paths[slot][0]) {
        remove(codegen_temp_paths[slot]);
        codegen_temp_paths[slot][0] = '\0';
    }
}

// Single global codegen context (all state in one place)
// Zero-initialized by default; gen_line starts at 1.
CodeGenContext g_codegen;

// Closure state (managed in codegen_closure.c, defined here)
ClosureInfo* closure_list = NULL;
int closure_counter = 0;
ClosureVar* closure_vars = NULL;
Scope* current_scope = NULL;
ASTNode* current_func_params = NULL;
bool current_func_returns_void = false;

// Class variable tracking (managed in codegen_types.c, defined here)
ClassVar* class_vars = NULL;

// Type substitution map (managed in codegen_types.c, defined here)
TypeSubst* type_subst_map = NULL;

// Generic spec tracking (managed in codegen_types.c, defined here)
GenSpec* gen_specs = NULL;

// Spec work queue (managed in codegen_generic.c, defined here)
SpecWork* spec_work_queue = NULL;
SpecWork* spec_work_last = NULL;

// Class spec queue (managed in codegen_classspec.c, defined here)
ClassSpecWork* class_spec_queue = NULL;
ClassSpecWork* class_spec_last = NULL;

// --- Public API ---

// Emits the C declarations that let a program refer to its own top-level
// functions and classes before they are defined: a forward typedef per
// aggregate, then a prototype per top-level function.
//
// Semantics already resolve these references (predeclare_global registers every
// top-level name up front), but codegen writes definitions in source order, so
// C was the first thing to see an upward call and rejected it with *implicit
// declaration* / *unknown type name*. Recursive descent parsers are defined by
// upward mutual recursion, so this is not an edge case.
//
// Aggregates are declared first because a prototype may name one as a
// parameter type, and a pointer to an incomplete type is legal in a prototype.
// Classes already emit `typedef struct X X;` ahead of their definition, so
// repeating it here is a no-op; structs are emitted in the same tagged shape
// for the same reason.
static void emit_forward_declarations(ASTNode* program) {
    bool wrote_typedef = false;
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child->type == AST_CLASS_DECL || child->type == AST_STRUCT_DECL) {
            fprintf(out, "typedef struct %.*s %.*s;\n",
                    child->token.length, child->token.start,
                    child->token.length, child->token.start);
            wrote_typedef = true;
        } else if (child->type == AST_ENUM_DECL) {
            // A variant-carrying enum is emitted as a tagged struct whose tag
            // is an anonymous enum; a plain one is a real C enum. Either way the
            // name itself is always defined via a struct or enum typedef.
            bool has_variants = false;
            for (int j = 0; j < child->child_count; j++) {
                if (child->children[j]->type == AST_VARIANT) { has_variants = true; break; }
            }
            fprintf(out, "typedef %s %.*s %.*s;\n", has_variants ? "struct" : "enum",
                    child->token.length, child->token.start,
                    child->token.length, child->token.start);
            wrote_typedef = true;
        } else if (child->type == AST_TYPE_ALIAS && child->left) {
            // Repeating the alias is harmless when it matches, and a no-op for
            // the common alias-to-primitive case that already worked.
            fprintf(out, "typedef ");
            generate_type(child->left->token, false);
            fprintf(out, " %.*s;\n", child->token.length, child->token.start);
            wrote_typedef = true;
        }
    }
    if (wrote_typedef) fprintf(out, "\n");

    bool wrote_proto = false;
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        // Generic declarations are instantiated into specialized functions
        // whose prototypes are emitted alongside them, and generators rewrite
        // their own signature entirely.
        if (child->type != AST_FUNC_DECL && child->type != AST_COROUTINE_DECL &&
            child->type != AST_ASYNC_FUNC_DECL) continue;
        if (child->type_param_count > 0 || child->is_generator) continue;
        if (!child->left) continue;  // declaration only, no body to emit later
        generate_function_prototype(child);
        wrote_proto = true;
    }
    if (wrote_proto) fprintf(out, "\n");
}

static void generate_runtime_preamble(void) {
    fprintf(out, "#include \"runtime.h\"\n\n");
    if (codegen_emit_lines && codegen_debug_source) {
        fprintf(out, "#line 1 \"%s\"\n", codegen_debug_source);
    }
}

void codegen_set_debug_source(const char* source_path) {
    free(codegen_debug_source);
    codegen_debug_source = NULL;
    codegen_emit_lines = false;
    codegen_last_emit_line = -1;
    if (!source_path || !source_path[0]) return;

    size_t len = strlen(source_path);
    char* escaped = (char*)checked_malloc(len * 2 + 1);
    if (!escaped) return;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (source_path[i] == '\\' || source_path[i] == '"') {
            escaped[j++] = '\\';
        }
        escaped[j++] = source_path[i];
    }
    escaped[j] = '\0';
    codegen_debug_source = escaped;
    codegen_emit_lines = true;
}

void set_codegen_sourcemap(SourceMap* map) {
    codegen_sourcemap = map;
    codegen_gen_line = 1;
    codegen_last_src_line = -1;
}

void reset_codegen_state(void) {
    indent_level = 0;
    current_parent_class = NULL;
    lambda_counter = 0;
    codegen_test_mode = false;
    codegen_in_test = false;
    codegen_test_count = 0;
    program_node = NULL;
    generator_counter = 0;
    codegen_sourcemap = NULL;
    codegen_gen_line = 1;
    codegen_last_src_line = -1;
    codegen_last_emit_line = -1;
    bench_counter = 0;
    loop_counter = 0;
    try_counter = 0;
    match_counter = 0;
    defer_counter = 0;

    while (closure_list) {
        ClosureInfo* ci = closure_list;
        closure_list = ci->next;
        free_captures(ci->captures);
        free(ci);
    }
    closure_counter = 0;

    while (closure_vars) {
        ClosureVar* cv = closure_vars;
        closure_vars = cv->next;
        free(cv->var_name);
        free(cv);
    }

    while (current_scope) {
        pop_scope();
    }

    while (class_vars) {
        ClassVar* cv = class_vars;
        class_vars = cv->next;
        free(cv->var_name);
        free(cv->class_name);
        free(cv);
    }

    clear_type_subst();
    free_gen_specs();
    free_spec_work_queue();
    free_class_spec_queue();
}

void set_codegen_profile_mode(bool enabled) {
    codegen_profile_mode = enabled;
}

bool generate_c_code(ASTNode* program, const char* output_filename, bool test_mode) {
    if (!program) return false;
    codegen_test_mode = test_mode;
    codegen_in_test = false;
    program_node = program;
    codegen_last_emit_line = -1;
    clear_type_subst();
    free_gen_specs();

    codegen_test_count = 0;
    for (int i = 0; i < program->child_count; i++) {
        if (program->children[i]->type == AST_TEST) codegen_test_count++;
    }
    int test_count = codegen_test_count;

    // Auto-cleaning temporary storage; codegen_open_temp falls back to a named
    // file when tmpfile() is unavailable.
    FILE* body_temp = codegen_open_temp(0);
    spec_out = codegen_open_temp(1);
    closure_defs_out = codegen_open_temp(2);
    defer_defs_out = codegen_open_temp(3);
    if (!body_temp || !spec_out || !closure_defs_out || !defer_defs_out) {
        diag_emit(DIAG_ERROR, ERR_CODEGEN_IO, 0, 0, "Could not create temporary files.", NULL, NULL);
        codegen_close_temp(body_temp, 0);
        codegen_close_temp(spec_out, 1);
        codegen_close_temp(closure_defs_out, 2);
        codegen_close_temp(defer_defs_out, 3);
        return false;
    }

    char** test_descriptions = NULL;
    if (test_count > 0) {
        test_descriptions = (char**)checked_malloc(sizeof(char*) * test_count);
        int idx = 0;
        for (int i = 0; i < program->child_count; i++) {
            if (program->children[i]->type == AST_TEST) {
                ASTNode* tn = program->children[i];
                test_descriptions[idx] = checked_strndup(tn->token.start, tn->token.length);
                idx++;
            }
        }
    }

    out = body_temp;
    indent_level = 0;

    // Phase 1: Discover all generic specializations
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child->type == AST_MAIN) {
            scan_node_for_generic_calls(child);
            scan_node_for_new_exprs(child);
        } else if (child->type == AST_FUNC_DECL && child->type_param_count == 0) {
            scan_node_for_generic_calls(child);
            scan_node_for_new_exprs(child);
        } else if ((child->type == AST_COROUTINE_DECL || child->type == AST_ASYNC_FUNC_DECL) && child->type_param_count == 0) {
            scan_node_for_generic_calls(child);
            scan_node_for_new_exprs(child);
        }
    }
    {
        SpecWork* sw = spec_work_queue;
        while (sw) {
            for (int i = 0; i < sw->func_decl->type_param_count && i < sw->resolved_count; i++) {
                ASTNode* tp = sw->func_decl->type_params[i];
                push_type_subst(
                    checked_strndup(tp->token.start, tp->token.length),
                    sw->resolved_args[i],
                    sw->resolved_is_array[i]
                );
            }
            scan_node_for_generic_calls(sw->func_decl->left);
            for (int i = 0; i < sw->func_decl->type_param_count && i < sw->resolved_count; i++) pop_type_subst();
            sw = sw->next;
        }
    }
    {
        FILE* saved_out = out;
        out = spec_out;
        for (SpecWork* sw = spec_work_queue; sw; sw = sw->next) {
            for (int ti = 0; ti < sw->func_decl->type_param_count && ti < sw->resolved_count; ti++) {
                ASTNode* tp = sw->func_decl->type_params[ti];
                push_type_subst(checked_strndup(tp->token.start, tp->token.length), sw->resolved_args[ti], sw->resolved_is_array[ti]);
            }
            generate_type(sw->func_decl->return_type, false);
            fprintf(out, " %s(", sw->mangled_name);
            for (int pi = 0; pi < sw->func_decl->child_count; pi++) {
                ASTNode* param = sw->func_decl->children[pi];
                if (pi > 0) fprintf(out, ", ");
                generate_node_type(param, param->is_pointer);
            }
            fprintf(out, ");\n");
            for (int ti = 0; ti < sw->func_decl->type_param_count && ti < sw->resolved_count; ti++) {
                pop_type_subst();
            }
        }
        for (ClassSpecWork* cs = class_spec_queue; cs; cs = cs->next) {
            fprintf(out, "typedef struct %s %s;\n", cs->mangled_name, cs->mangled_name);
        }
        emit_forward_declarations(program);
        if (gen_specs || class_spec_queue) fprintf(out, "\n");
        out = saved_out;
    }

    // Phase 2: Generate all nodes
    codegen_in_test = false;
    int test_idx = 0;
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* child = program->children[i];
        if (child->type == AST_TEST) {
            codegen_in_test = true;
            fprintf(out, "static bool _cnext_test_%d(void) {\n", test_idx);
            indent_level++;
            write_indent();
            fprintf(out, "bool _cnext_test_ok = true;\n");
            if (child->left) {
                for (int j = 0; j < child->left->child_count; j++) {
                    write_indent();
                    generate_node(child->left->children[j]);
                }
            }
            write_indent();
            fprintf(out, "return _cnext_test_ok;\n");
            indent_level--;
            fprintf(out, "}\n\n");
            codegen_in_test = false;
            test_idx++;
        } else {
            generate_node(child);
        }
    }

    if (test_count > 0) {
        bool has_main = false;
        for (int i = 0; i < program->child_count; i++) {
            if (program->children[i]->type == AST_MAIN) { has_main = true; break; }
        }
        if (!has_main) {
            fprintf(out, "int main(int argc, char** argv) {\n");
            indent_level++;
            write_indent();
            fprintf(out, "cnext_init_args(argc, argv);\n");
            write_indent();
            fprintf(out, "int _cnext_tp = 0, _cnext_tf = 0;\n");
            for (int i = 0; i < test_count; i++) {
                write_indent();
                fprintf(out, "if (_cnext_test_%d()) { _cnext_tp++; } else { _cnext_tf++; }\n", i);
            }
            write_indent();
            fprintf(out, "printf(\"Tests: %%d passed, %%d failed\\n\", _cnext_tp, _cnext_tf);\n");
            write_indent();
            fprintf(out, "_cnext_free_all();\n");
            write_indent();
            fprintf(out, "return _cnext_tf > 0 ? 1 : 0;\n");
            indent_level--;
            fprintf(out, "}\n\n");
        }
    }

    for (int i = 0; i < test_count; i++) {
        free(test_descriptions[i]);
    }
    free(test_descriptions);

    bool ok = !ferror(body_temp) && !ferror(spec_out) &&
              !ferror(closure_defs_out) && !ferror(defer_defs_out);

    // Flush streams before reading back — don't close yet
    fflush(spec_out);
    fflush(closure_defs_out);
    fflush(defer_defs_out);

    out = spec_out;
    indent_level = 0;
    clear_type_subst();
    process_spec_queue();
    clear_type_subst();
    process_class_spec_queue();
    clear_type_subst();
    free_gen_specs();
    out = NULL;

    if (!ok) {
        diag_emit(DIAG_ERROR, ERR_CODEGEN_IO, 0, 0, "Error writing temporary files.", NULL, NULL);
        codegen_close_temp(body_temp, 0);
        codegen_close_temp(spec_out, 1);
        codegen_close_temp(closure_defs_out, 2);
        codegen_close_temp(defer_defs_out, 3);
        return false;
    }

    // Combine final output from temporary streams
    out = fopen(output_filename, "w");
    if (!out) {
        char message[512];
        snprintf(message, sizeof(message), "Could not open output file \"%s\".", output_filename);
        diag_emit(DIAG_ERROR, ERR_CODEGEN_IO, 0, 0, message, NULL, NULL);
        codegen_close_temp(body_temp, 0);
        codegen_close_temp(spec_out, 1);
        codegen_close_temp(closure_defs_out, 2);
        codegen_close_temp(defer_defs_out, 3);
        return false;
    }

    generate_runtime_preamble();
    for (int i = 0; i < test_count; i++) {
        fprintf(out, "static bool _cnext_test_%d(void);\n", i);
    }
    if (test_count > 0) fprintf(out, "\n");

    // Copy spec temp to output
    if (spec_out) {
        rewind(spec_out);
        char line[4096];
        while (fgets(line, sizeof(line), spec_out)) {
            fprintf(out, "%s", line);
        }
        if (test_count > 0 || program->child_count > 0) fprintf(out, "\n");
    }

    // Copy closure temp to output
    if (closure_defs_out) {
        rewind(closure_defs_out);
        char line[4096];
        while (fgets(line, sizeof(line), closure_defs_out)) {
            fprintf(out, "%s", line);
        }
        if (test_count > 0 || program->child_count > 0) fprintf(out, "\n");
    }

    // Copy MSVC defer helpers to output. They must be at file scope because
    // MSVC has no nested functions and __finally cannot own the deferred body.
    if (defer_defs_out) {
        long defer_defs_size = ftell(defer_defs_out);
        rewind(defer_defs_out);
        char line[4096];
        while (fgets(line, sizeof(line), defer_defs_out)) {
            fprintf(out, "%s", line);
        }
        if (defer_defs_size > 0 && (test_count > 0 || program->child_count > 0)) fprintf(out, "\n");
    }

    // Copy body temp to output
    if (body_temp) {
        rewind(body_temp);
        char line[4096];
        while (fgets(line, sizeof(line), body_temp)) {
            fprintf(out, "%s", line);
        }
    }

    ok = !ferror(out);
    if (fclose(out) != 0) ok = false;
    if (!ok) {
        char message[512];
        snprintf(message, sizeof(message), "Could not write output file \"%s\".", output_filename);
        diag_emit(DIAG_ERROR, ERR_CODEGEN_IO, 0, 0, message, NULL, NULL);
    }

    // Closing the stream also removes the fallback file, if one was used.
    codegen_close_temp(body_temp, 0);
    codegen_close_temp(spec_out, 1);
    codegen_close_temp(closure_defs_out, 2);
    codegen_close_temp(defer_defs_out, 3);
    out = NULL;
    spec_out = NULL;
    closure_defs_out = NULL;
    defer_defs_out = NULL;
    return ok;
}
