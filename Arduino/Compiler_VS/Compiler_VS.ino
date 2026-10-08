
#include <Arduino.h>

#include "verishot_lexer.h"
#include "verishot_parser.h"
#include "semantic.h"
#include "verishot_mapper.h"
#include "verishot_bitstream.h"


/* ============================================================
 * VERISHOT CONFIGURATION
 * ============================================================ */

#define VERISHOT_BAUD_RATE        115200
#define VERISHOT_RX_BUFFER_SIZE   VERISHOT_MAX_SOURCE_LENGTH


/* ============================================================
 * HARDWARE CONFIGURATION
 * ============================================================
 *
 * IMPORTANT:
 *
 * Replace these six pin numbers with the actual Arduino pins
 * connected to your hardware.
 *
 * There are TWO completely independent 74HC595 chains:
 *
 *
 * 1. LUT configuration chain
 *
 *      4 × 74HC595
 *      32 bits total
 *
 *
 * 2. Routing / switch-matrix chain
 *
 *      3 × 74HC595
 *      24 bits total
 *
 * Each chain has its own:
 *
 *      SER
 *      SRCLK
 *      RCLK
 *
 * SRCLK and RCLK are NOT shared between the two chains.
 */


/* ------------------------------------------------------------
 * LUT configuration chain
 * ------------------------------------------------------------ */

#define LUT_SER_PIN       2
#define LUT_SRCLK_PIN     3
#define LUT_RCLK_PIN      4


/* ------------------------------------------------------------
 * Routing configuration chain
 * ------------------------------------------------------------ */

#define ROUTE_SER_PIN     5
#define ROUTE_SRCLK_PIN   6
#define ROUTE_RCLK_PIN    7


/* ============================================================
 * COMPILER OBJECTS
 * ============================================================ */

Lexer lexer;

Parser parser;

SemanticAnalyzer semantic;

Mapper mapper;


/* ============================================================
 * GENERATED BITSTREAM
 * ============================================================
 *
 * This is the final hardware configuration produced by the
 * Bitstream Generator.
 *
 *     lut_bytes[4]
 *         32 bits
 *
 *     route_bytes[3]
 *         24 bits
 */

VerishotBitstream bitstream;


/* ============================================================
 * SOURCE BUFFER
 * ============================================================ */

char source_buffer[
    VERISHOT_RX_BUFFER_SIZE
];

uint16_t source_length = 0;


/* ============================================================
 * FUNCTION PROTOTYPES
 * ============================================================ */

/* ------------------------------------------------------------
 * Compiler stages
 * ------------------------------------------------------------ */

static bool ReceiveProgram(void);

static bool RunLexer(void);

static bool RunParser(void);

static bool RunSemanticAnalyzer(void);

static bool RunMapper(void);

static bool RunBitstream(void);


/* ------------------------------------------------------------
 * Hardware control
 * ------------------------------------------------------------ */

static void Hardware_Init(void);

static void Load_LUT_Bitstream(
    const uint8_t *lut_bytes
);

static void Load_Route_Bitstream(
    const uint8_t *route_bytes
);

static void Load_Bitstream(
    const VerishotBitstream *bitstream
);


/* ------------------------------------------------------------
 * Error reporting
 * ------------------------------------------------------------ */

static void PrintCompilerError(void);


/* ============================================================
 * SETUP
 * ============================================================ */

