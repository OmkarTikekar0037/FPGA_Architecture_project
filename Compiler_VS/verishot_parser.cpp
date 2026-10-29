#include "verishot_parser.h"
#include <Arduino.h>
#include <string.h>

/* ============================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================ */

static Token *Parser_CurrentToken(Parser *parser)
{
    if (parser == nullptr)
        return nullptr;

    if (parser->position >= parser->token_count)
        return nullptr;

    return &parser->tokens[parser->position];
}


static Token *Parser_PeekToken(Parser *parser)
{
    if (parser == nullptr)
        return nullptr;

    if ((parser->position + 1) >= parser->token_count)
        return nullptr;

    return &parser->tokens[parser->position + 1];
}


static void Parser_Advance(Parser *parser)
{
    if (parser == nullptr)
        return;

    if (parser->position < parser->token_count)
        parser->position++;
}


static bool Parser_Match(Parser *parser, TokenType type)
{
    Token *token = Parser_CurrentToken(parser);

    if (token == nullptr)
        return false;

    if (token->type == type)
    {
        Parser_Advance(parser);
        return true;
    }

    return false;
}


static bool Parser_Expect(Parser *parser, TokenType type)
{
    Token *token = Parser_CurrentToken(parser);

    if (token == nullptr)
    {
        parser->status = PARSER_UNEXPECTED_EOF;
        return false;
    }

    if (token->type != type)
    {
        parser->status = PARSER_UNEXPECTED_TOKEN;
        return false;
    }

    Parser_Advance(parser);
    return true;
}


/* ============================================================
 * AST NODE CREATION
 * ============================================================ */

static ASTNode *Parser_CreateASTNode(Parser *parser,
                                     ASTNodeType type,
                                     const char *value,
                                     ASTNode *left,
                                     ASTNode *right)
{
    if (parser == nullptr)
        return nullptr;

    if (parser->ast_node_count >= VERISHOT_MAX_AST_NODES)
    {
        parser->status = PARSER_AST_OVERFLOW;
        return nullptr;
    }

    ASTNode *node = &parser->ast_nodes[parser->ast_node_count++];

    node->type = type;

    node->value[0] = '\0';

    if (value != nullptr)
    {
        strncpy(node->value,
                value,
                VERISHOT_MAX_LEXEME_LENGTH - 1);

        node->value[VERISHOT_MAX_LEXEME_LENGTH - 1] = '\0';
    }

    node->left = left;
    node->right = right;

    return node;
}


static ASTNode *Parser_CreateIdentifier(Parser *parser,
                                        const char *name)
{
    return Parser_CreateASTNode(parser,
                                AST_IDENTIFIER,
                                name,
                                nullptr,
                                nullptr);
}


static ASTNode *Parser_CreateConstant(Parser *parser,
                                      const char *value)
{
    return Parser_CreateASTNode(parser,
                                AST_CONSTANT,
                                value,
                                nullptr,
                                nullptr);
}


/* ============================================================
 * PRIMARY EXPRESSION
 *
 * primary:
 *      identifier
 *      constant
 *      '(' expression ')'
 * ============================================================ */

static ASTNode *Parser_ParsePrimary(Parser *parser)
{
    Token *token = Parser_CurrentToken(parser);

    if (token == nullptr)
    {
        parser->status = PARSER_UNEXPECTED_EOF;
        return nullptr;
    }

    /* Identifier */
    if (token->type == TOKEN_IDENTIFIER)
    {
        ASTNode *node = Parser_CreateIdentifier(parser,
                                                token->lexeme);

        if (node == nullptr)
            return nullptr;

        Parser_Advance(parser);

        return node;
    }

    /* Constant */
    if (token->type == TOKEN_CONSTANT)
    {
        ASTNode *node = Parser_CreateConstant(parser,
                                              token->lexeme);

        if (node == nullptr)
            return nullptr;

        Parser_Advance(parser);

        return node;
    }

    /* Parenthesized expression */
    if (token->type == TOKEN_LPAREN)
    {
        Parser_Advance(parser);

        ASTNode *node = nullptr;

        node = Parser_ParsePrimary(parser);

        /*
         * The original grammar allows an expression inside
         * parentheses, so call the full expression parser here.
         */
        if (node != nullptr)
        {
            /*
             * Rewind the expression handling by parsing through
             * the expression parser instead.
             *
             * This section is replaced below by the proper
             * forward declaration mechanism.
             */
        }

        parser->status = PARSER_EXPECTED_EXPRESSION;
        return nullptr;
    }

    parser->status = PARSER_EXPECTED_EXPRESSION;
    return nullptr;
}


