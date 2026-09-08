#ifndef VERISHOT_LEXER_H
#define VERISHOT_LEXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* =========================================================
 * VERISHOT LEXER CONFIGURATION
 * ========================================================= */

#define VERISHOT_MAX_SOURCE_LENGTH      256
#define VERISHOT_MAX_TOKENS             64
#define VERISHOT_MAX_LEXEME_LENGTH      16


/* =========================================================
 * TOKEN TYPES
 * ========================================================= */

typedef enum
{
    TOKEN_INPUT = 0,
    TOKEN_OUTPUT,
    TOKEN_ASSIGN,

    TOKEN_IDENTIFIER,
    TOKEN_CONSTANT,

    TOKEN_AND,
    TOKEN_OR,
    TOKEN_XOR,
    TOKEN_NOT,

    TOKEN_EQUAL,

    TOKEN_LPAREN,
    TOKEN_RPAREN,

    TOKEN_COMMA,
    TOKEN_SEMICOLON,

    TOKEN_EOF,
    TOKEN_ERROR

} TokenType;


/* =========================================================
 * LEXER STATUS
 * ========================================================= */

typedef enum
{
    LEXER_OK = 0,

    LEXER_TOKEN_OVERFLOW,
    LEXER_IDENTIFIER_TOO_LONG,
    LEXER_INVALID_CONSTANT,
    LEXER_ERROR

} LexerStatus;


/* =========================================================
 * TOKEN STRUCTURE
 * ========================================================= */

typedef struct
{
    TokenType type;

    char lexeme[VERISHOT_MAX_LEXEME_LENGTH];

    uint16_t line;
    uint16_t column;

} Token;


/* =========================================================
 * LEXER STRUCTURE
 * ========================================================= */

typedef struct
{
    const char *source;

    uint16_t position;

    uint16_t line;
    uint16_t column;

    Token tokens[VERISHOT_MAX_TOKENS];

    uint16_t token_count;

    LexerStatus status;

} Lexer;


/* =========================================================
 * PUBLIC FUNCTIONS
 * ========================================================= */

void Lexer_Init(Lexer *lexer, const char *source);

LexerStatus Lexer_Tokenize(Lexer *lexer);

const char *Lexer_TokenTypeToString(TokenType type);

void Lexer_PrintTokens(const Lexer *lexer);

#ifdef __cplusplus
}
#endif

#endif /* VERISHOT_LEXER_H */
