/*
 * verishot_lexer.c
 *
 *  Created on: 09-Sept-2026
 *      Author: ASUS
 */


#include "verishot_lexer.h"

#include <string.h>
#include <stdio.h>


/* =========================================================
 * PRIVATE CHARACTER FUNCTIONS
 * ========================================================= */

static uint8_t IsLetter(char c)
{
    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z'))
    {
        return 1;
    }

    return 0;
}


static uint8_t IsDigit(char c)
{
    if (c >= '0' && c <= '9')
    {
        return 1;
    }

    return 0;
}


static uint8_t IsAlphaNumeric(char c)
{
    if (IsLetter(c) || IsDigit(c))
    {
        return 1;
    }

    return 0;
}


static uint8_t IsIdentifierStart(char c)
{
    if (IsLetter(c) || c == '_')
    {
        return 1;
    }

    return 0;
}


static uint8_t IsIdentifierPart(char c)
{
    if (IsAlphaNumeric(c) || c == '_')
    {
        return 1;
    }

    return 0;
}


/* =========================================================
 * CURRENT CHARACTER
 * ========================================================= */

static char Lexer_CurrentChar(const Lexer *lexer)
{
    return lexer->source[lexer->position];
}


/* =========================================================
 * LOOK AHEAD
 * ========================================================= */

static char Lexer_PeekChar(const Lexer *lexer)
{
    if (lexer->source[lexer->position] == '\0')
    {
        return '\0';
    }

    return lexer->source[lexer->position + 1];
}


/* =========================================================
 * ADVANCE ONE CHARACTER
 * ========================================================= */

static void Lexer_Advance(Lexer *lexer)
{
    char current = Lexer_CurrentChar(lexer);

    if (current == '\n')
    {
        lexer->line++;
        lexer->column = 1;
    }
    else
    {
        lexer->column++;
    }

    lexer->position++;
}


/* =========================================================
 * ADD TOKEN
 * ========================================================= */

static LexerStatus Lexer_AddToken(Lexer *lexer,
                                  TokenType type,
                                  const char *lexeme,
                                  uint16_t line,
                                  uint16_t column)
{
    Token *token;

    if (lexer->token_count >= VERISHOT_MAX_TOKENS)
    {
        lexer->status = LEXER_TOKEN_OVERFLOW;
        return LEXER_TOKEN_OVERFLOW;
    }

    token = &lexer->tokens[lexer->token_count];

    token->type = type;

    strncpy(token->lexeme,
            lexeme,
            VERISHOT_MAX_LEXEME_LENGTH - 1);

    token->lexeme[VERISHOT_MAX_LEXEME_LENGTH - 1] = '\0';

    token->line = line;
    token->column = column;

    lexer->token_count++;

    return LEXER_OK;
}


/* =========================================================
 * KEYWORD CHECK
 * ========================================================= */

static TokenType Lexer_CheckKeyword(const char *word)
{
    if (strcmp(word, "input") == 0)
    {
        return TOKEN_INPUT;
    }

    if (strcmp(word, "output") == 0)
    {
        return TOKEN_OUTPUT;
    }

    if (strcmp(word, "assign") == 0)
    {
        return TOKEN_ASSIGN;
    }

    return TOKEN_IDENTIFIER;
}


/* =========================================================
 * READ IDENTIFIER / KEYWORD
 * ========================================================= */

static LexerStatus Lexer_ReadIdentifier(Lexer *lexer)
{
    char buffer[VERISHOT_MAX_LEXEME_LENGTH];

    uint16_t index = 0;

    uint16_t start_line = lexer->line;
    uint16_t start_column = lexer->column;

    char current;


    while (IsIdentifierPart(Lexer_CurrentChar(lexer)))
    {
        current = Lexer_CurrentChar(lexer);

        /*
         * Keep one byte available for '\0'
         */

        if (index >= VERISHOT_MAX_LEXEME_LENGTH - 1)
        {
            /*
             * Continue consuming the complete identifier
             * so the lexer does not start interpreting the
             * remainder as another token.
             */

            while (IsIdentifierPart(Lexer_CurrentChar(lexer)))
            {
                Lexer_Advance(lexer);
            }

            lexer->status = LEXER_IDENTIFIER_TOO_LONG;

            return LEXER_IDENTIFIER_TOO_LONG;
        }

        buffer[index++] = current;

        Lexer_Advance(lexer);
    }

    buffer[index] = '\0';


    return Lexer_AddToken(lexer,
                          Lexer_CheckKeyword(buffer),
                          buffer,
                          start_line,
                          start_column);
}


