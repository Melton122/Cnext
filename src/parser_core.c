#include "parser_internal.h"

const char* parser_source = NULL;
Parser parser;

void print_source_line(int line) {
    if (!parser_source) return;
    const char* p = parser_source;
    int current_line = 1;
    while (*p && current_line < line) {
        if (*p == '\n') current_line++;
        p++;
    }
    if (!*p) return;
    const char* line_start = p;
    while (*p && *p != '\n') p++;
    size_t line_len = (size_t)(p - line_start);
    if (line_len > 0) {
        fprintf(stderr, "  | %.*s\n", (int)line_len, line_start);
    }
}

void print_caret(Token* token) {
    if (!parser_source) return;
    int col = 1;
    const char* p = parser_source;
    int current_line = 1;
    while (*p && current_line < token->line) {
        if (*p == '\n') current_line++;
        p++;
    }
    while (*p && *p != '\n' && p < token->start) {
        col++;
        p++;
    }
    fprintf(stderr, "  | ");
    for (int i = 1; i < col; i++) fprintf(stderr, " ");
    if (token->length > 0) {
        fprintf(stderr, "^");
        for (int i = 1; i < token->length && i < 40; i++) fprintf(stderr, "~");
    } else {
        fprintf(stderr, "^");
    }
    fprintf(stderr, "\n");
}

void error_at(Token* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    fprintf(stderr, "\033[31m\033[1merror\033[0m");
    if (token->line > 0) {
        fprintf(stderr, " [\033[33m%d\033[0m]", token->line);
    }
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '\033[36m%.*s\033[0m'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    print_source_line(token->line);
    if (token->type != TOKEN_EOF) {
        print_caret(token);
    }
    if (strstr(message, "Expect '(' after")) {
        fprintf(stderr, "\033[32m  --> Did you forget parentheses?\033[0m\n");
    } else if (strstr(message, "Expect ')' after")) {
        fprintf(stderr, "\033[32m  --> Check for unmatched opening parenthesis.\033[0m\n");
    } else if (strstr(message, "Expect ';' after")) {
        fprintf(stderr, "\033[32m  --> Add a semicolon at the end of the statement.\033[0m\n");
    } else if (strstr(message, "Undeclared")) {
        fprintf(stderr, "\033[32m  --> Declare the variable first: var name = value\033[0m\n");
    }
    parser.had_error = true;
}

void error(const char* message) {
    error_at(&parser.previous, message);
}

void error_at_current(const char* message) {
    error_at(&parser.current, message);
}

void advance_token() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = next_token();
        if (parser.current.type != TOKEN_ERROR) break;
        error_at_current(parser.current.start);
    }
}

void consume(CnextTokenType type, const char* message) {
    if (parser.current.type == type) {
        advance_token();
        return;
    }
    error_at_current(message);
}

bool check(CnextTokenType type) {
    return parser.current.type == type;
}

bool match_token(CnextTokenType type) {
    if (!check(type)) return false;
    advance_token();
    return true;
}

void optionally_consume_semicolon() {
    match_token(TOKEN_SEMICOLON);
}

bool check_identifier_text(const char* text, int len) {
    return check(TOKEN_IDENTIFIER) && parser.current.length == len && 
           memcmp(parser.current.start, text, len) == 0;
}

bool match_identifier_text(const char* text, int len) {
    if (!check_identifier_text(text, len)) return false;
    advance_token();
    return true;
}

char* parse_attribute_name() {
    if (!match_token(TOKEN_AT)) return NULL;
    consume(TOKEN_IDENTIFIER, "Expect attribute name after '@'.");
    char* attr = (char*)malloc(parser.previous.length + 1);
    if (!attr) return NULL;
    memcpy(attr, parser.previous.start, parser.previous.length);
    attr[parser.previous.length] = '\0';
    if (match_token(TOKEN_LPAREN)) {
        int depth = 1;
        while (depth > 0 && !check(TOKEN_EOF)) {
            if (check(TOKEN_LPAREN)) depth++;
            else if (check(TOKEN_RPAREN)) depth--;
            if (depth > 0) advance_token();
        }
        consume(TOKEN_RPAREN, "Expect ')' after attribute arguments.");
    }
    return attr;
}

void synchronize() {
    parser.panic_mode = false;
    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_TRAIT:
            case TOKEN_FUNC:
            case TOKEN_EXTEND:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_RETURN:
            case TOKEN_MAIN:
            case TOKEN_TEST:
            case TOKEN_RBRACE:
            case TOKEN_CASE:
            case TOKEN_DEFAULT:
                return;
            default:
                ;
        }
        advance_token();
    }
}
