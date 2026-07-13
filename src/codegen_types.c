#include "codegen_internal.h"

// Generic type parameter substitution map

void push_type_subst(const char* name, Token type, bool is_array) {
    TypeSubst* ts = (TypeSubst*)malloc(sizeof(TypeSubst));
    ts->param_name = strdup(name);
    ts->concrete_type = type;
    ts->is_array = is_array;
    ts->next = type_subst_map;
    type_subst_map = ts;
}

void pop_type_subst(void) {
    if (type_subst_map) {
        TypeSubst* tmp = type_subst_map;
        type_subst_map = tmp->next;
        free(tmp->param_name);
        free(tmp);
    }
}

void clear_type_subst(void) {
    while (type_subst_map) {
        pop_type_subst();
    }
}

bool find_type_subst(const char* name, Token* out_type, bool* out_is_array) {
    for (TypeSubst* ts = type_subst_map; ts; ts = ts->next) {
        if (strcmp(ts->param_name, name) == 0) {
            *out_type = ts->concrete_type;
            *out_is_array = ts->is_array;
            return true;
        }
    }
    return false;
}

// Track class variables for pointer access in string interpolation

void track_class_var(const char* var_name, int var_len, const char* class_name, int class_len) {
    ClassVar* cv = (ClassVar*)malloc(sizeof(ClassVar));
    cv->var_name = strndup(var_name, var_len);
    cv->class_name = strndup(class_name, class_len);
    cv->next = class_vars;
    class_vars = cv;
}

const char* lookup_class_var(const char* var_name, int var_len) {
    for (ClassVar* cv = class_vars; cv; cv = cv->next) {
        if ((int)strlen(cv->var_name) == var_len && strncmp(cv->var_name, var_name, var_len) == 0) {
            return cv->class_name;
        }
    }
    return NULL;
}

// Track generated generic specializations to avoid duplication

bool has_gen_spec(const char* name) {
    for (GenSpec* gs = gen_specs; gs; gs = gs->next) {
        if (strcmp(gs->mangled_name, name) == 0) return true;
    }
    return false;
}

void add_gen_spec(const char* name, Token ret_type, bool ret_is_array) {
    GenSpec* gs = (GenSpec*)malloc(sizeof(GenSpec));
    gs->mangled_name = strdup(name);
    gs->return_type = ret_type;
    gs->return_is_array = ret_is_array;
    gs->next = gen_specs;
    gen_specs = gs;
}

void free_gen_specs(void) {
    while (gen_specs) {
        GenSpec* tmp = gen_specs;
        gen_specs = tmp->next;
        free(tmp->mangled_name);
        free(tmp);
    }
}
