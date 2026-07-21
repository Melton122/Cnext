#include "docgen.h"
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* data;
    int length;
    int capacity;
} DocBuilder;

static void db_init(DocBuilder* db) {
    db->capacity = 4096;
    db->data = (char*)checked_malloc(db->capacity);
    if (!db->data) { db->capacity = 0; return; }
    db->data[0] = '\0';
    db->length = 0;
}

static void db_append(DocBuilder* db, const char* text) {
    int len = (int)strlen(text);
    if (db->length + len + 1 > db->capacity) {
        int new_cap = (db->capacity + len) * 2;
        char* newdata = (char*)checked_realloc(db->data, new_cap);
        if (!newdata) return;
        db->data = newdata;
        db->capacity = new_cap;
    }
    memcpy(db->data + db->length, text, len);
    db->length += len;
    db->data[db->length] = '\0';
}

typedef struct {
    char name[128];
    char* doc_comment;
    int line;
    char kind; // 'f' = func, 's' = struct, 'v' = var, 'c' = const
} DocEntry;

typedef struct {
    DocEntry* entries;
    int count;
    int capacity;
} DocEntries;

static void entries_init(DocEntries* e) {
    e->entries = NULL;
    e->count = 0;
    e->capacity = 0;
}

static void entries_add(DocEntries* e, const char* name, const char* doc, int line, char kind) {
    if (e->count >= e->capacity) {
        e->capacity = e->capacity ? e->capacity * 2 : 16;
        DocEntry* ne = checked_realloc(e->entries, sizeof(DocEntry) * e->capacity);
        if (!ne) return;
        e->entries = ne;
    }
    DocEntry* entry = &e->entries[e->count++];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->doc_comment = doc ? checked_strdup(doc) : NULL;
    entry->line = line;
    entry->kind = kind;
}

static void entries_free(DocEntries* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->entries[i].doc_comment);
    }
    free(e->entries);
}

bool generate_docs_from_source(const char* source, const char* name, char** output) {
    init_lexer(source);

    DocBuilder db;
    db_init(&db);

    DocEntries entries;
    entries_init(&entries);

    char* pending_doc = NULL;

    for (;;) {
        Token token = next_token();
        if (token.type == TOKEN_EOF) break;
        if (token.type == TOKEN_ERROR) continue;

        // Simple doc comment tracking: we just find declarations
        if (token.type == TOKEN_FUNC) {
            Token name_tok = next_token();
            if (name_tok.type == TOKEN_IDENTIFIER) {
                entries_add(&entries, name_tok.start, pending_doc, token.line, 'f');
                free(pending_doc);
                pending_doc = NULL;
            }
        } else if (token.type == TOKEN_STRUCT) {
            Token name_tok = next_token();
            if (name_tok.type == TOKEN_IDENTIFIER) {
                entries_add(&entries, name_tok.start, pending_doc, token.line, 's');
                free(pending_doc);
                pending_doc = NULL;
            }
        } else if (token.type == TOKEN_VAR) {
            Token name_tok = next_token();
            if (name_tok.type == TOKEN_IDENTIFIER) {
                entries_add(&entries, name_tok.start, pending_doc, token.line, 'v');
                free(pending_doc);
                pending_doc = NULL;
            }
        } else if (token.type == TOKEN_CONST) {
            Token name_tok = next_token();
            if (name_tok.type == TOKEN_IDENTIFIER) {
                entries_add(&entries, name_tok.start, pending_doc, token.line, 'c');
                free(pending_doc);
                pending_doc = NULL;
            }
        }

    }

    // Generate markdown
    char header[512];
    snprintf(header, sizeof(header), "# %s - API Reference\n\n", name);
    db_append(&db, header);

    if (entries.count == 0) {
        db_append(&db, "_No public symbols found._\n");
    } else {
        // Functions
        bool has_funcs = false;
        for (int i = 0; i < entries.count; i++) {
            if (entries.entries[i].kind == 'f') {
                if (!has_funcs) {
                    db_append(&db, "## Functions\n\n");
                    has_funcs = true;
                }
                char buf[512];
                snprintf(buf, sizeof(buf), "### `%s` (line %d)\n\n",
                    entries.entries[i].name, entries.entries[i].line);
                db_append(&db, buf);
                if (entries.entries[i].doc_comment) {
                    db_append(&db, entries.entries[i].doc_comment);
                    db_append(&db, "\n\n");
                } else {
                    db_append(&db, "_No description available._\n\n");
                }
            }
        }

        // Structs
        bool has_structs = false;
        for (int i = 0; i < entries.count; i++) {
            if (entries.entries[i].kind == 's') {
                if (!has_structs) {
                    db_append(&db, "## Structs\n\n");
                    has_structs = true;
                }
                char buf[512];
                snprintf(buf, sizeof(buf), "### `%s` (line %d)\n\n",
                    entries.entries[i].name, entries.entries[i].line);
                db_append(&db, buf);
                if (entries.entries[i].doc_comment) {
                    db_append(&db, entries.entries[i].doc_comment);
                    db_append(&db, "\n\n");
                } else {
                    db_append(&db, "_No description available._\n\n");
                }
            }
        }

        // Variables & Constants
        bool has_vars = false;
        for (int i = 0; i < entries.count; i++) {
            if (entries.entries[i].kind == 'v' || entries.entries[i].kind == 'c') {
                if (!has_vars) {
                    db_append(&db, "## Variables & Constants\n\n");
                    has_vars = true;
                }
                const char* kind_str = entries.entries[i].kind == 'v' ? "Variable" : "Constant";
                char buf[512];
                snprintf(buf, sizeof(buf), "### `%s` (%s, line %d)\n\n",
                    entries.entries[i].name, kind_str, entries.entries[i].line);
                db_append(&db, buf);
                if (entries.entries[i].doc_comment) {
                    db_append(&db, entries.entries[i].doc_comment);
                    db_append(&db, "\n\n");
                } else {
                    db_append(&db, "_No description available._\n\n");
                }
            }
        }
    }

    entries_free(&entries);
    *output = db.data;
    return true;
}

bool generate_docs(const char* file_path, const char* output_dir) {
    FILE* f = fopen(file_path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open file \"%s\"\n", file_path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return false; }
    rewind(f);
    char* source = (char*)checked_malloc(size + 1);
    if (!source) { fclose(f); return false; }
    size_t bytes_read = fread(source, 1, size, f);
    source[bytes_read] = '\0';
    fclose(f);

    // Derive module name from filename
    const char* name = strrchr(file_path, '/');
    if (!name) name = strrchr(file_path, '\\');
    if (name) name++; else name = file_path;
    char name_buf[256];
    strncpy(name_buf, name, sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    char* dot = strrchr(name_buf, '.');
    if (dot) *dot = '\0';

    char* md = NULL;
    if (!generate_docs_from_source(source, name_buf, &md)) {
        free(source);
        return false;
    }
    free(source);

    // Write output
    char out_path[512];
    if (output_dir) {
        snprintf(out_path, sizeof(out_path), "%s/%s.md", output_dir, name_buf);
    } else {
        snprintf(out_path, sizeof(out_path), "%s.md", name_buf);
    }

    FILE* out = fopen(out_path, "wb");
    if (!out) {
        fprintf(stderr, "Could not write to \"%s\"\n", out_path);
        free(md);
        return false;
    }
    fwrite(md, 1, strlen(md), out);
    fclose(out);
    free(md);

    fprintf(stderr, "Generated %s\n", out_path);
    return true;
}
