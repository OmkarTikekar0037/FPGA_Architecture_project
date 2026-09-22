/*
 * verishot_semantic.h
 *
 * Semantic analyzer for the VeriShot language.
 *
 * The semantic analyzer operates on the parsed program produced
 * by verishot_parser and checks whether the design is logically
 * valid before configuration generation.
 */

#ifndef VERISHOT_SEMANTIC_H
#define VERISHOT_SEMANTIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "verishot_parser.h"


/* ============================================================
 * Semantic Analyzer Limits
 * ============================================================ */

#define VERISHOT_MAX_SYMBOLS    VERISHOT_MAX_SIGNALS


/* ============================================================
 * Semantic Status
 * ============================================================ */

typedef enum
{
    SEMANTIC_OK = 0,

    SEMANTIC_ERROR,

    SEMANTIC_DUPLICATE_SIGNAL,

	SEMANTIC_UNDECLARED_IDENTIFIER,

    SEMANTIC_INVALID_ASSIGNMENT_TARGET,
    SEMANTIC_DUPLICATE_ASSIGNMENT,

    SEMANTIC_OUTPUT_NOT_ASSIGNED,

    SEMANTIC_SYMBOL_TABLE_OVERFLOW

} SemanticStatus;


/* ============================================================
 * Signal Type
 * ============================================================ */

typedef enum
{
    SEMANTIC_SIGNAL_INPUT = 0,
    SEMANTIC_SIGNAL_OUTPUT

} SemanticSignalType;


/* ============================================================
 * Symbol
 * ============================================================ */

typedef struct
{
    char name[VERISHOT_MAX_LEXEME_LENGTH];

    SemanticSignalType type;

} SemanticSymbol;


/* ============================================================
 * Semantic Analyzer
 * ============================================================ */

typedef struct
{
    SemanticSymbol symbols[VERISHOT_MAX_SYMBOLS];

    uint16_t symbol_count;

    SemanticStatus status;

} SemanticAnalyzer;


/* ============================================================
 * Initialization
 * ============================================================ */

/*
 * Initialize the semantic analyzer.
 *
 * analyzer:
 *     Pointer to semantic analyzer structure.
 */

void Semantic_Init(SemanticAnalyzer *analyzer);


/* ============================================================
 * Main Semantic Analysis
 * ============================================================ */

/*
 * Analyze a parsed VeriShot program.
 *
 * parser:
 *     Pointer to a successfully parsed VeriShot program.
 *
 * analyzer:
 *     Pointer to semantic analyzer.
 *
 * Returns:
 *     SEMANTIC_OK if the program is semantically valid.
 *     Appropriate SemanticStatus otherwise.
 */

SemanticStatus Semantic_Analyze(
    SemanticAnalyzer *analyzer,
    const Parser *parser
);


/* ============================================================
 * Status Conversion
 * ============================================================ */

/*
 * Convert a SemanticStatus value into a readable string.
 */

const char *Semantic_StatusToString(
    SemanticStatus status
);


/* ============================================================
 * Symbol Table Access
 * ============================================================ */

/*
 * Find a signal in the semantic symbol table.
 *
 * Returns:
 *     Pointer to the symbol if found.
 *     NULL if the signal does not exist.
 */

const SemanticSymbol *Semantic_FindSymbol(
    const SemanticAnalyzer *analyzer,
    const char *name
);


/* ============================================================
 * Debug / Display
 * ============================================================ */

/*
 * Print the semantic symbol table.
 *
 * Useful during development and testing.
 */

void Semantic_PrintSymbols(
    const SemanticAnalyzer *analyzer
);


#ifdef __cplusplus
}
#endif

#endif /* VERISHOT_SEMANTIC_H */
