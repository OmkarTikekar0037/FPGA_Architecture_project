# VERISHOT — Language & Compiler Architecture
## Software Design Progress — Phase 1

> **VERISHOT = Very Short Verilog**
>
> VERISHOT is a small, Verilog-like hardware description language designed specifically for the project's discrete FPGA fabric. It is **not intended to implement full Verilog**. Its syntax and compiler are deliberately restricted to the capabilities of the target architecture.

---

# 1. Project Goal

The software goal is to build a miniature FPGA compiler running on an STM32.

The user should be able to enter a short, Verilog-like Boolean hardware description through a serial terminal. The compiler will eventually:

1. Parse the VERISHOT source.
2. Determine whether the design can be implemented by the target FPGA architecture.
3. Decompose the logic into LUT-sized functions.
4. Assign logical functions to physical LUTs.
5. Determine the required inter-LUT routing.
6. Generate LUT configuration bits.
7. Generate switch-matrix/MUX select bits.
8. Pack the configuration into a physical configuration bitstream.
9. Send the configuration to the FPGA through SIPO shift registers.

Conceptually:

```text
VERISHOT source
      |
      v
    Lexer
      |
      v
    Parser
      |
      v
     AST
      |
      v
  Logic / IR
      |
      v
 LUT Mapping
      |
      v
   Placement
      |
      v
   Routing
      |
      v
 Bitstream Generation
      |
      v
      SIPO
      |
      v
 Physical FPGA
```

The compiler is therefore a miniature version of the conceptual FPGA toolchain:

```text
HDL
 -> synthesis
 -> technology mapping
 -> placement
 -> routing
 -> configuration generation
```

The implementation will be much smaller and more specialized than an industrial FPGA tool.

---

# 2. Target Architecture Known to the Compiler

The compiler is being designed around a fixed Version 1 hardware architecture:

- 4 LUTs.
- Each LUT is a 3-input LUT.
- Each LUT can implement any Boolean function of three signals.
- Each LUT input is connected through a 4:1 MUX.
- There are 3 routing MUXes per LUT.
- Therefore there are 12 routing MUXes.
- Each routing MUX requires 2 select bits.
- LUT configuration requires 4 × 8 = 32 bits.
- Routing configuration requires 12 × 2 = 24 bits.
- Total meaningful configuration = 56 bits.
- A convenient physical representation is an 8-byte / 64-bit configuration packet, leaving 8 bits reserved.

The current architecture is combinational only.

Sequential logic and DFFs are deliberately excluded from the current compiler version.

---

# 3. VERISHOT Language Philosophy

VERISHOT is intentionally:

> **Verilog-like syntax + Boolean expression semantics + restrictions based on the physical FPGA architecture.**

It is not a general-purpose Verilog implementation.

The language should be:

- Small.
- Explicit.
- Easy to parse.
- Deterministic.
- Suitable for implementation on an STM32.
- Closely connected to the capabilities of the target FPGA.

The compiler should reject a valid-looking Boolean design if it cannot be mapped onto the physical fabric.

---

# 4. Initial VERISHOT HDL

The initial language will support:

```text
input
output
assign
```

Boolean operators:

```text
&       AND
|       OR
^       XOR
~       NOT
```

Parentheses are supported for grouping.

Boolean constants:

```text
0
1
```

Punctuation:

```text
(
)
,
;
=
```

Comments:

```text
// comment
```

Whitespace includes spaces, tabs, carriage returns, and newlines.

---

# 5. Example VERISHOT Program

A basic program can look like:

```verishot
input A, B, C;
output F;

assign F = (A & B) ^ C;
```

A multi-stage description can eventually look like:

```verishot
input A, B, C, D;
output F;

assign X = A & B;
assign Y = C & D;
assign F = X ^ Y;
```

The latter form is especially useful in early compiler development because the intermediate decomposition is explicitly specified.

---

# 6. Language Scope Decisions

The following are intentionally **not part of the initial language**:

