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
const int NUM_RETRIES = 8;

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
bool lcdNeedsUpdate = false;

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
  
  HidKeyboard.SetReportParser(0, &Prs);
  Serial.println("Keyboard ready");
  lcdNeedsUpdate = true;
}


void sendString(const char* str) {
  while (*str) {
    IrSender.sendNEC(0x00FF, *str, 0); // Send the character once
    delay(50); // Short pause so the receiver has time to catch it
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
  // 1. MUST BE CALLED CONTINUOUSLY (No delays!)
  Usb.Task();

  // 2. --- KEYBOARD INPUT ---
  char kbdChar = Prs.getChar();
  if (kbdChar != '\0') {
    // Handle newline (Enter)
    if (kbdChar == '\n') {
      Serial.println("Enter pressed - sending");
      sent_flag = 0; // Trigger send
    } 
    // Handle Backspace
    else if (kbdChar == '\b') {
      if (idx > 0) {
        idx--;
        inText[idx] = '\0'; // Properly terminate the string
        Serial.println("del char");
        lcdNeedsUpdate = true; // Refresh screen to show deleted char
      }
    }
    // Add regular characters to buffer
    else if (idx < sizeof(inText) - 1) {
      inText[idx++] = kbdChar;
      inText[idx] = '\0';
      Serial.print("Added: ");
      Serial.println(kbdChar);
      lcdNeedsUpdate = true; // Refresh screen to show new char
    }
  }

  // 3. --- SEND ONCE ---
  if (!sent_flag && idx > 0) {
    // Show sending status
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("   SENDING...   ");
    
    char rst[2] = {0x19, '\0'};
    sendString(rst);
    sendString(inText);
    
    sent_flag = 1;
    idx = 0;
    inText[0] = '\0'; // Clear the input buffer so it disappears from the screen
    lcdNeedsUpdate = true; // Refresh screen to show empty input
  }

  // 4. --- IR RECEIVE ---
  if (IrReceiver.decode()) {
    char c = IrReceiver.decodedIRData.command;

    if (c == 0x19) {  // The reset command
      recvIdx = 0;
      memset(recvText, 0, sizeof(recvText)); 
      lcdNeedsUpdate = true;
    } 
    else if (c != 0 && recvIdx < sizeof(recvText) - 1) { 
      // Normal character received!
      recvText[recvIdx++] = c;
      recvText[recvIdx] = '\0';
      lcdNeedsUpdate = true;
    }

    IrReceiver.resume();
  }

  // 5. --- LCD DISPLAY ---
  // Only update the display if something actually changed!
  if (lcdNeedsUpdate) {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print(inText);

    lcd.setCursor(0, 1);
    lcd.print(recvText);
    
    lcdNeedsUpdate = false; // Reset the flag
  }
}
