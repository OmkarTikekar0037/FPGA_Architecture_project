
/*
 * verishot_mapper.cpp
 *
 * VeriShot V1 Mapper
 *
 * Converts the Parser AST / VerishotProgram into:
 *
 *     - LUT truth tables
 *     - LUT input source assignments
 *     - physical 2-bit routing selections
 *
 * V1 mapping strategy:
 *
 *     assignment 0 -> LUT0
 *     assignment 1 -> LUT1
 *     assignment 2 -> LUT2
 *     assignment 3 -> LUT3
 *
 * A LUT input can be:
 *
 *     - external signal
 *     - previously mapped LUT output
 *     - unused
 *
 * A LUT truth table is generated from the Boolean expression
 * using the LUT's three logical inputs.
 */

#include "verishot_mapper.h"

#include <Arduino.h>
#include <string.h>
#include <stdio.h>


/* ============================================================
 * INTERNAL FUNCTIONS
 * ============================================================ */

/*
 * Clear the entire mapper configuration.
 */
static void Mapper_ClearConfig(
    Mapper *mapper
)
{
    if (mapper == NULL)
        return;


    memset(
        &mapper->config,
        0,
        sizeof(FPGA_Config)
    );


    mapper->lut_count = 0;
    mapper->status = MAPPER_OK;


    /*
     * Store physical LUT indices explicitly.
     */
    for (uint8_t i = 0;
         i < VERISHOT_NUM_LUTS;
         i++)
    {
        mapper->config.luts[i].lut_index = i;
    }
}


/* ============================================================
 * RESOLVE IDENTIFIER
 * ============================================================
 *
 * Given an identifier from an AST:
 *
 *     A
 *     B
 *     Y
 *
 * determine whether it represents:
 *
 *     external input
 *
 * or:
 *
 *     previous LUT output
 *
 * V1 does NOT allow forward references.
 */

static MapperStatus Mapper_ResolveIdentifier(
    const Mapper *mapper,
    uint8_t current_lut,
    const char *name,
    LUTInputSource *source
)
{
    if (mapper == NULL ||
        mapper->parser == NULL ||
        name == NULL ||
        source == NULL)
    {
        return MAPPER_ERROR;
    }


    const Parser *parser =
        mapper->parser;


    /* --------------------------------------------------------
     * Search declared external inputs.
     * -------------------------------------------------------- */

    for (uint16_t i = 0;
         i < parser->program.input_count;
         i++)
    {
        if (strcmp(
                parser->program.inputs[i].name,
                name
            ) == 0)
        {
            source->type =
                LUT_SOURCE_EXTERNAL;

            source->lut_index = 0;


            strncpy(
                source->signal_name,
                name,
                VERISHOT_MAX_LEXEME_LENGTH - 1
            );


            source->signal_name[
                VERISHOT_MAX_LEXEME_LENGTH - 1
            ] = '\0';


            return MAPPER_OK;
        }
    }


    /* --------------------------------------------------------
     * Search previous assignments.
     *
     * Assignment index == LUT index.
     *
     * Therefore:
     *
     *     assignment 0 -> LUT0
     *     assignment 1 -> LUT1
     *     ...
     *
     * Only previous assignments may be used.
     * -------------------------------------------------------- */

    for (uint16_t i = 0;
         i < parser->program.assignment_count;
         i++)
    {
        /*
         * Assignments after current LUT are forward
         * references and cannot be used yet.
         */
        if (i >= current_lut)
            break;


        if (strcmp(
                parser->program.assignments[i].name,
                name
            ) == 0)
        {
            source->type =
                LUT_SOURCE_LUT;

            source->lut_index =
                (uint8_t)i;


            strncpy(
                source->signal_name,
                name,
                VERISHOT_MAX_LEXEME_LENGTH - 1
            );


            source->signal_name[
                VERISHOT_MAX_LEXEME_LENGTH - 1
            ] = '\0';


            return MAPPER_OK;
        }
    }


    /* --------------------------------------------------------
     * Check whether the identifier is a future assignment.
     *
     * If yes, this is a forward LUT reference.
     * -------------------------------------------------------- */

    for (uint16_t i = current_lut + 1;
         i < parser->program.assignment_count;
         i++)
    {
        if (strcmp(
                parser->program.assignments[i].name,
                name
            ) == 0)
        {
            return MAPPER_FORWARD_REFERENCE;
        }
    }


    /*
     * Not an input and not an assignment.
     */
    return MAPPER_INPUT_NOT_FOUND;
}


