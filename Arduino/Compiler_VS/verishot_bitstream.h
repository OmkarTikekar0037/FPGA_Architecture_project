
/*
 * verishot_bitstream.h
 *
 * VeriShot hardware configuration bitstream.
 *
 * Physical configuration:
 *
 *     LUT CONFIGURATION
 *     -----------------
 *     4 x 8-bit 74HC595
 *
 *     LUT0 -> bits [0:7]
 *     LUT1 -> bits [8:15]
 *     LUT2 -> bits [16:23]
 *     LUT3 -> bits [24:31]
 *
 *
 *     ROUTING CONFIGURATION
 *     ---------------------
 *     3 x 8-bit 74HC595
 *
 *     LUT0 -> 6 bits
 *     LUT1 -> 6 bits
 *     LUT2 -> 6 bits
 *     LUT3 -> 6 bits
 *
 *     Total routing configuration = 24 bits
 *
 *
 *     Each LUT routing field:
 *
 *         bits [1:0] = LUT input 0 select
 *         bits [3:2] = LUT input 1 select
 *         bits [5:4] = LUT input 2 select
 *
 *
 *     Routing source encoding depends on destination LUT:
 *
 *         LUT0:
 *             00 = External
 *             01 = LUT1
 *             10 = LUT2
 *             11 = LUT3
 *
 *         LUT1:
 *             00 = External
 *             01 = LUT0
 *             10 = LUT2
 *             11 = LUT3
 *
 *         LUT2:
 *             00 = External
 *             01 = LUT0
 *             10 = LUT1
 *             11 = LUT3
 *
 *         LUT3:
 *             00 = External
 *             01 = LUT0
 *             10 = LUT1
 *             11 = LUT2
 *
 *
 *     Physical SIPO order:
 *
 *         nearest register = [0:7]
 *
 *     Therefore the byte stream is shifted from the
 *     highest physical register toward the nearest register.
 *
 *     Each byte is serialized Q7 -> Q0.
 */

#ifndef VERISHOT_BITSTREAM_H
#define VERISHOT_BITSTREAM_H

#include <stdint.h>

#include "verishot_mapper.h"


/* ============================================================
 * HARDWARE CONFIGURATION SIZES
 * ============================================================ */

#define VERISHOT_LUT_BYTE_COUNT       4
#define VERISHOT_ROUTE_BYTE_COUNT     3

#define VERISHOT_LUT_BITS             32
#define VERISHOT_ROUTE_BITS           24

#define VERISHOT_TOTAL_BITS           56


/* ============================================================
 * BITSTREAM STRUCTURE
 * ============================================================ */

typedef struct
{
    /*
     * LUT truth-table bytes.
     *
     * lut_bytes[0] -> LUT0
     * lut_bytes[1] -> LUT1
     * lut_bytes[2] -> LUT2
     * lut_bytes[3] -> LUT3
     */
    uint8_t lut_bytes[VERISHOT_LUT_BYTE_COUNT];


    /*
     * Packed routing bytes.
     *
     * These three bytes contain the four 6-bit LUT
     * routing fields.
     *
     * route_bytes[0] -> routing bits 0..7
     * route_bytes[1] -> routing bits 8..15
     * route_bytes[2] -> routing bits 16..23
     */
    uint8_t route_bytes[VERISHOT_ROUTE_BYTE_COUNT];

} VerishotBitstream;


/* ============================================================
 * GENERATION
 * ============================================================ */

/*
 * Generate the physical configuration bitstream from
 * the logical FPGA configuration.
 */
void Bitstream_Generate(
    const FPGA_Config *config,
    VerishotBitstream *bitstream
);


/* ============================================================
 * DEBUG / PRINTING
 * ============================================================ */

/*
 * Print the complete generated bitstream.
 */
void Bitstream_Print(
    const VerishotBitstream *bitstream
);


/*
 * Print only the LUT configuration.
 */
void Bitstream_PrintLUTBytes(
    const VerishotBitstream *bitstream
);


/*
 * Print only the routing configuration.
 */
void Bitstream_PrintRouteBytes(
    const VerishotBitstream *bitstream
);


#endif /* VERISHOT_BITSTREAM_H */
