#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>

#include <hidboot.h>
#include <usbhub.h>
// Satisfy the IDE, which needs to see the include statment in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

// Global constant for number of retries
const int NUM_RETRIES = 10;

LiquidCrystal_I2C lcd(0x27, 20, 4);

#include <hidboot.h>
#include <usbhub.h>
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>

class KbdRptParser : public KeyboardReportParser
{
  protected:
    void OnKeyDown(uint8_t mod, uint8_t key);
  public:
    char lastChar = '\0';
    bool newCharAvailable = false;
    
    char getChar() {
      if (newCharAvailable) {
        newCharAvailable = false;
        return lastChar;
      }
      return '\0';
    }
};

void KbdRptParser::OnKeyDown(uint8_t mod, uint8_t key)
{
  uint8_t c = OemToAscii(mod, key);

    if (key == 0x2A) {  // HID keycode for Backspace
      lastChar = '\b';  // Backspace character
      newCharAvailable = true;
    return;
    }
  
  if (c) {
    // Convert Return (0x0D) to newline (0x0A)
    if (c == 0x0D) {
      c = '\n';
    }
    
    lastChar = c;
    newCharAvailable = true;
  }
}

USB Usb;
HIDBoot<USB_HID_PROTOCOL_KEYBOARD> HidKeyboard(&Usb);
KbdRptParser Prs;

/*
void setup() {
  Serial.begin(9600);
  
  if (Usb.Init() == -1) {
    Serial.println("USB host init failed");
  }
  
  HidKeyboard.SetReportParser(0, &Prs);
  Serial.println("Keyboard ready");
}

void loop() {
  Usb.Task();
  
  // Check if a key is available
  char c = Prs.getChar();
  
  if (c != '\0') {
    // Print the character
    Serial.print(c);
    
    // Optional: do something special with newlines
    if (c == '\n') {
      Serial.print("(newline detected)");
    }
  }
}

*/


char inText[32];
char recvText[32];
int idx = 0;
int recvIdx = 0;
int sent_flag = 1;

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

  if (Usb.Init() == -1) {
    Serial.println("USB host init failed");
    lcd.setCursor(0, 1);
    lcd.print("USB FAIL");
  }
  
  // **MISSING: Set up keyboard parser**
  HidKeyboard.SetReportParser(0, &Prs);
  Serial.println("Keyboard ready");
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

  //Take in keyboard inputs
  char kbdChar = Prs.getChar();
  if (kbdChar != '\0') {
    // Handle newline - send current buffer and clear
    if (kbdChar == '\n') {
      Serial.println("Enter pressed - sending");
      sent_flag = 0; // Trigger send
    } 
    if (kbdChar == '\b' && idx > 0){
      idx--;
      inText[idx] = (char)0x0;
      Serial.println("del char");
      
    }
    // Add regular characters to buffer
    else if (idx < sizeof(inText) - 1) {
      inText[idx++] = kbdChar;
      inText[idx] = '\0';
      Serial.print("Added: ");
      Serial.println(kbdChar);
    }
  }

  // --- SEND ONCE ---
  else if (!sent_flag && idx > 0) {
    lcd.setCursor(0,1);
    lcd.print("   SENDING...   ");
    char rst [2] = {0x19, 0x00};
    sendString(rst);
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

        if (mostFrequent == 0x19) {  // No need for (char) cast
          recvIdx = 0;
          memset(recvText, 0, sizeof(recvText));  // Cleaner way to clear
        }
        
        // Only commit if we have enough samples (at least half expected)
        else if (bufferIdx >= NUM_RETRIES / 2) {
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

  // --- DISPLAY ---
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(inText);

  lcd.setCursor(0, 1);
  lcd.print(recvText);
  
  Usb.Task();

  delay(100); // small refresh delay
}