/* ============================================================
 * Forward declaration
 * ============================================================ */

static ASTNode *Parser_ParseExpression(Parser *parser);


/* ============================================================
 * PRIMARY EXPRESSION
 *
 * Re-implemented here after expression forward declaration.
 * ============================================================ */

static ASTNode *Parser_ParsePrimaryExpression(Parser *parser)
{
    Token *token = Parser_CurrentToken(parser);

    if (token == nullptr)
    {
        parser->status = PARSER_UNEXPECTED_EOF;
        return nullptr;
    }

    /* Identifier */
    if (token->type == TOKEN_IDENTIFIER)
    {
        ASTNode *node = Parser_CreateIdentifier(parser,
                                                token->lexeme);

        if (node == nullptr)
            return nullptr;

        Parser_Advance(parser);

        return node;
    }

    /* Constant */
    if (token->type == TOKEN_CONSTANT)
    {
        ASTNode *node = Parser_CreateConstant(parser,
                                              token->lexeme);

        if (node == nullptr)
            return nullptr;

        Parser_Advance(parser);

        return node;
    }

    /* Parenthesized expression */
    if (token->type == TOKEN_LPAREN)
    {
        Parser_Advance(parser);

        ASTNode *node = Parser_ParseExpression(parser);

        if (node == nullptr)
            return nullptr;

        if (!Parser_Expect(parser, TOKEN_RPAREN))
        {
            parser->status = PARSER_EXPECTED_EXPRESSION;
            return nullptr;
        }

        return node;
    }

    parser->status = PARSER_EXPECTED_EXPRESSION;
    return nullptr;
}


/* ============================================================
 * UNARY
 *
 * unary:
 *      '~' unary
 *      primary
 * ============================================================ */

static ASTNode *Parser_ParseUnary(Parser *parser)
{
    Token *token = Parser_CurrentToken(parser);

    if (token == nullptr)
    {
        parser->status = PARSER_UNEXPECTED_EOF;
        return nullptr;
    }

    if (Parser_Match(parser, TOKEN_NOT))
    {
        ASTNode *operand = Parser_ParseUnary(parser);

        if (operand == nullptr)
            return nullptr;

        return Parser_CreateASTNode(parser,
                                    AST_NOT,
                                    "~",
                                    operand,
                                    nullptr);
    }

    return Parser_ParsePrimaryExpression(parser);
}


/* ============================================================
 * AND
 *
 * AND has higher precedence than XOR and OR.
 *
 * and:
 *      unary ('&' unary)*
 * ============================================================ */

static ASTNode *Parser_ParseAnd(Parser *parser)
{
    ASTNode *left = Parser_ParseUnary(parser);

    if (left == nullptr)
        return nullptr;

    while (Parser_Match(parser, TOKEN_AND))
    {
        ASTNode *right = Parser_ParseUnary(parser);

        if (right == nullptr)
            return nullptr;

        left = Parser_CreateASTNode(parser,
                                    AST_AND,
                                    "&",
                                    left,
                                    right);

        if (left == nullptr)
            return nullptr;
    }

    return left;
}


/* ============================================================
 * XOR
 *
 * xor:
 *      and ('^' and)*
 * ============================================================ */

static ASTNode *Parser_ParseXor(Parser *parser)
{
    ASTNode *left = Parser_ParseAnd(parser);

    if (left == nullptr)
        return nullptr;

    while (Parser_Match(parser, TOKEN_XOR))
    {
        ASTNode *right = Parser_ParseAnd(parser);

        if (right == nullptr)
            return nullptr;

        left = Parser_CreateASTNode(parser,
                                    AST_XOR,
                                    "^",
                                    left,
                                    right);

        if (left == nullptr)
            return nullptr;
    }

    return left;
}


/* ============================================================
 * OR
 *
 * or:
 *      xor ('|' xor)*
 * ============================================================ */

static ASTNode *Parser_ParseOr(Parser *parser)
{
    ASTNode *left = Parser_ParseXor(parser);

    if (left == nullptr)
        return nullptr;

    while (Parser_Match(parser, TOKEN_OR))
    {
        ASTNode *right = Parser_ParseXor(parser);

        if (right == nullptr)
            return nullptr;

        left = Parser_CreateASTNode(parser,
                                    AST_OR,
                                    "|",
                                    left,
                                    right);

        if (left == nullptr)
            return nullptr;
    }

    return left;
}