void setup()
{
    Serial.begin(
        VERISHOT_BAUD_RATE
    );


    /*
     * Give the serial interface a moment to initialize.
     */
    delay(100);


    Serial.println();

    Serial.println(
        F("========================================")
    );

    Serial.println(
        F("          VERISHOT COMPILER             ")
    );

    Serial.println(
        F("========================================")
    );


    Serial.println(
        F("Initializing compiler...")
    );


    /* --------------------------------------------------------
     * Initialize source buffer
     * -------------------------------------------------------- */

    source_length = 0;

    source_buffer[0] = '\0';


    /* --------------------------------------------------------
     * Initialize lexer
     *
     * Lexer will be initialized again whenever a new source
     * program is received.
     * -------------------------------------------------------- */

    Lexer_Init(
        &lexer,
        source_buffer
    );


    /* --------------------------------------------------------
     * Initialize parser
     *
     * Actual token stream is supplied after lexical analysis.
     * -------------------------------------------------------- */

    Parser_Init(
        &parser,
        nullptr,
        0
    );


    /* --------------------------------------------------------
     * Initialize semantic analyzer
     * -------------------------------------------------------- */

    Semantic_Init(
        &semantic
    );


    /* --------------------------------------------------------
     * Initialize mapper
     * -------------------------------------------------------- */

    Mapper_Init(
        &mapper
    );


    /* --------------------------------------------------------
     * Initialize generated bitstream
     * -------------------------------------------------------- */

    memset(
        &bitstream,
        0,
        sizeof(VerishotBitstream)
    );


    /* --------------------------------------------------------
     * Initialize physical hardware
     * -------------------------------------------------------- */

    Hardware_Init();


    /* --------------------------------------------------------
     * Status
     * -------------------------------------------------------- */

    Serial.println(
        F("Lexer            : READY")
    );

    Serial.println(
        F("Parser           : READY")
    );

    Serial.println(
        F("Semantic Analyzer: READY")
    );

    Serial.println(
        F("LUT Mapper       : READY")
    );

    Serial.println(
        F("Bitstream Generator: READY")
    );

    Serial.println(
        F("SIPO Hardware    : READY")
    );


    Serial.println();

    Serial.println(
        F("VeriShot is ready.")
    );

    Serial.println(
        F("Enter a VeriShot program.")
    );

    Serial.println(
        F("Send END on its own line to compile.")
    );

    Serial.println();
}


/* ============================================================
 * MAIN LOOP
 * ============================================================ */

void loop()
{
    /* --------------------------------------------------------
     * STEP 1
     *
     * Receive a complete VeriShot program.
     * -------------------------------------------------------- */

    if (!ReceiveProgram())
    {
        return;
    }


    Serial.println();

    Serial.println(
        F("========================================")
    );

    Serial.println(
        F("        VERISHOT COMPILATION            ")
    );

    Serial.println(
        F("========================================")
    );


    /* --------------------------------------------------------
     * STEP 2
     *
     * Lexical analysis
     * -------------------------------------------------------- */

    if (!RunLexer())
    {
        PrintCompilerError();
        return;
    }


    /* --------------------------------------------------------
     * STEP 3
     *
     * Syntax analysis
     * -------------------------------------------------------- */

    if (!RunParser())
    {
        PrintCompilerError();
        return;
    }


    /* --------------------------------------------------------
     * STEP 4
     *
     * Semantic analysis
     * -------------------------------------------------------- */

    if (!RunSemanticAnalyzer())
    {
        PrintCompilerError();
        return;
    }


    /* --------------------------------------------------------
     * STEP 5
     *
     * LUT mapping and routing
     * -------------------------------------------------------- */

    if (!RunMapper())
    {
        PrintCompilerError();
        return;
    }


    /* --------------------------------------------------------
     * STEP 6
     *
     * Generate physical bitstream
     * -------------------------------------------------------- */

    if (!RunBitstream())
    {
        PrintCompilerError();
        return;
    }


    /* --------------------------------------------------------
     * STEP 7
     *
     * Physically program the FPGA prototype
     * -------------------------------------------------------- */

    Load_Bitstream(
        &bitstream
    );


    /* --------------------------------------------------------
     * Compilation complete
     * -------------------------------------------------------- */

    Serial.println();

    Serial.println(
        F("========================================")
    );

    Serial.println(
        F("       FPGA CONFIGURATION DONE          ")
    );

    Serial.println(
        F("========================================")
    );

    Serial.println();
}


/* ============================================================
 * HARDWARE INITIALIZATION
 * ============================================================ */

static void Hardware_Init(void)
{
    /* --------------------------------------------------------
     * LUT chain
     * -------------------------------------------------------- */

    pinMode(
        LUT_SER_PIN,
        OUTPUT
    );

    pinMode(
        LUT_SRCLK_PIN,
        OUTPUT
    );

    pinMode(
        LUT_RCLK_PIN,
        OUTPUT
    );


    /* --------------------------------------------------------
     * Routing chain
     * -------------------------------------------------------- */

    pinMode(
        ROUTE_SER_PIN,
        OUTPUT
    );

    pinMode(
        ROUTE_SRCLK_PIN,
        OUTPUT
    );

    pinMode(
        ROUTE_RCLK_PIN,
        OUTPUT
    );


    /* --------------------------------------------------------
     * Initial states
     * -------------------------------------------------------- */

    digitalWrite(
        LUT_SER_PIN,
        LOW
    );

    digitalWrite(
        LUT_SRCLK_PIN,
        LOW
    );

    digitalWrite(
        LUT_RCLK_PIN,
        LOW
    );


    digitalWrite(
        ROUTE_SER_PIN,
        LOW
    );

    digitalWrite(
        ROUTE_SRCLK_PIN,
        LOW
    );

    digitalWrite(
        ROUTE_RCLK_PIN,
        LOW
    );
}