/* ============================================================
 * CHECK WHETHER SOURCE ALREADY EXISTS
 * ============================================================ */

static bool Mapper_SourceExists(
    const LUTInputSource sources[],
    uint8_t source_count,
    const LUTInputSource *source
)
{
    if (sources == NULL ||
        source == NULL)
    {
        return false;
    }


    for (uint8_t i = 0;
         i < source_count;
         i++)
    {
        /*
         * External signals are identified by name.
         */
        if (source->type ==
            LUT_SOURCE_EXTERNAL)
        {
            if (sources[i].type ==
                LUT_SOURCE_EXTERNAL)
            {
                if (strcmp(
                        sources[i].signal_name,
                        source->signal_name
                    ) == 0)
                {
                    return true;
                }
            }
        }


        /*
         * LUT outputs are identified by LUT index.
         */
        else if (
            source->type ==
            LUT_SOURCE_LUT)
        {
            if (sources[i].type ==
                LUT_SOURCE_LUT)
            {
                if (sources[i].lut_index ==
                    source->lut_index)
                {
                    return true;
                }
            }
        }
    }


    return false;
}


/* ============================================================
 * COLLECT LUT SOURCES
 * ============================================================
 *
 * Walk the AST and find every unique logical source.
 *
 * Example:
 *
 *     (A & B) | A
 *
 * produces:
 *
 *     source[0] = A
 *     source[1] = B
 *
 * not:
 *
 *     A, B, A
 */

static MapperStatus Mapper_CollectSources(
    Mapper *mapper,
    uint8_t current_lut,
    const ASTNode *node,
    LUTInputSource sources[],
    uint8_t *source_count
)
{
    if (mapper == NULL ||
        node == NULL ||
        sources == NULL ||
        source_count == NULL)
    {
        return MAPPER_ERROR;
    }


    switch (node->type)
    {
        /* ====================================================
         * IDENTIFIER
         * ==================================================== */

        case AST_IDENTIFIER:
        {
            LUTInputSource source;

            memset(
                &source,
                0,
                sizeof(LUTInputSource)
            );


            MapperStatus status =
                Mapper_ResolveIdentifier(
                    mapper,
                    current_lut,
                    node->value,
                    &source
                );


            if (status != MAPPER_OK)
                return status;


            /*
             * Don't add duplicate sources.
             */
            if (Mapper_SourceExists(
                    sources,
                    *source_count,
                    &source
                ))
            {
                return MAPPER_OK;
            }


            /*
             * V1 supports only three LUT inputs.
             */
            if (*source_count >=
                VERISHOT_MAX_MAPPED_INPUTS)
            {
                return MAPPER_TOO_MANY_INPUTS;
            }


            sources[*source_count] =
                source;


            (*source_count)++;


            return MAPPER_OK;
        }


        /* ====================================================
         * CONSTANT
         * ==================================================== */

        case AST_CONSTANT:
        {
            /*
             * Constants do not consume LUT inputs.
             */
            return MAPPER_OK;
        }


        /* ====================================================
         * NOT
         * ==================================================== */

        case AST_NOT:
        {
            return Mapper_CollectSources(
                mapper,
                current_lut,
                node->left,
                sources,
                source_count
            );
        }


        /* ====================================================
         * BINARY OPERATORS
         * ==================================================== */

        case AST_AND:
        case AST_OR:
        case AST_XOR:
        {
            MapperStatus status;


            status =
                Mapper_CollectSources(
                    mapper,
                    current_lut,
                    node->left,
                    sources,
                    source_count
                );


            if (status != MAPPER_OK)
                return status;


            return Mapper_CollectSources(
                mapper,
                current_lut,
                node->right,
                sources,
                source_count
            );
        }


        default:
            return MAPPER_INVALID_AST;
    }
}


