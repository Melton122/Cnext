#include "lexer.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

typedef struct {
    const char* start;
    const char* current;
    int line;
    bool unterminated_comment;
} Lexer;

static Lexer lexer;

void init_lexer(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
    lexer.unterminated_comment = false;
}

static bool is_at_end(void) {
    return *lexer.current == '\0';
}

static char advance(void) {
    lexer.current++;
    return lexer.current[-1];
}

static char peek(void) {
    return *lexer.current;
}

static char peek_next(void) {
    if (is_at_end()) return '\0';
    return lexer.current[1];
}

static bool match(char expected) {
    if (is_at_end()) return false;
    if (*lexer.current != expected) return false;
    lexer.current++;
    return true;
}

static Token make_token(CnextTokenType type) {
    Token token;
    token.type = type;
    token.start = lexer.start;
    token.length = (int)(lexer.current - lexer.start);
    token.line = lexer.line;
    return token;
}

static Token error_token(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
    return token;
}

static void skip_whitespace(void) {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                lexer.line++;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) advance();
                } else if (peek_next() == '*') {
                    advance(); advance();
                    while (!is_at_end() && !(peek() == '*' && peek_next() == '/')) {
                        if (peek() == '\n') lexer.line++;
                        advance();
                    }
                    if (!is_at_end()) {
                        advance();
                        advance();
                    } else {
                        lexer.unterminated_comment = true;
                        return;
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static CnextTokenType check_keyword(int start, int length, const char* rest, CnextTokenType type) {
    if (lexer.current - lexer.start == start + length &&
        memcmp(lexer.start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static CnextTokenType identifier_type(void) {
    switch (lexer.start[0]) {
        case 'a': {
            CnextTokenType t = check_keyword(1, 5, "ssert", TOKEN_ASSERT);
            if (t != TOKEN_IDENTIFIER) return t;
            t = check_keyword(1, 4, "sync", TOKEN_ASYNC);
            if (t != TOKEN_IDENTIFIER) return t;
            t = check_keyword(1, 4, "wait", TOKEN_AWAIT);
            if (t != TOKEN_IDENTIFIER) return t;
            return check_keyword(1, 1, "s", TOKEN_AS);
        }
        case 'b':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'r': return check_keyword(2, 3, "eak", TOKEN_BREAK);
                    case 'o': return check_keyword(1, 3, "ool", TOKEN_BOOL_TYPE);
                    case 'e': return check_keyword(2, 3, "nch", TOKEN_BENCH);
                    case 'y': return check_keyword(2, 2, "te", TOKEN_BYTE_TYPE);
                }
            }
            return check_keyword(1, 3, "ool", TOKEN_BOOL_TYPE);
        case 'c': 
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'a': {
                        CnextTokenType t = check_keyword(2, 3, "tch", TOKEN_CATCH);
                        if (t != TOKEN_IDENTIFIER) return t;
                        return check_keyword(2, 2, "se", TOKEN_CASE);
                    }
                    case 'h': {
                        CnextTokenType t = check_keyword(2, 5, "annel", TOKEN_CHANNEL);
                        if (t != TOKEN_IDENTIFIER) return t;
                        return check_keyword(2, 2, "ar", TOKEN_CHAR_TYPE);
                    }
                    case 'l': return check_keyword(2, 3, "ass", TOKEN_CLASS);
                    case 'o': {
                        CnextTokenType t = check_keyword(2, 7, "routine", TOKEN_COROUTINE);
                        if (t != TOKEN_IDENTIFIER) return t;
                        t = check_keyword(3, 5, "tinue", TOKEN_CONTINUE);
                        if (t != TOKEN_IDENTIFIER) return t;
                        t = check_keyword(2, 7, "nstexpr", TOKEN_CONSTEXPR);
                        if (t != TOKEN_IDENTIFIER) return t;
                        return check_keyword(2, 3, "nst", TOKEN_CONST);
                    }
                }
            }
            break;
        case 'd': {
            CnextTokenType t = check_keyword(1, 6, "efault", TOKEN_DEFAULT);
            if (t != TOKEN_IDENTIFIER) return t;
            return check_keyword(1, 6, "ouble", TOKEN_DOUBLE_TYPE);
        }
        case 'e': 
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'l': return check_keyword(2, 2, "se", TOKEN_ELSE);
                    case 'n': return check_keyword(2, 2, "um", TOKEN_ENUM);
                    case 'x': {
                        CnextTokenType t = check_keyword(2, 5, "tends", TOKEN_EXTENDS);
                        if (t != TOKEN_IDENTIFIER) return t;
                        t = check_keyword(2, 4, "tend", TOKEN_EXTEND);
                        if (t != TOKEN_IDENTIFIER) return t;
                        return check_keyword(2, 3, "tern", TOKEN_EXTERN);
                    }
                    // 'err' handled as identifier in parser context
                }
            }
            break;
        case 'f':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'i': return check_keyword(2, 5, "nally", TOKEN_FINALLY);
                    case 'l': return check_keyword(2, 3, "oat", TOKEN_FLOAT_TYPE);
                    case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(2, 2, "nc", TOKEN_FUNC);
                }
            }
            break;
        case 'i':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'f': return check_keyword(2, 0, "", TOKEN_IF);
                    case 'n':
                        if (lexer.current - lexer.start == 2) return TOKEN_IN;
                        {
                            CnextTokenType t = check_keyword(2, 7, "terface", TOKEN_INTERFACE);
                            if (t != TOKEN_IDENTIFIER) return t;
                        }
                        return check_keyword(2, 1, "t", TOKEN_INT_TYPE);
                    case 'm': {
                        CnextTokenType t = check_keyword(2, 8, "plements", TOKEN_IMPLEMENTS);
                        if (t != TOKEN_IDENTIFIER) return t;
                        return check_keyword(2, 4, "port", TOKEN_IMPORT);
                    }
                    case 't': return check_keyword(2, 2, "er", TOKEN_ITER);
                }
            }
            break;
        case 'm':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'a': {
                        CnextTokenType t = check_keyword(2, 4, "cro", TOKEN_MACRO);
                        if (t != TOKEN_IDENTIFIER) return t;
                        t = check_keyword(2, 3, "tch", TOKEN_MATCH);
                        if (t != TOKEN_IDENTIFIER) return t;
                        return check_keyword(2, 2, "in", TOKEN_MAIN);
                    }
                    case 'u': return check_keyword(2, 3, "tex", TOKEN_MUTEX);
                }
            }
            break;
        case 'n':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'e': return check_keyword(2, 1, "w", TOKEN_NEW);
                    case 'u': return check_keyword(2, 2, "ll", TOKEN_NULL);
                }
            }
            break;
        case 'o':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'p': return check_keyword(2, 6, "erator", TOKEN_OPERATOR);
                    case 'v': return check_keyword(2, 6, "erride", TOKEN_OVERRIDE);
                    case 'w': return check_keyword(2, 1, "n", TOKEN_OWN);
                }
            }
            return check_keyword(1, 8, "verride", TOKEN_OVERRIDE);
        case 'r':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'e': {
                        CnextTokenType t = check_keyword(2, 4, "turn", TOKEN_RETURN);
                        if (t != TOKEN_IDENTIFIER) return t;
                        t = check_keyword(2, 4, "sume", TOKEN_RESUME);
                        if (t != TOKEN_IDENTIFIER) return t;
                        return check_keyword(2, 2, "cv", TOKEN_RECV);
                    }
                    case 'u': {
                        // run_async
                        if (lexer.current - lexer.start > 3 && lexer.start[2] == 'n') {
                            return check_keyword(3, 6, "_async", TOKEN_RUN_ASYNC);
                        }
                        break;
                    }
                }
            }
            break;
        case 's': 
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 't':
                        if (lexer.current - lexer.start == 3) return check_keyword(2, 1, "r", TOKEN_STR_TYPE);
                        return check_keyword(2, 4, "ruct", TOKEN_STRUCT);
                    case 'w': return check_keyword(2, 4, "itch", TOKEN_SWITCH);
                    case 'u': return check_keyword(2, 3, "per", TOKEN_SUPER);
                    case 'p': return check_keyword(2, 4, "awn", TOKEN_SPAWN);
                    case 'e': return check_keyword(2, 3, "nd", TOKEN_SEND);
                }
            }
            break;
        case 't':
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'r': {
                        CnextTokenType t = check_keyword(2, 3, "ait", TOKEN_TRAIT);
                        if (t != TOKEN_IDENTIFIER) return t;
                        t = check_keyword(2, 1, "y", TOKEN_TRY);
                        if (t != TOKEN_IDENTIFIER) return t;
                        return check_keyword(2, 2, "ue", TOKEN_TRUE);
                    }
                    case 'h': {
                        CnextTokenType t = check_keyword(2, 3, "row", TOKEN_THROW);
                        if (t != TOKEN_IDENTIFIER) return t;
                        return check_keyword(2, 4, "read", TOKEN_THREAD);
                    }
                    case 'y': {
                        CnextTokenType t = check_keyword(2, 4, "peof", TOKEN_TYPEOF);
                        if (t != TOKEN_IDENTIFIER) return t;
                        return check_keyword(2, 1, "ype", TOKEN_TYPE_ALIAS);
                    }
                    case 'e': return check_keyword(2, 2, "st", TOKEN_TEST);
                }
            }
            break;
        case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w': {
            CnextTokenType t = check_keyword(1, 4, "hile", TOKEN_WHILE);
            if (t != TOKEN_IDENTIFIER) return t;
            return check_keyword(1, 3, "ith", TOKEN_WITH);
        }
        case 'y': return check_keyword(1, 4, "ield", TOKEN_YIELD);
        case 'l': {
            CnextTokenType t = check_keyword(1, 3, "ock", TOKEN_LOCK);
            if (t != TOKEN_IDENTIFIER) return t;
            return check_keyword(1, 3, "ong", TOKEN_LONG_TYPE);
        }
        case 'u': {
            if (lexer.current - lexer.start > 1) {
                switch (lexer.start[1]) {
                    case 'n': {
                        CnextTokenType t = check_keyword(2, 4, "lock", TOKEN_UNLOCK);
                        if (t != TOKEN_IDENTIFIER) return t;
                        break;
                    }
                    case 'i': {
                        CnextTokenType t = check_keyword(2, 2, "nt", TOKEN_UINT_TYPE);
                        if (t != TOKEN_IDENTIFIER) return t;
                        t = check_keyword(2, 3, "ong", TOKEN_ULONG_TYPE);
                        if (t != TOKEN_IDENTIFIER) return t;
                        break;
                    }
                    case 's': {
                        CnextTokenType t = check_keyword(2, 4, "hort", TOKEN_USHORT_TYPE);
                        if (t != TOKEN_IDENTIFIER) return t;
                        break;
                    }
                    case 'b': {
                        CnextTokenType t = check_keyword(2, 3, "yte", TOKEN_UBYTE_TYPE);
                        if (t != TOKEN_IDENTIFIER) return t;
                        break;
                    }
                }
            }
            break;
        }
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(void) {
    while (isalpha(peek()) || isdigit(peek()) || peek() == '_') advance();
    return make_token(identifier_type());
}

