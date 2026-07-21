#include "parser_internal.h"
#include "diagnostics.h"

const char* parser_source = NULL;
Parser parser;

void print_source_line(int line) {
    if (!parser_source) return;
    print_source_context(parser_source, line);
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
    print_caret_at(parser_source, token->line, col, token->length);
}

void error_at(Token* token, const char* message) {
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    parser.had_error = true;

    ErrorCode code = ERR_UNEXPECTED_TOKEN;
    if (strstr(message, "Expect expression")) code = ERR_EXPECT_EXPR;
    else if (strstr(message, "Expect '")) code = ERR_EXPECT_TOKEN;
    else if (strstr(message, "Undeclared")) code = ERR_UNDECLARED;
    else if (strstr(message, "Type mismatch")) code = ERR_TYPE_MISMATCH;
    else if (strstr(message, "missing brace")) code = ERR_MISSING_BRACE;
    else if (strstr(message, "missing paren")) code = ERR_MISSING_PAREN;
    else if (strstr(message, "';'")) code = ERR_MISSING_SEMICOLON;
    else if (strstr(message, "already")) code = ERR_DUPLICATE_DECL;
    else if (strstr(message, "Cannot assign")) code = ERR_INVALID_ASSIGNMENT;
    else if (strstr(message, "Too many")) code = ERR_TOO_MANY_ARGS;
    else if (strstr(message, "Too few")) code = ERR_TOO_FEW_ARGS;
    else if (strstr(message, "return")) code = ERR_RETURN_TYPE;

    const char* hint = get_hint_for_error(code);

    // Build context message with token info
    char full_msg[512];
    if (token->type == TOKEN_EOF) {
        snprintf(full_msg, sizeof(full_msg), "%s (at end of file)", message);
    } else if (token->type == TOKEN_ERROR) {
        snprintf(full_msg, sizeof(full_msg), "%s", message);
    } else {
        snprintf(full_msg, sizeof(full_msg), "%s at '%.*s'", message, token->length, token->start);
    }

    diag_emit(DIAG_ERROR, code, token->line, 0, full_msg, hint, NULL);
    print_source_line(token->line);
    if (token->type != TOKEN_EOF && token->type != TOKEN_ERROR) {
        print_caret(token);
    }
}

void error_at_current(const char* message) {
    error_at(&parser.current, message);
}

void error(const char* message) {
    error_at(&parser.previous, message);
}

void advance_token(void) {
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

void optionally_consume_semicolon(void) {
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

char* parse_attribute_name(void) {
    if (!match_token(TOKEN_AT)) return NULL;
    consume(TOKEN_IDENTIFIER, "Expect attribute name after '@'.");
    char* attr = (char*)checked_malloc(parser.previous.length + 1);
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

void synchronize(void) {
    parser.panic_mode = false;
    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        // Also sync after closing brace (end of block)
        if (parser.previous.type == TOKEN_RBRACE) return;

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
            case TOKEN_IMPORT:
            case TOKEN_CONST:
            case TOKEN_VAR:
            case TOKEN_TYPE_ALIAS:
            case TOKEN_STRUCT:
            case TOKEN_ENUM:
            case TOKEN_INTERFACE:
            case TOKEN_TRY:
            case TOKEN_MATCH:
            case TOKEN_COROUTINE:
            case TOKEN_ASYNC:
            case TOKEN_MACRO:
            case TOKEN_CONSTEXPR:
            case TOKEN_EXTERN:
            case TOKEN_DEFER:
                return;
            default:
                ;
        }
        advance_token();
    }
}
