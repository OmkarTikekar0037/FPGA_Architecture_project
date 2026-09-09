/*
 * verishot_parser.c
 *
 *  Created on: 09-Sept-2026
 *      Author: ASUS
 */
#include "verishot_parser.h"

#include <stdio.h>
#include <string.h>


/* ============================================================
 * Internal Function Prototypes
 * ============================================================ */

static Token *Parser_CurrentToken(Parser *parser);
static Token *Parser_PeekToken(Parser *parser);

static void Parser_Advance(Parser *parser);

static int Parser_Match(Parser *parser, TokenType type);
static int Parser_Expect(Parser *parser, TokenType type);

static ASTNode *Parser_CreateASTNode(Parser *parser,
                                     ASTNodeType type);

static ASTNode *Parser_CreateIdentifier(Parser *parser,
                                         const char *name);

static ASTNode *Parser_CreateConstant(Parser *parser,
                                      const char *value);

static ASTNode *Parser_ParsePrimary(Parser *parser);
static ASTNode *Parser_ParseUnary(Parser *parser);
static ASTNode *Parser_ParseAnd(Parser *parser);
static ASTNode *Parser_ParseXor(Parser *parser);
static ASTNode *Parser_ParseOr(Parser *parser);
static ASTNode *Parser_ParseExpression(Parser *parser);

static int Parser_ParseInputDeclaration(Parser *parser);
static int Parser_ParseOutputDeclaration(Parser *parser);
static int Parser_ParseAssignment(Parser *parser);

static int Parser_AddInput(Parser *parser, const char *name);
static int Parser_AddOutput(Parser *parser, const char *name);
static int Parser_AddAssignment(Parser *parser,
                                const char *name,
                                ASTNode *expression);

static void Parser_PrintASTNode(const ASTNode *node, uint8_t depth);


/* ============================================================
 * Parser Initialization
 * ============================================================ */

void Parser_Init(Parser *parser,
                 Token *tokens,
                 uint16_t token_count)
{
    if (parser == NULL)
    {
        return;
    }

    parser->tokens = tokens;

    parser->position = 0;
    parser->token_count = token_count;

    parser->ast_node_count = 0;

    parser->program.input_count = 0;
    parser->program.output_count = 0;
    parser->program.assignment_count = 0;

    parser->status = PARSER_OK;
}


/* ============================================================
 * Current Token
 * ============================================================ */

static Token *Parser_CurrentToken(Parser *parser)
{
    if (parser == NULL)
    {
        return NULL;
    }

    if (parser->position >= parser->token_count)
    {
        return NULL;
    }

    return &parser->tokens[parser->position];
}


/* ============================================================
 * Peek Token
 * ============================================================ */

static Token *Parser_PeekToken(Parser *parser)
{
    if (parser == NULL)
    {
        return NULL;
    }

    if ((parser->position + 1) >= parser->token_count)
    {
        return NULL;
    }

    return &parser->tokens[parser->position + 1];
}


/* ============================================================
 * Advance Parser
 * ============================================================ */

static void Parser_Advance(Parser *parser)
{
    if (parser == NULL)
    {
        return;
    }

    if (parser->position < parser->token_count)
    {
        parser->position++;
    }
}


/* ============================================================
 * Match Token
 *
 * If current token matches the requested type:
 *     consume it
 *     return 1
 *
 * Otherwise:
 *     do not consume anything
 *     return 0
 * ============================================================ */

static int Parser_Match(Parser *parser, TokenType type)
{
    Token *token;

    token = Parser_CurrentToken(parser);

    if (token == NULL)
    {
        return 0;
    }

    if (token->type == type)
    {
        Parser_Advance(parser);
        return 1;
    }

    return 0;
}


/* ============================================================
 * Expect Token
 *
 * The requested token MUST be present.
 * If it is not present, parser enters an error state.
 * ============================================================ */

static int Parser_Expect(Parser *parser, TokenType type)
{
    Token *token;

    token = Parser_CurrentToken(parser);

    if (token == NULL)
    {
        parser->status = PARSER_UNEXPECTED_EOF;
        return 0;
    }

    if (token->type != type)
    {
        parser->status = PARSER_UNEXPECTED_TOKEN;
        return 0;
    }

    Parser_Advance(parser);

    return 1;
}


/* ============================================================
 * Create AST Node
 * ============================================================ */

static ASTNode *Parser_CreateASTNode(Parser *parser,
                                     ASTNodeType type)
{
    ASTNode *node;

    if (parser == NULL)
    {
        return NULL;
    }

    if (parser->ast_node_count >= VERISHOT_MAX_AST_NODES)
    {
        parser->status = PARSER_AST_OVERFLOW;
        return NULL;
    }

    node = &parser->ast_nodes[parser->ast_node_count];

    parser->ast_node_count++;

    node->type = type;

    node->value[0] = '\0';

    node->left = NULL;
    node->right = NULL;

    return node;
}


