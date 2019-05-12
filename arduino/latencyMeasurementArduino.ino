/*
 * Arduino part for serial communication delay measurement.
 * This code will simply listen to one byte messages and send
 * them back to the host.
 * 
 * (c) 2019 by Clemens Rabe <info@clemensrabe.de>
 */

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
}

void loop() {
  const int input_i = Serial.read();
  if (input_i != -1) {
    digitalWrite(2, ((input_i & 0x01) == 0x01)?HIGH:LOW);
    Serial.write(input_i);
    Serial.flush();
  }
}
