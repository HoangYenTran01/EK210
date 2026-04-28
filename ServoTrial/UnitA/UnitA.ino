// =============================================================================
//  IR Servo Tracker — UNIT A  (TX master / message sender)
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
//    arduino-cli compile --fqbn arduino:avr:uno UnitA
//    arduino-cli upload  --fqbn arduino:avr:uno -p /dev/ttyUSB0 UnitA
//
//  MONITOR:
//    arduino-cli monitor -p /dev/ttyUSB0 --config baudrate=115200
//
//  BEHAVIOUR:
//    1. Servo sweeps 45°–135° (90° window)
//    2. Unit A fires IR beacons continuously while scanning
//    3. Listens for ACK from Unit B
//    4. After HANDSHAKE_RETRIES confirmed ACKs, servo freezes
//    5. Unit A then repeats a message to Unit B every MESSAGE_INTERVAL_MS
// =============================================================================

#define SEND_PWM_BY_TIMER
#include <IRremote.hpp>
#include <Servo.h>

// ── Pins ─────────────────────────────────────────────────────────────────────
#define IR_RECEIVE_PIN    2
#define IR_SEND_PIN       3
#define SERVO_PIN         5

// ── IR codes (must match UnitB exactly) ──────────────────────────────────────
#define IR_ADDR           0xAB
#define CODE_BEACON       0x01    // Unit A → Unit B: "are you there?"
#define CODE_ACK          0x02    // Unit B → Unit A: "yes, locked"
#define CODE_MESSAGE      0x10    // Unit A → Unit B: message header
#define CODE_MSG_PAYLOAD  0x42    // message data byte (0x42 = ASCII 'B')

// ── Scan parameters ───────────────────────────────────────────────────────────
#define SCAN_MIN_DEG        0
#define SCAN_MAX_DEG        180
#define SCAN_STEP_DEG       3
#define SCAN_STEP_MS        60    // ms between servo steps

// ── Handshake ─────────────────────────────────────────────────────────────────
#define BEACON_INTERVAL_MS  100   // fire a beacon every 100ms while scanning
#define HANDSHAKE_RETRIES   3     // need this many ACKs to confirm lock

// ── Comms ─────────────────────────────────────────────────────────────────────
#define MESSAGE_INTERVAL_MS 1500  // repeat message every 1.5s once locked

#define BAUD                115200

// ── State machine ─────────────────────────────────────────────────────────────
enum State { SCANNING, LOCKED, COMMS };
State state = SCANNING;

Servo myServo;

int  servoPos       = SCAN_MIN_DEG;
int  scanDir        = 1;
int  ackCount       = 0;
int  lockedAngle    = 90;

unsigned long lastStep    = 0;
unsigned long lastBeacon  = 0;
unsigned long lastMessage = 0;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(BAUD);
  while (!Serial) {}

  Serial.println(F("============================================"));
  Serial.println(F("  UNIT A  —  TX master"));
  Serial.println(F("============================================"));
  Serial.println(F("[SCANNING] Sweeping 45-135 degrees..."));

  myServo.attach(SERVO_PIN);
  myServo.write(servoPos);
  delay(400);

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  IrSender.begin(IR_SEND_PIN);

  lastStep   = millis();
  lastBeacon = millis();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  switch (state) {

    // ── SCANNING ───────────────────────────────────────────────────────────
    case SCANNING:
      stepServo();

      // Fire beacon on schedule
      if (millis() - lastBeacon >= BEACON_INTERVAL_MS) {
        lastBeacon = millis();
        IrSender.sendNEC(IR_ADDR, CODE_BEACON, 1);
        Serial.print(F("[SCAN] Beacon sent at "));
        Serial.print(servoPos);
        Serial.println(F("deg"));
      }

      // Listen for ACK from Unit B
      if (IrReceiver.decode()) {
        uint8_t addr = IrReceiver.decodedIRData.address;
        uint8_t cmd  = IrReceiver.decodedIRData.command;
        IrReceiver.resume();

        if (addr == IR_ADDR && cmd == CODE_ACK) {
          ackCount++;
          Serial.print(F("[SCAN] ACK received! Count: "));
          Serial.print(ackCount);
          Serial.print(F("/"));
          Serial.print(HANDSHAKE_RETRIES);
          Serial.print(F(" at "));
          Serial.print(servoPos);
          Serial.println(F("deg"));

          if (ackCount >= HANDSHAKE_RETRIES) {
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
            delay(500);
            state = COMMS;
            Serial.println(F("[COMMS] Entering message loop..."));
            lastMessage = millis();
          }
        }
      }
      break;

    // ── COMMS ─────────────────────────────────────────────────────────────
    case COMMS:
      // Servo stays put — no stepServo() call here

      if (millis() - lastMessage >= MESSAGE_INTERVAL_MS) {
        lastMessage = millis();

        // Send a two-code message: header then payload
        // Small gap between them so Unit B can decode each separately
        IrSender.sendNEC(IR_ADDR, CODE_MESSAGE, 1);
        delay(30);
        IrSender.sendNEC(IR_ADDR, CODE_MSG_PAYLOAD, 1);

        Serial.print(F("[COMMS] Message sent → header:0x"));
        Serial.print(CODE_MESSAGE, HEX);
        Serial.print(F(" payload:0x"));
        Serial.print(CODE_MSG_PAYLOAD, HEX);
        Serial.print(F(" (ASCII '"));
        Serial.print((char)CODE_MSG_PAYLOAD);
        Serial.println(F("')"));
      }

      // Unit A can also hear any reply from Unit B (optional ack loop)
      if (IrReceiver.decode()) {
        uint8_t addr = IrReceiver.decodedIRData.address;
        uint8_t cmd  = IrReceiver.decodedIRData.command;
        IrReceiver.resume();
        Serial.print(F("[COMMS] Received back — addr:0x"));
        Serial.print(addr, HEX);
        Serial.print(F(" cmd:0x"));
        Serial.println(cmd, HEX);
      }
      break;

    // LOCKED is transient — handled inline above
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