/* ============================================================
 * FIND SOURCE VALUE
 * ============================================================
 *
 * During truth-table generation:
 *
 *     input_values[i]
 *
 * corresponds to:
 *
 *     sources[i]
 *
 * Therefore an AST identifier is resolved by finding its
 * corresponding source.
 *
 * IMPORTANT:
 *
 * A previous LUT output is treated as an ordinary Boolean
 * input of the current LUT.
 *
 * We do NOT recursively evaluate its original AST here.
 *
 * This is exactly how the physical hardware behaves.
 */

static int Mapper_GetIdentifierValue(
    const ASTNode *node,
    const LUTInputSource sources[],
    uint8_t source_count,
    const uint8_t input_values[]
)
{
    if (node == NULL ||
        sources == NULL ||
        input_values == NULL)
    {
        return 0;
    }


    for (uint8_t i = 0;
         i < source_count;
         i++)
    {
        if (strcmp(
                sources[i].signal_name,
                node->value
            ) == 0)
        {
            return input_values[i] ? 1 : 0;
        }
    }


    /*
     * This should never happen if semantic analysis and
     * source collection were successful.
     */
    return 0;
}


/* ============================================================
 * AST EVALUATION
 * ============================================================
 *
 * Evaluate an AST for one LUT input combination.
 *
 * Example:
 *
 *     expression = A & B
 *
 *     input_values = {1, 1, 0}
 *
 *     result = 1
 *
 *
 * If the expression contains a previous LUT output:
 *
 *     Z = Y | C
 *
 * then Y is simply one of the current LUT's three logical
 * inputs.
 *
 * For example:
 *
 *     sources[0] = Y -> LUT0
 *     sources[1] = C -> External
 *
 * The truth-table generator assigns values to these inputs
 * exactly like S0 and S1 of the physical LUT.
 */

static int Mapper_EvaluateAST(
    const ASTNode *node,
    const LUTInputSource sources[],
    uint8_t source_count,
    const uint8_t input_values[]
)
{
    if (node == NULL)
        return 0;


    switch (node->type)
    {
        /* ====================================================
         * IDENTIFIER
         * ==================================================== */

        case AST_IDENTIFIER:
        {
            return Mapper_GetIdentifierValue(
                node,
                sources,
                source_count,
                input_values
            );
        }


        /* ====================================================
         * CONSTANT
         * ==================================================== */

        case AST_CONSTANT:
        {
            if (strcmp(
                    node->value,
                    "1"
                ) == 0)
            {
                return 1;
            }

            return 0;
        }


        /* ====================================================
         * NOT
         * ==================================================== */

        case AST_NOT:
        {
            return !Mapper_EvaluateAST(
                node->left,
                sources,
                source_count,
                input_values
            );
        }


        /* ====================================================
         * AND
         * ==================================================== */

        case AST_AND:
        {
            int left =
                Mapper_EvaluateAST(
                    node->left,
                    sources,
                    source_count,
                    input_values
                );


            int right =
                Mapper_EvaluateAST(
                    node->right,
                    sources,
                    source_count,
                    input_values
                );


            return left && right;
        }


        /* ====================================================
         * OR
         * ==================================================== */

        case AST_OR:
        {
            int left =
                Mapper_EvaluateAST(
                    node->left,
                    sources,
                    source_count,
                    input_values
                );


            int right =
                Mapper_EvaluateAST(
                    node->right,
                    sources,
                    source_count,
                    input_values
                );


            return left || right;
        }


        /* ====================================================
         * XOR
         * ==================================================== */

        case AST_XOR:
        {
            int left =
                Mapper_EvaluateAST(
                    node->left,
                    sources,
                    source_count,
                    input_values
                );


            int right =
                Mapper_EvaluateAST(
                    node->right,
                    sources,
                    source_count,
                    input_values
                );


            return left ^ right;
        }


        default:
            return 0;
    }
}