```text
always
begin / end
reg
wire
if / else
case
generate
parameters
nonblocking assignments
clocked logic
sequential logic
```

The project will not attempt to implement complete Verilog.

Additional constructs can be considered after the basic combinational compiler works.

---

# 7. Compiler Architecture

The compiler is divided into independent stages.

## 7.1 Input / Command Interface

The current communication decision is:

```text
PC Serial Monitor
       |
      USB
       |
      FTDI
       |
     UART
       |
     STM32
```

The STM32 receives VERISHOT source commands over UART through an FTDI USB-to-UART bridge.

The STM32 sends compiler output back through UART to the FTDI and therefore to the PC Serial Monitor.

The compiler itself should not be coupled to the UART peripheral.

The intended architecture is:

```text
UART / I/O layer
       |
       v
input buffer
       |
       v
VERISHOT compiler
       |
       v
output layer
       |
       v
UART / FTDI
```

HAL is used at the hardware/I/O boundary. The compiler modules should remain ordinary C wherever possible.

---

# 8. Compiler Stages

## 8.1 Lexer

The lexer converts raw source characters into tokens.

Example:

```verishot
assign F = (A & B) ^ C;
```

becomes conceptually:

```text
TOKEN_ASSIGN
TOKEN_IDENTIFIER("F")
TOKEN_EQUAL
TOKEN_LPAREN
TOKEN_IDENTIFIER("A")
TOKEN_AND
TOKEN_IDENTIFIER("B")
TOKEN_RPAREN
TOKEN_XOR
TOKEN_IDENTIFIER("C")
TOKEN_SEMICOLON
```

The lexer does **not** understand Boolean logic, operator precedence, LUTs, or routing.

Its job is only lexical analysis.

---

## 8.2 Parser

The parser consumes tokens produced by the lexer and determines their grammatical structure.

A recursive-descent parser has been selected for the initial implementation because:

- The language grammar is small.
- Operator precedence can be represented naturally.
- It is easy to implement in C.
- It is easy to debug.
- It avoids the complexity of a parser generator.
- It is appropriate for a small embedded compiler.

---

## 8.3 AST

The parser will construct an Abstract Syntax Tree.

For:

```verishot
assign F = (A & B) ^ C;
```

the logical structure is:

```text
          XOR
         /   \
       AND    C
      /   \
     A     B
```

The AST represents what the user wrote.

It is deliberately separate from the eventual hardware representation.

---

## 8.4 Logic IR / Netlist

The compiler will eventually need an intermediate representation that describes the hardware-oriented logic network.

For:

```verishot
assign F = (A & B) ^ (C & D);
```

the compiler can produce:

```text
N0 = A & B
N1 = C & D
N2 = N0 ^ N1
```

This is different from the AST.

The distinction is:

```text
AST
    = what the user wrote

Logic IR / Netlist
    = logical hardware network to be mapped
```

This separation is important for later LUT mapping and routing.

---

# 9. LUT Mapping Strategy

A 3-input LUT can implement any Boolean function of three signals.

Therefore, if a Boolean expression depends on three or fewer independent signals, it can potentially be represented by one LUT.

For example:

```verishot
assign F = A & B & C;
```

can be mapped to one LUT.

For larger logic, the compiler needs to decompose the logic.

Example:

```verishot
assign F = (A & B) ^ (C & D);
```

can become:

```text
L0 = A & B
L1 = C & D
L2 = L0 ^ L1
```

This uses three LUTs.

The compiler must ultimately verify:

```text
required LUTs <= 4
```

However, LUT count alone is not sufficient for compilation success.

A design must also satisfy routing constraints.

Therefore:

```text
Compilation successful
    only if
    LUT mapping is valid
    AND
    routing is valid
```

---

# 10. LUT Assignment Strategy

The first implementation will use a simple placement strategy:

> **First-fit LUT allocation.**

For example:

```text
first logical LUT  -> L0
second logical LUT -> L1
third logical LUT  -> L2
fourth logical LUT -> L3
```

No optimization is initially required.

If five LUTs are required:

```text
ERROR: LUT resources exhausted.
Available LUTs: 4.
```

Optimization can be added later.

The initial priority is:

```text
correctness > optimization
```

---

# 11. Routing Strategy

Once logical functions have been assigned to physical LUTs, the compiler determines what each LUT input must receive.

Example:

```text
L2 = L0 ^ L1
```

requires:

```text
L2.A <- L0
L2.B <- L1
```

The compiler then converts these logical connections into the fixed 2-bit select values of the physical 4:1 routing MUXes.

The compiler should therefore follow:

```text
Boolean source
      |
      v
AST
      |
      v
Logic network
      |
      v
LUT assignment
      |
      v
Physical connections
      |
      v
MUX select values
```

It should not attempt to generate routing bits directly from the original Boolean expression.

---

# 12. Routing Source Model

Each LUT input has a 4:1 MUX.

The source mapping is fixed:

### LUT0

```text
00 -> external input
01 -> LUT1
10 -> LUT2
11 -> LUT3
```

### LUT1

```text
00 -> external input
01 -> LUT0
10 -> LUT2
11 -> LUT3
```

### LUT2

```text
00 -> external input
01 -> LUT0
10 -> LUT1
11 -> LUT3
```

### LUT3

```text
00 -> external input
01 -> LUT0
10 -> LUT1
11 -> LUT2
```

A LUT must not select its own output as a routing source in Version 1.

This avoids accidental combinational feedback.

---

# 13. LUT Configuration Convention

The compiler must preserve the experimentally verified LUT truth-table convention.

For each 3-input LUT:

```text
S0 = A
S1 = B
S2 = C
```

The MUX addresses are:

```text
000 -> D0
001 -> D1
010 -> D2
011 -> D3
100 -> D4
101 -> D5
110 -> D6
111 -> D7
```

The configuration byte is represented as:

```text
D7 D6 D5 D4 D3 D2 D1 D0
```

Bit n corresponds directly to Dn.

Therefore:

```c
config |= function_output << n;
```

No bit reversal is performed.

Known verified examples:

```text
A & B & C       -> 0x80
A | B | C       -> 0xFE
A ^ B ^ C       -> 0x96
~(A ^ B ^ C)    -> 0x69
AB + AC + BC    -> 0xE8
A & B           -> 0x88
```

These values should become golden test cases for the compiler.

---

# 14. Configuration Representation

The compiler will maintain a logical configuration structure similar to:

```c
typedef struct
{
    uint8_t lut_config[4];
    uint8_t route_select[4][3];

} FPGA_Config;
```

Conceptually:

```text
lut_config[0] -> L0 truth table
lut_config[1] -> L1 truth table
lut_config[2] -> L2 truth table
lut_config[3] -> L3 truth table
```

and:

```text
route_select[lut][input]
```

contains the logical routing selection for the corresponding LUT input.

This logical configuration is later converted to the physical 64-bit configuration packet.

---

# 15. Configuration Packing

The current proposed physical packet is 8 bytes:

```text
BYTE 0 -> L0 truth table
BYTE 1 -> L1 truth table
BYTE 2 -> L2 truth table
BYTE 3 -> L3 truth table
```

Then:

```text
BYTE 4
bits 1:0 -> L0.A select
bits 3:2 -> L0.B select
bits 5:4 -> L0.C select
bits 7:6 -> reserved

BYTE 5
bits 1:0 -> L1.A select
bits 3:2 -> L1.B select
bits 5:4 -> L1.C select
bits 7:6 -> reserved

BYTE 6
bits 1:0 -> L2.A select
bits 3:2 -> L2.B select
bits 5:4 -> L2.C select
bits 7:6 -> reserved

BYTE 7
bits 1:0 -> L3.A select
bits 3:2 -> L3.B select
bits 5:4 -> L3.C select
bits 7:6 -> reserved
```

