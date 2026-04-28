#include <IRremote.hpp>

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(2, ENABLE_LED_FEEDBACK);
  Serial.println("Listening for any IR signal...");
}

void loop() {
  if (IrReceiver.decode()) {
    Serial.print("RAW code received: 0x");
    Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);
    IrReceiver.resume();
  }
}
