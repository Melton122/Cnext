#ifndef CNEXT_JSON_H
#define CNEXT_JSON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// --- JSON types ---

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

struct json_value;

typedef struct json_value {
    json_type_t type;
    union {
        int b;           // bool
        double n;        // number
        char* s;         // string
        struct {
            struct json_value** items;
            size_t count;
        } a;             // array
        struct {
            char** keys;
            struct json_value** values;
            size_t count;
        } o;             // object
    } u;
} json_value_t;

// Forward declaration
static inline void json_free(void* root);

// --- Simple string builder ---

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} sb_t;

static inline void sb_init(sb_t* sb) {
    sb->data = (char*)malloc(256);
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = 256;
}

static inline void sb_append(sb_t* sb, const char* s) {
    size_t n = strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        sb->cap = (sb->len + n + 1) * 2;
        sb->data = (char*)realloc(sb->data, sb->cap);
    }
    memcpy(sb->data + sb->len, s, n + 1);
    sb->len += n;
}

static inline void sb_append_char(sb_t* sb, char c) {
    if (sb->len + 2 > sb->cap) {
        sb->cap *= 2;
        sb->data = (char*)realloc(sb->data, sb->cap);
    }
    sb->data[sb->len] = c;
    sb->data[sb->len + 1] = '\0';
    sb->len++;
}

static inline CnextString sb_to_string(sb_t* sb) {
    if (!sb->data) return (CnextString){NULL, 0};
    _cnext_track(sb->data);
    return (CnextString){sb->data, sb->len};
}

// --- JSON parser ---

typedef struct {
    const char* json;
    size_t pos;
    size_t len;
} json_parser_t;

static inline void json_skip_ws(json_parser_t* p) {
    while (p->pos < p->len && isspace((unsigned char)p->json[p->pos])) p->pos++;
}

static inline char json_peek(json_parser_t* p) {
    json_skip_ws(p);
    return (p->pos < p->len) ? p->json[p->pos] : '\0';
}

static inline char json_advance(json_parser_t* p) {
    json_skip_ws(p);
    return (p->pos < p->len) ? p->json[p->pos++] : '\0';
}

static inline bool json_expect(json_parser_t* p, char c) {
    if (json_advance(p) == c) return true;
    return false;
}

static inline char* json_parse_string(json_parser_t* p) {
    if (!json_expect(p, '"')) return NULL;
    size_t start = p->pos;
    sb_t sb;
    sb_init(&sb);
    while (p->pos < p->len && p->json[p->pos] != '"') {
        if (p->json[p->pos] == '\\') {
            p->pos++;
            if (p->pos >= p->len) break;
            char c = p->json[p->pos];
            switch (c) {
                case '"': case '\\': case '/': sb_append_char(&sb, c); break;
                case 'b': sb_append_char(&sb, '\b'); break;
                case 'f': sb_append_char(&sb, '\f'); break;
                case 'n': sb_append_char(&sb, '\n'); break;
                case 'r': sb_append_char(&sb, '\r'); break;
                case 't': sb_append_char(&sb, '\t'); break;
                case 'u': {
                    if (p->pos + 4 < p->len) {
                        sb_append_char(&sb, '?'); // Unicode \uXXXX not fully supported
                        p->pos += 4;
                    }
                    break;
                }
                default: sb_append_char(&sb, c); break;
            }
            p->pos++;
        } else {
            sb_append_char(&sb, p->json[p->pos]);
            p->pos++;
        }
    }
    if (!json_expect(p, '"')) { free(sb.data); return NULL; }
    char* result = sb.data;
    return result;
}

static inline json_value_t* json_parse_value(json_parser_t* p);

static inline json_value_t* json_parse_number(json_parser_t* p) {
    json_skip_ws(p);
    size_t start = p->pos;
    bool has_dot = false;
    if (p->pos < p->len && (p->json[p->pos] == '-' || p->json[p->pos] == '+')) p->pos++;
    while (p->pos < p->len && isdigit((unsigned char)p->json[p->pos])) p->pos++;
    if (p->pos < p->len && p->json[p->pos] == '.') {
        has_dot = true;
        p->pos++;
        while (p->pos < p->len && isdigit((unsigned char)p->json[p->pos])) p->pos++;
    }
    if (p->pos < p->len && (p->json[p->pos] == 'e' || p->json[p->pos] == 'E')) {
        p->pos++;
        if (p->pos < p->len && (p->json[p->pos] == '-' || p->json[p->pos] == '+')) p->pos++;
        while (p->pos < p->len && isdigit((unsigned char)p->json[p->pos])) p->pos++;
    }
    char* num_str = (char*)malloc(p->pos - start + 1);
    memcpy(num_str, p->json + start, p->pos - start);
    num_str[p->pos - start] = '\0';
    double val = atof(num_str);
    free(num_str);

    json_value_t* node = (json_value_t*)malloc(sizeof(json_value_t));
    node->type = JSON_NUMBER;
    node->u.n = val;
    return node;
}