/* ============================================================
 * Create Identifier AST Node
 * ============================================================ */

static ASTNode *Parser_CreateIdentifier(Parser *parser,
                                         const char *name)
{
    ASTNode *node;

    node = Parser_CreateASTNode(parser, AST_IDENTIFIER);

    if (node == NULL)
    {
        return NULL;
    }

    strncpy(node->value,
            name,
            VERISHOT_MAX_LEXEME_LENGTH - 1);

    node->value[VERISHOT_MAX_LEXEME_LENGTH - 1] = '\0';

    return node;
}


/* ============================================================
 * Create Constant AST Node
 * ============================================================ */

static ASTNode *Parser_CreateConstant(Parser *parser,
                                      const char *value)
{
    ASTNode *node;

    node = Parser_CreateASTNode(parser, AST_CONSTANT);

    if (node == NULL)
    {
        return NULL;
    }

    strncpy(node->value,
            value,
            VERISHOT_MAX_LEXEME_LENGTH - 1);

    node->value[VERISHOT_MAX_LEXEME_LENGTH - 1] = '\0';

    return node;
}


/* ============================================================
 * Parse Primary Expression
 *
 * primary:
 *
 *     IDENTIFIER
 *     CONSTANT
 *     '(' expression ')'
 * ============================================================ */

static ASTNode *Parser_ParsePrimary(Parser *parser)
{
    Token *token;
    ASTNode *node;

    token = Parser_CurrentToken(parser);

    if (token == NULL)
    {
        parser->status = PARSER_UNEXPECTED_EOF;
        return NULL;
    }


    /* --------------------------------------------------------
     * Identifier
     * -------------------------------------------------------- */

    if (token->type == TOKEN_IDENTIFIER)
    {
        node = Parser_CreateIdentifier(parser,
                                       token->lexeme);

        if (node == NULL)
        {
            return NULL;
        }

        Parser_Advance(parser);

        return node;
    }


    /* --------------------------------------------------------
     * Constant
     * -------------------------------------------------------- */

    if (token->type == TOKEN_CONSTANT)
    {
        node = Parser_CreateConstant(parser,
                                     token->lexeme);

        if (node == NULL)
        {
            return NULL;
        }

        Parser_Advance(parser);

        return node;
    }


    /* --------------------------------------------------------
     * Parenthesized Expression
     * -------------------------------------------------------- */

    if (Parser_Match(parser, TOKEN_LPAREN))
    {
        node = Parser_ParseExpression(parser);

        if (node == NULL)
        {
            return NULL;
        }

        if (!Parser_Expect(parser, TOKEN_RPAREN))
        {
            return NULL;
        }

        return node;
    }


    /* --------------------------------------------------------
     * Nothing valid found
     * -------------------------------------------------------- */

    parser->status = PARSER_EXPECTED_EXPRESSION;

    return NULL;
}


/* ============================================================
 * Parse Unary Expression
 *
 * unary:
 *
 *     '~' unary
 *     primary
 * ============================================================ */

static ASTNode *Parser_ParseUnary(Parser *parser)
{
    ASTNode *node;

    if (Parser_Match(parser, TOKEN_NOT))
    {
        node = Parser_CreateASTNode(parser, AST_NOT);

        if (node == NULL)
        {
            return NULL;
        }

        node->left = Parser_ParseUnary(parser);

        if (node->left == NULL)
        {
            return NULL;
        }

        return node;
    }

    return Parser_ParsePrimary(parser);
}


/* ============================================================
 * Parse AND Expression
 *
 * and_expression:
 *
 *     unary_expression
 *     ( '&' unary_expression )*
 * ============================================================ */

static ASTNode *Parser_ParseAnd(Parser *parser)
{
    ASTNode *left;
    ASTNode *right;
    ASTNode *node;

    left = Parser_ParseUnary(parser);

    if (left == NULL)
    {
        return NULL;
    }

    while (Parser_Match(parser, TOKEN_AND))
    {
        right = Parser_ParseUnary(parser);

        if (right == NULL)
        {
            return NULL;
        }

        node = Parser_CreateASTNode(parser, AST_AND);

        if (node == NULL)
        {
            return NULL;
        }

        node->left = left;
        node->right = right;

        left = node;
    }

    return left;
}


/* ============================================================
 * Parse XOR Expression
 *
 * xor_expression:
 *
 *     and_expression
 *     ( '^' and_expression )*
 * ============================================================ */

