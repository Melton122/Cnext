#include "lexer.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    const char* start;
    const char* current;
    int line;
    int column;
    int start_column;
    bool unterminated_comment;
} Lexer;

static Lexer lexer;

void init_lexer(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
    lexer.column = 1;
    lexer.unterminated_comment = false;
}

static bool is_at_end(void) {
    return *lexer.current == '\0';
}

static char advance(void) {
    lexer.current++;
    lexer.column++;
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
    token.column = lexer.start_column;
    return token;
}

static Token error_token(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
    token.column = lexer.column;
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
                lexer.column = 0;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) advance();
                } else if (peek_next() == '*') {
                    advance(); advance();
                    while (!is_at_end() && !(peek() == '*' && peek_next() == '/')) {
                        if (peek() == '\n') { lexer.line++; lexer.column = 0; }
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

typedef struct {
    const char* word;
    CnextTokenType type;
} KeywordEntry;

static int kw_cmp(const void* a, const void* b) {
    return strcmp(((const KeywordEntry*)a)->word, ((const KeywordEntry*)b)->word);
}

static const KeywordEntry keywords[] = {
    {"abstract", TOKEN_ABSTRACT},
    {"as", TOKEN_AS},
    {"assert", TOKEN_ASSERT},
    {"async", TOKEN_ASYNC},
    {"await", TOKEN_AWAIT},
    {"bench", TOKEN_BENCH},
    {"bool", TOKEN_BOOL_TYPE},
    {"break", TOKEN_BREAK},
    {"byte", TOKEN_BYTE_TYPE},
    {"case", TOKEN_CASE},
    {"catch", TOKEN_CATCH},
    {"channel", TOKEN_CHANNEL},
    {"char", TOKEN_CHAR_TYPE},
    {"class", TOKEN_CLASS},
    {"const", TOKEN_CONST},
    {"constexpr", TOKEN_CONSTEXPR},
    {"continue", TOKEN_CONTINUE},
    {"coroutine", TOKEN_COROUTINE},
    {"default", TOKEN_DEFAULT},
    {"defer", TOKEN_DEFER},
    {"double", TOKEN_DOUBLE_TYPE},
    {"else", TOKEN_ELSE},
    {"enum", TOKEN_ENUM},
    {"err", TOKEN_ERR},
    {"exit", TOKEN_IDENTIFIER},
    {"extend", TOKEN_EXTEND},
    {"extends", TOKEN_EXTENDS},
    {"extern", TOKEN_EXTERN},
    {"false", TOKEN_FALSE},
    {"final", TOKEN_FINAL},
    {"finally", TOKEN_FINALLY},
    {"float", TOKEN_FLOAT_TYPE},
    {"for", TOKEN_FOR},
    {"func", TOKEN_FUNC},
    {"if", TOKEN_IF},
    {"implements", TOKEN_IMPLEMENTS},
    {"import", TOKEN_IMPORT},
    {"in", TOKEN_IN},
    {"int", TOKEN_INT_TYPE},
    {"interface", TOKEN_INTERFACE},
    {"iter", TOKEN_ITER},
    {"lock", TOKEN_LOCK},
    {"long", TOKEN_LONG_TYPE},
    {"macro", TOKEN_MACRO},
    {"main", TOKEN_MAIN},
    {"match", TOKEN_MATCH},
    {"mutex", TOKEN_MUTEX},
    {"new", TOKEN_NEW},
    {"none", TOKEN_NONE},
    {"null", TOKEN_NULL},
    {"ok", TOKEN_OK},
    {"operator", TOKEN_OPERATOR},
    {"option", TOKEN_OPTION},
    {"override", TOKEN_OVERRIDE},
    {"own", TOKEN_OWN},
    {"recv", TOKEN_RECV},
    {"resume", TOKEN_RESUME},
    {"return", TOKEN_RETURN},
    {"run_async", TOKEN_RUN_ASYNC},
    {"send", TOKEN_SEND},
    {"spawn", TOKEN_SPAWN},
    {"static", TOKEN_STATIC},
    {"str", TOKEN_STR_TYPE},
    {"struct", TOKEN_STRUCT},
    {"super", TOKEN_SUPER},
    {"switch", TOKEN_SWITCH},
    {"test", TOKEN_TEST},
    {"thread", TOKEN_THREAD},
    {"throw", TOKEN_THROW},
    {"trait", TOKEN_TRAIT},
    {"true", TOKEN_TRUE},
    {"try", TOKEN_TRY},
    {"type", TOKEN_TYPE_ALIAS},
    {"typeof", TOKEN_TYPEOF},
    {"ubyte", TOKEN_UBYTE_TYPE},
    {"uint", TOKEN_UINT_TYPE},
    {"ulong", TOKEN_ULONG_TYPE},
    {"unlock", TOKEN_UNLOCK},
    {"ushort", TOKEN_USHORT_TYPE},
    {"var", TOKEN_VAR},
    {"when", TOKEN_WHEN},
    {"while", TOKEN_WHILE},
    {"with", TOKEN_WITH},
    {"yield", TOKEN_YIELD},
};

#define KEYWORD_COUNT (sizeof(keywords) / sizeof(keywords[0]))

static CnextTokenType identifier_type(void) {
    int len = (int)(lexer.current - lexer.start);
    if (len > 63) return TOKEN_IDENTIFIER;
    char buf[64];
    memcpy(buf, lexer.start, len);
    buf[len] = '\0';
    KeywordEntry key = {buf, TOKEN_EOF};
    KeywordEntry* found = (KeywordEntry*)bsearch(&key, keywords, KEYWORD_COUNT, sizeof(KeywordEntry), kw_cmp);
    return found ? found->type : TOKEN_IDENTIFIER;
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
            if (peek() == '\n') { lexer.line++; lexer.column = 0; }
            advance();
        }
    }
    if (is_at_end()) return error_token("Unterminated string.");
    advance(); // closing quote
    return make_token(TOKEN_STRING_LITERAL);
}