static inline json_value_t* json_parse_array(json_parser_t* p) {
    if (!json_expect(p, '[')) return NULL;
    json_value_t* node = (json_value_t*)malloc(sizeof(json_value_t));
    node->type = JSON_ARRAY;
    node->u.a.count = 0;
    node->u.a.items = NULL;

    if (json_peek(p) == ']') {
        json_advance(p);
        return node;
    }

    size_t cap = 4;
    node->u.a.items = (json_value_t**)malloc(sizeof(json_value_t*) * cap);
    while (1) {
        json_value_t* item = json_parse_value(p);
        if (!item) { json_free(node); return NULL; }
        if (node->u.a.count >= cap) {
            cap *= 2;
            node->u.a.items = (json_value_t**)realloc(node->u.a.items, sizeof(json_value_t*) * cap);
        }
        node->u.a.items[node->u.a.count++] = item;
        if (json_peek(p) == ',') { json_advance(p); continue; }
        break;
    }
    if (!json_expect(p, ']')) { json_free(node); return NULL; }
    return node;
}

static inline json_value_t* json_parse_object(json_parser_t* p) {
    if (!json_expect(p, '{')) return NULL;
    json_value_t* node = (json_value_t*)malloc(sizeof(json_value_t));
    node->type = JSON_OBJECT;
    node->u.o.count = 0;
    node->u.o.keys = NULL;
    node->u.o.values = NULL;

    if (json_peek(p) == '}') {
        json_advance(p);
        return node;
    }

    size_t cap = 4;
    node->u.o.keys = (char**)malloc(sizeof(char*) * cap);
    node->u.o.values = (json_value_t**)malloc(sizeof(json_value_t*) * cap);
    while (1) {
        char* key = json_parse_string(p);
        if (!key) { json_free(node); return NULL; }
        if (!json_expect(p, ':')) { free(key); json_free(node); return NULL; }
        json_value_t* value = json_parse_value(p);
        if (!value) { free(key); json_free(node); return NULL; }
        if (node->u.o.count >= cap) {
            cap *= 2;
            node->u.o.keys = (char**)realloc(node->u.o.keys, sizeof(char*) * cap);
            node->u.o.values = (json_value_t**)realloc(node->u.o.values, sizeof(json_value_t*) * cap);
        }
        node->u.o.keys[node->u.o.count] = key;
        node->u.o.values[node->u.o.count] = value;
        node->u.o.count++;
        if (json_peek(p) == ',') { json_advance(p); continue; }
        break;
    }
    if (!json_expect(p, '}')) { json_free(node); return NULL; }
    return node;
}

static inline json_value_t* json_parse_value(json_parser_t* p) {
    char c = json_peek(p);
    if (c == '"') {
        char* s = json_parse_string(p);
        if (!s) return NULL;
        json_value_t* node = (json_value_t*)malloc(sizeof(json_value_t));
        node->type = JSON_STRING;
        node->u.s = s;
        return node;
    }
    if (c == '{') return json_parse_object(p);
    if (c == '[') return json_parse_array(p);
    if (c == 't' || c == 'f') {
        bool val = false;
        if (p->pos + 4 <= p->len && strncmp(p->json + p->pos, "true", 4) == 0) { val = true; p->pos += 4; }
        else if (p->pos + 5 <= p->len && strncmp(p->json + p->pos, "false", 5) == 0) { val = false; p->pos += 5; }
        else return NULL;
        json_value_t* node = (json_value_t*)malloc(sizeof(json_value_t));
        node->type = JSON_BOOL;
        node->u.b = val;
        return node;
    }
    if (c == 'n') {
        if (p->pos + 4 <= p->len && strncmp(p->json + p->pos, "null", 4) == 0) { p->pos += 4; }
        else return NULL;
        json_value_t* node = (json_value_t*)malloc(sizeof(json_value_t));
        node->type = JSON_NULL;
        return node;
    }
    return json_parse_number(p);
}

// --- Public API ---

static inline void* json_parse(CnextString text) {
    if (!text.data) return NULL;
    json_parser_t p = {text.data, 0, text.length};
    json_skip_ws(&p);
    return json_parse_value(&p);
}

static inline json_value_t* json_find_key(void* root, const char* key) {
    json_value_t* r = (json_value_t*)root;
    if (!r || r->type != JSON_OBJECT) return NULL;
    for (size_t i = 0; i < r->u.o.count; i++) {
        if (strcmp(r->u.o.keys[i], key) == 0) return r->u.o.values[i];
    }
    return NULL;
}

