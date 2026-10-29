// #define SER1   12
// #define RCLK1  11
// #define SCLK1  10

// // 74LS151 select lines
// #define S0    6
// #define S1    5
// #define S2    

#define SER1   17
#define RCLK1  5
#define SCLK1  18

// 74LS151 select lines
#define S0    2
#define S1    4
#define S2    16

#define SER2   19
#define RCLK2  21 
#define SCLK2  22

// 74LS151 select lines
#define S01    12
#define S11    14




// ============================================================
// Current gate
// ============================================================

String currentGate = "";


// ============================================================
// Load LUT configuration into 74HC595
// ============================================================

void loadLUT(byte data)
{
  // Shift data into 74HC595

  digitalWrite(RCLK1, LOW);

  shiftOut(SER1, SCLK11, MSBFIRST, data);

  // Latch data to QA-QH

  digitalWrite(RCLK1, HIGH);
}
void loadLUT1(byte data){
digitalWrite(RCLK2, LOW);

  shiftOut(SER2, SCLK2, MSBFIRST, data);

  // Latch data to QA-QH

  digitalWrite(RCLK2, HIGH);

}


// ============================================================
// Set 74LS151 select lines
//
// S2 = A
// S1 = B
// S0 = C
// ============================================================

void setMUX(byte A, byte B, byte C)
{
  digitalWrite(S2, A);
  digitalWrite(S1, B);
  digitalWrite(S0, C);
}


// ============================================================
// Get LUT configuration for a gate
//
// Bit 0 -> QA -> I0
// Bit 1 -> QB -> I1
// ...
// Bit 7 -> QH -> I7
// ============================================================

byte getLUT(String gate)
{
  if (gate == "AND")
  {
    // 00000000 -> 0
    // 00000001 -> 0
    // 00000010 -> 0
    // 00000011 -> 0
    // 00000100 -> 0
    // 00000101 -> 0
    // 00000110 -> 0
    // 00000111 -> 1

    return 0b10000000;
  }


  else if (gate == "OR")
  {
    // Only 000 = 0

    return 0b11111110;
  }


  else if (gate == "NAND")
  {
    // Only 111 = 0

    return 0b01111111;
  }


  else if (gate == "NOR")
  {
    // Only 000 = 1

    return 0b00000001;
  }


  else if (gate == "XOR")
  {
    /*
        3-input XOR

        A B C | Y
        --------------
        0 0 0 | 0
        0 0 1 | 1
        0 1 0 | 1
        0 1 1 | 0
        1 0 0 | 1
        1 0 1 | 0
        1 1 0 | 0
        1 1 1 | 1

        I7 I6 I5 I4 I3 I2 I1 I0
         1  0  0  1  0  1  1  0
    */

    return 0b10010110;
  }


  else if (gate == "XNOR")
  {
    /*
        3-input XNOR

        A B C | Y
        --------------
        0 0 0 | 1
        0 0 1 | 0
        0 1 0 | 0
        0 1 1 | 1
        1 0 0 | 0
        1 0 1 | 1
        1 1 0 | 1
        1 1 1 | 0

        I7 I6 I5 I4 I3 I2 I1 I0
         0  1  1  0  1  0  0  1
    */

    return 0b01101001;
  }


  // Invalid gate

  return 0;
}


// ============================================================
// Check whether command is a valid gate
// ============================================================

bool isGate(String command)
{
  return
    command == "AND"  ||
    command == "OR"   ||
    command == "NAND" ||
    command == "NOR"  ||
    command == "XOR"  ||
    command == "XNOR";
}


// ============================================================
// Print LUT
// ============================================================

void printLUT(byte data)
{
  SER1ial.print("LUT = ");

  for (int i = 7; i >= 0; i--)
  {
    SER1ial.print((data >> i) & 1);
  }

  SER1ial.println();

  SER1ial.println();
  SER1ial.println("QH QG QF QE QD QC QB QA");

  for (int i = 7; i >= 0; i--)
  {
    SER1ial.print(" ");
    SER1ial.print((data >> i) & 1);
    SER1ial.print(" ");
  }

  SER1ial.println();
}


// ============================================================
// Configure a new gate
// ============================================================

void configureGate(String gate)
{
  byte lutData = getLUT(gate);

  currentGate = gate;

  SER1ial.println();
  SER1ial.print("Configuring ");
  SER1ial.print(gate);
  SER1ial.println("...");

  // Load LUT into 595

  loadLUT(lutData);

  SER1ial.println("LUT configuration loaded.");

  printLUT(lutData);

  SER1ial.println();
  SER1ial.print("Current gate: ");
  SER1ial.println(currentGate);

  SER1ial.println();
  SER1ial.println("Enter A B C");
  SER1ial.println("Example: 1 0 1");
  SER1ial.println();
}


// ============================================================
// Setup
// ============================================================