/* ============================================================
 * LOAD LUT BITSTREAM
 * ============================================================
 *
 * Physical arrangement:
 *
 *
 * MCU
 *  |
 *  v
 * +--------+    +--------+    +--------+    +--------+
 * |  LUT0  | -> |  LUT1  | -> |  LUT2  | -> |  LUT3  |
 * +--------+    +--------+    +--------+    +--------+
 *
 *
 * Logical bytes:
 *
 *     lut_bytes[0] = LUT0
 *     lut_bytes[1] = LUT1
 *     lut_bytes[2] = LUT2
 *     lut_bytes[3] = LUT3
 *
 *
 * Since LUT0 is physically nearest to the MCU, the bytes
 * must be shifted in reverse order:
 *
 *     LUT3
 *     LUT2
 *     LUT1
 *     LUT0
 *
 *
 * Each byte itself is transmitted:
 *
 *     Q7 -> Q0
 *
 * therefore:
 *
 *     MSBFIRST
 * ============================================================ */

static void Load_LUT_Bitstream(
    const uint8_t *lut_bytes
)
{
    if (lut_bytes == nullptr)
    {
        return;
    }


    Serial.println(
        F("Loading LUT configuration...")
    );


    /* --------------------------------------------------------
     * LUT3
     * -------------------------------------------------------- */

    shiftOut(
        LUT_SER_PIN,
        LUT_SRCLK_PIN,
        MSBFIRST,
        lut_bytes[3]
    );


    /* --------------------------------------------------------
     * LUT2
     * -------------------------------------------------------- */

    shiftOut(
        LUT_SER_PIN,
        LUT_SRCLK_PIN,
        MSBFIRST,
        lut_bytes[2]
    );


    /* --------------------------------------------------------
     * LUT1
     * -------------------------------------------------------- */

    shiftOut(
        LUT_SER_PIN,
        LUT_SRCLK_PIN,
        MSBFIRST,
        lut_bytes[1]
    );


    /* --------------------------------------------------------
     * LUT0
     * -------------------------------------------------------- */

    shiftOut(
        LUT_SER_PIN,
        LUT_SRCLK_PIN,
        MSBFIRST,
        lut_bytes[0]
    );


    /*
     * Transfer shift-register contents into the output
     * registers.
     */

    digitalWrite(
        LUT_RCLK_PIN,
        HIGH
    );

    digitalWrite(
        LUT_RCLK_PIN,
        LOW
    );
}


/* ============================================================
 * LOAD ROUTING BITSTREAM
 * ============================================================
 *
 * Physical arrangement:
 *
 *
 * MCU
 *  |
 *  v
 * +--------+    +--------+    +--------+
 * |  0:7   | -> |  8:15  | -> | 16:23  |
 * +--------+    +--------+    +--------+
 *
 *
 * Logical bytes:
 *
 *     route_bytes[0] = bits 0..7
 *     route_bytes[1] = bits 8..15
 *     route_bytes[2] = bits 16..23
 *
 *
 * Since byte 0 is physically nearest the MCU:
 *
 *     route_bytes[2]
 *     route_bytes[1]
 *     route_bytes[0]
 *
 * are shifted in this order.
 *
 *
 * Each byte is transmitted MSB first.
 * ============================================================ */

static void Load_Route_Bitstream(
    const uint8_t *route_bytes
)
{
    if (route_bytes == nullptr)
    {
        return;
    }


    Serial.println(
        F("Loading routing configuration...")
    );


    /* --------------------------------------------------------
     * Route byte 2
     * -------------------------------------------------------- */

    shiftOut(
        ROUTE_SER_PIN,
        ROUTE_SRCLK_PIN,
        MSBFIRST,
        route_bytes[2]
    );


    /* --------------------------------------------------------
     * Route byte 1
     * -------------------------------------------------------- */

    shiftOut(
        ROUTE_SER_PIN,
        ROUTE_SRCLK_PIN,
        MSBFIRST,
        route_bytes[1]
    );


    /* --------------------------------------------------------
     * Route byte 0
     * -------------------------------------------------------- */

    shiftOut(
        ROUTE_SER_PIN,
        ROUTE_SRCLK_PIN,
        MSBFIRST,
        route_bytes[0]
    );


    /*
     * Latch routing configuration.
     */

    digitalWrite(
        ROUTE_RCLK_PIN,
        HIGH
    );

    digitalWrite(
        ROUTE_RCLK_PIN,
        LOW
    );
}


