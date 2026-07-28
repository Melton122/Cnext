#include "codegen_internal.h"

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

static void generate_runtime_preamble(void) {
    fprintf(out, "#include \"runtime.h\"\n\n");
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
    bench_counter = 0;
    loop_counter = 0;
    try_counter = 0;
    match_counter = 0;

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
    clear_type_subst();
    free_gen_specs();

    codegen_test_count = 0;
    for (int i = 0; i < program->child_count; i++) {
        if (program->children[i]->type == AST_TEST) codegen_test_count++;
    }
    int test_count = codegen_test_count;

    // Use tmpfile() for auto-cleaning temporary storage
    FILE* body_temp = tmpfile();
    spec_out = tmpfile();
    closure_defs_out = tmpfile();
    if (!body_temp || !spec_out || !closure_defs_out) {
        fprintf(stderr, "Could not create temporary files.\n");
        if (body_temp) fclose(body_temp);
        if (spec_out) fclose(spec_out);
        if (closure_defs_out) fclose(closure_defs_out);
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
                generate_type(param->var_type, param->is_pointer);
            }
            fprintf(out, ");\n");
            for (int ti = 0; ti < sw->func_decl->type_param_count && ti < sw->resolved_count; ti++) {
                pop_type_subst();
            }
        }
        for (ClassSpecWork* cs = class_spec_queue; cs; cs = cs->next) {
            fprintf(out, "typedef struct %s %s;\n", cs->mangled_name, cs->mangled_name);
        }
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
    
    bool ok = !ferror(body_temp);
    
    // Flush streams before reading back — don't close yet
    fflush(spec_out);
    fflush(closure_defs_out);
    
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
        fprintf(stderr, "Error writing temporary files.\n");
        fclose(body_temp);
        fclose(spec_out);
        fclose(closure_defs_out);
        return false;
    }
    
    // Combine final output from temporary streams
    out = fopen(output_filename, "w");
    if (!out) {
        fprintf(stderr, "Could not open output file \"%s\".\n", output_filename);
        if (body_temp) fclose(body_temp);
        if (spec_out) fclose(spec_out);
        if (closure_defs_out) fclose(closure_defs_out);
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
        fprintf(stderr, "Could not write output file \"%s\".\n", output_filename);
    }
    
    // tmpfile() handles auto-cleanup on close
    if (body_temp) fclose(body_temp);
    if (spec_out) fclose(spec_out);
    if (closure_defs_out) fclose(closure_defs_out);
    out = NULL;
    spec_out = NULL;
    closure_defs_out = NULL;
    return ok;
}
