#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>

// Global constant for number of retries
const int NUM_RETRIES = 10;

LiquidCrystal_I2C lcd(0x27, 20, 4);

char inText[32];
char recvText[32];
int idx = 0;
int recvIdx = 0;
int sent_flag = 0;

// Buffer to store received characters before committing
char charBuffer[NUM_RETRIES];
int bufferIdx = 0;

void setup() {
  Serial.begin(9600);
  IrSender.begin(3);
  IrReceiver.begin(4);

  lcd.init();
  lcd.backlight();

  lcd.print("Online");
}

void sendString(const char* str) {
  while (*str) {
    // Send character NUM_RETRIES times
    for (int i = 0; i < NUM_RETRIES; i++){
      IrSender.sendNEC(0x00FF, *str, 0);
      delay(40);
    }
    // Send delimiter NUM_RETRIES times
    for (int i = 0; i < NUM_RETRIES; i++){
      IrSender.sendNEC(0x00FF, '\17', 0);
      delay(40);
    }

    delay(40);
    str++;
  }
}

// Function to find the most frequent character in the buffer
char getMostFrequentChar() {
  if (bufferIdx == 0) return '\0';
  
  // Simple frequency counting
  int maxCount = 0;
  char mostFrequent = charBuffer[0];
  
  for (int i = 0; i < bufferIdx; i++) {
    int count = 0;
    for (int j = 0; j < bufferIdx; j++) {
      if (charBuffer[i] == charBuffer[j]) {
        count++;
      }
    }
    if (count > maxCount) {
      maxCount = count;
      mostFrequent = charBuffer[i];
    }
  }
  
  return mostFrequent;
}

void loop() {

  // --- SERIAL INPUT (top buffer) ---
  if (Serial.available()) {
    lcd.clear();
    if (idx < sizeof(inText) - 1) {
      char read = Serial.read();
      lcd.print(read);

      if (read != '\0') {
        inText[idx++] = read;
      }
      inText[idx] = '\0';
    }

    sent_flag = 0;
  }

  // --- SEND ONCE ---
  else if (!sent_flag && idx > 0) {
    sendString(inText);
    sent_flag = 1;
    idx = 0;
  }

  // --- IR RECEIVE (bottom buffer) ---
  if (IrReceiver.decode()) {

    char c = IrReceiver.decodedIRData.command;

    // --- DELIMITER DETECTED ---
    if (c == '\17') {
      // Get the most frequent character from the buffer
      if (bufferIdx > 0) {
        char mostFrequent = getMostFrequentChar();
        
        // Only commit if we have enough samples (at least half expected)
        if (bufferIdx >= NUM_RETRIES / 2) {
          if (recvIdx < sizeof(recvText) - 1) {
            recvText[recvIdx++] = mostFrequent;
            recvText[recvIdx] = '\0';
          }
        }
      }
      
      // Reset buffer for next character
      bufferIdx = 0;
    }

    // --- NORMAL CHARACTER ---
    else {
      // Store character in buffer if there's space
      if (bufferIdx < NUM_RETRIES) {
        charBuffer[bufferIdx++] = c;
      }
    }

    IrReceiver.resume();
  }
  else {
    // --- DISPLAY ---
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print(inText);

    lcd.setCursor(0, 1);
    lcd.print(recvText);

    delay(100); // small refresh delay
  }
}