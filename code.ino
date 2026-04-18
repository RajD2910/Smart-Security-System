#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// PIN MAPPING
#define BUZZER_PIN      A0 
#define PIR_PIN         A1 
#define SMOKE_PIN       A2 
#define DOOR_PIN        12 
#define RED_LED_PIN     13 
#define GREEN_LED_PIN   11 
#define YELLOW_LED_PIN  10 

// SETTINGS
int SMOKE_THRESHOLD = 600; 
String masterPassword = "1234"; 
String sapID = "24027"; // Aapka naya SAP ID
String enteredInput = "";
int systemState = 0; // 0:Safe, 1:Away, 2:Alarm, 3:Warning, 4:Home
unsigned long entryDelayStart = 0;
String currentAlarmReason = "";

// LCD & KEYPAD
LiquidCrystal_I2C lcd(0x27, 16, 2); 
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'}, {'4','5','6','B'}, {'7','8','9','C'}, {'*','0','#','D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  pinMode(PIR_PIN, INPUT);
  pinMode(SMOKE_PIN, INPUT);
  pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.print("  WELCOME BACK");
  lcd.setCursor(4, 1);
  lcd.print("DITU SEC.");
  delay(3000);
  disarmSystem();
}

void loop() {
  int smokeVal = analogRead(SMOKE_PIN);
  bool doorOpen = (digitalRead(DOOR_PIN) == LOW);

  // 1. Fire Check
  if (smokeVal > SMOKE_THRESHOLD) {
    triggerAlarm("FIRE DETECTED!");
  }

  // 2. Dashboard Update
  updateDisplay(smokeVal, doorOpen);

  // 3. Keypad Input
  char key = keypad.getKey();
  if (key) {
    if (key >= '0' && key <= '9') {
      enteredInput += key;
    } else if (key == '*') {
      if(systemState == 0) armAway(); 
    } else if (key == 'B') {
      if(systemState == 0) armHome(); 
    } else if (key == '#') {
      handleHashKey();
    } else if (key == 'D') {
      enteredInput = "";
    }
  }

  // 4. Intrusion Detection
  if (systemState == 1) { // AWAY
    if (doorOpen) startWarning("DOOR OPEN FOUND");
    else if (digitalRead(PIR_PIN) == HIGH) startWarning("MOTION DETECTED");
  } 
  else if (systemState == 4) { // HOME
    if (doorOpen) startWarning("DOOR OPEN FOUND");
  }

  // 5. Warning Timer
  if (systemState == 3) {
    unsigned long currentMillis = millis();
    int timeLeft = 10 - (currentMillis - entryDelayStart) / 1000;
    if (timeLeft <= 0) triggerAlarm(currentAlarmReason);
    else {
      if (currentMillis % 1000 < 150) tone(BUZZER_PIN, 800);
      else noTone(BUZZER_PIN);
    }
  }
}

void startWarning(String reason) {
  systemState = 3;
  entryDelayStart = millis();
  currentAlarmReason = reason;
  enteredInput = ""; 
  lcd.clear();
}

void updateDisplay(int smoke, bool door) {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 300) return; 
  lastUpdate = millis();

  lcd.setCursor(0, 0);
  if (systemState == 3) {
    int timeLeft = 10 - (millis() - entryDelayStart) / 1000;
    lcd.print("WARN! T-"); lcd.print(timeLeft); lcd.print("s    ");
    lcd.setCursor(0, 1);
    lcd.print("PIN:"); lcd.print(enteredInput); lcd.print("      ");
  } 
  else if (systemState != 2) {
    if (systemState == 0) lcd.print("SAFE  ");
    else if (systemState == 1) lcd.print("AWAY  ");
    else if (systemState == 4) lcd.print("HOME  ");
    
    lcd.print(" S:"); lcd.print(smoke); lcd.print("  ");
    lcd.setCursor(0, 1);
    lcd.print(door ? "D:OPEN " : "D:CLSD ");
    lcd.print("PIN:");
    for(int i=0; i<enteredInput.length(); i++) lcd.print("*");
    lcd.print("    ");
  }
}

void handleHashKey() {
  // Scenario 1: Password entry to Disarm
  if (enteredInput == masterPassword) {
    disarmSystem();
    lcd.clear();
    lcd.print("SYSTEM DISARMED");
    delay(1500);
  } 
  // Scenario 2: Enter Admin Menu (Hold # for 3s in SAFE mode)
  else if (systemState == 0) {
    lcd.clear();
    lcd.print("VERIFYING...");
    unsigned long pressStart = millis();
    bool held = true;
    
    // Check if # is held for 3 seconds
    while (keypad.getState() == PRESSED) {
      if (millis() - pressStart > 3000) {
        adminMenu();
        return;
      }
    }
    
    // If released before 3s and wrong PIN
    lcd.clear();
    lcd.print("WRONG PIN!");
    delay(1000);
    enteredInput = "";
  }
  else {
    lcd.clear();
    lcd.print("WRONG PIN!");
    delay(1000);
    enteredInput = "";
  }
}

void adminMenu() {
  lcd.clear();
  lcd.print("ENTER SAP ID:");
  enteredInput = "";
  while(true) {
    char key = keypad.waitForKey();
    if (key >= '0' && key <= '9') {
      enteredInput += key;
      lcd.setCursor(0, 1);
      lcd.print(enteredInput);
    } else if (key == '#') {
      if (enteredInput == sapID) {
        changePIN();
        break;
      } else {
        lcd.clear();
        lcd.print("INVALID SAP ID!");
        delay(2000);
        break;
      }
    } else if (key == 'D') {
      break; // Cancel admin menu
    }
  }
  disarmSystem();
}

void changePIN() {
  lcd.clear();
  lcd.print("NEW 4-DIGIT PIN:");
  enteredInput = "";
  while(true) {
    char key = keypad.waitForKey();
    if (key >= '0' && key <= '9' && enteredInput.length() < 4) {
      enteredInput += key;
      lcd.setCursor(enteredInput.length()-1, 1);
      lcd.print("*");
    } else if (key == '#') {
      if(enteredInput.length() == 4) {
        masterPassword = enteredInput;
        lcd.clear();
        lcd.print("PIN UPDATED!");
        delay(2000);
        break;
      }
    }
  }
}

void armAway() {
  lcd.clear();
  lcd.print("AWAY MODE IN:");
  digitalWrite(GREEN_LED_PIN, LOW);
  for(int i=10; i>0; i--) {
    lcd.setCursor(14, 0); lcd.print(i); lcd.print(" ");
    digitalWrite(YELLOW_LED_PIN, HIGH);
    tone(BUZZER_PIN, 1500, 100);
    delay(1000);
    digitalWrite(YELLOW_LED_PIN, LOW);
  }
  systemState = 1;
  digitalWrite(YELLOW_LED_PIN, HIGH);
  lcd.clear();
}

void armHome() {
  lcd.clear();
  lcd.print("HOME MODE ON");
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  tone(BUZZER_PIN, 2000, 500); 
  delay(2000);
  systemState = 4;
  lcd.clear();
}

void disarmSystem() {
  systemState = 0;
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  noTone(BUZZER_PIN);
  enteredInput = "";
  lcd.clear();
}

void triggerAlarm(String msg) {
  systemState = 2;
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("!! ALARM !!");
  lcd.setCursor(0, 1);
  lcd.print(msg);
  tone(BUZZER_PIN, 1000); 
}