void setup()
{
  pinMode(SER1, OUTPUT);
  pinMode(RCLK1, OUTPUT);
  pinMode(SCLK1, OUTPUT);

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);

  SER1ial.begin(9600);

  digitalWrite(RCLK1, LOW);

  // Initial MUX state

  setMUX(0, 0, 0);

  SER1ial.println();
  SER1ial.println("======================================");
  SER1ial.println("        3-INPUT LUT TESTER");
  SER1ial.println("======================================");
  SER1ial.println();

  SER1ial.println("Available gates:");
  SER1ial.println("  AND");
  SER1ial.println("  OR");
  SER1ial.println("  NAND");
  SER1ial.println("  NOR");
  SER1ial.println("  XOR");
  SER1ial.println("  XNOR");

  SER1ial.println();
  SER1ial.println("Enter a gate:");
}


// ============================================================
// Main loop
// ============================================================

void loop()
{
  if (!SER1ial.available())
    return;


  // Read complete line

  String input = SER1ial.readStringUntil('\n');

  input.trim();
  input.toUpperCase();


  // Ignore empty input

  if (input.length() == 0)
    return;


  // ==========================================================
  // FIRST: Check whether uSER1 entered a GATE
  //
  // This is checked regardless of the current state.
  // Therefore you can change the gate at any time.
  // ==========================================================

  if (isGate(input))
  {
    configureGate(input);

    return;
  }


  // ==========================================================
  // If no gate has been selected yet
  // ==========================================================

  if (currentGate == "")
  {
    SER1ial.println();
    SER1ial.println("Please select a gate first.");

    SER1ial.println();
    SER1ial.println("Available:");
    SER1ial.println("AND");
    SER1ial.println("OR");
    SER1ial.println("NAND");
    SER1ial.println("NOR");
    SER1ial.println("XOR");
    SER1ial.println("XNOR");

    return;
  }


  // ==========================================================
  // Otherwise interpret input as A B C
  // ==========================================================

  int A;
  int B;
  int C;


  // Parse three integers

  if (sscanf(input.c_str(), "%d %d %d", &A, &B, &C) != 3)
  {
    SER1ial.println();
    SER1ial.println("Invalid input.");

    SER1ial.println();
    SER1ial.println("Enter:");
    SER1ial.println("A B C");

    SER1ial.println();
    SER1ial.println("Example:");
    SER1ial.println("1 0 1");

    SER1ial.println();
    SER1ial.println("Or enter a new gate:");
    SER1ial.println("AND / OR / NAND / NOR / XOR / XNOR");

    return;
  }


  // ==========================================================
  // Validate A B C
  // ==========================================================

  if ((A != 0 && A != 1) ||
      (B != 0 && B != 1) ||
      (C != 0 && C != 1))
  {
    SER1ial.println();
    SER1ial.println("ERROR: A, B and C must be 0 or 1.");

    return;
  }


  // ==========================================================
  // Calculate MUX address
  //
  // S2 S1 S0 = A B C
  //
  // Address = ABC
  // ==========================================================

  int address =
      (A << 2) |
      (B << 1) |
       C;


  // ==========================================================
  // Set MUX
  // ==========================================================

  setMUX(A, B, C);


  // ==========================================================
  // Calculate expected result
  //
  // This is ONLY for SER1ial-monitor verification.
  // The actual result is generated by:
  //
  // 595 -> 151 -> LED
  //
  // ==========================================================

  int expected = 0;


  if (currentGate == "AND")
  {
    expected = A & B & C;
  }

  else if (currentGate == "OR")
  {
    expected = A | B | C;
  }

  else if (currentGate == "NAND")
  {
    expected = !(A & B & C);
  }

  else if (currentGate == "NOR")
  {
    expected = !(A | B | C);
  }

  else if (currentGate == "XOR")
  {
    expected = A ^ B ^ C;
  }

  else if (currentGate == "XNOR")
  {
    expected = !(A ^ B ^ C);
  }


  // ==========================================================
  // Display information
  // ==========================================================

  SER1ial.println();

  SER1ial.print("Gate : ");
  SER1ial.println(currentGate);

  SER1ial.print("A B C : ");
  SER1ial.print(A);
  SER1ial.print(" ");
  SER1ial.print(B);
  SER1ial.print(" ");
  SER1ial.println(C);

  SER1ial.print("S2 S1 S0 : ");
  SER1ial.print(A);
  SER1ial.print(" ");
  SER1ial.print(B);
  SER1ial.print(" ");
  SER1ial.println(C);

  SER1ial.print("Selected : I");
  SER1ial.println(address);

  SER1ial.print("Expected Y : ");
  SER1ial.println(expected);

  SER1ial.println();

  SER1ial.println("LED = actual LUT output.");

  SER1ial.println();
  SER1ial.println("Enter another A B C");
  SER1ial.println("or enter a new gate.");
}