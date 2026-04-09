// =============================================================================
//  IR Servo Tracker — UNIT B  (RX responder / message receiver)
//
//  WIRING FOR THIS ARDUINO:
//    IR Receiver  (TSOP38238) → Pin 2  (INT0 hardware interrupt)
//                               also connect its VCC→5V, GND→GND
//    IR Transmitter LED       → Pin 3  → 100Ω resistor → GND
//    Servo signal wire        → Pin 5  (PWM)
//                               Servo VCC→5V, GND→GND
//
//  LIBRARY REQUIRED:
//    IRremote v3.x
//    Install: arduino-cli lib install "IRremote"
//
//  FLASH:
//    arduino-cli compile --fqbn arduino:avr:uno UnitB
//    arduino-cli upload  --fqbn arduino:avr:uno -p /dev/ttyACM0 UnitB
//    (use a different port from Unit A — plug them in separately first to find out)
//
//  MONITOR:
//    arduino-cli monitor -p /dev/ttyACM0 --config baudrate=115200
//
//  BEHAVIOUR:
//    1. Servo sweeps 45°–135° (90° window)
//    2. Listens for IR beacon from Unit A
//    3. On beacon: replies with ACK and records hit count
//    4. After HANDSHAKE_RETRIES beacon hits, servo freezes
//    5. Unit B then listens for and prints incoming messages from Unit A
// =============================================================================

#define SEND_PWM_BY_TIMER
#include <IRremote.hpp>
#include <Servo.h>

// ── Pins ─────────────────────────────────────────────────────────────────────
#define IR_RECEIVE_PIN    2
#define IR_SEND_PIN       3
#define SERVO_PIN         5

// ── IR codes (must match UnitA exactly) ──────────────────────────────────────
#define IR_ADDR           0xAB
#define CODE_BEACON       0x01
#define CODE_ACK          0x02
#define CODE_MESSAGE      0x10
#define CODE_MSG_PAYLOAD  0x42

// ── Scan parameters ───────────────────────────────────────────────────────────
#define SCAN_MIN_DEG        45
#define SCAN_MAX_DEG        135
#define SCAN_STEP_DEG       3
#define SCAN_STEP_MS        60

// ── Handshake ─────────────────────────────────────────────────────────────────
#define HANDSHAKE_RETRIES   4     // beacon hits needed to confirm lock

#define BAUD                115200

// ── State machine ─────────────────────────────────────────────────────────────
enum State { SCANNING, LOCKED, COMMS };
State state = SCANNING;

Servo myServo;

int  servoPos       = SCAN_MIN_DEG;
int  scanDir        = 1;
int  beaconCount    = 0;
int  lockedAngle    = 90;
bool expectingPayload = false;   // true after MESSAGE header received

unsigned long lastStep = 0;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(BAUD);
  while (!Serial) {}

  Serial.println(F("============================================"));
  Serial.println(F("  UNIT B  —  RX responder"));
  Serial.println(F("============================================"));
  Serial.println(F("[SCANNING] Sweeping 45-135 degrees..."));

  myServo.attach(SERVO_PIN);
  myServo.write(servoPos);
  delay(400);

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  IrSender.begin(IR_SEND_PIN);

  lastStep = millis();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  switch (state) {

    // ── SCANNING ───────────────────────────────────────────────────────────
    case SCANNING:
      stepServo();

      if (IrReceiver.decode()) {
        uint8_t addr = IrReceiver.decodedIRData.address;
        uint8_t cmd  = IrReceiver.decodedIRData.command;
        IrReceiver.resume();

        if (addr == IR_ADDR && cmd == CODE_BEACON) {
          beaconCount++;
          Serial.print(F("[SCAN] Beacon received! Count: "));
          Serial.print(beaconCount);
          Serial.print(F("/"));
          Serial.print(HANDSHAKE_RETRIES);
          Serial.print(F(" at "));
          Serial.print(servoPos);
          Serial.println(F("deg"));

          // Reply with ACK — small gap so we don't talk over each other
          delay(15);
          IrSender.sendNEC(IR_ADDR, CODE_ACK, 1);
          Serial.println(F("[SCAN] ACK sent"));

          if (beaconCount >= HANDSHAKE_RETRIES) {
            lockedAngle = servoPos;
            myServo.write(lockedAngle);
            state = LOCKED;

            Serial.println();
            Serial.println(F("============================================"));
            Serial.println(F("[LOCKED] Handshake complete. Servo frozen."));
            Serial.print(F("         Locked angle: "));
            Serial.print(lockedAngle);
            Serial.println(F("deg"));
            Serial.println(F("============================================"));
            Serial.println();
            delay(300);

            state = COMMS;
            Serial.println(F("[COMMS] Listening for messages from Unit A..."));
            Serial.println();
          }
        }
      }
      break;

    // ── COMMS ─────────────────────────────────────────────────────────────
    case COMMS:
      // Servo stays put — no stepServo() call

      if (IrReceiver.decode()) {
        uint8_t addr = IrReceiver.decodedIRData.address;
        uint8_t cmd  = IrReceiver.decodedIRData.command;
        IrReceiver.resume();

        // Only process packets addressed to us
        if (addr != IR_ADDR) break;

        if (cmd == CODE_MESSAGE) {
          // Header received — next packet is the payload
          expectingPayload = true;
          Serial.println(F("[COMMS] Message header received, waiting for payload..."));
        }
        else if (expectingPayload && cmd == CODE_MSG_PAYLOAD) {
          expectingPayload = false;
          Serial.println(F("--------------------------------------------"));
          Serial.print(F("[COMMS] MESSAGE RECEIVED → 0x"));
          Serial.print(cmd, HEX);
          Serial.print(F("  decimal: "));
          Serial.print(cmd, DEC);
          Serial.print(F("  ASCII: '"));
          Serial.print((char)cmd);
          Serial.println(F("'"));
          Serial.println(F("--------------------------------------------"));
        }
        else {
          // Unexpected code — log it anyway for debugging
          Serial.print(F("[COMMS] Unknown code — addr:0x"));
          Serial.print(addr, HEX);
          Serial.print(F(" cmd:0x"));
          Serial.println(cmd, HEX);
          expectingPayload = false;
        }
      }
      break;

    case LOCKED:
      break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void stepServo() {
  if (millis() - lastStep < SCAN_STEP_MS) return;
  lastStep = millis();

  servoPos += SCAN_STEP_DEG * scanDir;

  if (servoPos >= SCAN_MAX_DEG) { servoPos = SCAN_MAX_DEG; scanDir = -1; }
  if (servoPos <= SCAN_MIN_DEG) { servoPos = SCAN_MIN_DEG; scanDir =  1; }

  myServo.write(servoPos);
}

