/*
 * verishot_semantic.c
 *
 *  Created on: 22-Sept-2026
 *      Author: F.R.I.D.A.Y
 */
/*
 * verishot_semantic.c
 *
 * Semantic analyzer for the VeriShot language.
 *
 * The semantic analyzer operates on the parsed program produced
 * by verishot_parser and checks whether the design is logically
 * valid before configuration generation.
 */

#include "verishot_semantic.h"

#include <stdio.h>
#include <string.h>


/* ============================================================
 * Internal Function Prototypes
 * ============================================================ */

static int Semantic_AddSymbol(
    SemanticAnalyzer *analyzer,
    const char *name,
    SemanticSignalType type
);

static int Semantic_SymbolExists(
    const SemanticAnalyzer *analyzer,
    const char *name
);

static int Semantic_IsInput(
    const SemanticAnalyzer *analyzer,
    const char *name
);

static int Semantic_IsOutput(
    const SemanticAnalyzer *analyzer,
    const char *name
);

static int Semantic_IsAssigned(
    const Parser *parser,
    const char *name
);

static int Semantic_CheckExpression(
    SemanticAnalyzer *analyzer,
    const ASTNode *node
);


/* ============================================================
 * Semantic Analyzer Initialization
 * ============================================================ */

void Semantic_Init(SemanticAnalyzer *analyzer)
{
    if (analyzer == NULL)
    {
        return;
    }

    analyzer->symbol_count = 0;
    analyzer->status = SEMANTIC_OK;
}


/* ============================================================
 * Find Symbol
 * ============================================================ */

const SemanticSymbol *Semantic_FindSymbol(
    const SemanticAnalyzer *analyzer,
    const char *name)
{
    uint16_t i;

    if (analyzer == NULL || name == NULL)
    {
        return NULL;
    }

    for (i = 0; i < analyzer->symbol_count; i++)
    {
        if (strcmp(analyzer->symbols[i].name, name) == 0)
        {
            return &analyzer->symbols[i];
        }
    }

    return NULL;
}


/* ============================================================
 * Check Whether Symbol Exists
 * ============================================================ */

static int Semantic_SymbolExists(
    const SemanticAnalyzer *analyzer,
    const char *name)
{
    return (Semantic_FindSymbol(analyzer, name) != NULL);
}


/* ============================================================
 * Check Whether Symbol Is Input
 * ============================================================ */

static int Semantic_IsInput(
    const SemanticAnalyzer *analyzer,
    const char *name)
{
    const SemanticSymbol *symbol;

    symbol = Semantic_FindSymbol(analyzer, name);

    if (symbol == NULL)
    {
        return 0;
    }

    return (symbol->type == SEMANTIC_SIGNAL_INPUT);
}


/* ============================================================
 * Check Whether Symbol Is Output
 * ============================================================ */

static int Semantic_IsOutput(
    const SemanticAnalyzer *analyzer,
    const char *name)
{
    const SemanticSymbol *symbol;

    symbol = Semantic_FindSymbol(analyzer, name);

    if (symbol == NULL)
    {
        return 0;
    }

    return (symbol->type == SEMANTIC_SIGNAL_OUTPUT);
}


/* ============================================================
 * Add Symbol
 * ============================================================ */

static int Semantic_AddSymbol(
    SemanticAnalyzer *analyzer,
    const char *name,
    SemanticSignalType type)
{
    SemanticSymbol *symbol;

    if (analyzer == NULL || name == NULL)
    {
        return 0;
    }

    /*
     * Check for duplicate declaration or
     * input/output name conflict.
     */
    if (Semantic_SymbolExists(analyzer, name))
    {
        analyzer->status = SEMANTIC_DUPLICATE_SIGNAL;
        return 0;
    }

    /*
     * Check symbol table capacity.
     */
    if (analyzer->symbol_count >= VERISHOT_MAX_SYMBOLS)
    {
        analyzer->status = SEMANTIC_SYMBOL_TABLE_OVERFLOW;
        return 0;
    }

    symbol = &analyzer->symbols[analyzer->symbol_count];

    strncpy(
        symbol->name,
        name,
        VERISHOT_MAX_LEXEME_LENGTH - 1
    );

    symbol->name[
        VERISHOT_MAX_LEXEME_LENGTH - 1
    ] = '\0';

    symbol->type = type;

    analyzer->symbol_count++;

    return 1;
}