static Token raw_string(void) {
    // Already consumed 'r' and '"'; now read until closing '"'
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') { lexer.line++; lexer.column = 0; }
        advance();
    }
    if (is_at_end()) return error_token("Unterminated raw string.");
    advance(); // closing quote
    return make_token(TOKEN_RAW_STRING);
}

Token next_token(void) {
    skip_whitespace();
    if (lexer.unterminated_comment) {
        lexer.unterminated_comment = false;
        return error_token("Unterminated block comment.");
    }
    lexer.start = lexer.current;
    lexer.start_column = lexer.column;
    if (is_at_end()) return make_token(TOKEN_EOF);

    char c = advance();
    if (isalpha(c) || c == '_') {
        if (c == 'r' && peek() == '"') {
            advance(); // consume the '"'
            return raw_string();
        }
        return identifier();
    }
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
            if (match('.')) {
                if (match('.')) return make_token(TOKEN_ELLIPSIS);
                if (match('=')) return make_token(TOKEN_RANGE_INCLUSIVE);
                return make_token(TOKEN_RANGE);
            }
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
        case '%': return make_token(TOKEN_PERCENT);
        case '!': return make_token(match('=') ? TOKEN_BANG_EQ : TOKEN_BANG);
        case '=': 
            if (match('=')) return make_token(TOKEN_EQ_EQ);
            if (match('>')) return make_token(TOKEN_FAT_ARROW);
            return make_token(TOKEN_EQUAL);
        case '<': return make_token(match('=') ? TOKEN_LESS_EQ : TOKEN_LESS);
        case '>': return make_token(match('=') ? TOKEN_GREATER_EQ : TOKEN_GREATER);
        case '&': if (match('&')) return make_token(TOKEN_AND_AND); break;
        case '|': if (match('|')) return make_token(TOKEN_OR_OR); return make_token(TOKEN_PIPE);
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
