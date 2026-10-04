#define Ser 12
#define Rclk 11
#define Sclk 10

void setup() {
  pinMode(Ser, OUTPUT);
  pinMode(Rclk, OUTPUT);
  pinMode(Sclk, OUTPUT);

  Serial.begin(9600);

  digitalWrite(Rclk, LOW);
}

void loop() {

  if (Serial.available()) {

    int num = Serial.parseInt();

    // Limit to 0–255
    byte data = constrain(num, 0, 255);

    Serial.print("Decimal: ");
    Serial.println(data);

    Serial.print("Binary: ");
    Serial.println(data, BIN);

    // Shift data into 74xx595
    digitalWrite(Rclk, LOW);

    shiftOut(Ser, Sclk, MSBFIRST, data);

    // Transfer shift register -> output register
    digitalWrite(Rclk, HIGH);

    // Clear remaining Serial characters
    while (Serial.available()) {
      Serial.read();
    }
  }
}