/* ============================================================
 * Check Whether Assignment Exists
 * ============================================================ */

static int Semantic_IsAssigned(
    const Parser *parser,
    const char *name)
{
    uint16_t i;

    if (parser == NULL || name == NULL)
    {
        return 0;
    }

    for (i = 0;
         i < parser->program.assignment_count;
         i++)
    {
        if (strcmp(
                parser->program.assignments[i].name,
                name) == 0)
        {
            return 1;
        }
    }

    return 0;
}


/* ============================================================
 * Check Expression
 *
 * Recursively walks the expression AST.
 *
 * Identifier:
 *     Must exist in the symbol table.
 *
 * Constant:
 *     Always valid.
 *
 * NOT:
 *     Check left child.
 *
 * AND/XOR/OR:
 *     Check both children.
 * ============================================================ */

static int Semantic_CheckExpression(
    SemanticAnalyzer *analyzer,
    const ASTNode *node)
{
    if (analyzer == NULL || node == NULL)
    {
        if (analyzer != NULL)
        {
            analyzer->status = SEMANTIC_ERROR;
        }

        return 0;
    }

    /* --------------------------------------------------------
     * Identifier
     * -------------------------------------------------------- */

    if (node->type == AST_IDENTIFIER)
    {
        if (!Semantic_SymbolExists(
                analyzer,
                node->value))
        {
            analyzer->status =
                SEMANTIC_UNDECLARED_IDENTIFIER;

            return 0;
        }

        return 1;
    }


    /* --------------------------------------------------------
     * Constant
     * -------------------------------------------------------- */

    if (node->type == AST_CONSTANT)
    {
        return 1;
    }


    /* --------------------------------------------------------
     * NOT
     * -------------------------------------------------------- */

    if (node->type == AST_NOT)
    {
        if (node->left == NULL)
        {
            analyzer->status = SEMANTIC_ERROR;
            return 0;
        }

        return Semantic_CheckExpression(
            analyzer,
            node->left
        );
    }


    /* --------------------------------------------------------
     * Binary Operators
     * -------------------------------------------------------- */

    if (node->type == AST_AND ||
        node->type == AST_XOR ||
        node->type == AST_OR)
    {
        if (node->left == NULL ||
            node->right == NULL)
        {
            analyzer->status = SEMANTIC_ERROR;
            return 0;
        }

        if (!Semantic_CheckExpression(
                analyzer,
                node->left))
        {
            return 0;
        }

        if (!Semantic_CheckExpression(
                analyzer,
                node->right))
        {
            return 0;
        }

        return 1;
    }


    /* --------------------------------------------------------
     * Unknown AST node
     * -------------------------------------------------------- */

    analyzer->status = SEMANTIC_ERROR;

    return 0;
}


/* ============================================================
 * Main Semantic Analysis
 * ============================================================ */

