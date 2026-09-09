#ifndef VERISHOT_PARSER_H
#define VERISHOT_PARSER_H

#include <stdint.h>

#include "verishot_lexer.h"


/* ============================================================
 * Configuration
 * ============================================================ */

#define VERISHOT_MAX_AST_NODES 64
#define VERISHOT_MAX_SIGNALS       16
#define VERISHOT_MAX_ASSIGNMENTS   16


/* ============================================================
 * AST Node Types
 * ============================================================ */

typedef enum
{
    AST_IDENTIFIER = 0,
    AST_CONSTANT,

    AST_AND,
    AST_OR,
    AST_XOR,
    AST_NOT

} ASTNodeType;


/* ============================================================
 * AST Node
 * ============================================================ */

typedef struct ASTNode
{
    ASTNodeType type;

    char value[VERISHOT_MAX_LEXEME_LENGTH];

    struct ASTNode *left;
    struct ASTNode *right;

} ASTNode;


/* ============================================================
 * Parser Status
 * ============================================================ */

typedef enum
{
    PARSER_OK = 0,

    PARSER_UNEXPECTED_TOKEN,
    PARSER_EXPECTED_IDENTIFIER,
    PARSER_EXPECTED_SEMICOLON,
    PARSER_EXPECTED_EQUAL,
    PARSER_EXPECTED_EXPRESSION,
    PARSER_UNEXPECTED_EOF,

    PARSER_AST_OVERFLOW,
    PARSER_ERROR

} ParserStatus;

typedef struct
{
    char name[VERISHOT_MAX_LEXEME_LENGTH];
} VerishotSignal;


typedef struct
{
    char name[VERISHOT_MAX_LEXEME_LENGTH];
    ASTNode *expression;
} VerishotAssignment;

typedef struct
{
    VerishotSignal inputs[VERISHOT_MAX_SIGNALS];
    uint16_t input_count;

    VerishotSignal outputs[VERISHOT_MAX_SIGNALS];
    uint16_t output_count;

    VerishotAssignment assignments[VERISHOT_MAX_ASSIGNMENTS];
    uint16_t assignment_count;

} VerishotProgram;


/* ============================================================
 * Parser
 * ============================================================ */

typedef struct
{
	Token *tokens;

	    uint16_t position;
	    uint16_t token_count;

	    ASTNode ast_nodes[VERISHOT_MAX_AST_NODES];
	    uint16_t ast_node_count;

	    VerishotProgram program;

	    ParserStatus status;

} Parser;







/* ============================================================
 * Public Functions
 * ============================================================ */

void Parser_Init(Parser *parser,
                 Token *tokens,
                 uint16_t token_count);

ParserStatus Parser_Parse(Parser *parser);

const char *Parser_StatusToString(ParserStatus status);

const char *Parser_ASTNodeTypeToString(ASTNodeType type);

void Parser_PrintAST(const Parser *parser);


#endif /* VERISHOT_PARSER_H */