/* ============================================================
 * EXPRESSION
 * ============================================================ */

static ASTNode *Parser_ParseExpression(Parser *parser)
{
    return Parser_ParseOr(parser);
}


/* ============================================================
 * PROGRAM ARRAY HELPERS
 * ============================================================ */

static bool Parser_AddInput(Parser *parser,
                            const char *name)
{
    if (parser->program.input_count >= VERISHOT_MAX_SIGNALS)
    {
        parser->status = PARSER_ERROR;
        return false;
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

    return true;
}


static bool Parser_AddOutput(Parser *parser,
                             const char *name)
{
    if (parser->program.output_count >= VERISHOT_MAX_SIGNALS)
    {
        parser->status = PARSER_ERROR;
        return false;
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

    return true;
}


static bool Parser_AddAssignment(Parser *parser,
                                 const char *name,
                                 ASTNode *expression)
{
    if (parser->program.assignment_count >=
        VERISHOT_MAX_ASSIGNMENTS)
    {
        parser->status = PARSER_ERROR;
        return false;
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

    return true;
}


/* ============================================================
 * INPUT DECLARATION
 *
 * input A, B, C;
 * ============================================================ */

static bool Parser_ParseInputDeclaration(Parser *parser)
{
    if (!Parser_Expect(parser, TOKEN_INPUT))
        return false;

    Token *token = Parser_CurrentToken(parser);

    if (token == nullptr)
    {
        parser->status = PARSER_UNEXPECTED_EOF;
        return false;
    }

    if (token->type != TOKEN_IDENTIFIER)
    {
        parser->status = PARSER_EXPECTED_IDENTIFIER;
        return false;
    }

    if (!Parser_AddInput(parser, token->lexeme))
        return false;

    Parser_Advance(parser);

    while (Parser_Match(parser, TOKEN_COMMA))
    {
        token = Parser_CurrentToken(parser);

        if (token == nullptr)
        {
            parser->status = PARSER_UNEXPECTED_EOF;
            return false;
        }

        if (token->type != TOKEN_IDENTIFIER)
        {
            parser->status = PARSER_EXPECTED_IDENTIFIER;
            return false;
        }

        if (!Parser_AddInput(parser, token->lexeme))
            return false;

        Parser_Advance(parser);
    }

    if (!Parser_Expect(parser, TOKEN_SEMICOLON))
    {
        parser->status = PARSER_EXPECTED_SEMICOLON;
        return false;
    }

    return true;
}


/* ============================================================
 * OUTPUT DECLARATION
 *
 * output Y, Z;
 * ============================================================ */

static bool Parser_ParseOutputDeclaration(Parser *parser)
{
    if (!Parser_Expect(parser, TOKEN_OUTPUT))
        return false;

    Token *token = Parser_CurrentToken(parser);

    if (token == nullptr)
    {
        parser->status = PARSER_UNEXPECTED_EOF;
        return false;
    }

    if (token->type != TOKEN_IDENTIFIER)
    {
        parser->status = PARSER_EXPECTED_IDENTIFIER;
        return false;
    }

    if (!Parser_AddOutput(parser, token->lexeme))
        return false;

    Parser_Advance(parser);

    while (Parser_Match(parser, TOKEN_COMMA))
    {
        token = Parser_CurrentToken(parser);

        if (token == nullptr)
        {
            parser->status = PARSER_UNEXPECTED_EOF;
            return false;
        }

        if (token->type != TOKEN_IDENTIFIER)
        {
            parser->status = PARSER_EXPECTED_IDENTIFIER;
            return false;
        }

        if (!Parser_AddOutput(parser, token->lexeme))
            return false;

        Parser_Advance(parser);
    }

    if (!Parser_Expect(parser, TOKEN_SEMICOLON))
    {
        parser->status = PARSER_EXPECTED_SEMICOLON;
        return false;
    }

    return true;
}


/* ============================================================
 * ASSIGNMENT
 *
 * assign Y = A & B;
 * ============================================================ */

static bool Parser_ParseAssignment(Parser *parser)
{
    if (!Parser_Expect(parser, TOKEN_ASSIGN))
        return false;

    Token *token = Parser_CurrentToken(parser);

    if (token == nullptr)
    {
        parser->status = PARSER_UNEXPECTED_EOF;
        return false;
    }

    if (token->type != TOKEN_IDENTIFIER)
    {
        parser->status = PARSER_EXPECTED_IDENTIFIER;
        return false;
    }

    char assignment_name[VERISHOT_MAX_LEXEME_LENGTH];

    strncpy(assignment_name,
            token->lexeme,
            VERISHOT_MAX_LEXEME_LENGTH - 1);

    assignment_name[
        VERISHOT_MAX_LEXEME_LENGTH - 1
    ] = '\0';

    Parser_Advance(parser);

    if (!Parser_Expect(parser, TOKEN_EQUAL))
    {
        parser->status = PARSER_EXPECTED_EQUAL;
        return false;
    }

    ASTNode *expression = Parser_ParseExpression(parser);

    if (expression == nullptr)
    {
        parser->status = PARSER_EXPECTED_EXPRESSION;
        return false;
    }

    if (!Parser_Expect(parser, TOKEN_SEMICOLON))
    {
        parser->status = PARSER_EXPECTED_SEMICOLON;
        return false;
    }

    if (!Parser_AddAssignment(parser,
                              assignment_name,
                              expression))
    {
        return false;
    }

    return true;
}


/* ============================================================
 * INITIALIZATION
 * ============================================================ */

void Parser_Init(Parser *parser,
                 Token *tokens,
                 uint16_t token_count)
{
    if (parser == nullptr)
        return;

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
 * MAIN PARSER
 *
 * program:
 *      (input | output | assign)* EOF
 * ============================================================ */

ParserStatus Parser_Parse(Parser *parser)
{
    if (parser == nullptr)
        return PARSER_ERROR;

    parser->status = PARSER_OK;

    while (parser->position < parser->token_count)
    {
        Token *token = Parser_CurrentToken(parser);

        if (token == nullptr)
        {
            parser->status = PARSER_UNEXPECTED_EOF;
            return parser->status;
        }

        switch (token->type)
        {
            case TOKEN_INPUT:

                if (!Parser_ParseInputDeclaration(parser))
                    return parser->status;

                break;


            case TOKEN_OUTPUT:

                if (!Parser_ParseOutputDeclaration(parser))
                    return parser->status;

                break;


            case TOKEN_ASSIGN:

                if (!Parser_ParseAssignment(parser))
                    return parser->status;

                break;


            case TOKEN_EOF:

                return PARSER_OK;


            default:

                parser->status = PARSER_UNEXPECTED_TOKEN;
                return parser->status;
        }
    }

    parser->status = PARSER_UNEXPECTED_EOF;

    return parser->status;
}


/* ============================================================
 * STATUS TO STRING
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
 * AST NODE TYPE TO STRING
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
            return "UNKNOWN";
    }
}


/* ============================================================
 * AST PRINTING
 * ============================================================ */

static void Parser_PrintASTNode(const ASTNode *node,
                                uint8_t depth)
{
    if (node == nullptr)
        return;

    for (uint8_t i = 0; i < depth; i++)
        Serial.print("  ");

    Serial.print(Parser_ASTNodeTypeToString(node->type));

    if (node->type == AST_IDENTIFIER ||
        node->type == AST_CONSTANT)
    {
        Serial.print(" : ");
        Serial.println(node->value);
    }
    else
    {
        Serial.println();

        Parser_PrintASTNode(node->left,
                            depth + 1);

        Parser_PrintASTNode(node->right,
                            depth + 1);
    }
}


void Parser_PrintAST(const Parser *parser)
{
    if (parser == nullptr)
        return;

    Serial.println();
    Serial.println(F("========== PARSER RESULT =========="));

    Serial.print(F("Inputs: "));
    Serial.println(parser->program.input_count);

    for (uint16_t i = 0;
         i < parser->program.input_count;
         i++)
    {
        Serial.print(F("  INPUT: "));
        Serial.println(parser->program.inputs[i].name);
    }

    Serial.print(F("Outputs: "));
    Serial.println(parser->program.output_count);

    for (uint16_t i = 0;
         i < parser->program.output_count;
         i++)
    {
        Serial.print(F("  OUTPUT: "));
        Serial.println(parser->program.outputs[i].name);
    }

    Serial.print(F("Assignments: "));
    Serial.println(parser->program.assignment_count);

    for (uint16_t i = 0;
         i < parser->program.assignment_count;
         i++)
    {
        Serial.println();

        Serial.print(F("  ASSIGN "));
        Serial.print(parser->program.assignments[i].name);
        Serial.println(F(":"));

        Parser_PrintASTNode(
            parser->program.assignments[i].expression,
            2
        );
    }

    Serial.println(F("==================================="));
}