SemanticStatus Semantic_Analyze(
    SemanticAnalyzer *analyzer,
    const Parser *parser)
{
    uint16_t i;
    const char *name;

    if (analyzer == NULL || parser == NULL)
    {
        if (analyzer != NULL)
        {
            analyzer->status = SEMANTIC_ERROR;
        }

        return SEMANTIC_ERROR;
    }

    /*
     * Start from a clean analyzer.
     */
    Semantic_Init(analyzer);


    /* ========================================================
     * Parser Validation
     * ======================================================== */

    /*
     * Semantic analysis should only operate on a
     * successfully parsed program.
     */

    if (parser->status != PARSER_OK)
    {
        analyzer->status = SEMANTIC_ERROR;
        return analyzer->status;
    }


    /* ========================================================
     * Build Symbol Table - Inputs
     * ======================================================== */

    for (i = 0;
         i < parser->program.input_count;
         i++)
    {
        name = parser->program.inputs[i].name;

        if (!Semantic_AddSymbol(
                analyzer,
                name,
                SEMANTIC_SIGNAL_INPUT))
        {
            return analyzer->status;
        }
    }


    /* ========================================================
     * Build Symbol Table - Outputs
     * ======================================================== */

    for (i = 0;
         i < parser->program.output_count;
         i++)
    {
        name = parser->program.outputs[i].name;

        if (!Semantic_AddSymbol(
                analyzer,
                name,
                SEMANTIC_SIGNAL_OUTPUT))
        {
            return analyzer->status;
        }
    }


    /* ========================================================
     * Check Assignments
     * ======================================================== */

    for (i = 0;
         i < parser->program.assignment_count;
         i++)
    {
        const char *assignment_name;
        const ASTNode *expression;

        assignment_name =
            parser->program.assignments[i].name;

        expression =
            parser->program.assignments[i].expression;


        /* ----------------------------------------------------
         * Assignment target must exist
         * ---------------------------------------------------- */

        if (!Semantic_SymbolExists(
                analyzer,
                assignment_name))
        {
            analyzer->status =
                SEMANTIC_UNDECLARED_IDENTIFIER;

            return analyzer->status;
        }


        /* ----------------------------------------------------
         * Assignment target must be an output
         * ---------------------------------------------------- */

        if (!Semantic_IsOutput(
                analyzer,
                assignment_name))
        {
            analyzer->status =
                SEMANTIC_INVALID_ASSIGNMENT_TARGET;

            return analyzer->status;
        }


        /* ----------------------------------------------------
         * Check duplicate assignments
         * ---------------------------------------------------- */

        {
            uint16_t j;

            for (j = 0; j < i; j++)
            {
                if (strcmp(
                        parser->program.assignments[j].name,
                        assignment_name) == 0)
                {
                    analyzer->status =
                        SEMANTIC_DUPLICATE_ASSIGNMENT;

                    return analyzer->status;
                }
            }
        }


        /* ----------------------------------------------------
         * Check RHS expression
         * ---------------------------------------------------- */

        if (!Semantic_CheckExpression(
                analyzer,
                expression))
        {
            return analyzer->status;
        }
    }


    /* ========================================================
     * Check Every Output Is Assigned
     * ======================================================== */

    for (i = 0;
         i < parser->program.output_count;
         i++)
    {
        name = parser->program.outputs[i].name;

        if (!Semantic_IsAssigned(
                parser,
                name))
        {
            analyzer->status =
                SEMANTIC_OUTPUT_NOT_ASSIGNED;

            return analyzer->status;
        }
    }


    /* ========================================================
     * Semantic Analysis Successful
     * ======================================================== */

    analyzer->status = SEMANTIC_OK;

    return analyzer->status;
}


/* ============================================================
 * Status To String
 * ============================================================ */

const char *Semantic_StatusToString(
    SemanticStatus status)
{
    switch (status)
    {
        case SEMANTIC_OK:
            return "SEMANTIC_OK";

        case SEMANTIC_ERROR:
            return "SEMANTIC_ERROR";

        case SEMANTIC_DUPLICATE_SIGNAL:
            return "SEMANTIC_DUPLICATE_SIGNAL";

        case SEMANTIC_UNDECLARED_IDENTIFIER:
            return "SEMANTIC_UNDECLARED_IDENTIFIER";

        case SEMANTIC_INVALID_ASSIGNMENT_TARGET:
            return "SEMANTIC_INVALID_ASSIGNMENT_TARGET";

        case SEMANTIC_DUPLICATE_ASSIGNMENT:
            return "SEMANTIC_DUPLICATE_ASSIGNMENT";

        case SEMANTIC_OUTPUT_NOT_ASSIGNED:
            return "SEMANTIC_OUTPUT_NOT_ASSIGNED";

        case SEMANTIC_SYMBOL_TABLE_OVERFLOW:
            return "SEMANTIC_SYMBOL_TABLE_OVERFLOW";

        default:
            return "UNKNOWN_SEMANTIC_STATUS";
    }
}


/* ============================================================
 * Print Symbol Table
 * ============================================================ */

void Semantic_PrintSymbols(
    const SemanticAnalyzer *analyzer)
{
    uint16_t i;

    if (analyzer == NULL)
    {
        return;
    }

    printf("\r\n");
    printf("========== VERISHOT SYMBOL TABLE ==========\r\n");

    printf("\r\nSymbols (%u):\r\n",
           analyzer->symbol_count);

    for (i = 0;
         i < analyzer->symbol_count;
         i++)
    {
        printf(
            "  %u: %s -> %s\r\n",
            i,
            analyzer->symbols[i].name,
            (analyzer->symbols[i].type ==
             SEMANTIC_SIGNAL_INPUT)
                ? "INPUT"
                : "OUTPUT"
        );
    }

    printf(
        "\r\nSemantic Status: %s\r\n",
        Semantic_StatusToString(
            analyzer->status)
    );

    printf(
        "===========================================\r\n"
    );
}