/* ============================================================
 * LOAD COMPLETE BITSTREAM
 * ============================================================ */

static void Load_Bitstream(
    const VerishotBitstream *generated_bitstream
)
{
    if (generated_bitstream == nullptr)
    {
        return;
    }


    Serial.println();

    Serial.println(
        F("[6] FPGA PROGRAMMING")
    );

    Serial.println(
        F("----------------------------------------")
    );


    /* --------------------------------------------------------
     * Print generated data before loading
     * -------------------------------------------------------- */

    Serial.println(
        F("LUT bytes:")
    );


    for (uint8_t i = 0;
         i < VERISHOT_LUT_BYTE_COUNT;
         i++)
    {
        Serial.print(
            F("  LUT")
        );

        Serial.print(
            i
        );

        Serial.print(
            F(" = 0x")
        );


        if (
            generated_bitstream->lut_bytes[i]
            < 0x10
        )
        {
            Serial.print(
                F("0")
            );
        }


        Serial.println(
            generated_bitstream->lut_bytes[i],
            HEX
        );
    }


    Serial.println();


    Serial.println(
        F("Routing bytes:")
    );


    for (uint8_t i = 0;
         i < VERISHOT_ROUTE_BYTE_COUNT;
         i++)
    {
        Serial.print(
            F("  BYTE")
        );

        Serial.print(
            i
        );

        Serial.print(
            F(" = 0x")
        );


        if (
            generated_bitstream->route_bytes[i]
            < 0x10
        )
        {
            Serial.print(
                F("0")
            );
        }


        Serial.println(
            generated_bitstream->route_bytes[i],
            HEX
        );
    }


    Serial.println();


    /* --------------------------------------------------------
     * Program LUT chain
     * -------------------------------------------------------- */

    Load_LUT_Bitstream(
        generated_bitstream->lut_bytes
    );


    /* --------------------------------------------------------
     * Program routing chain
     * -------------------------------------------------------- */

    Load_Route_Bitstream(
        generated_bitstream->route_bytes
    );


    Serial.println();

    Serial.println(
        F("FPGA configuration loaded successfully.")
    );
}


/* ============================================================
 * RECEIVE PROGRAM
 * ============================================================
 *
 * Protocol:
 *
 *     input A, B;
 *     output Y;
 *     assign Y = A & B;
 *     END
 *
 *
 * END must be on its own line.
 *
 * END is consumed by this function and is NOT inserted into
 * source_buffer.
 * ============================================================ */