/* ============================================================
 * ROUTING SELECT CODE
 * ============================================================
 *
 * Destination-dependent physical routing table.
 *
 * LUT0:
 *
 *     External = 00
 *     LUT1     = 01
 *     LUT2     = 10
 *     LUT3     = 11
 *
 * LUT1:
 *
 *     External = 00
 *     LUT0     = 01
 *     LUT2     = 10
 *     LUT3     = 11
 *
 * LUT2:
 *
 *     External = 00
 *     LUT0     = 01
 *     LUT1     = 10
 *     LUT3     = 11
 *
 * LUT3:
 *
 *     External = 00
 *     LUT0     = 01
 *     LUT1     = 10
 *     LUT2     = 11
 */

uint8_t Mapper_GetRouteCode(
    uint8_t destination_lut,
    LUTSourceType source_type,
    uint8_t source_lut
)
{
    /*
     * External input.
     */
    if (source_type ==
        LUT_SOURCE_EXTERNAL)
    {
        return VERISHOT_ROUTE_EXTERNAL;
    }


    /*
     * Unused input.
     *
     * V1 uses 00 for unused.
     */
    if (source_type ==
        LUT_SOURCE_UNUSED)
    {
        return VERISHOT_ROUTE_EXTERNAL;
    }


    /*
     * Invalid LUT indices.
     */
    if (destination_lut >=
        VERISHOT_NUM_LUTS)
    {
        return VERISHOT_ROUTE_EXTERNAL;
    }


    if (source_lut >=
        VERISHOT_NUM_LUTS)
    {
        return VERISHOT_ROUTE_EXTERNAL;
    }


    /*
     * Self-routing is invalid.
     */
    if (destination_lut ==
        source_lut)
    {
        return VERISHOT_ROUTE_EXTERNAL;
    }


    switch (destination_lut)
    {
        /* ----------------------------------------------------
         * LUT0
         * ---------------------------------------------------- */

        case 0:

            if (source_lut == 1)
                return VERISHOT_ROUTE_SELECT_01;

            if (source_lut == 2)
                return VERISHOT_ROUTE_SELECT_10;

            if (source_lut == 3)
                return VERISHOT_ROUTE_SELECT_11;

            break;


        /* ----------------------------------------------------
         * LUT1
         * ---------------------------------------------------- */

        case 1:

            if (source_lut == 0)
                return VERISHOT_ROUTE_SELECT_01;

            if (source_lut == 2)
                return VERISHOT_ROUTE_SELECT_10;

            if (source_lut == 3)
                return VERISHOT_ROUTE_SELECT_11;

            break;


        /* ----------------------------------------------------
         * LUT2
         * ---------------------------------------------------- */

        case 2:

            if (source_lut == 0)
                return VERISHOT_ROUTE_SELECT_01;

            if (source_lut == 1)
                return VERISHOT_ROUTE_SELECT_10;

            if (source_lut == 3)
                return VERISHOT_ROUTE_SELECT_11;

            break;


        /* ----------------------------------------------------
         * LUT3
         * ---------------------------------------------------- */

        case 3:

            if (source_lut == 0)
                return VERISHOT_ROUTE_SELECT_01;

            if (source_lut == 1)
                return VERISHOT_ROUTE_SELECT_10;

            if (source_lut == 2)
                return VERISHOT_ROUTE_SELECT_11;

            break;


        default:
            break;
    }


    return VERISHOT_ROUTE_EXTERNAL;
}


/* ============================================================
 * INITIALIZATION
 * ============================================================ */

void Mapper_Init(
    Mapper *mapper
)
{
    if (mapper == NULL)
        return;


    Mapper_ClearConfig(mapper);


    mapper->parser = NULL;
}


