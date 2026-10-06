#include <Arduino.h>

#include "verishot_lexer.h"
#include "verishot_parser.h"
#include "semantic.h"


/* ============================================================
 * VERISHOT CONFIGURATION
 * ============================================================ */

#define VERISHOT_BAUD_RATE       115200
#define VERISHOT_RX_BUFFER_SIZE  VERISHOT_MAX_SOURCE_LENGTH


/* ============================================================
 * COMPILER OBJECTS
 * ============================================================ */

Lexer lexer;

Parser parser;

SemanticAnalyzer semantic;


/* ============================================================
 * SOURCE BUFFER
 * ============================================================ */

char source_buffer[VERISHOT_RX_BUFFER_SIZE];

uint16_t source_length = 0;


/* ============================================================
 * FUNCTION PROTOTYPES
 * ============================================================ */

static bool ReceiveProgram(void);

static bool RunLexer(void);

static bool RunParser(void);

static bool RunSemanticAnalyzer(void);

static void PrintCompilerError(void);


/* ============================================================
 * SETUP
 * ============================================================ */

void setup()
{
    Serial.begin(VERISHOT_BAUD_RATE);

    /*
     * Give the serial interface a moment to initialize.
     */
    delay(100);

    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("          VERISHOT COMPILER             "));
    Serial.println(F("========================================"));

    Serial.println(F("Initializing compiler..."));

    /*
     * Initialize source buffer.
     */
    source_length = 0;
    source_buffer[0] = '\0';


    /*
     * Initialize compiler modules.
     *
     * Lexer will be initialized again when a new source
     * program is received. This is intentional because the
     * lexer needs the current source buffer.
     */
    Lexer_Init(
        &lexer,
        source_buffer
    );


    /*
     * Parser initially has no token stream.
     *
     * The actual token array will be supplied after
     * lexical analysis.
     */
    Parser_Init(
        &parser,
        nullptr,
        0
    );


    /*
     * Initialize semantic analyzer.
     */
    Semantic_Init(
        &semantic
    );


    Serial.println(F("Lexer            : READY"));
    Serial.println(F("Parser           : READY"));
    Serial.println(F("Semantic Analyzer: READY"));

    Serial.println();
    Serial.println(F("VeriShot is ready."));
    Serial.println(F("Enter a VeriShot program."));
    Serial.println(F("Send an empty line to compile."));
    Serial.println();

}


/* ============================================================
 * MAIN LOOP
 * ============================================================ */

void loop()
{
    /*
     * --------------------------------------------------------
     * STEP 1
     *
     * Receive a complete VeriShot program.
     * --------------------------------------------------------
     */

    if (!ReceiveProgram())
    {
        return;
    }


    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("        VERISHOT COMPILATION            "));
    Serial.println(F("========================================"));


    /*
     * --------------------------------------------------------
     * STEP 2
     *
     * Lexical analysis.
     * --------------------------------------------------------
     */

    if (!RunLexer())
    {
        PrintCompilerError();
        return;
    }


    /*
     * --------------------------------------------------------
     * STEP 3
     *
     * Syntax analysis / parsing.
     * --------------------------------------------------------
     */

    if (!RunParser())
    {
        PrintCompilerError();
        return;
    }


    /*
     * --------------------------------------------------------
     * STEP 4
     *
     * Semantic analysis.
     * --------------------------------------------------------
     */

    if (!RunSemanticAnalyzer())
    {
        PrintCompilerError();
        return;
    }


    /*
     * --------------------------------------------------------
     * STEP 5
     *
     * Frontend compilation successful.
     *
     * Configuration generator will be inserted here.
     * --------------------------------------------------------
     */

    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("       COMPILATION SUCCESSFUL            "));
    Serial.println(F("========================================"));

    Serial.println();
    Serial.println(F("VeriShot program is semantically valid."));
    Serial.println();

    /*
     * NEXT STAGE:
     *
     * GenerateConfiguration(&parser);
     *
     * This will eventually convert the parsed/validated
     * program into LUT and routing configuration data.
     */
}


/* ============================================================
 * RECEIVE PROGRAM
 * ============================================================ */

/*
 * The current serial protocol is:
 *
 *     input A, B;
 *     output Y;
 *     assign Y = A & B;
 *
 *     <empty line>
 *
 * The empty line tells the compiler that the complete
 * source program has been received.
 */

