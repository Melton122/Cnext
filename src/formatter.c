#include "formatter.h"
#include "lexer.h"
#include "checked_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Formatter configuration
typedef struct {
    int indent_size;      // Number of spaces per indent (default: 4)
    bool use_tabs;        // Use tabs instead of spaces
    int max_line_length;  // Max line length before wrapping (default: 80)
    bool trailing_commas; // Add trailing commas in multi-line expressions
} FormatterConfig;

static FormatterConfig default_config = {
    .indent_size = 4,
    .use_tabs = false,
    .max_line_length = 80,
    .trailing_commas = true
};

typedef struct {
    char* data;
    int length;
    int capacity;
} StringBuilder;

static void sb_init(StringBuilder* sb) {
    sb->capacity = 4096;
    sb->data = (char*)checked_malloc(sb->capacity);
    sb->data[0] = '\0';
    sb->length = 0;
}

static void sb_append(StringBuilder* sb, const char* text, int len) {
    if (sb->length + len + 1 > sb->capacity) {
        sb->capacity = (sb->capacity + len) * 2;
        sb->data = (char*)checked_realloc(sb->data, sb->capacity);
    }
    memcpy(sb->data + sb->length, text, len);
    sb->length += len;
    sb->data[sb->length] = '\0';
}

static void sb_append_str(StringBuilder* sb, const char* str) {
    sb_append(sb, str, (int)strlen(str));
}

static void sb_append_char(StringBuilder* sb, char c) {
    sb_append(sb, &c, 1);
}

static bool is_decl_keyword(CnextTokenType type) {
    return type == TOKEN_FUNC || type == TOKEN_VAR || type == TOKEN_CONST ||
           type == TOKEN_STRUCT || type == TOKEN_CLASS || type == TOKEN_ENUM ||
           type == TOKEN_INTERFACE || type == TOKEN_TRAIT || type == TOKEN_MACRO ||
           type == TOKEN_CONSTEXPR || type == TOKEN_EXTERN;
}

bool format_source(const char* source, char** output) {
    init_lexer(source);
    StringBuilder sb;
    sb_init(&sb);

    int indent = 0;
    bool need_indent = true;
    Token prev_token = {TOKEN_EOF, "", 0, 0};

    for (;;) {
        Token token = next_token();
        if (token.type == TOKEN_EOF) break;
        if (token.type == TOKEN_ERROR) {
            // Pass through errors as-is
            sb_append(&sb, token.start, token.length);
            continue;
        }

        // Handle newlines
        if (token.line > prev_token.line) {
            int newlines = token.line - prev_token.line;
            if (newlines > 2) newlines = 2; // Max 2 blank lines
            for (int i = 0; i < newlines; i++) {
                sb_append_char(&sb, '\n');
            }
            need_indent = true;
        }

        // Emit indentation
        if (need_indent) {
            for (int i = 0; i < indent; i++) {
                sb_append_str(&sb, "    "); // 4-space indent
            }
            need_indent = false;
        }

        // Add space before token if needed
        if (prev_token.type != TOKEN_EOF && token.type != TOKEN_EOF) {
            bool add_space = true;

            // No space before these
            if (token.type == TOKEN_SEMICOLON || token.type == TOKEN_COMMA ||
                token.type == TOKEN_RPAREN || token.type == TOKEN_RBRACKET ||
                token.type == TOKEN_RBRACE || token.type == TOKEN_COLON ||
                token.type == TOKEN_DOT) {
                add_space = false;
            }

            // No space after these
            if (prev_token.type == TOKEN_LPAREN || prev_token.type == TOKEN_LBRACKET ||
                prev_token.type == TOKEN_LBRACE || prev_token.type == TOKEN_DOT ||
                prev_token.type == TOKEN_COMMA || prev_token.type == TOKEN_SEMICOLON) {
                add_space = false;
            }

            // No space around :: operator or in generic params
            if (prev_token.type == TOKEN_COLON && token.type == TOKEN_COLON) {
                add_space = false;
            }

            // No space before/after in identifiers directly
            if ((prev_token.type == TOKEN_IDENTIFIER || is_decl_keyword(prev_token.type)) &&
                (token.type == TOKEN_IDENTIFIER)) {
                // Space between keywords and identifiers
                if (is_decl_keyword(prev_token.type)) add_space = true;
                else add_space = false;
            }

            if (add_space) sb_append_char(&sb, ' ');
        }

        // Emit token text
        sb_append(&sb, token.start, token.length);

        // Indentation changes
        if (token.type == TOKEN_LBRACE) {
            sb_append_char(&sb, '\n');
            indent++;
            need_indent = true;
        } else if (token.type == TOKEN_RBRACE) {
            if (indent > 0) indent--;
            sb_append_char(&sb, '\n');
            need_indent = true;
            // Re-indent this line
            for (int i = 0; i < indent; i++) {
                sb_append_str(&sb, "    ");
            }
            sb_append(&sb, token.start, token.length); // Re-emit }
        }

        // Semicolons and newlines
        if (token.type == TOKEN_SEMICOLON) {
            sb_append_char(&sb, '\n');
            need_indent = true;
        }

        prev_token = token;
    }

    // Ensure file ends with newline
    if (sb.length > 0 && sb.data[sb.length - 1] != '\n') {
        sb_append_char(&sb, '\n');
    }

    *output = sb.data;
    return true;
}

bool format_file(const char* input_path, const char* output_path, bool in_place) {
    FILE* f = fopen(input_path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open file \"%s\"\n", input_path);
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return false; }
    rewind(f);
    char* source = (char*)checked_malloc(size + 1);
    size_t read = fread(source, 1, size, f);
    source[read] = '\0';
    fclose(f);

    char* formatted = NULL;
    if (!format_source(source, &formatted)) {
        free(source);
        return false;
    }
    free(source);

    const char* out_path = in_place ? input_path : output_path;
    FILE* out = fopen(out_path, "wb");
    if (!out) {
        fprintf(stderr, "Could not write to \"%s\"\n", out_path);
        free(formatted);
        return false;
    }
    fwrite(formatted, 1, strlen(formatted), out);
    fclose(out);
    free(formatted);
    return true;
}