/* ============================================================
 * TRUTH TABLE GENERATION
 * ============================================================ */

MapperStatus Mapper_GenerateTruthTable(
    Mapper *mapper,
    uint8_t lut_index
)
{
    if (mapper == NULL)
        return MAPPER_ERROR;


    if (lut_index >=
        VERISHOT_NUM_LUTS)
    {
        return MAPPER_ERROR;
    }


    VerishotLUT *lut =
        &mapper->config.luts[lut_index];


    if (!lut->used)
        return MAPPER_ERROR;


    if (lut->expression == NULL)
        return MAPPER_INVALID_AST;


    /* --------------------------------------------------------
     * Resolve all logical sources.
     * -------------------------------------------------------- */

    LUTInputSource sources[
        VERISHOT_MAX_MAPPED_INPUTS
    ];


    uint8_t source_count = 0;


    memset(
        sources,
        0,
        sizeof(sources)
    );


    MapperStatus status =
        Mapper_CollectSources(
            mapper,
            lut_index,
            lut->expression,
            sources,
            &source_count
        );


    if (status != MAPPER_OK)
        return status;


    /* --------------------------------------------------------
     * Clear all three LUT input source slots.
     * -------------------------------------------------------- */

    for (uint8_t i = 0;
         i < VERISHOT_LUT_INPUTS;
         i++)
    {
        lut->input_source[i].type =
            LUT_SOURCE_UNUSED;


        lut->input_source[i].lut_index =
            0;


        memset(
            lut->input_source[i].signal_name,
            0,
            VERISHOT_MAX_LEXEME_LENGTH
        );
    }


    /* --------------------------------------------------------
     * Store resolved sources.
     *
     * source[0] -> S0
     * source[1] -> S1
     * source[2] -> S2
     * -------------------------------------------------------- */

    for (uint8_t i = 0;
         i < source_count;
         i++)
    {
        lut->input_source[i] =
            sources[i];
    }


    /* --------------------------------------------------------
     * Generate all eight LUT truth-table addresses.
     *
     * Address:
     *
     *     bit 0 = S0
     *     bit 1 = S1
     *     bit 2 = S2
     *
     * Therefore:
     *
     *     address = S0 + 2*S1 + 4*S2
     * -------------------------------------------------------- */

    uint8_t truth_table = 0;


    for (uint8_t address = 0;
         address < 8;
         address++)
    {
        uint8_t input_values[
            VERISHOT_MAX_MAPPED_INPUTS
        ] = {0};


        /*
         * Assign the current truth-table address to
         * the logical LUT inputs.
         */
        for (uint8_t i = 0;
             i < source_count;
             i++)
        {
            input_values[i] =
                (address >> i) & 0x01;
        }


        /*
         * Evaluate the original Boolean expression using
         * these LUT input values.
         */
        int result =
            Mapper_EvaluateAST(
                lut->expression,
                sources,
                source_count,
                input_values
            );


        if (result)
        {
            truth_table |=
                (uint8_t)(1U << address);
        }
    }


    /*
     * Store truth table.
     */
    lut->truth_table =
        truth_table;


    /*
     * Copy into the final FPGA configuration.
     */
    mapper->config.lut_config[lut_index] =
        truth_table;


    return MAPPER_OK;
}


/* ============================================================
 * MAP ONE ASSIGNMENT
 * ============================================================ */