/* =========================================================
 * READ CONSTANT
 * ========================================================= */

static LexerStatus Lexer_ReadConstant(Lexer *lexer)
{
    char buffer[2];

    uint16_t line = lexer->line;
    uint16_t column = lexer->column;

    char current = Lexer_CurrentChar(lexer);


    /*
     * VERISHOT V0.1 only supports
     *
     *     0
     *     1
     */

    if (current != '0' && current != '1')
    {
        lexer->status = LEXER_INVALID_CONSTANT;

        return LEXER_INVALID_CONSTANT;
    }


    buffer[0] = current;
    buffer[1] = '\0';

    Lexer_Advance(lexer);


    /*
     * If another digit immediately follows,
     * reject it.
     *
     * Example:
     *
     *     10
     *     101
     *     2
     */

    if (IsDigit(Lexer_CurrentChar(lexer)))
    {
        while (IsDigit(Lexer_CurrentChar(lexer)))
        {
            Lexer_Advance(lexer);
        }

        lexer->status = LEXER_INVALID_CONSTANT;

        return LEXER_INVALID_CONSTANT;
    }


    return Lexer_AddToken(lexer,
                          TOKEN_CONSTANT,
                          buffer,
                          line,
                          column);
}


/* =========================================================
 * SKIP WHITESPACE
 * ========================================================= */

static void Lexer_SkipWhitespace(Lexer *lexer)
{
    while (1)
    {
        char c = Lexer_CurrentChar(lexer);

        if (c == ' ' ||
            c == '\t' ||
            c == '\r' ||
            c == '\n')
        {
            Lexer_Advance(lexer);
        }
        else
        {
            break;
        }
    }
}


/* =========================================================
 * SKIP COMMENT
 * ========================================================= */

static void Lexer_SkipComment(Lexer *lexer)
{
    /*
     * Current position is at first '/'
     *
     * Skip until newline or end of source.
     */

    while (Lexer_CurrentChar(lexer) != '\0' &&
           Lexer_CurrentChar(lexer) != '\n')
    {
        Lexer_Advance(lexer);
    }
}


/* =========================================================
 * INITIALIZE LEXER
 * ========================================================= */

void Lexer_Init(Lexer *lexer, const char *source)
{
    lexer->source = source;

    lexer->position = 0;

    lexer->line = 1;
    lexer->column = 1;

    lexer->token_count = 0;

    lexer->status = LEXER_OK;


    /*
     * Clear token storage.
     */

    memset(lexer->tokens,
           0,
           sizeof(lexer->tokens));
}


/* =========================================================
 * MAIN TOKENIZER
 * ========================================================= */