static bool ReceiveProgram(void)
{
    static bool receiving = false;

    /*
     * Temporary buffer for the current input line.
     */
    static char line_buffer[VERISHOT_RX_BUFFER_SIZE];

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
         * NORMAL CHARACTER
         * ---------------------------------------------------- */

        if (c != '\n')
        {
            if (line_length <
                VERISHOT_RX_BUFFER_SIZE - 1)
            {
                line_buffer[line_length] = c;

                line_length++;

                line_buffer[line_length] = '\0';
            }
            else
            {
                /*
                 * Current line is too long.
                 */
                line_length = 0;

                source_length = 0;

                line_buffer[0] = '\0';
                source_buffer[0] = '\0';

                receiving = false;

                Serial.println();
                Serial.println(
                    F("ERROR: INPUT LINE TOO LONG.")
                );

                return false;
            }

            continue;
        }


        /* ----------------------------------------------------
         * END OF LINE
         * ---------------------------------------------------- */

        line_buffer[line_length] = '\0';


        /* ----------------------------------------------------
         * CHECK FOR END COMMAND
         * ---------------------------------------------------- */

        if (strcmp(line_buffer, "END") == 0)
        {
            /*
             * END is only valid after receiving some source.
             */
            if (!receiving)
            {
                Serial.println(
                    F("ERROR: No source program received.")
                );

                line_length = 0;

                return false;
            }


            /*
             * Terminate source buffer.
             */
            source_buffer[source_length] = '\0';


            /*
             * Reset line receiver for next program.
             */
            line_length = 0;

            receiving = false;


            Serial.println();
            Serial.println(
                F("END received. Source complete.")
            );

            return true;
        }


        /* ----------------------------------------------------
         * NORMAL SOURCE LINE
         * ---------------------------------------------------- */

        if (line_length > 0)
        {
            /*
             * Make sure there is room for:
             *
             *     line
             *     newline
             *     '\0'
             */
            if (source_length +
                    line_length + 1 >=
                VERISHOT_RX_BUFFER_SIZE)
            {
                source_length = 0;

                line_length = 0;

                receiving = false;

                source_buffer[0] = '\0';
                line_buffer[0] = '\0';

                Serial.println();
                Serial.println(
                    F("ERROR: SOURCE BUFFER OVERFLOW.")
                );

                return false;
            }


            /*
             * Copy the line into the source buffer.
             */
            memcpy(
                &source_buffer[source_length],
                line_buffer,
                line_length
            );

            source_length += line_length;


            /*
             * Preserve newline.
             */
            source_buffer[source_length] = '\n';

            source_length++;


            /*
             * Keep source null terminated.
             */
            source_buffer[source_length] = '\0';


            receiving = true;
        }


        /*
         * Prepare for next line.
         */
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
    Serial.println(F("[1] LEXICAL ANALYSIS"));
    Serial.println(F("----------------------------------------"));


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
        Serial.println(F("Lexer FAILED."));

        return false;
    }


    Serial.println(F("Lexer PASSED."));

    Serial.print(F("Tokens generated: "));
    Serial.println(lexer.token_count);


    /*
     * Print token stream for debugging.
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
    Serial.println(F("[2] SYNTAX ANALYSIS"));
    Serial.println(F("----------------------------------------"));


    /*
     * Give parser the token array generated by lexer.
     */
    Parser_Init(
        &parser,
        lexer.tokens,
        lexer.token_count
    );


    /*
     * Parse the token stream.
     */
    status = Parser_Parse(
        &parser
    );


    /*
     * Check parser result.
     */
    if (status != PARSER_OK)
    {
        Serial.println(F("Parser FAILED."));

        return false;
    }


    Serial.println(F("Parser PASSED."));

    Serial.print(F("AST nodes generated: "));
    Serial.println(parser.ast_node_count);


    /*
     * Print parsed program and AST.
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
    Serial.println(F("[3] SEMANTIC ANALYSIS"));
    Serial.println(F("----------------------------------------"));


    /*
     * Semantic_Analyze() initializes the analyzer itself.
     */
    status = Semantic_Analyze(
        &semantic,
        &parser
    );


    /*
     * Check semantic result.
     */
    if (status != SEMANTIC_OK)
    {
        Serial.println(F("Semantic analysis FAILED."));

        return false;
    }


    Serial.println(F("Semantic analysis PASSED."));


    /*
     * Print symbol table.
     */
    Semantic_PrintSymbols(
        &semantic
    );


    return true;
}


/* ============================================================
 * ERROR REPORTING
 * ============================================================ */

static void PrintCompilerError(void)
{
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(F("          COMPILATION FAILED            "));
    Serial.println(F("========================================"));


    /*
     * --------------------------------------------------------
     * LEXER ERROR
     * --------------------------------------------------------
     *
     * We intentionally DO NOT call:
     *
     *     Lexer_StatusToString()
     *
     * because it is not declared in verishot_lexer.h.
     *
     * The .ino therefore remains compatible with the
     * actual public lexer interface.
     */

    if (lexer.status != LEXER_OK)
    {
        Serial.println(F("Stage : LEXER"));

        Serial.print(F("Status code: "));
        Serial.println(
            (int)lexer.status
        );

        return;
    }


    /*
     * --------------------------------------------------------
     * PARSER ERROR
     * --------------------------------------------------------
     */

    if (parser.status != PARSER_OK)
    {
        Serial.println(F("Stage : PARSER"));

        Serial.print(F("Error : "));
        Serial.println(
            Parser_StatusToString(
                parser.status
            )
        );

        return;
    }


    /*
     * --------------------------------------------------------
     * SEMANTIC ERROR
     * --------------------------------------------------------
     */

    if (semantic.status != SEMANTIC_OK)
    {
        Serial.println(F("Stage : SEMANTIC ANALYZER"));

        Serial.print(F("Error : "));
        Serial.println(
            Semantic_StatusToString(
                semantic.status
            )
        );

        return;
    }


    /*
     * --------------------------------------------------------
     * UNKNOWN ERROR
     * --------------------------------------------------------
     */

    Serial.println(F("Stage : UNKNOWN"));
    Serial.println(F("Error : UNKNOWN"));
}