static inline CnextString json_get_string(void* root, CnextString key) {
    json_value_t* v = json_find_key(root, key.data);
    if (v && v->type == JSON_STRING) return (CnextString){v->u.s, strlen(v->u.s)};
    return (CnextString){NULL, 0};
}

static inline int json_get_int(void* root, CnextString key) {
    json_value_t* v = json_find_key(root, key.data);
    if (v && v->type == JSON_NUMBER) return (int)v->u.n;
    return 0;
}

static inline float json_get_float(void* root, CnextString key) {
    json_value_t* v = json_find_key(root, key.data);
    if (v && v->type == JSON_NUMBER) return (float)v->u.n;
    return 0.0f;
}

static inline bool json_get_bool(void* root, CnextString key) {
    json_value_t* v = json_find_key(root, key.data);
    if (v && v->type == JSON_BOOL) return v->u.b != 0;
    return false;
}

static inline void* json_get_array(void* root, CnextString key) {
    json_value_t* v = json_find_key(root, key.data);
    if (v && v->type == JSON_ARRAY) return v;
    return NULL;
}

static inline size_t json_array_length(void* arr) {
    json_value_t* v = (json_value_t*)arr;
    if (!v || v->type != JSON_ARRAY) return 0;
    return v->u.a.count;
}

static inline void* json_array_get(void* arr, size_t index) {
    json_value_t* v = (json_value_t*)arr;
    if (!v || v->type != JSON_ARRAY || index >= v->u.a.count) return NULL;
    return v->u.a.items[index];
}

// --- Stringify ---

static inline void json_stringify_value(sb_t* sb, json_value_t* v);

static inline void json_stringify_string(sb_t* sb, const char* s) {
    sb_append_char(sb, '"');
    for (const char* p = s; *p; p++) {
        switch (*p) {
            case '"': sb_append(sb, "\\\""); break;
            case '\\': sb_append(sb, "\\\\"); break;
            case '\b': sb_append(sb, "\\b"); break;
            case '\f': sb_append(sb, "\\f"); break;
            case '\n': sb_append(sb, "\\n"); break;
            case '\r': sb_append(sb, "\\r"); break;
            case '\t': sb_append(sb, "\\t"); break;
            default: sb_append_char(sb, *p); break;
        }
    }
    sb_append_char(sb, '"');
}

static inline void json_stringify_value(sb_t* sb, json_value_t* v) {
    if (!v) { sb_append(sb, "null"); return; }
    switch (v->type) {
        case JSON_NULL: sb_append(sb, "null"); break;
        case JSON_BOOL: sb_append(sb, v->u.b ? "true" : "false"); break;
        case JSON_NUMBER: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", v->u.n);
            sb_append(sb, buf);
            break;
        }
        case JSON_STRING: json_stringify_string(sb, v->u.s); break;
        case JSON_ARRAY: {
            sb_append_char(sb, '[');
            for (size_t i = 0; i < v->u.a.count; i++) {
                if (i > 0) sb_append_char(sb, ',');
                json_stringify_value(sb, v->u.a.items[i]);
            }
            sb_append_char(sb, ']');
            break;
        }
        case JSON_OBJECT: {
            sb_append_char(sb, '{');
            for (size_t i = 0; i < v->u.o.count; i++) {
                if (i > 0) sb_append_char(sb, ',');
                json_stringify_string(sb, v->u.o.keys[i]);
                sb_append_char(sb, ':');
                json_stringify_value(sb, v->u.o.values[i]);
            }
            sb_append_char(sb, '}');
            break;
        }
    }
}

static inline CnextString json_stringify(void* root) {
    if (!root) return (CnextString){NULL, 0};
    sb_t sb;
    sb_init(&sb);
    json_stringify_value(&sb, (json_value_t*)root);
    return sb_to_string(&sb);
}

// --- Free ---

static inline void json_free(void* root) {
    json_value_t* v = (json_value_t*)root;
    if (!v) return;
    switch (v->type) {
        case JSON_STRING:
            free(v->u.s);
            break;
        case JSON_ARRAY: {
            for (size_t i = 0; i < v->u.a.count; i++) json_free(v->u.a.items[i]);
            free(v->u.a.items);
            break;
        }
        case JSON_OBJECT: {
            for (size_t i = 0; i < v->u.o.count; i++) {
                free(v->u.o.keys[i]);
                json_free(v->u.o.values[i]);
            }
            free(v->u.o.keys);
            free(v->u.o.values);
            break;
        }
        default: break;
    }
    free(v);
}

#endif
