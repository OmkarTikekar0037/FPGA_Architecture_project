
/*
 * verishot_mapper.h
 *
 * VeriShot V1 Mapper
 *
 * Converts a semantically valid VeriShot program into
 * a physical FPGA configuration consisting of:
 *
 *     - 4 × 8-bit LUT truth tables
 *     - 4 × 3 × 2-bit routing selections
 *
 * V1 limitations:
 *
 *     - 4 LUTs maximum
 *     - 3 inputs per LUT
 *     - Assignment order determines LUT allocation
 *     - LUT0 has highest priority
 *     - A LUT may depend on:
 *           - external inputs
 *           - previously allocated LUTs
 *     - Forward LUT references are not allowed
 *     - No LUT optimization/sharing
 *     - No automatic decomposition
 */

#ifndef VERISHOT_MAPPER_H
#define VERISHOT_MAPPER_H

#include <stdint.h>

#include "verishot_parser.h"


/* ============================================================
 * MAPPER CONFIGURATION
 * ============================================================ */

#define VERISHOT_NUM_LUTS          4
#define VERISHOT_LUT_INPUTS        3
#define VERISHOT_LUT_TRUTH_BITS    8

/*
 * Maximum number of unique logical sources that can be
 * connected to one 3-input LUT.
 */
#define VERISHOT_MAX_MAPPED_INPUTS 3


/* ============================================================
 * ROUTING SELECT CODES
 * ============================================================
 *
 * These are the actual 2-bit values sent to the physical
 * 4:1 MUX select inputs.
 *
 * 00 is always External.
 *
 * The meaning of 01/10/11 depends on the destination LUT.
 */

#define VERISHOT_ROUTE_EXTERNAL    0
#define VERISHOT_ROUTE_SELECT_01   1
#define VERISHOT_ROUTE_SELECT_10   2
#define VERISHOT_ROUTE_SELECT_11   3


/* ============================================================
 * MAPPER STATUS
 * ============================================================ */

typedef enum
{
    MAPPER_OK = 0,

    MAPPER_ERROR,

    MAPPER_NO_LUT_AVAILABLE,

    MAPPER_TOO_MANY_INPUTS,

    MAPPER_INVALID_AST,

    MAPPER_INVALID_ASSIGNMENT,

    MAPPER_INPUT_NOT_FOUND,

    MAPPER_FORWARD_REFERENCE,

    MAPPER_ROUTING_ERROR,

    MAPPER_TRUTH_TABLE_ERROR

} MapperStatus;


/* ============================================================
 * LUT SOURCE TYPE
 * ============================================================
 *
 * Every LUT input is connected to one of:
 *
 *     UNUSED
 *     EXTERNAL signal
 *     Previous LUT output
 */

typedef enum
{
    LUT_SOURCE_UNUSED = 0,

    LUT_SOURCE_EXTERNAL,

    LUT_SOURCE_LUT

} LUTSourceType;


/* ============================================================
 * LUT INPUT SOURCE
 * ============================================================ */

typedef struct
{
    /*
     * Type of source.
     */
    LUTSourceType type;


    /*
     * Name of the logical signal.
     *
     * Example:
     *
     *     A
     *     B
     *     Y
     *
     * For LUT sources this is the output signal name.
     */
    char signal_name[VERISHOT_MAX_LEXEME_LENGTH];


    /*
     * Valid only when:
     *
     *     type == LUT_SOURCE_LUT
     *
     * Example:
     *
     *     Y -> LUT0
     */
    uint8_t lut_index;

} LUTInputSource;


/* ============================================================
 * LUT CONFIGURATION
 * ============================================================ */

