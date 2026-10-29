
/*
 * verishot_bitstream.cpp
 *
 * VeriShot hardware configuration bitstream generator.
 */

#include "verishot_bitstream.h"

#include <Arduino.h>
#include <string.h>


/* ============================================================
 * INTERNAL HELPERS
 * ============================================================ */

/*
 * Pack one LUT's three 2-bit routing selections
 * into a 6-bit value.
 *
 * Layout:
 *
 *     bit 0..1 = input 0 select
 *     bit 2..3 = input 1 select
 *     bit 4..5 = input 2 select
 */
static uint8_t Bitstream_PackLUTRoute(
    const FPGA_Config *config,
    uint8_t lut)
{
    uint8_t route = 0;

    route |= (config->route_select[lut][0] & 0x03) << 0;
    route |= (config->route_select[lut][1] & 0x03) << 2;
    route |= (config->route_select[lut][2] & 0x03) << 4;

    return route & 0x3F;
}


/* ============================================================
 * GENERATE BITSTREAM
 * ============================================================ */

void Bitstream_Generate(
    const FPGA_Config *config,
    VerishotBitstream *bitstream)
{
    if (config == nullptr || bitstream == nullptr)
        return;


    /*
     * Clear the complete bitstream.
     */
    memset(
        bitstream,
        0,
        sizeof(VerishotBitstream)
    );


    /* ========================================================
     * LUT CONFIGURATION
     * ======================================================== */

    /*
     * One byte per LUT.
     *
     * Logical arrangement:
     *
     *     LUT0 -> [0:7]
     *     LUT1 -> [8:15]
     *     LUT2 -> [16:23]
     *     LUT3 -> [24:31]
     */
    for (uint8_t lut = 0;
         lut < VERISHOT_LUT_BYTE_COUNT;
         lut++)
    {
        bitstream->lut_bytes[lut] =
            config->lut_config[lut];
    }


    /* ========================================================
     * ROUTING CONFIGURATION
     * ======================================================== */

    /*
     * First construct the logical 24-bit routing value.
     *
     *     bits  0.. 5 = LUT0
     *     bits  6..11 = LUT1
     *     bits 12..17 = LUT2
     *     bits 18..23 = LUT3
     *
     * Each LUT contributes exactly 6 bits.
     */

    uint32_t routing = 0;


    for (uint8_t lut = 0;
         lut < VERISHOT_NUM_LUTS;
         lut++)
    {
        uint8_t lut_route =
            Bitstream_PackLUTRoute(config, lut);

        routing |=
            ((uint32_t)lut_route)
            << (6 * lut);
    }


    /*
     * Split the 24-bit routing value into
     * three 8-bit SIPO bytes.
     *
     * route_bytes[0] = bits  0.. 7
     * route_bytes[1] = bits  8..15
     * route_bytes[2] = bits 16..23
     */
    bitstream->route_bytes[0] =
        (uint8_t)((routing >> 0) & 0xFF);

    bitstream->route_bytes[1] =
        (uint8_t)((routing >> 8) & 0xFF);

    bitstream->route_bytes[2] =
        (uint8_t)((routing >> 16) & 0xFF);
}


/* ============================================================
 * PRINT LUT BYTES
 * ============================================================ */

void Bitstream_PrintLUTBytes(
    const VerishotBitstream *bitstream)
{
    if (bitstream == nullptr)
        return;


    Serial.println();
    Serial.println(
        F("========== LUT CONFIGURATION ==========")
    );


    for (uint8_t lut = 0;
         lut < VERISHOT_LUT_BYTE_COUNT;
         lut++)
    {
        Serial.print(F("LUT"));
        Serial.print(lut);

        Serial.print(F(" : 0x"));

        if (bitstream->lut_bytes[lut] < 0x10)
            Serial.print('0');

        Serial.println(
            bitstream->lut_bytes[lut],
            HEX
        );
    }


    Serial.println(
        F("=======================================")
    );
}


/* ============================================================
 * PRINT ROUTING BYTES
 * ============================================================ */

void Bitstream_PrintRouteBytes(
    const VerishotBitstream *bitstream)
{
    if (bitstream == nullptr)
        return;


    Serial.println();
    Serial.println(
        F("========= ROUTING CONFIGURATION =========")
    );


    Serial.print(F("Route byte 0 : 0x"));

    if (bitstream->route_bytes[0] < 0x10)
        Serial.print('0');

    Serial.println(
        bitstream->route_bytes[0],
        HEX
    );


    Serial.print(F("Route byte 1 : 0x"));

    if (bitstream->route_bytes[1] < 0x10)
        Serial.print('0');

    Serial.println(
        bitstream->route_bytes[1],
        HEX
    );


    Serial.print(F("Route byte 2 : 0x"));

    if (bitstream->route_bytes[2] < 0x10)
        Serial.print('0');

    Serial.println(
        bitstream->route_bytes[2],
        HEX
    );


    Serial.println(
        F("=========================================")
    );
}


/* ============================================================
 * PRINT COMPLETE BITSTREAM
 * ============================================================ */

void Bitstream_Print(
    const VerishotBitstream *bitstream)
{
    if (bitstream == nullptr)
        return;


    Bitstream_PrintLUTBytes(bitstream);

    Bitstream_PrintRouteBytes(bitstream);


    Serial.println();
    Serial.println(
        F("========== PHYSICAL BITSTREAM ==========")
    );


    /*
     * LUT chain
     *
     * The nearest register is LUT0.
     *
     * Therefore the physical shift order is:
     *
     *     LUT3 -> LUT2 -> LUT1 -> LUT0
     *
     * Each byte is serialized:
     *
     *     Q7 -> Q0
     */

    Serial.println(
        F("LUT shift order:")
    );

    Serial.println(
        F("  LUT3 -> LUT2 -> LUT1 -> LUT0")
    );


    for (int8_t lut = 3;
         lut >= 0;
         lut--)
    {
        Serial.print(F("  LUT"));
        Serial.print(lut);
        Serial.print(F(" : 0x"));

        if (bitstream->lut_bytes[lut] < 0x10)
            Serial.print('0');

        Serial.println(
            bitstream->lut_bytes[lut],
            HEX
        );
    }


    /*
     * Routing chain
     *
     * route_bytes[0] belongs to physical
     * register [0:7], which is nearest the MCU.
     *
     * Therefore the physical shift order is:
     *
     *     route byte 2
     *     route byte 1
     *     route byte 0
     */

    Serial.println();

    Serial.println(
        F("Routing shift order:")
    );

    Serial.println(
        F("  BYTE2 -> BYTE1 -> BYTE0")
    );


    for (int8_t i = 2;
         i >= 0;
         i--)
    {
        Serial.print(F("  BYTE"));
        Serial.print(i);
        Serial.print(F(" : 0x"));

        if (bitstream->route_bytes[i] < 0x10)
            Serial.print('0');

        Serial.println(
            bitstream->route_bytes[i],
            HEX
        );
    }


    Serial.println(
        F("=========================================")
    );
}
