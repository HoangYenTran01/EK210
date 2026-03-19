#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>

// ================= CONFIG =================
#define IR_SEND_PIN 3
#define IR_RECV_PIN 4

#define MAX_MSG_LEN 32
#define NUM_PACKET_RETRIES 4
#define START_BYTE 0xAA

// ===========================================

LiquidCrystal_I2C lcd(0x27, 20, 4);

// ---------- transmit buffer ----------
char txBuffer[MAX_MSG_LEN];
uint8_t txLen = 0;
bool messagePending = false;

// ---------- receive buffer ----------
char rxBuffer[MAX_MSG_LEN];
char displayBuffer[MAX_MSG_LEN];

// ---------- packet receive state machine ----------
enum RxState {
  WAIT_START,
  WAIT_LEN,
  WAIT_DATA,
  WAIT_CRC
};

RxState rxState = WAIT_START;

uint8_t expectedLen = 0;
uint8_t rxIndex = 0;

// ---------- LCD refresh ----------
unsigned long lastDisplay = 0;

// ===========================================
// CHECKSUM (XOR)
// ===========================================
uint8_t computeChecksum(const char *data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++)
    crc ^= data[i];
  return crc;
}

// ===========================================
// IR PACKET SEND
// ===========================================
void sendPacket(const char *msg) {
  uint8_t len = strlen(msg);
  uint8_t crc = computeChecksum(msg, len);

  for (int retry = 0; retry < NUM_PACKET_RETRIES; retry++) {

    IrSender.sendNEC(0x00FF, START_BYTE, 0);
    delay(20);

    IrSender.sendNEC(0x00FF, len, 0);
    delay(20);

    for (uint8_t i = 0; i < len; i++) {
      IrSender.sendNEC(0x00FF, msg[i], 0);
      delay(20);
    }

    IrSender.sendNEC(0x00FF, crc, 0);
    delay(60);
  }
}

// ===========================================
// IR RECEIVE STATE MACHINE
// ===========================================
void processIR(uint8_t value) {

  switch (rxState) {

    case WAIT_START:
      if (value == START_BYTE) {
        rxState = WAIT_LEN;
      }
      break;

    case WAIT_LEN:
      if (value > 0 && value <= MAX_MSG_LEN) {
        expectedLen = value;
        rxIndex = 0;
        rxState = WAIT_DATA;
      } else {
        rxState = WAIT_START;
      }
      break;

    case WAIT_DATA:
      rxBuffer[rxIndex++] = value;

      if (rxIndex >= expectedLen) {
        rxState = WAIT_CRC;
      }
      break;

    case WAIT_CRC: {
      uint8_t crc = computeChecksum(rxBuffer, expectedLen);

      if (crc == value) {
        // Valid packet
        memcpy(displayBuffer, rxBuffer, expectedLen);
        displayBuffer[expectedLen] = '\0';
      }

      rxState = WAIT_START;
      break;
    }
  }
}

// ===========================================
// SETUP
// ===========================================
void setup() {
  Serial.begin(9600);

  IrSender.begin(IR_SEND_PIN);
  IrReceiver.begin(IR_RECV_PIN);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("IR Link Ready");
}

// ===========================================
// SERIAL INPUT HANDLING
// ===========================================
void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n') {
      txBuffer[txLen] = '\0';
      messagePending = true;
    }
    else if (txLen < MAX_MSG_LEN - 1) {
      txBuffer[txLen++] = c;
    }
  }
}

// ===========================================
// LCD DISPLAY
// ===========================================
void updateDisplay() {
  if (millis() - lastDisplay < 200)
    return;

  lcd.setCursor(0, 0);
  lcd.print("TX:                ");
  lcd.setCursor(4, 0);
  lcd.print(txBuffer);

  lcd.setCursor(0, 1);
  lcd.print("RX:                ");
  lcd.setCursor(4, 1);
  lcd.print(displayBuffer);

  lastDisplay = millis();
}

// ===========================================
// MAIN LOOP
// ===========================================
void loop() {

  // ---- read serial input ----
  readSerial();

  // ---- send pending packet ----
  if (messagePending) {
    sendPacket(txBuffer);
    txLen = 0;
    messagePending = false;
  }

  // ---- process IR receiver ----
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    processIR(cmd);
    IrReceiver.resume();
  }

  // ---- refresh LCD ----
  updateDisplay();
}