typedef struct
{
    /*
     * Physical LUT number.
     *
     *     0 -> LUT0
     *     1 -> LUT1
     *     2 -> LUT2
     *     3 -> LUT3
     */
    uint8_t lut_index;


    /*
     * 1 if LUT is used.
     * 0 if LUT is unused.
     */
    uint8_t used;


    /*
     * Logical output signal produced by this LUT.
     *
     * Example:
     *
     *     assign Y = A & B;
     *
     * output_name = "Y"
     */
    char output_name[VERISHOT_MAX_LEXEME_LENGTH];


    /*
     * AST expression implemented by this LUT.
     */
    const ASTNode *expression;


    /*
     * Logical source connected to each LUT input.
     *
     *     input_source[0] -> S0
     *     input_source[1] -> S1
     *     input_source[2] -> S2
     */
    LUTInputSource input_source[VERISHOT_LUT_INPUTS];


    /*
     * Generated 8-bit truth table.
     *
     * Address convention:
     *
     *     address bit 0 -> S0
     *     address bit 1 -> S1
     *     address bit 2 -> S2
     *
     * Therefore:
     *
     *     address = S0 + 2*S1 + 4*S2
     */
    uint8_t truth_table;


    /*
     * Physical 2-bit routing select codes.
     *
     *     route_select[0] -> S0
     *     route_select[1] -> S1
     *     route_select[2] -> S2
     */
    uint8_t route_select[VERISHOT_LUT_INPUTS];

} VerishotLUT;


/* ============================================================
 * COMPLETE FPGA CONFIGURATION
 * ============================================================ */

typedef struct
{
    /*
     * Complete configuration of all four LUTs.
     */
    VerishotLUT luts[VERISHOT_NUM_LUTS];


    /*
     * Convenience array containing only the four LUT
     * truth-table bytes.
     *
     *     lut_config[0] -> LUT0
     *     lut_config[1] -> LUT1
     *     lut_config[2] -> LUT2
     *     lut_config[3] -> LUT3
     */
    uint8_t lut_config[VERISHOT_NUM_LUTS];


    /*
     * Physical routing select codes.
     *
     *     route_select[lut][input]
     *
     * Four LUTs × three inputs × two bits
     * = 24 routing bits.
     */
    uint8_t route_select[
        VERISHOT_NUM_LUTS
    ][VERISHOT_LUT_INPUTS];

} FPGA_Config;


/* ============================================================
 * MAPPER OBJECT
 * ============================================================ */

typedef struct
{
    /*
     * Generated FPGA configuration.
     */
    FPGA_Config config;


    /*
     * Number of LUTs allocated.
     */
    uint8_t lut_count;


    /*
     * Current mapper status.
     */
    MapperStatus status;


    /*
     * Reference to the parsed program.
     *
     * Used for resolving:
     *
     *     external input
     *
     * versus:
     *
     *     previous LUT output
     */
    const Parser *parser;

} Mapper;


/* ============================================================
 * PUBLIC API
 * ============================================================ */

/*
 * Initialize mapper.
 */
void Mapper_Init(
    Mapper *mapper
);


/*
 * Map a semantically valid VeriShot program.
 *
 * Assignment order determines LUT allocation:
 *
 *     assignment 0 -> LUT0
 *     assignment 1 -> LUT1
 *     assignment 2 -> LUT2
 *     assignment 3 -> LUT3
 */
MapperStatus Mapper_Map(
    Mapper *mapper,
    const Parser *parser
);


/*
 * Convert a logical source into the physical 2-bit routing
 * code for a particular destination LUT.
 */
uint8_t Mapper_GetRouteCode(
    uint8_t destination_lut,
    LUTSourceType source_type,
    uint8_t source_lut
);


/*
 * Generate the 8-bit truth table for a LUT.
 */
MapperStatus Mapper_GenerateTruthTable(
    Mapper *mapper,
    uint8_t lut_index
);


/*
 * Convert MapperStatus into readable text.
 */
const char *Mapper_StatusToString(
    MapperStatus status
);


/*
 * Print complete mapper configuration for debugging.
 */
void Mapper_PrintConfig(
    const Mapper *mapper
);


#endif /* VERISHOT_MAPPER_H */