The exact physical shift-register ordering will be verified when the SIPO hardware interface is implemented.

---

# 16. Lexer Design — Phase 1

The first compiler implementation stage is the lexer.

The lexer has been designed around fixed-size storage because the final target is an STM32.

Initial limits:

```c
#define VERISHOT_MAX_SOURCE_LENGTH  256
#define VERISHOT_MAX_TOKENS         64
#define VERISHOT_MAX_LEXEME_LENGTH  16
```

These are implementation limits for the first version and can be changed later.

---

# 17. Token Types

The initial token enumeration is:

```c
typedef enum
{
    TOKEN_INPUT,
    TOKEN_OUTPUT,
    TOKEN_ASSIGN,

    TOKEN_IDENTIFIER,
    TOKEN_CONSTANT,

    TOKEN_AND,
    TOKEN_OR,
    TOKEN_XOR,
    TOKEN_NOT,

    TOKEN_EQUAL,

    TOKEN_LPAREN,
    TOKEN_RPAREN,

    TOKEN_COMMA,
    TOKEN_SEMICOLON,

    TOKEN_EOF,
    TOKEN_ERROR

} TokenType;
```

---

# 18. Token Structure

The initial token structure is:

```c
typedef struct
{
    TokenType type;

    char lexeme[VERISHOT_MAX_LEXEME_LENGTH];

    uint16_t line;
    uint16_t column;

} Token;
```

Lexemes are intentionally **copied** into the token rather than referenced through pointers into the source buffer.

Reason:

- Simpler lifetime management.
- Easier debugging.
- Safer when the input buffer is reused.
- More straightforward for the initial STM32 implementation.

The memory overhead is accepted for the first version.

---

# 19. Lexer Structure

The lexer maintains:

```c
typedef struct
{
    const char *source;

    uint16_t position;

    uint16_t line;
    uint16_t column;

    Token tokens[VERISHOT_MAX_TOKENS];

    uint16_t token_count;

    LexerStatus status;

} Lexer;
```

The lexer therefore keeps track of:

- Current source string.
- Current character position.
- Line number.
- Column number.
- Generated tokens.
- Number of generated tokens.
- Lexer status.

---

# 20. Lexer Status

The initial status model includes:

```text
LEXER_OK
LEXER_TOKEN_OVERFLOW
LEXER_IDENTIFIER_TOO_LONG
LEXER_INVALID_CONSTANT
LEXER_ERROR
```

The lexer should fail clearly rather than silently truncating or ignoring invalid input.

Example:

```text
VERISHOT ERROR:
Identifier too long at line 1, column 8
```

will eventually be possible because tokens store source location.

---

# 21. Identifier Rules

Initial identifier syntax:

```text
[A-Za-z_][A-Za-z0-9_]*
```

Examples:

```text
A
B
Cin
SUM
carry
TEMP1
my_signal
```

Invalid examples:

```text
1A
2signal
```

Keywords are recognized by comparing an identifier against the reserved words:

```text
input
output
assign
```

For example:

```text
input
```

is emitted as:

```text
TOKEN_INPUT
```

while:

```text
Cin
```

is emitted as:

```text
TOKEN_IDENTIFIER
```

The initial language is case-sensitive.

Therefore:

```text
input
```

and:

```text
INPUT
```

are different strings, and only the lowercase keyword is recognized.

---

# 22. Constants

VERISHOT V1 supports Boolean constants:

```text
0
1
```

Any other digit is invalid.

Multi-digit numbers such as:

```text
10
101
```

are not part of the initial Boolean constant syntax.

---

# 23. Comments

The lexer supports:

```text
// comment
```

Everything from `//` until the next newline is ignored.

---

# 24. Error Handling Philosophy

The initial lexer should stop at the first lexical error.

For example:

```verishot
assign F = A @ B;
```

should produce an error for:

```text
@
```

rather than silently ignoring it.