static MapperStatus Mapper_MapAssignment(
    Mapper *mapper,
    const VerishotAssignment *assignment,
    uint8_t lut_index
)
{
    if (mapper == NULL ||
        assignment == NULL)
    {
        return MAPPER_ERROR;
    }


    if (lut_index >=
        VERISHOT_NUM_LUTS)
    {
        return MAPPER_NO_LUT_AVAILABLE;
    }


    VerishotLUT *lut =
        &mapper->config.luts[lut_index];


    /* --------------------------------------------------------
     * Basic LUT information.
     * -------------------------------------------------------- */

    lut->used = 1;

    lut->lut_index =
        lut_index;


    lut->expression =
        assignment->expression;


    strncpy(
        lut->output_name,
        assignment->name,
        VERISHOT_MAX_LEXEME_LENGTH - 1
    );


    lut->output_name[
        VERISHOT_MAX_LEXEME_LENGTH - 1
    ] = '\0';


    /* --------------------------------------------------------
     * Generate truth table and resolve sources.
     * -------------------------------------------------------- */

    MapperStatus status =
        Mapper_GenerateTruthTable(
            mapper,
            lut_index
        );


    if (status != MAPPER_OK)
        return status;


    /* --------------------------------------------------------
     * Convert logical sources into physical route codes.
     * -------------------------------------------------------- */

    for (uint8_t input = 0;
         input < VERISHOT_LUT_INPUTS;
         input++)
    {
        uint8_t route_code =
            Mapper_GetRouteCode(
                lut_index,
                lut->input_source[input].type,
                lut->input_source[input].lut_index
            );


        lut->route_select[input] =
            route_code;


        mapper->config.route_select
            [lut_index][input] =
                route_code;
    }


    return MAPPER_OK;
}


/* ============================================================
 * COMPLETE PROGRAM MAPPING
 * ============================================================ */

MapperStatus Mapper_Map(
    Mapper *mapper,
    const Parser *parser
)
{
    if (mapper == NULL ||
        parser == NULL)
    {
        return MAPPER_ERROR;
    }


    /*
     * Start with a completely clean configuration.
     */
    Mapper_ClearConfig(mapper);


    /*
     * Retain the parser so that the mapper can resolve
     * identifiers against:
     *
     *     input declarations
     *     previous assignment outputs
     */
    mapper->parser =
        parser;


    /* --------------------------------------------------------
     * Require at least one assignment.
     * -------------------------------------------------------- */

    if (parser->program.assignment_count == 0)
    {
        mapper->status =
            MAPPER_INVALID_ASSIGNMENT;

        return mapper->status;
    }


    /* --------------------------------------------------------
     * Allocate assignments sequentially.
     *
     *     assignment 0 -> LUT0
     *     assignment 1 -> LUT1
     *     assignment 2 -> LUT2
     *     assignment 3 -> LUT3
     *
     * This is the V1 LUT priority rule.
     * -------------------------------------------------------- */

    for (uint16_t i = 0;
         i < parser->program.assignment_count;
         i++)
    {
        /*
         * Only four physical LUTs exist.
         */
        if (mapper->lut_count >=
            VERISHOT_NUM_LUTS)
        {
            mapper->status =
                MAPPER_NO_LUT_AVAILABLE;

            return mapper->status;
        }


        MapperStatus status =
            Mapper_MapAssignment(
                mapper,
                &parser->program.assignments[i],
                mapper->lut_count
            );


        if (status != MAPPER_OK)
        {
            mapper->status =
                status;

            return status;
        }


        mapper->lut_count++;
    }


    mapper->status =
        MAPPER_OK;


    return MAPPER_OK;
}


/* ============================================================
 * STATUS STRING
 * ============================================================ */

const char *Mapper_StatusToString(
    MapperStatus status
)
{
    switch (status)
    {
        case MAPPER_OK:
            return "OK";


        case MAPPER_ERROR:
            return "General mapper error";


        case MAPPER_NO_LUT_AVAILABLE:
            return "No LUT available";


        case MAPPER_TOO_MANY_INPUTS:
            return "Expression uses more than 3 inputs";


        case MAPPER_INVALID_AST:
            return "Invalid AST";


        case MAPPER_INVALID_ASSIGNMENT:
            return "Invalid assignment";


        case MAPPER_INPUT_NOT_FOUND:
            return "Identifier is neither an input nor a LUT output";


        case MAPPER_FORWARD_REFERENCE:
            return "Forward LUT reference is not allowed";


        case MAPPER_ROUTING_ERROR:
            return "Routing error";


        case MAPPER_TRUTH_TABLE_ERROR:
            return "Truth table generation error";


        default:
            return "Unknown mapper status";
    }
}