static bool ReceiveProgram(void)
{
    static bool receiving = false;


    /*
     * Temporary buffer for one input line.
     */

    static char line_buffer[
        VERISHOT_RX_BUFFER_SIZE
    ];


    static uint16_t line_length = 0;


    while (Serial.available())
    {
        char c = Serial.read();


        /* ----------------------------------------------------
         * Ignore carriage return
         * ---------------------------------------------------- */

        if (c == '\r')
        {
            continue;
        }


        /* ----------------------------------------------------
         * Normal character
         * ---------------------------------------------------- */

        if (c != '\n')
        {
            if (
                line_length <
                VERISHOT_RX_BUFFER_SIZE - 1
            )
            {
                line_buffer[line_length] =
                    c;

                line_length++;

                line_buffer[line_length] =
                    '\0';
            }
            else
            {
                /*
                 * Input line too long.
                 */

                line_length = 0;

                source_length = 0;

                line_buffer[0] = '\0';

                source_buffer[0] = '\0';

                receiving = false;


                Serial.println();

                Serial.println(
                    F(
                        "ERROR: INPUT LINE TOO LONG."
                    )
                );


                return false;
            }


            continue;
        }


        /* ----------------------------------------------------
         * End of line
         * ---------------------------------------------------- */

        line_buffer[line_length] =
            '\0';


        /* ----------------------------------------------------
         * Check END command
         * ---------------------------------------------------- */

        if (
            strcmp(
                line_buffer,
                "END"
            ) == 0
        )
        {
            /*
             * END is valid only after at least one source
             * line has been received.
             */

            if (!receiving)
            {
                Serial.println(
                    F(
                        "ERROR: No source program received."
                    )
                );


                line_length = 0;

                return false;
            }


            /*
             * Terminate source buffer.
             */

            source_buffer[source_length] =
                '\0';


            /*
             * Reset receiver for next program.
             */

            line_length = 0;

            receiving = false;


            Serial.println();

            Serial.println(
                F(
                    "END received. Source complete."
                )
            );


            return true;
        }


        /* ----------------------------------------------------
         * Normal source line
         * ---------------------------------------------------- */

        if (line_length > 0)
        {
            /*
             * Need room for:
             *
             *     line
             *     newline
             *     '\0'
             */

            if (
                source_length +
                line_length + 1 >=
                VERISHOT_RX_BUFFER_SIZE
            )
            {
                source_length = 0;

                line_length = 0;

                receiving = false;

                source_buffer[0] =
                    '\0';

                line_buffer[0] =
                    '\0';


                Serial.println();

                Serial.println(
                    F(
                        "ERROR: SOURCE BUFFER OVERFLOW."
                    )
                );


                return false;
            }


            /*
             * Copy line into source buffer.
             */

            memcpy(
                &source_buffer[source_length],
                line_buffer,
                line_length
            );


            source_length +=
                line_length;


            /*
             * Preserve newline.
             */

            source_buffer[source_length] =
                '\n';

            source_length++;


            /*
             * Keep source null terminated.
             */

            source_buffer[source_length] =
                '\0';


            receiving = true;
        }


        /* ----------------------------------------------------
         * Prepare for next line
         * ---------------------------------------------------- */

        line_length = 0;

        line_buffer[0] = '\0';
    }


    return false;
}


/* ============================================================
 * LEXER STAGE
 * ============================================================ */

static bool RunLexer(void)
{
    LexerStatus status;


    Serial.println();

    Serial.println(
        F("[1] LEXICAL ANALYSIS")
    );

    Serial.println(
        F("----------------------------------------")
    );


    /*
     * Give lexer the newly received source.
     */

    Lexer_Init(
        &lexer,
        source_buffer
    );


    /*
     * Tokenize.
     */

    status = Lexer_Tokenize(
        &lexer
    );


    /*
     * Check result.
     */

    if (status != LEXER_OK)
    {
        Serial.println(
            F("Lexer FAILED.")
        );

        return false;
    }


    Serial.println(
        F("Lexer PASSED.")
    );


    Serial.print(
        F("Tokens generated: ")
    );

    Serial.println(
        lexer.token_count
    );


    /*
     * Print token stream.
     */

    Lexer_PrintTokens(
        &lexer
    );


    return true;
}


/* ============================================================
 * PARSER STAGE
 * ============================================================ */

static bool RunParser(void)
{
    ParserStatus status;


    Serial.println();

    Serial.println(
        F("[2] SYNTAX ANALYSIS")
    );

    Serial.println(
        F("----------------------------------------")
    );


    /*
     * Supply lexer token stream to parser.
     */

    Parser_Init(
        &parser,
        lexer.tokens,
        lexer.token_count
    );


    /*
     * Parse.
     */

    status = Parser_Parse(
        &parser
    );


    /*
     * Check result.
     */

    if (status != PARSER_OK)
    {
        Serial.println(
            F("Parser FAILED.")
        );

        return false;
    }


    Serial.println(
        F("Parser PASSED.")
    );


    Serial.print(
        F("AST nodes generated: ")
    );

    Serial.println(
        parser.ast_node_count
    );


    /*
     * Print AST / parsed program.
     */

    Parser_PrintAST(
        &parser
    );


    return true;
}


/* ============================================================
 * SEMANTIC ANALYSIS STAGE
 * ============================================================ */

static bool RunSemanticAnalyzer(void)
{
    SemanticStatus status;


    Serial.println();

    Serial.println(
        F("[3] SEMANTIC ANALYSIS")
    );

    Serial.println(
        F("----------------------------------------")
    );


    /*
     * Semantic_Analyze() initializes the analyzer internally.
     */

    status = Semantic_Analyze(
        &semantic,
        &parser
    );


    /*
     * Check result.
     */

    if (status != SEMANTIC_OK)
    {
        Serial.println(
            F("Semantic analysis FAILED.")
        );

        return false;
    }


    Serial.println(
        F("Semantic analysis PASSED.")
    );


    /*
     * Print symbol table.
     */

    Semantic_PrintSymbols(
        &semantic
    );


    return true;
}


