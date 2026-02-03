#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>

#define BTN_TOTAL   34
#define BTN_PTIME   35
#define BUZZER_PIN  15

LiquidCrystal_I2C lcd(0x27, 16, 2);
Preferences prefs;

/* ================= TIME ================= */
unsigned long totalStoredSec, ptimeStoredSec;
unsigned long totalStartMs, ptimeStartMs;
unsigned long totalSessionSec, ptimeSessionSec;

/* ================= STATE ================= */
bool pauseTotal = false;
bool pausePtime = false;

/* ================= BUTTON ================= */
unsigned long btnTotalMs = 0, btnPtimeMs = 0;
bool totalHandled = false, ptimeHandled = false;

/* ================= LCD ================= */
unsigned long lastLcdUpdate = 0;

/* ================= BUZZER ================= */
bool buzzerActive = false;
unsigned long buzzerMs = 0;
int buzzerToggleRemain = 0;

/* ================= SAVE ================= */
unsigned long lastSaveMs = 0;

/* ================= PROTOTYPES ================= */
void handleButton(int pin, unsigned long &tPress, bool &handled,
                  void (*shortPress)(), void (*longPress)());
void togglePauseTotal();
void togglePausePtime();
void resetTotal();
void resetPtime();
void beepStart(int count);
void buzzerUpdate();

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();

  pinMode(BTN_TOTAL, INPUT);
  pinMode(BTN_PTIME, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); // buzzer active LOW

  prefs.begin("timer", false);
  totalStoredSec = prefs.getULong("total", 0);
  ptimeStoredSec = prefs.getULong("ptime", 0);

  unsigned long now = millis();
  totalStartMs = now;
  ptimeStartMs = now;
}

void loop() {
  unsigned long now = millis();

  /* ===== BUZZER ===== */
  buzzerUpdate();

  /* ===== BUTTON ===== */
  handleButton(BTN_TOTAL, btnTotalMs, totalHandled,
               togglePauseTotal, resetTotal);
  handleButton(BTN_PTIME, btnPtimeMs, ptimeHandled,
               togglePausePtime, resetPtime);

  /* ===== TIME UPDATE ===== */
  totalSessionSec = pauseTotal ? 0 : (now - totalStartMs) / 1000;
  ptimeSessionSec = pausePtime ? 0 : (now - ptimeStartMs) / 1000;

  unsigned long totalSec = totalStoredSec + totalSessionSec;
  unsigned long ptimeSec = ptimeStoredSec + ptimeSessionSec;

  /* ===== LCD UPDATE (5Hz) ===== */
  if (now - lastLcdUpdate >= 200) {
    lastLcdUpdate = now;
    char line[17];

    // ----- LINE 1 -----
    if (pauseTotal) {
      snprintf(line, 17, "TOTAL:   PAUSE  ");
    } else {
      float h = totalSec / 3600.0;
      snprintf(line, 17, "TOTAL: %5.1f Hr ", h);
    }
    lcd.setCursor(0, 0);
    lcd.print(line);

    // ----- LINE 2 -----
    if (pausePtime) {
      snprintf(line, 17, "PTIME:   PAUSE  ");
    } else {
      float h = ptimeSec / 3600.0;
      snprintf(line, 17, "PTIME: %5.1f Hr ", h);
    }
    lcd.setCursor(0, 1);
    lcd.print(line);
  }

  /* ===== SAVE EEPROM ===== */
  if (now - lastSaveMs >= 5000) {
    prefs.putULong("total", totalSec);
    prefs.putULong("ptime", ptimeSec);
    lastSaveMs = now;
  }
}

/* ================= FUNCTIONS ================= */

void handleButton(int pin, unsigned long &tPress, bool &handled,
                  void (*shortPress)(), void (*longPress)()) {
  unsigned long now = millis();
  bool pressed = digitalRead(pin);

  if (pressed) {
    if (tPress == 0) tPress = now;
    if (!handled && now - tPress >= 10000) {
      longPress();
      handled = true;
    }
  } else if (tPress > 0) {
    if (!handled && now - tPress < 800) {
      shortPress();
    }
    tPress = 0;
    handled = false;
  }
}

/* ===== TOGGLE ===== */
void togglePauseTotal() {
  beepStart(1);
  if (!pauseTotal) {
    totalStoredSec += totalSessionSec;
    pauseTotal = true;
  } else {
    pauseTotal = false;
    totalStartMs = millis();
  }
}

void togglePausePtime() {
  beepStart(1);
  if (!pausePtime) {
    ptimeStoredSec += ptimeSessionSec;
    pausePtime = true;
  } else {
    pausePtime = false;
    ptimeStartMs = millis();
  }
}

/* ===== RESET ===== */
void resetTotal() {
  totalStoredSec = 0;
  prefs.putULong("total", 0);
  totalStartMs = millis();
  pauseTotal = false;
  beepStart(3);
}

void resetPtime() {
  ptimeStoredSec = 0;
  prefs.putULong("ptime", 0);
  ptimeStartMs = millis();
  pausePtime = false;
  beepStart(3);
}

/* ===== BUZZER ===== */
void beepStart(int count) {
  buzzerToggleRemain = count * 2;
  buzzerMs = millis();
  buzzerActive = true;
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerUpdate() {
  if (!buzzerActive) return;
  if (millis() - buzzerMs >= 150) {
    buzzerMs = millis();
    digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    if (--buzzerToggleRemain <= 0) {
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerActive = false;
    }
  }
}