static ASTNode *Parser_ParseXor(Parser *parser)
{
    ASTNode *left;
    ASTNode *right;
    ASTNode *node;

    left = Parser_ParseAnd(parser);

    if (left == NULL)
    {
        return NULL;
    }

    while (Parser_Match(parser, TOKEN_XOR))
    {
        right = Parser_ParseAnd(parser);

        if (right == NULL)
        {
            return NULL;
        }

        node = Parser_CreateASTNode(parser, AST_XOR);

        if (node == NULL)
        {
            return NULL;
        }

        node->left = left;
        node->right = right;

        left = node;
    }

    return left;
}


/* ============================================================
 * Parse OR Expression
 *
 * or_expression:
 *
 *     xor_expression
 *     ( '|' xor_expression )*
 * ============================================================ */

static ASTNode *Parser_ParseOr(Parser *parser)
{
    ASTNode *left;
    ASTNode *right;
    ASTNode *node;

    left = Parser_ParseXor(parser);

    if (left == NULL)
    {
        return NULL;
    }

    while (Parser_Match(parser, TOKEN_OR))
    {
        right = Parser_ParseXor(parser);

        if (right == NULL)
        {
            return NULL;
        }

        node = Parser_CreateASTNode(parser, AST_OR);

        if (node == NULL)
        {
            return NULL;
        }

        node->left = left;
        node->right = right;

        left = node;
    }

    return left;
}


/* ============================================================
 * Parse Expression
 * ============================================================ */

static ASTNode *Parser_ParseExpression(Parser *parser)
{
    return Parser_ParseOr(parser);
}


/* ============================================================
 * Add Input
 * ============================================================ */

static int Parser_AddInput(Parser *parser, const char *name)
{
    if (parser->program.input_count >= VERISHOT_MAX_SIGNALS)
    {
        parser->status = PARSER_ERROR;
        return 0;
    }

    strncpy(parser->program.inputs[
                parser->program.input_count
            ].name,
            name,
            VERISHOT_MAX_LEXEME_LENGTH - 1);

    parser->program.inputs[
        parser->program.input_count
    ].name[VERISHOT_MAX_LEXEME_LENGTH - 1] = '\0';

    parser->program.input_count++;

    return 1;
}


/* ============================================================
 * Add Output
 * ============================================================ */

static int Parser_AddOutput(Parser *parser, const char *name)
{
    if (parser->program.output_count >= VERISHOT_MAX_SIGNALS)
    {
        parser->status = PARSER_ERROR;
        return 0;
    }

    strncpy(parser->program.outputs[
                parser->program.output_count
            ].name,
            name,
            VERISHOT_MAX_LEXEME_LENGTH - 1);

    parser->program.outputs[
        parser->program.output_count
    ].name[VERISHOT_MAX_LEXEME_LENGTH - 1] = '\0';

    parser->program.output_count++;

    return 1;
}


/* ============================================================
 * Add Assignment
 * ============================================================ */

static int Parser_AddAssignment(Parser *parser,
                                const char *name,
                                ASTNode *expression)
{
    if (parser->program.assignment_count >=
        VERISHOT_MAX_ASSIGNMENTS)
    {
        parser->status = PARSER_ERROR;
        return 0;
    }

    strncpy(parser->program.assignments[
                parser->program.assignment_count
            ].name,
            name,
            VERISHOT_MAX_LEXEME_LENGTH - 1);

    parser->program.assignments[
        parser->program.assignment_count
    ].name[VERISHOT_MAX_LEXEME_LENGTH - 1] = '\0';

    parser->program.assignments[
        parser->program.assignment_count
    ].expression = expression;

    parser->program.assignment_count++;

    return 1;
}


/* ============================================================
 * Parse Input Declaration
 *
 * Grammar:
 *
 *     input identifier ( ',' identifier )* ';'
 *
 * Example:
 *
 *     input A, B, C;
 * ============================================================ */

