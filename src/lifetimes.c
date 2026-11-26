#include "semantics_internal.h"

#define LIFE_LIMIT 4096
typedef struct { int scope; bool borrowed, escaped; } LifeGroup;
typedef struct { ASTNode* decl; int group, birth; bool moved, parameter; } LifeEntry;
typedef struct { int id, depth; bool deferred; } LifeArena;
typedef struct {
    ASTNode* root;
    LifeEntry entries[LIFE_LIMIT];
    LifeGroup groups[LIFE_LIMIT];
    LifeArena arenas[128];
    int entry_count, group_count, arena_count, next_scope, depth, control, move_floor;
    bool stopped;
} LifeState;
static int life_next_id;

static void life_node(LifeState* state, ASTNode* node);
static int life_value(LifeState* state, ASTNode* node, bool transfer);
static void life_error(ASTNode* node, const char* message) {
    report_token_error(ERR_SEM_TYPE_MISMATCH, node->token, message);
}
static bool life_named(ASTNode* call, const char* name) {
    return call && call->type == AST_CALL && call->left && call->left->type == AST_IDENTIFIER &&
           token_matches_name(call->left->token, name);
}
static bool life_named_list(ASTNode* call, const char* names) {
    if (!call || call->type != AST_CALL || !call->left || call->left->type != AST_IDENTIFIER) return false;
    const char* p = names;
    while (*p) {
        const char* end = strchr(p, '|');
        if (!end) end = p + strlen(p);
        if (call->left->token.length == end - p && !strncmp(call->left->token.start, p, (size_t)(end - p))) return true;
        p = *end ? end + 1 : end;
    }
    return false;
}
static bool life_fresh_call(ASTNode* node) {
    return life_named_list(node, "str_upper|str_lower|str_trim|str_replace|str_repeat|to_str|read_file|input|to_cstr|from_cstr");
}
static bool life_safe_call(ASTNode* node) {
    return life_fresh_call(node) || life_named_list(node, "print|printin|len|typeof|assert|debug|cstr_free|free");
}
static bool life_constructs(ASTNode* node) {
    if (!node) return false;
    if (node->type == AST_OWN_EXPR || life_named(node, "mem_scope_begin") || life_named(node, "mem_scope_end")) return true;
    if (life_constructs(node->left) || life_constructs(node->right) || life_constructs(node->condition) ||
        life_constructs(node->init) || life_constructs(node->increment) || life_constructs(node->default_value)) return true;
    for (int i = 0; i < node->child_count; i++) if (life_constructs(node->children[i])) return true;
    return false;
}
static bool life_resource(ASTNode* node) {
    if (!node) return false;
    return node->is_array || node->is_class || node->type == AST_NEW_EXPR || node->expr_type == TOKEN_STR_TYPE ||
           node->var_type.type == TOKEN_STR_TYPE || node->var_type.type == TOKEN_IDENTIFIER ||
           node->expr_type == TOKEN_IDENTIFIER;
}
static int life_scope_join(int a, int b) { return a > b ? a : b; }
static int life_group(LifeState* state, int scope, bool borrowed) {
    if (state->group_count + 1 >= LIFE_LIMIT) {
        life_error(state->root, "Too many resource values for lifetime analysis:");
        return 0;
    }
    int id = ++state->group_count;
    state->groups[id] = (LifeGroup){scope, borrowed, false};
    return id;
}
static LifeEntry* life_entry(LifeState* state, ASTNode* binding) {
    if (!binding || !binding->lifetime_id) return NULL;
    for (int i = state->entry_count - 1; i >= 0; i--)
        if (state->entries[i].decl->lifetime_id == binding->lifetime_id) return &state->entries[i];
    return NULL;
}
static bool life_scope_active(LifeState* state, int scope) {
    if (!scope) return true;
    for (int i = 0; i < state->arena_count; i++) if (state->arenas[i].id == scope) return true;
    return false;
}
static int life_current_scope(LifeState* state) {
    return state->arena_count ? state->arenas[state->arena_count - 1].id : 0;
}
static void life_read(LifeState* state, LifeEntry* entry, ASTNode* use) {
    if (entry->moved) life_error(use, "Use of moved binding:");
    if (entry->group && !life_scope_active(state, state->groups[entry->group].scope))
        life_error(use, "Use of value after its memory scope ended:");
}
static void life_add(LifeState* state, ASTNode* decl, int group, bool parameter) {
    if (state->entry_count >= LIFE_LIMIT) { life_error(decl, "Too many bindings for lifetime analysis:"); return; }
    decl->lifetime_id = ++life_next_id;
    state->entries[state->entry_count++] = (LifeEntry){decl, group, state->next_scope, false, parameter};
}
static int life_alias_count(LifeState* state, int group) {
    int count = 0;
    for (int i = 0; i < state->entry_count; i++)
        if (!state->entries[i].moved && state->entries[i].group == group) count++;
    return count;
}
// Snapshots copy binding state; group identities remain unique across branches.
static LifeEntry* life_snapshot(LifeState* state, int count) {
    LifeEntry* result = checked_malloc(sizeof(LifeEntry) * (size_t)(count ? count : 1));
    memcpy(result, state->entries, sizeof(LifeEntry) * (size_t)count);
    return result;
}
static void life_restore(LifeState* state, LifeEntry* entries, int count) {
    memcpy(state->entries, entries, sizeof(LifeEntry) * (size_t)count);
    state->entry_count = count;
    state->stopped = false;
}
static void life_merge(LifeState* state, LifeEntry* other, int count) {
    for (int i = 0; i < count; i++) {
        LifeEntry* entry = &state->entries[i];
        entry->moved = entry->moved || other[i].moved;
        if (entry->group != other[i].group) {
            int a = entry->group, b = other[i].group;
            if (a) state->groups[a].escaped = true;
            if (b) state->groups[b].escaped = true;
            entry->group = life_group(state, life_scope_join(a ? state->groups[a].scope : 0,
                                                           b ? state->groups[b].scope : 0), true);
        }
    }
    state->entry_count = count;
}
static bool life_owned_result(ASTNode* expr, int depth) {
    if (!expr || depth > 32) return false;
    if (expr->type == AST_OWN_EXPR || expr->type == AST_NEW_EXPR || expr->type == AST_LITERAL_ARRAY || life_fresh_call(expr)) return true;
    if (expr->type == AST_TERNARY) return life_owned_result(expr->left, depth + 1) && life_owned_result(expr->right, depth + 1);
    return false;
}
static void life_returns(ASTNode* body, bool* found, bool* fresh) {
    if (!body || body->type == AST_LAMBDA) return;
    if (body->type == AST_RETURN) { *found = true; *fresh = *fresh && life_owned_result(body->left, 0); return; }
    life_returns(body->left, found, fresh); life_returns(body->right, found, fresh);
    for (int i = 0; i < body->child_count; i++) life_returns(body->children[i], found, fresh);
}
typedef struct { bool move, brk, cont; } LifeScan;
static void life_scan(ASTNode* node, LifeScan* scan) {
    if (!node || node->type == AST_LAMBDA || node->type == AST_FUNC_DECL) return;
    if (node->type == AST_OWN_EXPR) scan->move = true;
    if (node->type == AST_BREAK) scan->brk = true;
    if (node->type == AST_CONTINUE) scan->cont = true;
    life_scan(node->left, scan); life_scan(node->right, scan); life_scan(node->condition, scan);
    life_scan(node->init, scan); life_scan(node->increment, scan); life_scan(node->default_value, scan);
    for (int i = 0; i < node->child_count; i++) life_scan(node->children[i], scan);
}
static void life_interpolation(LifeState* state, ASTNode* node, bool capture) {
    if (node->token.type != TOKEN_STRING_LITERAL) return;
    const char* p = node->token.start;
    const char* end = p + node->token.length;
    bool inside = false;
    while (p < end) {
        if (*p == '{') { inside = true; p++; continue; }
        if (*p == '}') { inside = false; p++; continue; }
        if (inside && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')) {
            const char* start = p++;
            while (p < end && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) p++;
            if (start > node->token.start && start[-1] == '.') continue;
            for (int i = state->entry_count - 1; i >= 0; i--) {
                Token token = state->entries[i].decl->token;
                if (token.length == p - start && !strncmp(token.start, start, (size_t)(p - start))) {
                    LifeEntry* entry = &state->entries[i];
                    life_read(state, entry, node);
                    if (capture && entry->group) {
                        if (state->groups[entry->group].scope) life_error(node, "Scoped references cannot be captured by callbacks or deferred work:");
                        state->groups[entry->group].escaped = true;
                    }
                    break;
                }
            }
        } else p++;
    }
}
static void life_capture(LifeState* state, ASTNode* node) {
    if (!node) return;
    if (node->type == AST_OWN_EXPR) { life_error(node, "Moves in callbacks or deferred work are not supported:"); return; }
    if (life_named(node, "mem_scope_begin") || life_named(node, "mem_scope_end")) {
        life_error(node, "Memory scopes in callbacks or deferred work are not supported:"); return;
    }
    if (node->type == AST_IDENTIFIER) {
        LifeEntry* entry = life_entry(state, node->binding);
        if (entry) {
            life_read(state, entry, node);
            if (entry->group) {
                if (state->groups[entry->group].scope) life_error(node, "Scoped references cannot be captured by callbacks or deferred work:");
                state->groups[entry->group].escaped = true;
            }
        }
    }
    if (node->type == AST_LITERAL) life_interpolation(state, node, true);
    life_capture(state, node->left); life_capture(state, node->right); life_capture(state, node->condition);
    life_capture(state, node->init); life_capture(state, node->increment);
    life_capture(state, node->default_value);
    for (int i = 0; i < node->child_count; i++) life_capture(state, node->children[i]);
}
static int life_value(LifeState* state, ASTNode* node, bool transfer) {
    if (!node) return 0;
    if (node->type == AST_IDENTIFIER) {
        LifeEntry* entry = life_entry(state, node->binding);
        if (entry) { life_read(state, entry, node); return entry->group; }
        return life_resource(node) ? life_group(state, 0, true) : 0;
    }
    if (node->type == AST_OWN_EXPR) {
        if (!transfer || state->control) life_error(node, "own requires an unconditional transfer position:");
        int group = life_value(state, node->left, false);
        LifeEntry* entry = node->left && node->left->type == AST_IDENTIFIER ? life_entry(state, node->left->binding) : NULL;
        bool fresh_position = node->left && (node->left->type == AST_CALL || node->left->type == AST_NEW_EXPR || node->left->type == AST_LITERAL_ARRAY);
        if ((!entry && !fresh_position) || (entry && entry->parameter) || !group) {
            life_error(node, "own requires a local resource binding or a fresh value:"); return group;
        }
        if (entry && entry - state->entries < state->move_floor)
            life_error(node, "Cannot move a binding carried between loop iterations:");
        if (state->groups[group].scope) life_error(node, "Cannot move a value out of an arena scope:");
        if (state->groups[group].borrowed || state->groups[group].escaped) life_error(node, "Cannot move a borrowed, captured, or escaped value:");
        if (entry && life_alias_count(state, group) > 1) life_error(node, "Cannot move an aliased value:");
        if (entry) entry->moved = true;
        return group;
    }
    if (node->type == AST_LITERAL) { life_interpolation(state, node, false); return life_resource(node) ? life_group(state, 0, true) : 0; }
    if (node->type == AST_NULL_LITERAL) return 0;
    if (node->type == AST_LITERAL_ARRAY) {
        for (int i = 0; i < node->child_count; i++) {
            int group = life_value(state, node->children[i], false);
            if (group) {
                if (state->groups[group].scope) life_error(node, "Scoped reference cannot escape through an array:");
                state->groups[group].escaped = true;
            }
        }
        return life_group(state, 0, false);
    }
    if (node->type == AST_LAMBDA) { life_capture(state, node); return 0; }
    if (node->type == AST_MEMBER_ACCESS || node->type == AST_INDEX || node->type == AST_SLICE) {
        int parent = life_value(state, node->left, false);
        life_value(state, node->right, false);
        if (node->type == AST_SLICE) return parent;
        if (parent && life_resource(node)) {
            state->groups[parent].escaped = true;
            return life_group(state, state->groups[parent].scope, true);
        }
        return 0;
    }
    if (node->type == AST_TERNARY || node->type == AST_WHEN) {
        life_value(state, node->condition, false);
        state->control++;
        int result = 0;
        if (node->type == AST_TERNARY) {
            int a = life_value(state, node->left, false), b = life_value(state, node->right, false);
            if (a == b) result = a;
            else {
                int scope = life_scope_join(a ? state->groups[a].scope : 0, b ? state->groups[b].scope : 0);
                if (a) state->groups[a].escaped = true;
                if (b) state->groups[b].escaped = true;
                if (a || b) result = life_group(state, scope, true);
            }
        } else for (int i = 0; i < node->child_count; i++) {
            ASTNode* arm = node->children[i];
            life_value(state, arm->condition, false);
            int group = life_value(state, arm->left, false);
            if (group) {
                state->groups[group].escaped = true;
                int scope = life_scope_join(result ? state->groups[result].scope : 0, state->groups[group].scope);
                result = life_group(state, scope, true);
            }
        }
        state->control--;
        return result;
    }
    if (node->type == AST_CALL || node->type == AST_NEW_EXPR) {
        if (life_named(node, "mem_scope_begin") || life_named(node, "mem_scope_end")) {
            life_error(node, "Memory-scope operations require a standalone statement:"); return 0;
        }
        bool safe = life_safe_call(node);
        ASTNode* callee = node->left ? node->left->binding : NULL;
        int groups[128], count = 0;
        bool moved[128];
        if (node->child_count > 127) { life_error(node, "Too many arguments for lifetime analysis:"); return 0; }
        if (node->type == AST_CALL && node->left && node->left->type != AST_IDENTIFIER) {
            ASTNode* receiver = node->left->type == AST_MEMBER_ACCESS ? node->left->left : node->left;
            int group = life_value(state, receiver, false);
            if (group) { groups[count] = group; moved[count++] = false; }
        }
        for (int i = 0; i < node->child_count; i++) {
            ASTNode* arg = node->children[i];
            if (arg->type == AST_NAMED_ARG) arg = arg->right;
            int group = life_value(state, arg, true);
            bool move = arg->type == AST_OWN_EXPR;
            if (move && !safe && node->type != AST_NEW_EXPR && (!callee || !callee->left))
                life_error(arg, "Ownership transfer to foreign or unknown calls is not supported:");
            if (group) {
                for (int j = 0; j < count; j++) if (groups[j] == group && (move || moved[j]))
                    life_error(arg, "Cannot move and borrow the same value in one call:");
                groups[count] = group; moved[count++] = move;
            }
        }
        for (int i = 0; i < count; i++) if (!safe && !moved[i]) {
            if (state->groups[groups[i]].scope) life_error(node, "Scoped references cannot escape through calls:");
            state->groups[groups[i]].escaped = true;
        }
        if (node->type == AST_NEW_EXPR) return life_group(state, life_current_scope(state), false);
        if (life_resource(node) || life_fresh_call(node)) {
            bool found = false, fresh = true;
            if (callee && callee->left) life_returns(callee->left, &found, &fresh);
            int scope = node->is_class || node->expr_type == TOKEN_IDENTIFIER ? life_current_scope(state) : 0;
            return life_group(state, scope, !(life_fresh_call(node) || (found && fresh)));
        }
        return 0;
    }
    if (node->type == AST_ASSIGN) { life_node(state, node); return 0; }
    int scope = 0;
    // Conservative for wrappers, short-circuit operators, and propagation expressions.
    state->control++;
    ASTNode* parts[] = {node->left, node->right, node->condition};
    for (int i = 0; i < 3 + node->child_count; i++) {
        int group = life_value(state, i < 3 ? parts[i] : node->children[i - 3], false);
        if (group && life_resource(node)) {
            state->groups[group].escaped = true;
            scope = life_scope_join(scope, state->groups[group].scope);
        }
    }
    state->control--;
    return life_resource(node) ? life_group(state, scope, true) : 0;
}
static void life_end(LifeState* state, ASTNode* node, bool deferred) {
    if (!state->arena_count || (!deferred && state->arenas[state->arena_count - 1].depth != state->depth)) {
        life_error(node, "Memory scopes must be balanced in their lexical block:"); return;
    }
    if (!deferred && state->arenas[state->arena_count - 1].deferred) {
        life_error(node, "Memory scope is already scheduled for deferred cleanup:"); return;
    }
    state->arena_count--;
}
static void life_node(LifeState* state, ASTNode* node) {
    if (!node || state->stopped) return;
    if (node->type == AST_BLOCK || node->type == AST_MAIN) {
        if (node->type == AST_MAIN) { life_node(state, node->left); return; }
        int entries = state->entry_count, arenas = state->arena_count;
        state->depth++;
        for (int i = 0; i < node->child_count; i++) life_node(state, node->children[i]);
        while (state->arena_count > arenas) {
            if (!state->arenas[state->arena_count - 1].deferred) life_error(node, "Memory scope may leave without cleanup; close it or defer its end:");
            life_end(state, node, true);
        }
        state->entry_count = entries;
        state->depth--;
    } else if (node->type == AST_VAR_DECL) {
        int group = life_value(state, node->init, true);
        life_add(state, node, group, false);
    } else if (node->type == AST_ASSIGN) {
        LifeEntry* entry = node->left && node->left->type == AST_IDENTIFIER ? life_entry(state, node->left->binding) : NULL;
        if (!entry || node->token.type != TOKEN_EQUAL) life_value(state, node->left, false);
        int group = life_value(state, node->right, true);
        if (entry) {
            if (node->right && node->right->type == AST_OWN_EXPR && node->right->left && node->right->left->binding == entry->decl)
                life_error(node, "Cannot move a binding into itself:");
            if (group && state->groups[group].scope && entry->birth < state->groups[group].scope)
                life_error(node, "Scoped reference cannot be stored in an outer binding:");
            // Preserve possible loop-exit assignments even when a break/return
            // removes that branch from the body's continuing path.
            if (state->control || entry - state->entries < state->move_floor) {
                if (entry->group) state->groups[entry->group].escaped = true;
                if (group) state->groups[group].escaped = true;
                int scope = life_scope_join(entry->group ? state->groups[entry->group].scope : 0, group ? state->groups[group].scope : 0);
                group = entry->group || group ? life_group(state, scope, true) : 0;
            } else entry->moved = false;
            entry->group = group;
        } else if (group) {
            if (state->groups[group].scope) life_error(node, "Scoped reference cannot escape through fields or globals:");
            state->groups[group].escaped = true;
        }
    } else if (node->type == AST_RETURN || node->type == AST_THROW || node->type == AST_BREAK || node->type == AST_CONTINUE) {
        int group = life_value(state, node->left, true);
        if (group && state->groups[group].scope) life_error(node, "Scoped reference cannot be returned or thrown:");
        for (int i = 0; i < state->arena_count; i++) if (!state->arenas[i].deferred)
            life_error(node, "Memory scope may leave without cleanup; close it or defer its end:");
        state->stopped = true;
    } else if (node->type == AST_DEFER) {
        ASTNode* value = node->left;
        if (value && value->type == AST_BLOCK && value->child_count == 1) value = value->children[0];
        if (value && value->type == AST_EXPR_STMT) value = value->left;
        if (life_named(value, "mem_scope_end")) {
            if (!state->arena_count || state->arenas[state->arena_count - 1].depth != state->depth || state->arenas[state->arena_count - 1].deferred)
                life_error(node, "Deferred scope cleanup requires one open local memory scope:");
            else state->arenas[state->arena_count - 1].deferred = true;
        } else life_capture(state, node->left);
    } else if (node->type == AST_EXPR_STMT && life_named(node->left, "mem_scope_begin")) {
        if (state->arena_count == 128) { life_error(node, "Too many nested memory scopes:"); return; }
        state->arenas[state->arena_count++] = (LifeArena){++state->next_scope, state->depth, false};
    } else if (node->type == AST_EXPR_STMT && life_named(node->left, "mem_scope_end")) {
        life_end(state, node, false);
    } else if (node->type == AST_IF) {
        state->control++;
        life_value(state, node->condition, false);
        state->control--;
        int entries = state->entry_count;
        LifeEntry* before = life_snapshot(state, entries);
        life_node(state, node->left);
        bool left_stopped = state->stopped;
        LifeEntry* left = life_snapshot(state, entries);
        life_restore(state, before, entries);
        life_node(state, node->right);
        bool right_stopped = state->stopped;
        if (right_stopped) life_restore(state, left, entries);
        else if (!left_stopped) life_merge(state, left, entries);
        state->stopped = left_stopped && right_stopped;
        free(before); free(left);
    } else if (node->type == AST_WHILE || node->type == AST_FOR || node->type == AST_FOR_IN) {
        int entries = state->entry_count, floor = state->move_floor;
        life_node(state, node->init);
        int loop_entries = state->entry_count;
        LifeEntry* before = life_snapshot(state, loop_entries);
        state->move_floor = loop_entries;
        state->control++;
        life_value(state, node->condition, false);
        state->control--;
        life_node(state, node->left);
        // A loop may execute zero times. Outer moves remain forbidden, including
        // on early exits, and conditional initialization cannot revive a binding.
        state->stopped = false;
        state->control++;
        life_node(state, node->increment);
        state->control--;
        life_merge(state, before, loop_entries);
        state->entry_count = entries;
        state->move_floor = floor;
        free(before);
    } else if (node->type == AST_SWITCH || node->type == AST_MATCH) {
        int entries = state->entry_count;
        state->control++;
        life_value(state, node->condition, false);
        state->control--;
        LifeEntry* before = life_snapshot(state, entries);
        int branches = node->child_count;
        LifeEntry** results = checked_malloc(sizeof(LifeEntry*) * (size_t)(branches ? branches : 1));
        bool* continues = checked_malloc(sizeof(bool) * (size_t)(branches ? branches : 1));
        bool has_default = false;
        for (int i = 0; i < branches; i++) {
            ASTNode* arm = node->children[i];
            if (arm->type == AST_DEFAULT || (arm->type == AST_MATCH_ARM && arm->is_const && !arm->condition))
                has_default = true;
            LifeScan scan = {false, false, false};
            life_scan(arm->left, &scan);
            if (scan.move && (scan.brk || scan.cont))
                life_error(arm, "Moves in switch/match arms containing break or continue are not supported:");
            life_restore(state, before, entries);
            life_node(state, arm);
            // A break exits only the switch, so that arm still reaches the code
            // after it. Other terminating exits leave the construct entirely.
            continues[i] = !state->stopped || scan.brk;
            results[i] = life_snapshot(state, entries);
        }
        int first = -1;
        for (int i = 0; i < branches; i++) if (continues[i]) { first = i; break; }
        if (first < 0) {
            // Without a default, the no-match path still reaches after the switch.
            life_restore(state, before, entries);
            state->stopped = has_default && branches > 0;
        } else if (has_default) {
            life_restore(state, results[first], entries);
            for (int i = first + 1; i < branches; i++) if (continues[i]) life_merge(state, results[i], entries);
            state->stopped = false;
        } else {
            life_restore(state, before, entries);
            for (int i = 0; i < branches; i++) if (continues[i]) life_merge(state, results[i], entries);
            state->stopped = false;
        }
        for (int i = 0; i < branches; i++) free(results[i]);
        free(results); free(continues); free(before);
    } else if (node->type == AST_TRY) {
        int entries = state->entry_count;
        if (node->init) life_node(state, node->init);
        state->control++;
        if (node->condition) life_node(state, node->condition);
        state->stopped = false;
        life_node(state, node->left); state->stopped = false;
        life_node(state, node->right); state->stopped = false;
        life_node(state, node->increment); state->stopped = false;
        for (int i = 0; i < node->child_count; i++) {
            life_node(state, node->children[i]); state->stopped = false;
        }
        state->control--;
        state->entry_count = entries;
        // Exception joins remain conservative sequential analysis for now.
        state->stopped = false;
    } else if (node->type == AST_FUNC_DECL || node->type == AST_LAMBDA) {
        life_capture(state, node);
    } else if (node->type == AST_EXPR_STMT) life_value(state, node->left, false);
    else if (node->type == AST_DESTRUCTURE) life_capture(state, node);
    else if (node->type == AST_CATCH || node->type == AST_CASE || node->type == AST_DEFAULT ||
             node->type == AST_MATCH_ARM || node->type == AST_BENCH_DECL) {
        life_value(state, node->condition, false);
        life_node(state, node->left); life_node(state, node->right);
        for (int i = 0; i < node->child_count; i++) life_node(state, node->children[i]);
    } else life_value(state, node, false);
}
static void life_function(ASTNode* function) {
    if (!function->left) return;
    for (int i = 0; i < function->child_count; i++) {
        ASTNode* param = function->children[i];
        if (life_constructs(param->default_value)) life_error(param, "Ownership and memory scopes are not supported in default arguments:");
    }
    if (function->is_generator || function->type == AST_ASYNC_FUNC_DECL || function->type == AST_COROUTINE_DECL) {
        if (life_constructs(function->left)) life_error(function, "Ownership and memory scopes are not supported in suspended functions:");
        return;
    }
    LifeState* state = (LifeState*)checked_calloc(1, sizeof(*state));
    state->root = function;
    for (int i = 0; i < function->child_count; i++) {
        ASTNode* param = function->children[i];
        if (param->type == AST_VAR_DECL) life_add(state, param, life_resource(param) ? life_group(state, 0, true) : 0, true);
    }
    life_node(state, function->left);
    free(state);
}
void analyze_lifetimes(ASTNode* program) {
    if (!life_constructs(program)) return;
    life_next_id = 0;
    for (int i = 0; i < program->child_count; i++) {
        ASTNode* node = program->children[i];
        if (node->type == AST_MAIN || node->type == AST_TEST || node->type == AST_FUNC_DECL || node->type == AST_ASYNC_FUNC_DECL || node->type == AST_COROUTINE_DECL)
            life_function(node);
        else if (node->type == AST_CLASS_DECL || node->type == AST_STRUCT_DECL) {
            for (int j = 0; j < node->child_count; j++) {
                ASTNode* member = node->children[j];
                if (member->type == AST_FUNC_DECL || member->type == AST_CONSTRUCTOR || member->type == AST_OPERATOR_DECL)
                    life_function(member);
                else if (life_constructs(member)) life_error(member, "Ownership and memory scopes require a supported function body:");
            }
        } else if (life_constructs(node)) life_error(node, "Ownership and memory scopes require a function body:");
    }
}