LexerStatus Lexer_Tokenize(Lexer *lexer)
{
    char current;

    LexerStatus result;


    while (Lexer_CurrentChar(lexer) != '\0')
    {
        current = Lexer_CurrentChar(lexer);


        /* -------------------------------------------------
         * WHITESPACE
         * ------------------------------------------------- */

        if (current == ' ' ||
            current == '\t' ||
            current == '\r' ||
            current == '\n')
        {
            Lexer_SkipWhitespace(lexer);

            continue;
        }


        /* -------------------------------------------------
         * COMMENTS
         * ------------------------------------------------- */

        if (current == '/' &&
            Lexer_PeekChar(lexer) == '/')
        {
            Lexer_SkipComment(lexer);

            continue;
        }


        /* -------------------------------------------------
         * IDENTIFIER / KEYWORD
         * ------------------------------------------------- */

        if (IsIdentifierStart(current))
        {
            result = Lexer_ReadIdentifier(lexer);

            if (result != LEXER_OK)
            {
                return result;
            }

            continue;
        }


        /* -------------------------------------------------
         * CONSTANT
         * ------------------------------------------------- */

        if (IsDigit(current))
        {
            result = Lexer_ReadConstant(lexer);

            if (result != LEXER_OK)
            {
                return result;
            }

            continue;
        }


        /* -------------------------------------------------
         * SINGLE CHARACTER TOKENS
         * ------------------------------------------------- */

        uint16_t line = lexer->line;
        uint16_t column = lexer->column;

        switch (current)
        {
            case '&':

                result = Lexer_AddToken(lexer,
                                        TOKEN_AND,
                                        "&",
                                        line,
                                        column);

                Lexer_Advance(lexer);

                break;


            case '|':

                result = Lexer_AddToken(lexer,
                                        TOKEN_OR,
                                        "|",
                                        line,
                                        column);

                Lexer_Advance(lexer);

                break;


            case '^':

                result = Lexer_AddToken(lexer,
                                        TOKEN_XOR,
                                        "^",
                                        line,
                                        column);

                Lexer_Advance(lexer);

                break;


            case '~':

                result = Lexer_AddToken(lexer,
                                        TOKEN_NOT,
                                        "~",
                                        line,
                                        column);

                Lexer_Advance(lexer);

                break;


            case '=':

                result = Lexer_AddToken(lexer,
                                        TOKEN_EQUAL,
                                        "=",
                                        line,
                                        column);

                Lexer_Advance(lexer);

                break;


            case '(':

                result = Lexer_AddToken(lexer,
                                        TOKEN_LPAREN,
                                        "(",
                                        line,
                                        column);

                Lexer_Advance(lexer);

                break;


            case ')':

                result = Lexer_AddToken(lexer,
                                        TOKEN_RPAREN,
                                        ")",
                                        line,
                                        column);

                Lexer_Advance(lexer);

                break;


            case ',':

                result = Lexer_AddToken(lexer,
                                        TOKEN_COMMA,
                                        ",",
                                        line,
                                        column);

                Lexer_Advance(lexer);

                break;


            case ';':

                result = Lexer_AddToken(lexer,
                                        TOKEN_SEMICOLON,
                                        ";",
                                        line,
                                        column);

                Lexer_Advance(lexer);

                break;


            default:
            {
                char error_char[2];

                error_char[0] = current;
                error_char[1] = '\0';


                result = Lexer_AddToken(lexer,
                                        TOKEN_ERROR,
                                        error_char,
                                        line,
                                        column);

                Lexer_Advance(lexer);

                lexer->status = LEXER_ERROR;

                return LEXER_ERROR;
            }
        }


        if (result != LEXER_OK)
        {
            return result;
        }
    }


    /* -----------------------------------------------------
     * END OF FILE TOKEN
     * ----------------------------------------------------- */

    result = Lexer_AddToken(lexer,
                            TOKEN_EOF,
                            "",
                            lexer->line,
                            lexer->column);

    if (result != LEXER_OK)
    {
        return result;
    }


    lexer->status = LEXER_OK;

    return LEXER_OK;
}


/* =========================================================
 * TOKEN TYPE → STRING
 * ========================================================= */

const char *Lexer_TokenTypeToString(TokenType type)
{
    switch (type)
    {
        case TOKEN_INPUT:
            return "INPUT";

        case TOKEN_OUTPUT:
            return "OUTPUT";

        case TOKEN_ASSIGN:
            return "ASSIGN";

        case TOKEN_IDENTIFIER:
            return "IDENTIFIER";

        case TOKEN_CONSTANT:
            return "CONSTANT";

        case TOKEN_AND:
            return "AND";

        case TOKEN_OR:
            return "OR";

        case TOKEN_XOR:
            return "XOR";

        case TOKEN_NOT:
            return "NOT";

        case TOKEN_EQUAL:
            return "EQUAL";

        case TOKEN_LPAREN:
            return "LPAREN";

        case TOKEN_RPAREN:
            return "RPAREN";

        case TOKEN_COMMA:
            return "COMMA";

        case TOKEN_SEMICOLON:
            return "SEMICOLON";

        case TOKEN_EOF:
            return "EOF";

        case TOKEN_ERROR:
            return "ERROR";

        default:
            return "UNKNOWN";
    }
}


/* =========================================================
 * PRINT ALL TOKENS
 *
 * NOTE:
 * This function uses printf().
 *
 * On STM32, redirect printf() to UART/USB CDC.
 * ========================================================= */

void Lexer_PrintTokens(const Lexer *lexer)
{
    uint16_t i;

    printf("\r\n");
    printf("========================================\r\n");
    printf("       VERISHOT LEXER TOKEN DUMP       \r\n");
    printf("========================================\r\n");


    for (i = 0; i < lexer->token_count; i++)
    {
        const Token *token = &lexer->tokens[i];

        printf("[%02u] %-12s  \"%s\"  "
               "Line:%u Col:%u\r\n",

               i,

               Lexer_TokenTypeToString(token->type),

               token->lexeme,

               token->line,

               token->column);
    }


    printf("========================================\r\n");
    printf("Tokens: %u\r\n", lexer->token_count);
    printf("========================================\r\n");
}