Error recovery can be introduced later.

The goal of the first implementation is deterministic and easily debuggable behavior.

---

# 25. Memory Strategy

The compiler is intended to run on an STM32.

Therefore the initial implementation deliberately avoids:

```c
malloc()
free()
```

and other unnecessary dynamic allocation.

The preferred design is:

```text
fixed-size buffers
fixed-size token arrays
deterministic memory usage
```

This makes the compiler easier to reason about on an embedded target.

If memory becomes a problem later, data structures can be optimized.

---

# 26. Compiler Development Strategy

The compiler should first be implemented and tested independently from the final hardware configuration.

The recommended progression is:

```text
V0
Boolean expression -> truth table -> LUT configuration byte

V1
Lexer + parser

V2
Single-LUT compiler

V3
Explicit multi-LUT assignments

V4
Automatic multi-LUT decomposition

V5
LUT placement

V6
Routing generation and validation

V7
Configuration packing / bitstream generation

V8
STM32 UART interface

V9
SIPO hardware configuration

V10
Physical end-to-end compilation and FPGA configuration

Future
Optimization and sequential logic
```

The compiler core should remain independent from the STM32 communication layer as much as possible.

---

# 27. Optimization Strategy

Optimization is deliberately postponed.

The first compiler should prioritize:

```text
correctness
```

over:

```text
minimum LUT count
```

For example, Boolean simplification and common-subexpression optimization are not initial requirements.

Later optimization may include:

- Boolean simplification.
- Common subexpression sharing.
- LUT count reduction.
- Better LUT placement.
- Routing-aware mapping.

But none of these should complicate the first working compiler.

---

# 28. Current Phase

The project is currently at:

```text
PHASE 1 — VERISHOT FRONT END
```

Specifically:

```text
Language definition
        |
        v
Lexer design     <- CURRENT WORK
        |
        v
Parser
        |
        v
AST
```

The immediate implementation target is a working STM32 lexer that can accept a VERISHOT source string and produce a token array.

Example input:

```verishot
input A, B, C;
output F;
assign F = (A & B) ^ C;
```

Expected lexical output:

```text
INPUT
IDENTIFIER("A")
COMMA
IDENTIFIER("B")
COMMA
IDENTIFIER("C")
SEMICOLON

OUTPUT
IDENTIFIER("F")
SEMICOLON

ASSIGN
IDENTIFIER("F")
EQUAL
LPAREN
IDENTIFIER("A")
AND
IDENTIFIER("B")
RPAREN
XOR
IDENTIFIER("C")
SEMICOLON

EOF
```

Once this works reliably, the next major stage is the VERISHOT grammar and recursive-descent parser.

---

# 29. Important Design Principle

The compiler must maintain strict separation between:

```text
VERISHOT language
    |
    | what the user writes
    v

Compiler
    |
    | interprets and transforms the design
    v

Target FPGA architecture
    |
    | physical resources and routing constraints
    v

Bitstream
    |
    | actual configuration bits
    v

SIPO / hardware
```

In particular:

```text
LUT
    = programmable Boolean function

Routing MUX
    = determines what signal feeds a LUT input

Compiler
    = translates user intent into LUT functions and routing connections

Bitstream
    = physical representation of that configuration
```

These responsibilities should not be mixed together.

---

# 30. Current Project Definition

**VERISHOT** is a deliberately small HDL and compiler created specifically for the project's 4-LUT discrete FPGA.

Its central purpose is to demonstrate that a user can describe Boolean hardware at a high level and have an embedded compiler automatically transform that description into:

```text
Boolean logic
    ->
LUT configuration
    +
programmable routing
    ->
physical FPGA configuration
```

The compiler is therefore not merely a configuration-bit calculator.

It is intended to be a miniature:

```text
HDL frontend
+
logic synthesizer
+
technology mapper
+
placer
+
router
+
bitstream generator
```

implemented specifically for a small, physically constructed FPGA architecture.