static Token number(void) {
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(peek_next())) {
        advance();
        while (isdigit(peek())) advance();
        return make_token(TOKEN_FLOAT_LITERAL);
    }
    return make_token(TOKEN_NUMBER);
}

static Token string(void) {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\\') {
            advance();
            if (is_at_end()) break;
            advance();
        } else {
            if (peek() == '\n') lexer.line++;
            advance();
        }
    }
    if (is_at_end()) return error_token("Unterminated string.");
    advance(); // closing quote
    return make_token(TOKEN_STRING_LITERAL);
}

Token next_token(void) {
    skip_whitespace();
    if (lexer.unterminated_comment) {
        lexer.unterminated_comment = false;
        return error_token("Unterminated block comment.");
    }
    lexer.start = lexer.current;
    if (is_at_end()) return make_token(TOKEN_EOF);

    char c = advance();
    if (isalpha(c) || c == '_') return identifier();
    if (isdigit(c)) return number();

    switch (c) {
        case '(': return make_token(TOKEN_LPAREN);
        case ')': return make_token(TOKEN_RPAREN);
        case '{': return make_token(TOKEN_LBRACE);
        case '}': return make_token(TOKEN_RBRACE);
        case '[': return make_token(TOKEN_LBRACKET);
        case ']': return make_token(TOKEN_RBRACKET);
        case ';': return make_token(TOKEN_SEMICOLON);
        case ',': return make_token(TOKEN_COMMA);
        case '.':
            if (match('.') && match('.')) return make_token(TOKEN_ELLIPSIS);
            return make_token(TOKEN_DOT);
        case ':': return make_token(TOKEN_COLON);
        case '?':
            if (match('.')) return make_token(TOKEN_QUESTION_DOT);
            if (match('?')) return make_token(TOKEN_QUESTION_QUESTION);
            return make_token(TOKEN_QUESTION);
        case '-':
            if (match('>')) return make_token(TOKEN_ARROW);
            if (match('-')) return make_token(TOKEN_DECREMENT);
            return make_token(match('=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
        case '+':
            if (match('+')) return make_token(TOKEN_INCREMENT);
            return make_token(match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
        case '/': return make_token(match('=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
        case '*': return make_token(match('=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
        case '!': return make_token(match('=') ? TOKEN_BANG_EQ : TOKEN_BANG);
        case '=': 
            if (match('=')) return make_token(TOKEN_EQ_EQ);
            if (match('>')) return make_token(TOKEN_FAT_ARROW);
            return make_token(TOKEN_EQUAL);
        case '<': return make_token(match('=') ? TOKEN_LESS_EQ : TOKEN_LESS);
        case '>': return make_token(match('=') ? TOKEN_GREATER_EQ : TOKEN_GREATER);
        case '&': if (match('&')) return make_token(TOKEN_AND_AND); break;
        case '|': if (match('|')) return make_token(TOKEN_OR_OR); break;
        case '@': return make_token(TOKEN_AT);
        case '$': return make_token(TOKEN_DOLLAR);
        case '"': return string();
        case '\'': {
            if (is_at_end()) return error_token("Unterminated char.");
            char inner = advance();
            if (inner == '\\') {
                if (is_at_end()) return error_token("Unterminated char.");
                advance(); // consume escaped char
            }
            if (!match('\'')) return error_token("Unterminated char.");
            return make_token(TOKEN_CHAR_LITERAL);
        }
    }

    return error_token("Unexpected character.");
}
