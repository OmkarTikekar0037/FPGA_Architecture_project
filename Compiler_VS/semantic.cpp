/*
 * verishot_semantic.cpp
 *
 * Semantic analyzer for the VeriShot language.
 *
 * Performs semantic validation on the program generated
 * by the VeriShot parser.
 */

#include "semantic.h"
#include <Arduino.h>
#include <string.h>


/* ============================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================ */

/*
 * Add a symbol to the semantic symbol table.
 */
static bool Semantic_AddSymbol(
    SemanticAnalyzer *analyzer,
    const char *name,
    SemanticSignalType type)
{
    if (analyzer == nullptr || name == nullptr)
        return false;

    /*
     * Check symbol-table capacity.
     */
    if (analyzer->symbol_count >= VERISHOT_MAX_SYMBOLS)
    {
        analyzer->status = SEMANTIC_SYMBOL_TABLE_OVERFLOW;
        return false;
    }

    /*
     * Check whether the symbol already exists.
     *
     * This catches:
     *
     * input A;
     * input A;
     *
     * as well as:
     *
     * input A;
     * output A;
     */
    if (Semantic_FindSymbol(analyzer, name) != nullptr)
    {
        analyzer->status = SEMANTIC_DUPLICATE_SIGNAL;
        return false;
    }

    SemanticSymbol *symbol =
        &analyzer->symbols[analyzer->symbol_count];

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

    return true;
}


/*
 * Recursively validate an AST expression.
 *
 * Every identifier appearing in an expression must already
 * exist in the semantic symbol table.
 */
static bool Semantic_ValidateExpression(
    SemanticAnalyzer *analyzer,
    const ASTNode *node)
{
    if (analyzer == nullptr || node == nullptr)
        return false;

    switch (node->type)
    {
        /*
         * Identifier:
         *
         * A and B in:
         *
         * assign Y = A & B;
         *
         * must have been declared.
         */
        case AST_IDENTIFIER:
        {
            if (Semantic_FindSymbol(
                    analyzer,
                    node->value) == nullptr)
            {
                analyzer->status =
                    SEMANTIC_UNDECLARED_IDENTIFIER;

                return false;
            }

            return true;
        }


        /*
         * Constant:
         *
         * 0 or 1
         *
         * The lexer already guarantees that constants are
         * syntactically valid.
         */
        case AST_CONSTANT:

            return true;


        /*
         * NOT:
         *
         *        NOT
         *         |
         *         A
         */
        case AST_NOT:

            return Semantic_ValidateExpression(
                analyzer,
                node->left
            );


        /*
         * Binary operators:
         *
         *       AND / OR / XOR
         *          /      \
         *         A        B
         */
        case AST_AND:
        case AST_OR:
        case AST_XOR:

            if (!Semantic_ValidateExpression(
                    analyzer,
                    node->left))
            {
                return false;
            }

            if (!Semantic_ValidateExpression(
                    analyzer,
                    node->right))
            {
                return false;
            }

            return true;


        default:

            analyzer->status = SEMANTIC_ERROR;
            return false;
    }
}


/*
 * Check whether an assignment exists for a particular
 * output signal.
 */
static bool Semantic_HasAssignment(
    const Parser *parser,
    const char *name)
{
    if (parser == nullptr || name == nullptr)
        return false;

    for (uint16_t i = 0;
         i < parser->program.assignment_count;
         i++)
    {
        if (strcmp(
                parser->program.assignments[i].name,
                name) == 0)
        {
            return true;
        }
    }

    return false;
}


/*
 * Check for duplicate assignments.
 *
 * Example:
 *
 * assign Y = A;
 * assign Y = B;
 *
 * is invalid.
 */
static bool Semantic_CheckDuplicateAssignments(
    SemanticAnalyzer *analyzer,
    const Parser *parser)
{
    for (uint16_t i = 0;
         i < parser->program.assignment_count;
         i++)
    {
        for (uint16_t j = i + 1;
             j < parser->program.assignment_count;
             j++)
        {
            if (strcmp(
                    parser->program.assignments[i].name,
                    parser->program.assignments[j].name) == 0)
            {
                analyzer->status =
                    SEMANTIC_DUPLICATE_ASSIGNMENT;

                return false;
            }
        }
    }

    return true;
}


/*
 * Check whether every assignment target is a declared output.
 */
static bool Semantic_CheckAssignmentTargets(
    SemanticAnalyzer *analyzer,
    const Parser *parser)
{
    for (uint16_t i = 0;
         i < parser->program.assignment_count;
         i++)
    {
        const char *assignment_name =
            parser->program.assignments[i].name;

        const SemanticSymbol *symbol =
            Semantic_FindSymbol(
                analyzer,
                assignment_name
            );

        /*
         * Assignment target doesn't exist at all.
         */
        if (symbol == nullptr)
        {
            analyzer->status =
                SEMANTIC_INVALID_ASSIGNMENT_TARGET;

            return false;
        }

        /*
         * Assignment target exists, but is an input.
         */
        if (symbol->type != SEMANTIC_SIGNAL_OUTPUT)
        {
            analyzer->status =
                SEMANTIC_INVALID_ASSIGNMENT_TARGET;

            return false;
        }
    }

    return true;
}


/*
 * Every declared output must have exactly one assignment.
 */