/* ============================================================
 * LUT MAPPING STAGE
 * ============================================================ */

static bool RunMapper(void)
{
    MapperStatus status;


    Serial.println();

    Serial.println(
        F("[4] LUT MAPPING & ROUTING")
    );

    Serial.println(
        F("----------------------------------------")
    );


    status = Mapper_Map(
        &mapper,
        &parser
    );


    /*
     * Check result.
     */

    if (status != MAPPER_OK)
    {
        Serial.println(
            F("LUT mapping FAILED.")
        );

        return false;
    }


    Serial.println(
        F("LUT mapping PASSED.")
    );

    Serial.println();


    /*
     * IMPORTANT:
     *
     * The new mapper API uses Mapper_PrintConfig().
     */

    Mapper_PrintConfig(
        &mapper
    );


    return true;
}


/* ============================================================
 * BITSTREAM STAGE
 * ============================================================
 *
 * Converts:
 *
 *     mapper.config
 *
 * into:
 *
 *     bitstream.lut_bytes[4]
 *     bitstream.route_bytes[3]
 * ============================================================ */

static bool RunBitstream(void)
{
    Serial.println();
    Serial.println("=== BITSTREAM GENERATION ===");

    Bitstream_Generate(
        &mapper.config,
        &bitstream
    );

    Serial.println("Bitstream generated successfully.");

    Serial.println("LUT bytes:");
    for (uint8_t i = 0; i < VERISHOT_LUT_BYTE_COUNT; i++)
    {
        Serial.print("  LUT");
        Serial.print(i);
        Serial.print(" = 0x");

        if (bitstream.lut_bytes[i] < 0x10)
            Serial.print("0");

        Serial.println(bitstream.lut_bytes[i], HEX);
    }

    Serial.println("Route bytes:");
    for (uint8_t i = 0; i < VERISHOT_ROUTE_BYTE_COUNT; i++)
    {
        Serial.print("  ROUTE");
        Serial.print(i);
        Serial.print(" = 0x");

        if (bitstream.route_bytes[i] < 0x10)
            Serial.print("0");

        Serial.println(bitstream.route_bytes[i], HEX);
    }

    return true;
}

/* ============================================================
 * ERROR REPORTING
 * ============================================================ */

static void PrintCompilerError(void)
{
    Serial.println();

    Serial.println(
        F("========================================")
    );

    Serial.println(
        F("          COMPILATION FAILED            ")
    );

    Serial.println(
        F("========================================")
    );


    /* --------------------------------------------------------
     * Lexer error
     * -------------------------------------------------------- */

    if (
        lexer.status != LEXER_OK
    )
    {
        Serial.println(
            F("Stage : LEXER")
        );

        Serial.print(
            F("Status code: ")
        );

        Serial.println(
            (int)lexer.status
        );

        return;
    }


    /* --------------------------------------------------------
     * Parser error
     * -------------------------------------------------------- */

    if (
        parser.status != PARSER_OK
    )
    {
        Serial.println(
            F("Stage : PARSER")
        );

        Serial.print(
            F("Error : ")
        );

        Serial.println(
            Parser_StatusToString(
                parser.status
            )
        );

        return;
    }


    /* --------------------------------------------------------
     * Semantic error
     * -------------------------------------------------------- */

    if (
        semantic.status != SEMANTIC_OK
    )
    {
        Serial.println(
            F("Stage : SEMANTIC ANALYZER")
        );

        Serial.print(
            F("Error : ")
        );

        Serial.println(
            Semantic_StatusToString(
                semantic.status
            )
        );

        return;
    }


    /* --------------------------------------------------------
     * Mapper error
     * -------------------------------------------------------- */

    if (
        mapper.status != MAPPER_OK
    )
    {
        Serial.println(
            F("Stage : LUT MAPPER")
        );

        Serial.print(
            F("Error : ")
        );

        Serial.println(
            Mapper_StatusToString(
                mapper.status
            )
        );

        return;
    }


    /* --------------------------------------------------------
     * Unknown error
     * -------------------------------------------------------- */

    Serial.println(
        F("Stage : UNKNOWN")
    );

    Serial.println(
        F("Error : UNKNOWN")
    );
}
