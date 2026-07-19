#ifndef CNEXT_CSV_H
#define CNEXT_CSV_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    char** fields;
    int field_count;
    int field_capacity;
} CsvRow;

typedef struct {
    CsvRow* rows;
    int row_count;
    int row_capacity;
    char delimiter;
    bool has_header;
} CsvDocument;

static inline CsvDocument csv_new(char delimiter, bool has_header) {
    CsvDocument doc;
    doc.rows = NULL;
    doc.row_count = 0;
    doc.row_capacity = 0;
    doc.delimiter = delimiter > 0 ? delimiter : ',';
    doc.has_header = has_header;
    return doc;
}

static inline void csv_free_row(CsvRow* row) {
    if (!row) return;
    for (int i = 0; i < row->field_count; i++) {
        free(row->fields[i]);
    }
    free(row->fields);
    row->fields = NULL;
    row->field_count = 0;
}

static inline void csv_free(CsvDocument* doc) {
    if (!doc) return;
    for (int i = 0; i < doc->row_count; i++) {
        csv_free_row(&doc->rows[i]);
    }
    free(doc->rows);
    doc->rows = NULL;
    doc->row_count = 0;
}

static inline void _csv_add_row(CsvDocument* doc) {
    if (doc->row_count >= doc->row_capacity) {
        int new_cap = doc->row_capacity == 0 ? 16 : doc->row_capacity * 2;
        CsvRow* nr = (CsvRow*)realloc(doc->rows, new_cap * sizeof(CsvRow));
        if (!nr) return;
        doc->rows = nr;
        doc->row_capacity = new_cap;
    }
    CsvRow* row = &doc->rows[doc->row_count];
    row->fields = NULL;
    row->field_count = 0;
    row->field_capacity = 0;
    doc->row_count++;
}

static inline void _csv_add_field(CsvRow* row, const char* start, size_t len) {
    if (row->field_count >= row->field_capacity) {
        int new_cap = row->field_capacity == 0 ? 8 : row->field_capacity * 2;
        char** nf = (char**)realloc(row->fields, new_cap * sizeof(char*));
        if (!nf) return;
        row->fields = nf;
        row->field_capacity = new_cap;
    }
    char* field = (char*)malloc(len + 1);
    if (!field) return;
    memcpy(field, start, len);
    field[len] = '\0';
    row->fields[row->field_count++] = field;
}

static inline CsvDocument csv_parse(CnextString text) {
    CsvDocument doc = csv_new(0, false);
    if (!text.data || text.length == 0) return doc;

    _csv_add_row(&doc);
    const char* p = text.data;
    const char* end = text.data + text.length;
    char delim = ',';

    while (p < end) {
        CsvRow* current = &doc.rows[doc.row_count - 1];

        if (*p == '"') {
            p++;
            const char* start = p;
            size_t field_len = 0;
            while (p < end) {
                if (*p == '"') {
                    if (p + 1 < end && *(p + 1) == '"') {
                        field_len += 2;
                        p += 2;
                    } else {
                        break;
                    }
                } else {
                    field_len++;
                    p++;
                }
            }
            _csv_add_field(current, start, field_len);
            if (p < end) p++;
            if (p < end && *p == delim) { p++; continue; }
            if (p < end && (*p == '\n' || *p == '\r')) {
                if (*p == '\r' && p + 1 < end && *(p + 1) == '\n') p++;
                p++;
                if (p < end) _csv_add_row(&doc);
                continue;
            }
            if (p >= end) break;
        } else {
            const char* start = p;
            while (p < end && *p != delim && *p != '\n' && *p != '\r') p++;
            _csv_add_field(current, start, (size_t)(p - start));
            if (p < end && *p == delim) { p++; continue; }
            if (p < end && (*p == '\n' || *p == '\r')) {
                if (*p == '\r' && p + 1 < end && *(p + 1) == '\n') p++;
                p++;
                if (p < end) _csv_add_row(&doc);
                continue;
            }
            if (p >= end) break;
        }
    }

    return doc;
}

static inline int csv_column_index(CsvDocument* doc, CnextString header) {
    if (!doc->has_header || doc->row_count == 0) return -1;
    CsvRow* hdr = &doc->rows[0];
    for (int i = 0; i < hdr->field_count; i++) {
        if (strlen(hdr->fields[i]) == header.length &&
            memcmp(hdr->fields[i], header.data, header.length) == 0) {
            return i;
        }
    }
    return -1;
}

static inline CnextString csv_get(CsvDocument* doc, int row, int col) {
    if (row < 0 || row >= doc->row_count) return (CnextString){NULL, 0};
    CsvRow* r = &doc->rows[row];
    if (col < 0 || col >= r->field_count) return (CnextString){NULL, 0};
    char* s = r->fields[col];
    return (CnextString){s, strlen(s)};
}

static inline int csv_get_int(CsvDocument* doc, int row, int col) {
    CnextString s = csv_get(doc, row, col);
    if (!s.data) return 0;
    return atoi(s.data);
}

static inline CnextString csv_to_string(CsvDocument* doc) {
    size_t cap = 1024;
    char* buf = (char*)malloc(cap);
    if (!buf) return (CnextString){NULL, 0};
    buf[0] = '\0';
    size_t len = 0;

    for (int i = 0; i < doc->row_count; i++) {
        CsvRow* row = &doc->rows[i];
        for (int j = 0; j < row->field_count; j++) {
            if (j > 0) {
                if (len + 1 >= cap) { cap *= 2; char* nb = (char*)realloc(buf, cap); if (!nb) break; buf = nb; }
                buf[len++] = doc->delimiter;
            }
            char* field = row->fields[j];
            size_t flen = strlen(field);
            bool needs_quote = false;
            for (size_t k = 0; k < flen; k++) {
                if (field[k] == '"' || field[k] == doc->delimiter || field[k] == '\n') {
                    needs_quote = true;
                    break;
                }
            }
            if (needs_quote) {
                if (len + flen * 2 + 3 >= cap) { cap = (len + flen * 2 + 3) * 2; char* nb = (char*)realloc(buf, cap); if (!nb) break; buf = nb; }
                buf[len++] = '"';
                for (size_t k = 0; k < flen; k++) {
                    if (field[k] == '"') buf[len++] = '"';
                    buf[len++] = field[k];
                }
                buf[len++] = '"';
            } else {
                if (len + flen + 1 >= cap) { cap = (len + flen + 1) * 2; char* nb = (char*)realloc(buf, cap); if (!nb) break; buf = nb; }
                memcpy(buf + len, field, flen);
                len += flen;
            }
        }
        if (i < doc->row_count - 1) {
            if (len + 1 >= cap) { cap *= 2; char* nb = (char*)realloc(buf, cap); if (!nb) break; buf = nb; }
            buf[len++] = '\n';
        }
    }
    buf[len] = '\0';
    _cnext_track(buf);
    return (CnextString){buf, len};
}

static inline void csv_add_row(CsvDocument* doc, CnextString* fields, int count) {
    _csv_add_row(doc);
    CsvRow* row = &doc->rows[doc->row_count - 1];
    for (int i = 0; i < count; i++) {
        _csv_add_field(row, fields[i].data, fields[i].length);
    }
}

static inline int csv_row_count(CsvDocument* doc) {
    return doc->row_count;
}

static inline int csv_col_count(CsvDocument* doc) {
    if (doc->row_count == 0) return 0;
    return doc->rows[0].field_count;
}

#endif
