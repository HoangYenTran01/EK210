#include <IRremote.hpp>
#include <Servo.h>

#define UNIT_ID      1 // Either 1 or 0. Make sure both are DIFFERENT
#define IR_RECV_PIN  2
#define IR_SEND_PIN  3
#define SERVO_PIN    5

#define IR_ADDR      0x42
#define CMD_BEACON   0x01
#define CMD_ACK      0x02

#define SLOT_MS      60
#define SCAN_STEP_MS 80
#define SERVO_MIN    10
#define SERVO_MAX    170
#define SERVO_STEP   3
#define LOCK_CONFIRMS 4
#define TX_BLANK_MS  10    // ignore RX for this long after we transmit

Servo servo;
int   servoAngle = SERVO_MIN;
int   servoDir   = 1;

enum State { SCAN, PAIRED };
State state = SCAN;

int           confirmCount = 0;
unsigned long lastTx       = 0;
unsigned long lastStep     = 0;

void sendCmd(uint8_t cmd) {
  IrSender.sendNEC(IR_ADDR, cmd, 0);
  lastTx = millis();             // record exactly when we fired
}

bool myTxSlot() {
  return (millis() / SLOT_MS) % 2 == (unsigned long)UNIT_ID;
}

bool inBlankingWindow() {
  return millis() - lastTx < TX_BLANK_MS;
}

void stepServo() {
  if (millis() - lastStep < SCAN_STEP_MS) return;
  lastStep = millis();
  servoAngle += servoDir * SERVO_STEP;
  if (servoAngle >= SERVO_MAX) { servoAngle = SERVO_MAX; servoDir = -1; }
  if (servoAngle <= SERVO_MIN) { servoAngle = SERVO_MIN; servoDir =  1; }
  servo.write(servoAngle);
}

void setup() {
  Serial.begin(9600);
  servo.attach(SERVO_PIN);
  servo.write(servoAngle);
  IrReceiver.begin(IR_RECV_PIN, DISABLE_LED_FEEDBACK);
  IrSender.begin(IR_SEND_PIN);
  Serial.println(UNIT_ID == 0 ? "Unit A — scanning" : "Unit B — scanning");
}

void loop() {
  if (state == PAIRED) return;

  stepServo();

  if (myTxSlot() && millis() - lastTx > SLOT_MS) {
    sendCmd(CMD_BEACON);
  }

  if (IrReceiver.decode()) {
    uint8_t  cmd  = IrReceiver.decodedIRData.command;
    uint16_t addr = IrReceiver.decodedIRData.address;
    IrReceiver.resume();

    if (!inBlankingWindow() && addr == IR_ADDR && (cmd == CMD_BEACON || cmd == CMD_ACK)) {
      sendCmd(CMD_ACK);
      confirmCount++;
      Serial.print("Confirmed: ");
      Serial.println(confirmCount);

      if (confirmCount >= LOCK_CONFIRMS) {
        state = PAIRED;
        Serial.println(">>> PAIRED — servo locked <<<");
      }
    }
  } else {
    IrReceiver.resume();
  }
}
