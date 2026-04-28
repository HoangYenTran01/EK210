void setup() {
  pinMode(5, OUTPUT);
}

void sendPulse(int microseconds) {
  digitalWrite(5, HIGH);
  delayMicroseconds(microseconds);
  digitalWrite(5, LOW);
  delay(20);
}

void loop() {
  // sweep from 0 to 180
  for (int us = 1000; us <= 2000; us += 10) {
    sendPulse(us);
  }
  // sweep back
  for (int us = 2000; us >= 1000; us -= 10) {
    sendPulse(us);
  }
}