static int Parser_ParseInputDeclaration(Parser *parser)
{
    Token *token;

    while (1)
    {
        token = Parser_CurrentToken(parser);

        if (token == NULL)
        {
            parser->status = PARSER_UNEXPECTED_EOF;
            return 0;
        }

        if (token->type != TOKEN_IDENTIFIER)
        {
            parser->status = PARSER_EXPECTED_IDENTIFIER;
            return 0;
        }

        if (!Parser_AddInput(parser, token->lexeme))
        {
            return 0;
        }

        Parser_Advance(parser);

        if (!Parser_Match(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!Parser_Expect(parser, TOKEN_SEMICOLON))
    {
        parser->status = PARSER_EXPECTED_SEMICOLON;
        return 0;
    }

    return 1;
}


/* ============================================================
 * Parse Output Declaration
 *
 * Grammar:
 *
 *     output identifier ( ',' identifier )* ';'
 *
 * Example:
 *
 *     output F;
 * ============================================================ */

static int Parser_ParseOutputDeclaration(Parser *parser)
{
    Token *token;

    while (1)
    {
        token = Parser_CurrentToken(parser);

        if (token == NULL)
        {
            parser->status = PARSER_UNEXPECTED_EOF;
            return 0;
        }

        if (token->type != TOKEN_IDENTIFIER)
        {
            parser->status = PARSER_EXPECTED_IDENTIFIER;
            return 0;
        }

        if (!Parser_AddOutput(parser, token->lexeme))
        {
            return 0;
        }

        Parser_Advance(parser);

        if (!Parser_Match(parser, TOKEN_COMMA))
        {
            break;
        }
    }

    if (!Parser_Expect(parser, TOKEN_SEMICOLON))
    {
        parser->status = PARSER_EXPECTED_SEMICOLON;
        return 0;
    }

    return 1;
}


/* ============================================================
 * Parse Assignment
 *
 * Grammar:
 *
 *     assign identifier '=' expression ';'
 *
 * Example:
 *
 *     assign F = (A & B) ^ C;
 * ============================================================ */

static int Parser_ParseAssignment(Parser *parser)
{
    Token *token;
    char assignment_name[VERISHOT_MAX_LEXEME_LENGTH];

    ASTNode *expression;

    token = Parser_CurrentToken(parser);

    if (token == NULL)
    {
        parser->status = PARSER_UNEXPECTED_EOF;
        return 0;
    }

    if (token->type != TOKEN_IDENTIFIER)
    {
        parser->status = PARSER_EXPECTED_IDENTIFIER;
        return 0;
    }

    strncpy(assignment_name,
            token->lexeme,
            VERISHOT_MAX_LEXEME_LENGTH - 1);

    assignment_name[
        VERISHOT_MAX_LEXEME_LENGTH - 1
    ] = '\0';

    Parser_Advance(parser);


    /* --------------------------------------------------------
     * Expect '='
     * -------------------------------------------------------- */

    if (!Parser_Expect(parser, TOKEN_EQUAL))
    {
        parser->status = PARSER_EXPECTED_EQUAL;
        return 0;
    }


    /* --------------------------------------------------------
     * Parse RHS expression
     * -------------------------------------------------------- */

    expression = Parser_ParseExpression(parser);

    if (expression == NULL)
    {
        return 0;
    }


    /* --------------------------------------------------------
     * Expect ';'
     * -------------------------------------------------------- */

    if (!Parser_Expect(parser, TOKEN_SEMICOLON))
    {
        parser->status = PARSER_EXPECTED_SEMICOLON;
        return 0;
    }


    /* --------------------------------------------------------
     * Store assignment
     * -------------------------------------------------------- */

    if (!Parser_AddAssignment(parser,
                              assignment_name,
                              expression))
    {
        return 0;
    }

    return 1;
}


/* ============================================================
 * Main Parser
 *
 * Grammar:
 *
 *     program:
 *         (input_declaration
 *        | output_declaration
 *        | assignment)*
 *         EOF
 * ============================================================ */

ParserStatus Parser_Parse(Parser *parser)
{
    Token *token;

    if (parser == NULL)
    {
        return PARSER_ERROR;
    }

    parser->status = PARSER_OK;

    while (1)
    {
        token = Parser_CurrentToken(parser);

        if (token == NULL)
        {
            parser->status = PARSER_UNEXPECTED_EOF;
            return parser->status;
        }


        /* ----------------------------------------------------
         * End of source
         * ---------------------------------------------------- */

        if (token->type == TOKEN_EOF)
        {
            return PARSER_OK;
        }


        /* ----------------------------------------------------
         * Input declaration
         * ---------------------------------------------------- */

        if (token->type == TOKEN_INPUT)
        {
            Parser_Advance(parser);

            if (!Parser_ParseInputDeclaration(parser))
            {
                return parser->status;
            }

            continue;
        }


        /* ----------------------------------------------------
         * Output declaration
         * ---------------------------------------------------- */

        if (token->type == TOKEN_OUTPUT)
        {
            Parser_Advance(parser);

            if (!Parser_ParseOutputDeclaration(parser))
            {
                return parser->status;
            }

            continue;
        }


        /* ----------------------------------------------------
         * Assignment
         * ---------------------------------------------------- */

        if (token->type == TOKEN_ASSIGN)
        {
            Parser_Advance(parser);

            if (!Parser_ParseAssignment(parser))
            {
                return parser->status;
            }

            continue;
        }


        /* ----------------------------------------------------
         * Unknown statement
         * ---------------------------------------------------- */

        parser->status = PARSER_UNEXPECTED_TOKEN;

        return parser->status;
    }
}


/* ============================================================
 * Parser Status To String
 * ============================================================ */

const char *Parser_StatusToString(ParserStatus status)
{
    switch (status)
    {
        case PARSER_OK:
            return "PARSER_OK";

        case PARSER_UNEXPECTED_TOKEN:
            return "PARSER_UNEXPECTED_TOKEN";

        case PARSER_EXPECTED_IDENTIFIER:
            return "PARSER_EXPECTED_IDENTIFIER";

        case PARSER_EXPECTED_SEMICOLON:
            return "PARSER_EXPECTED_SEMICOLON";

        case PARSER_EXPECTED_EQUAL:
            return "PARSER_EXPECTED_EQUAL";

        case PARSER_EXPECTED_EXPRESSION:
            return "PARSER_EXPECTED_EXPRESSION";

        case PARSER_UNEXPECTED_EOF:
            return "PARSER_UNEXPECTED_EOF";

        case PARSER_AST_OVERFLOW:
            return "PARSER_AST_OVERFLOW";

        case PARSER_ERROR:
            return "PARSER_ERROR";

        default:
            return "UNKNOWN_PARSER_STATUS";
    }
}


/* ============================================================
 * AST Node Type To String
 * ============================================================ */

const char *Parser_ASTNodeTypeToString(ASTNodeType type)
{
    switch (type)
    {
        case AST_IDENTIFIER:
            return "IDENTIFIER";

        case AST_CONSTANT:
            return "CONSTANT";

        case AST_AND:
            return "AND";

        case AST_OR:
            return "OR";

        case AST_XOR:
            return "XOR";

        case AST_NOT:
            return "NOT";

        default:
            return "UNKNOWN_AST_NODE";
    }
}


/* ============================================================
 * Print AST Node
 * ============================================================ */

static void Parser_PrintASTNode(const ASTNode *node,
                                uint8_t depth)
{
    uint8_t i;

    if (node == NULL)
    {
        return;
    }


    /* --------------------------------------------------------
     * Indentation
     * -------------------------------------------------------- */

    for (i = 0; i < depth; i++)
    {
        printf("    ");
    }


    /* --------------------------------------------------------
     * Print node
     * -------------------------------------------------------- */

    printf("%s",
           Parser_ASTNodeTypeToString(node->type));


    if (node->type == AST_IDENTIFIER ||
        node->type == AST_CONSTANT)
    {
        printf(" (%s)", node->value);
    }

    printf("\r\n");


    /* --------------------------------------------------------
     * Print children
     * -------------------------------------------------------- */

    if (node->left != NULL)
    {
        Parser_PrintASTNode(node->left,
                            depth + 1);
    }

    if (node->right != NULL)
    {
        Parser_PrintASTNode(node->right,
                            depth + 1);
    }
}


/* ============================================================
 * Print Complete AST / Program
 * ============================================================ */

void Parser_PrintAST(const Parser *parser)
{
    uint16_t i;

    if (parser == NULL)
    {
        return;
    }

    printf("\r\n");
    printf("========== VERISHOT PARSED PROGRAM ==========\r\n");


    /* --------------------------------------------------------
     * Inputs
     * -------------------------------------------------------- */

    printf("\r\nInputs (%u):\r\n",
           parser->program.input_count);

    for (i = 0;
         i < parser->program.input_count;
         i++)
    {
        printf("  %u: %s\r\n",
               i,
               parser->program.inputs[i].name);
    }


    /* --------------------------------------------------------
     * Outputs
     * -------------------------------------------------------- */

    printf("\r\nOutputs (%u):\r\n",
           parser->program.output_count);

    for (i = 0;
         i < parser->program.output_count;
         i++)
    {
        printf("  %u: %s\r\n",
               i,
               parser->program.outputs[i].name);
    }


    /* --------------------------------------------------------
     * Assignments
     * -------------------------------------------------------- */

    printf("\r\nAssignments (%u):\r\n",
           parser->program.assignment_count);

    for (i = 0;
         i < parser->program.assignment_count;
         i++)
    {
        printf("\r\n  %u: %s =\r\n",
               i,
               parser->program.assignments[i].name);

        Parser_PrintASTNode(
            parser->program.assignments[i].expression,
            2);
    }


    printf("\r\n=============================================\r\n");
}