static bool Semantic_CheckOutputsAssigned(
    SemanticAnalyzer *analyzer,
    const Parser *parser)
{
    for (uint16_t i = 0;
         i < parser->program.output_count;
         i++)
    {
        const char *output_name =
            parser->program.outputs[i].name;

        if (!Semantic_HasAssignment(
                parser,
                output_name))
        {
            analyzer->status =
                SEMANTIC_OUTPUT_NOT_ASSIGNED;

            return false;
        }
    }

    return true;
}


/* ============================================================
 * INITIALIZATION
 * ============================================================ */

void Semantic_Init(
    SemanticAnalyzer *analyzer)
{
    if (analyzer == nullptr)
        return;

    analyzer->symbol_count = 0;
    analyzer->status = SEMANTIC_OK;

    /*
     * Clear the symbol table.
     */
    for (uint16_t i = 0;
         i < VERISHOT_MAX_SYMBOLS;
         i++)
    {
        analyzer->symbols[i].name[0] = '\0';
        analyzer->symbols[i].type =
            SEMANTIC_SIGNAL_INPUT;
    }
}


/* ============================================================
 * SYMBOL TABLE SEARCH
 * ============================================================ */

const SemanticSymbol *Semantic_FindSymbol(
    const SemanticAnalyzer *analyzer,
    const char *name)
{
    if (analyzer == nullptr || name == nullptr)
        return nullptr;

    for (uint16_t i = 0;
         i < analyzer->symbol_count;
         i++)
    {
        if (strcmp(
                analyzer->symbols[i].name,
                name) == 0)
        {
            return &analyzer->symbols[i];
        }
    }

    return nullptr;
}


/* ============================================================
 * MAIN SEMANTIC ANALYSIS
 * ============================================================ */

SemanticStatus Semantic_Analyze(
    SemanticAnalyzer *analyzer,
    const Parser *parser)
{
    if (analyzer == nullptr ||
        parser == nullptr)
    {
        return SEMANTIC_ERROR;
    }

    /*
     * Start with a clean semantic analyzer.
     */
    Semantic_Init(analyzer);

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
     * STEP 1
     *
     * Build symbol table from inputs.
     * ======================================================== */

    for (uint16_t i = 0;
         i < parser->program.input_count;
         i++)
    {
        if (!Semantic_AddSymbol(
                analyzer,
                parser->program.inputs[i].name,
                SEMANTIC_SIGNAL_INPUT))
        {
            return analyzer->status;
        }
    }


    /* ========================================================
     * STEP 2
     *
     * Add outputs to symbol table.
     *
     * This also catches input/output name collisions.
     * ======================================================== */

    for (uint16_t i = 0;
         i < parser->program.output_count;
         i++)
    {
        if (!Semantic_AddSymbol(
                analyzer,
                parser->program.outputs[i].name,
                SEMANTIC_SIGNAL_OUTPUT))
        {
            return analyzer->status;
        }
    }


    /* ========================================================
     * STEP 3
     *
     * Detect duplicate assignments.
     * ======================================================== */

    if (!Semantic_CheckDuplicateAssignments(
            analyzer,
            parser))
    {
        return analyzer->status;
    }


    /* ========================================================
     * STEP 4
     *
     * Verify that every assignment target is an output.
     * ======================================================== */

    if (!Semantic_CheckAssignmentTargets(
            analyzer,
            parser))
    {
        return analyzer->status;
    }


    /* ========================================================
     * STEP 5
     *
     * Validate every expression recursively.
     *
     * Example:
     *
     * assign Y = ~(A & B) ^ C;
     *
     * Every identifier in the AST must exist.
     * ======================================================== */

    for (uint16_t i = 0;
         i < parser->program.assignment_count;
         i++)
    {
        const ASTNode *expression =
            parser->program.assignments[i].expression;

        if (!Semantic_ValidateExpression(
                analyzer,
                expression))
        {
            return analyzer->status;
        }
    }


    /* ========================================================
     * STEP 6
     *
     * Every declared output must have an assignment.
     * ======================================================== */

    if (!Semantic_CheckOutputsAssigned(
            analyzer,
            parser))
    {
        return analyzer->status;
    }


    /* ========================================================
     * SUCCESS
     * ======================================================== */

    analyzer->status = SEMANTIC_OK;

    return SEMANTIC_OK;
}


/* ============================================================
 * STATUS TO STRING
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
 * DEBUG / DISPLAY
 * ============================================================ */

void Semantic_PrintSymbols(
    const SemanticAnalyzer *analyzer)
{
    if (analyzer == nullptr)
        return;

    Serial.println();
    Serial.println(F("======= SEMANTIC SYMBOL TABLE ======="));

    Serial.print(F("Symbol count: "));
    Serial.println(analyzer->symbol_count);

    for (uint16_t i = 0;
         i < analyzer->symbol_count;
         i++)
    {
        Serial.print(F("  "));

        Serial.print(analyzer->symbols[i].name);

        Serial.print(F(" : "));

        if (analyzer->symbols[i].type ==
            SEMANTIC_SIGNAL_INPUT)
        {
            Serial.println(F("INPUT"));
        }
        else
        {
            Serial.println(F("OUTPUT"));
        }
    }

    Serial.println(F("====================================="));
}