/* ============================================================
 * DEBUG PRINT
 * ============================================================ */

void Mapper_PrintConfig(
    const Mapper *mapper
)
{
    if (mapper == NULL)
        return;


    Serial.println();
    Serial.println(
        "========== MAPPER CONFIGURATION =========="
    );


    Serial.print("Mapper status : ");

    Serial.println(
        Mapper_StatusToString(
            mapper->status
        )
    );


    Serial.print("LUT count     : ");

    Serial.println(
        mapper->lut_count
    );


    /* --------------------------------------------------------
     * Individual LUTs
     * -------------------------------------------------------- */

    for (uint8_t lut_index = 0;
         lut_index < VERISHOT_NUM_LUTS;
         lut_index++)
    {
        const VerishotLUT *lut =
            &mapper->config.luts[lut_index];


        Serial.println();

        Serial.print("LUT");
        Serial.println(lut_index);


        if (!lut->used)
        {
            Serial.println(
                "  UNUSED"
            );

            continue;
        }


        Serial.print(
            "  Output      : "
        );

        Serial.println(
            lut->output_name
        );


        Serial.print(
            "  Truth table : 0x"
        );


        if (lut->truth_table < 0x10)
            Serial.print("0");


        Serial.println(
            lut->truth_table,
            HEX
        );


        Serial.println(
            "  Inputs:"
        );


        for (uint8_t input = 0;
             input < VERISHOT_LUT_INPUTS;
             input++)
        {
            const LUTInputSource *source =
                &lut->input_source[input];


            Serial.print(
                "    S"
            );

            Serial.print(
                input
            );

            Serial.print(
                " -> "
            );


            if (source->type ==
                LUT_SOURCE_EXTERNAL)
            {
                Serial.print(
                    "External "
                );

                Serial.println(
                    source->signal_name
                );
            }
            else if (
                source->type ==
                LUT_SOURCE_LUT)
            {
                Serial.print(
                    "LUT"
                );

                Serial.print(
                    source->lut_index
                );

                Serial.print(
                    " ("
                );

                Serial.print(
                    source->signal_name
                );

                Serial.println(
                    ")"
                );
            }
            else
            {
                Serial.println(
                    "UNUSED"
                );
            }


            Serial.print(
                "       route = "
            );

            Serial.println(
                lut->route_select[input]
            );
        }
    }


    /* --------------------------------------------------------
     * Raw LUT bytes
     * -------------------------------------------------------- */

    Serial.println();

    Serial.println(
        "Raw LUT bytes:"
    );


    for (uint8_t i = 0;
         i < VERISHOT_NUM_LUTS;
         i++)
    {
        Serial.print(
            "  LUT"
        );

        Serial.print(
            i
        );

        Serial.print(
            " = 0x"
        );


        if (mapper->config.lut_config[i] < 0x10)
            Serial.print("0");


        Serial.println(
            mapper->config.lut_config[i],
            HEX
        );
    }


    /* --------------------------------------------------------
     * Raw route select codes
     * -------------------------------------------------------- */

    Serial.println();

    Serial.println(
        "Raw route select codes:"
    );


    for (uint8_t lut = 0;
         lut < VERISHOT_NUM_LUTS;
         lut++)
    {
        Serial.print(
            "  LUT"
        );

        Serial.print(
            lut
        );

        Serial.print(
            " : "
        );


        for (uint8_t input = 0;
             input < VERISHOT_LUT_INPUTS;
             input++)
        {
            Serial.print(
                mapper->config.route_select[lut][input]
            );


            if (input <
                VERISHOT_LUT_INPUTS - 1)
            {
                Serial.print(
                    ", "
                );
            }
        }


        Serial.println();
    }


    Serial.println();

    Serial.println(
        "=========================================="
    );
}
