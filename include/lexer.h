#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,

    // Keywords
    TOKEN_MAIN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_FUNC,
    TOKEN_RETURN,
    TOKEN_CLASS,
    TOKEN_VAR,
    TOKEN_IN,
    TOKEN_IMPORT,
    TOKEN_CONST,
    TOKEN_ENUM,
    TOKEN_STRUCT,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_TEST,
    TOKEN_ASSERT,

    TOKEN_BREAK,
    TOKEN_CONTINUE,

    // Error handling
    TOKEN_TRY,
    TOKEN_CATCH,
    TOKEN_THROW,
    TOKEN_FINALLY,

    // OOP
    TOKEN_EXTENDS,
    TOKEN_INTERFACE,
    TOKEN_IMPLEMENTS,
    TOKEN_OVERRIDE,
    TOKEN_NEW,
    TOKEN_SUPER,
    TOKEN_AS,

    // Pattern matching
    TOKEN_MATCH,

    // Traits / Mixins
    TOKEN_TRAIT,

    // Types
    TOKEN_INT_TYPE,
    TOKEN_FLOAT_TYPE,
    TOKEN_STR_TYPE,
    TOKEN_BOOL_TYPE,
    TOKEN_CHAR_TYPE,

    // Literals
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,

    // Operators
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_EQUAL,        // =
    TOKEN_PLUS_EQUAL,   // +=
    TOKEN_MINUS_EQUAL,  // -=
    TOKEN_STAR_EQUAL,   // *=
    TOKEN_SLASH_EQUAL,  // /=
    TOKEN_EQ_EQ,        // ==
    TOKEN_BANG_EQ,      // !=
    TOKEN_LESS,         // <
    TOKEN_LESS_EQ,      // <=
    TOKEN_GREATER,      // >
    TOKEN_GREATER_EQ,   // >=
    TOKEN_AND_AND,      // &&
    TOKEN_OR_OR,        // ||
    TOKEN_BANG,         // !
    TOKEN_ARROW,        // ->
    TOKEN_FAT_ARROW,    // =>
    TOKEN_INCREMENT,    // ++
    TOKEN_DECREMENT,    // --

    // Punctuation
    TOKEN_LBRACE,       // {
    TOKEN_RBRACE,       // }
    TOKEN_LPAREN,       // (
    TOKEN_RPAREN,       // )
    TOKEN_LBRACKET,     // [
    TOKEN_RBRACKET,     // ]
    TOKEN_COMMA,        // ,
    TOKEN_SEMICOLON,    // ;
    TOKEN_DOT,          // .
    TOKEN_COLON,        // :
    TOKEN_QUESTION,     // ?
    TOKEN_QUESTION_DOT, // ?.
    TOKEN_QUESTION_QUESTION, // ??
    TOKEN_ELLIPSIS,     // ...
    TOKEN_OPTIONAL,     // Optional
    TOKEN_RESULT,       // Result
    TOKEN_OPERATOR,     // operator
    TOKEN_EXTEND,       // extend
    TOKEN_OPTION,       // option
    TOKEN_OK,           // ok
    TOKEN_ERR,          // err

    // v3.5: Iterators
    TOKEN_YIELD,        // yield
    TOKEN_ITER,         // iter

    // v3.5: Coroutines
    TOKEN_COROUTINE,    // coroutine
    TOKEN_RESUME,       // resume
    TOKEN_WITH,         // with

    // v3.5: Async/Await
    TOKEN_ASYNC,        // async
    TOKEN_AWAIT,        // await
    TOKEN_RUN_ASYNC,    // run_async

    // v3.5: Threading
    TOKEN_THREAD,       // thread
    TOKEN_SPAWN,        // spawn
    TOKEN_MUTEX,        // mutex
    TOKEN_CHANNEL,      // channel
    TOKEN_LOCK,         // lock
    TOKEN_UNLOCK,       // unlock
    TOKEN_SEND,         // send
    TOKEN_RECV,         // recv

    // v4.0: Reflection and macros
    TOKEN_TYPEOF,       // typeof
    TOKEN_MACRO,        // macro
    TOKEN_CONSTEXPR,    // constexpr
    TOKEN_AT,           // @ attribute marker

    // v4.5: Memory & FFI
    TOKEN_EXTERN,       // extern
    TOKEN_OWN,          // own
    TOKEN_BENCH,        // bench
    TOKEN_DOLLAR        // $ (for FFI naming: $function_name)
} CnextTokenType;

typedef struct {
    CnextTokenType type;
    const char* start;
    int length;
    int line;
} Token;

void init_lexer(const char* source);
Token next_token();

#endif // LEXER_H
