#define SER   12
#define RCLK  11
#define SCLK  10

// 74LS151 select lines
#define S0    6
#define S1    5
#define S2    4


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

  digitalWrite(RCLK, LOW);

  shiftOut(SER, SCLK, MSBFIRST, data);

  // Latch data to QA-QH

  digitalWrite(RCLK, HIGH);
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
  Serial.print("LUT = ");

  for (int i = 7; i >= 0; i--)
  {
    Serial.print((data >> i) & 1);
  }

  Serial.println();

  Serial.println();
  Serial.println("QH QG QF QE QD QC QB QA");

  for (int i = 7; i >= 0; i--)
  {
    Serial.print(" ");
    Serial.print((data >> i) & 1);
    Serial.print(" ");
  }

  Serial.println();
}


// ============================================================
// Configure a new gate
// ============================================================

void configureGate(String gate)
{
  byte lutData = getLUT(gate);

  currentGate = gate;

  Serial.println();
  Serial.print("Configuring ");
  Serial.print(gate);
  Serial.println("...");

  // Load LUT into 595

  loadLUT(lutData);

  Serial.println("LUT configuration loaded.");

  printLUT(lutData);

  Serial.println();
  Serial.print("Current gate: ");
  Serial.println(currentGate);

  Serial.println();
  Serial.println("Enter A B C");
  Serial.println("Example: 1 0 1");
  Serial.println();
}


// ============================================================
// Setup
// ============================================================

void setup()
{
  pinMode(SER, OUTPUT);
  pinMode(RCLK, OUTPUT);
  pinMode(SCLK, OUTPUT);

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);

  Serial.begin(9600);

  digitalWrite(RCLK, LOW);

  // Initial MUX state

  setMUX(0, 0, 0);

  Serial.println();
  Serial.println("======================================");
  Serial.println("        3-INPUT LUT TESTER");
  Serial.println("======================================");
  Serial.println();

  Serial.println("Available gates:");
  Serial.println("  AND");
  Serial.println("  OR");
  Serial.println("  NAND");
  Serial.println("  NOR");
  Serial.println("  XOR");
  Serial.println("  XNOR");

  Serial.println();
  Serial.println("Enter a gate:");
}


// ============================================================
// Main loop
// ============================================================

void loop()
{
  if (!Serial.available())
    return;


  // Read complete line

  String input = Serial.readStringUntil('\n');

  input.trim();
  input.toUpperCase();


  // Ignore empty input

  if (input.length() == 0)
    return;


  // ==========================================================
  // FIRST: Check whether user entered a GATE
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
    Serial.println();
    Serial.println("Please select a gate first.");

    Serial.println();
    Serial.println("Available:");
    Serial.println("AND");
    Serial.println("OR");
    Serial.println("NAND");
    Serial.println("NOR");
    Serial.println("XOR");
    Serial.println("XNOR");

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
    Serial.println();
    Serial.println("Invalid input.");

    Serial.println();
    Serial.println("Enter:");
    Serial.println("A B C");

    Serial.println();
    Serial.println("Example:");
    Serial.println("1 0 1");

    Serial.println();
    Serial.println("Or enter a new gate:");
    Serial.println("AND / OR / NAND / NOR / XOR / XNOR");

    return;
  }


  // ==========================================================
  // Validate A B C
  // ==========================================================

  if ((A != 0 && A != 1) ||
      (B != 0 && B != 1) ||
      (C != 0 && C != 1))
  {
    Serial.println();
    Serial.println("ERROR: A, B and C must be 0 or 1.");

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
  // This is ONLY for serial-monitor verification.
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

  Serial.println();

  Serial.print("Gate : ");
  Serial.println(currentGate);

  Serial.print("A B C : ");
  Serial.print(A);
  Serial.print(" ");
  Serial.print(B);
  Serial.print(" ");
  Serial.println(C);

  Serial.print("S2 S1 S0 : ");
  Serial.print(A);
  Serial.print(" ");
  Serial.print(B);
  Serial.print(" ");
  Serial.println(C);

  Serial.print("Selected : I");
  Serial.println(address);

  Serial.print("Expected Y : ");
  Serial.println(expected);

  Serial.println();

  Serial.println("LED = actual LUT output.");

  Serial.println();
  Serial.println("Enter another A B C");
  Serial.println("or enter a new gate.");
}