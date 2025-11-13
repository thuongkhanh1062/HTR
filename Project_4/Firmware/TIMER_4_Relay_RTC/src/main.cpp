#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <RTClib.h>
#include <U8g2lib.h>

// ==========================
// --- system configuration ---
// ==========================
#define NUM_RELAYS 4
#define NUM_BUTTONS 4
#define DEBOUNCE_MS 60
#define HOLD_TIME_MS 1000
#define DISPLAY_REFRESH_MS 80

// ==========================
// ----------- IO -----------
// ==========================
const int relayPins[NUM_RELAYS] = {12, 14, 27, 26};
const int buttonPins[NUM_BUTTONS] = {33, 32, 35, 34}; // UP / NEXT / DOWN / BACK
const int buzzerPin = 4;

// ==========================
// ---- Global Variable -----
// ==========================
RTC_DS1307 rtc;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void saveTimers();
void loadTimers();
void updateRelays();
void showHome(DateTime now);
void showEditTimer();
void showSetRTC();
void beep(unsigned int freq, unsigned int duration);
void beepToggle();
void beepRelay();
void beepSave();
void beepHold();
void beepClick();

enum MenuState
{
  MENU_HOME,
  MENU_EDIT_TIMER,
  MENU_SET_RTC
};

struct TimerData
{
  int on[3];
  int off[3];
  bool enabled;
};

TimerData timers[NUM_RELAYS];

// ==========================
// --- button class process ---
// ==========================
class Button
{
private:
  int pin;
  bool lastState;
  unsigned long lastDebounce;
  unsigned long pressedTime;
  bool holdReported;

public:
  Button(int p) : pin(p), lastState(HIGH), lastDebounce(0), pressedTime(0), holdReported(false) {}

  void begin()
  {
    pinMode(pin, INPUT_PULLUP);
  }

  int read()
  {
    bool reading = digitalRead(pin);
    unsigned long now = millis();

    if (reading != lastState)
    {
      lastDebounce = now;
      lastState = reading;
    }

    if (now - lastDebounce < DEBOUNCE_MS)
      return 0;

    if (reading == LOW)
    {
      if (pressedTime == 0)
        pressedTime = now;
      if (!holdReported && now - pressedTime > HOLD_TIME_MS)
      {
        holdReported = true;
        beepHold();
        return 2; // long press
      }
    }
    else
    {
      if (pressedTime != 0 && !holdReported)
      {
        pressedTime = 0;
        beepClick();
        return 1; // short press
      }
      pressedTime = 0;
      holdReported = false;
    }
    return 0;
  }
};

Button buttons[NUM_BUTTONS] = {
    Button(buttonPins[0]), // UP
    Button(buttonPins[1]), // NEXT
    Button(buttonPins[2]), // DOWN
    Button(buttonPins[3])  // BACK
};

// ==========================
// ----- Variable menu ------
// ==========================
MenuState menuState = MENU_HOME;
int currentTimer = 0;
int editState = 0;
unsigned long lastMenuActivity = 0;
int rtcH, rtcM, rtcS;

// ==========================
// --------- Setup ----------
// ==========================
void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);
  u8g2.begin();

  if (!rtc.begin())
  {
    Serial.println("RTC not found!");
    while (1)
      delay(100);
  }

  EEPROM.begin(128);
  loadTimers();

  pinMode(buzzerPin, OUTPUT);
  for (int i = 0; i < NUM_RELAYS; i++)
  {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  for (int i = 0; i < NUM_BUTTONS; i++)
    buttons[i].begin();
}

// ==========================
// ---------- Loop ----------
// ==========================
void loop()
{
  static unsigned long lastDisplay = 0;
  DateTime now = rtc.now();
  updateRelays();

  if (millis() - lastDisplay > DISPLAY_REFRESH_MS)
  {
    lastDisplay = millis();
    if (menuState == MENU_HOME)
      showHome(now);
    else if (menuState == MENU_EDIT_TIMER)
      showEditTimer();
    else if (menuState == MENU_SET_RTC)
      showSetRTC();
  }

  int up = buttons[0].read();
  int next = buttons[1].read();
  int down = buttons[2].read();
  int back = buttons[3].read();

  // ========== MAIN MENU ==========
  if (menuState == MENU_HOME)
  {
    if (next == 2)
    {
      menuState = MENU_EDIT_TIMER;
      currentTimer = 0;
      editState = 0;
    }
    else if (back == 2)
    {
      menuState = MENU_SET_RTC;
      DateTime t = rtc.now();
      rtcH = t.hour();
      rtcM = t.minute();
      rtcS = t.second();
      editState = 0;
    }
  }

  // ========== MENU TIMER ==========
  else if (menuState == MENU_EDIT_TIMER)
  {
    if (up == 1)
    {
      int pos = editState % 3;
      int *target = (editState <= 2) ? timers[currentTimer].on : timers[currentTimer].off;
      target[pos] = (target[pos] + 1) % ((pos == 0) ? 24 : 60);
    }
    if (down == 1)
    {
      int pos = editState % 3;
      int *target = (editState <= 2) ? timers[currentTimer].on : timers[currentTimer].off;
      target[pos] = (target[pos] - 1 + ((pos == 0) ? 24 : 60)) % ((pos == 0) ? 24 : 60);
    }
    if (next == 1)
    {
      editState = (editState + 1) % 6;
    }
    if (next == 2)
    {
      timers[currentTimer].enabled = !timers[currentTimer].enabled;
      beepToggle();
      saveTimers();
    }
    if (back == 1)
    {
      menuState = MENU_HOME;
      saveTimers();
    }
    if (back == 2)
    {
      currentTimer = (currentTimer + 1) % NUM_RELAYS;
      editState = 0;
    }
  }

  // ---- MENU RTC ----
  else if (menuState == MENU_SET_RTC)
  {
    if (up == 1)
    {
      if (editState == 0)
        rtcH = (rtcH + 1) % 24;
      else if (editState == 1)
        rtcM = (rtcM + 1) % 60;
      else
        rtcS = (rtcS + 1) % 60;
    }
    if (down == 1)
    {
      if (editState == 0)
        rtcH = (rtcH - 1 + 24) % 24;
      else if (editState == 1)
        rtcM = (rtcM - 1 + 60) % 60;
      else
        rtcS = (rtcS - 1 + 60) % 60;
    }
    if (next == 1)
    {
      editState = (editState + 1) % 3;
    }
    if (back == 2)
    {
      DateTime t = rtc.now();
      rtc.adjust(DateTime(t.year(), t.month(), t.day(), rtcH, rtcM, rtcS));
      beepSave();
      menuState = MENU_HOME;
    }
  }
}

// ==========================
// --------- EEPROM ---------
// ==========================
void saveTimers()
{
  int addr = 0;
  for (int i = 0; i < NUM_RELAYS; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      EEPROM.write(addr++, timers[i].on[j]);
      EEPROM.write(addr++, timers[i].off[j]);
    }
    EEPROM.write(addr++, timers[i].enabled);
  }
  EEPROM.commit();
  beepSave();
}

void loadTimers()
{
  int addr = 0;
  for (int i = 0; i < NUM_RELAYS; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      timers[i].on[j] = EEPROM.read(addr++);
      timers[i].off[j] = EEPROM.read(addr++);
    }
    timers[i].enabled = EEPROM.read(addr++);
  }
}

// ==========================
// --------- Relay ----------
// ==========================
void updateRelays()
{
  static bool lastRelayState[NUM_RELAYS] = {false};
  DateTime now = rtc.now();

  for (int i = 0; i < NUM_RELAYS; i++)
  {
    bool relayState = LOW;

    if (timers[i].enabled)
    {
      int onH = timers[i].on[0];
      int onM = timers[i].on[1];
      int onS = timers[i].on[2];
      int offH = timers[i].off[0];
      int offM = timers[i].off[1];
      int offS = timers[i].off[2];

      unsigned long cur = now.hour() * 3600 + now.minute() * 60 + now.second();
      unsigned long onSec = onH * 3600 + onM * 60 + onS;
      unsigned long offSec = offH * 3600 + offM * 60 + offS;

      bool normal = onSec < offSec;
      if (normal)
        relayState = (cur >= onSec && cur < offSec);
      else
        relayState = (cur >= onSec || cur < offSec);
    }

    digitalWrite(relayPins[i], relayState ? HIGH : LOW);

    if (relayState != lastRelayState[i])
    {
      lastRelayState[i] = relayState;
      beepRelay();
    }
  }
}

// ==========================
// -------- Display ---------
// ==========================
void showHome(DateTime now)
{
  static bool blink = false;
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 500)
  {
    blink = !blink;
    lastBlink = millis();
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawUTF8((u8g2.getWidth() - u8g2.getUTF8Width("REAL TIME CLOCK")) / 2, 10, "REAL TIME CLOCK");

  char timebuf[16];
  sprintf(timebuf, "%02d%s%02d%s%02d",
          now.hour(), blink ? ":" : " ",
          now.minute(), blink ? ":" : " ",
          now.second());
  u8g2.setFont(u8g2_font_fub20_tr);
  u8g2.drawUTF8((u8g2.getWidth() - u8g2.getUTF8Width(timebuf)) / 2, 31, timebuf);
  u8g2.sendBuffer();
}

// ==========================
// ---- Show Edit Timer -----
// ==========================
void showEditTimer()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tr);

  char onStr[20], offStr[20];
  sprintf(onStr, "T%d ON  %02d:%02d:%02d", currentTimer + 1,
          timers[currentTimer].on[0], timers[currentTimer].on[1], timers[currentTimer].on[2]);
  sprintf(offStr, "T%d OFF %02d:%02d:%02d", currentTimer + 1,
          timers[currentTimer].off[0], timers[currentTimer].off[1], timers[currentTimer].off[2]);
  u8g2.drawStr(0, 12, onStr);
  u8g2.drawStr(0, 28, offStr);

  if (timers[currentTimer].enabled)
    u8g2.drawStr(100, 28, "ON");
  else
    u8g2.drawStr(100, 28, "OFF");

  static bool blink = false;
  static unsigned long blinkTime = 0;
  if (millis() - blinkTime > 400)
  {
    blink = !blink;
    blinkTime = millis();
  }

  if (blink)
  {
    int rowY = (editState <= 2) ? 12 : 28;
    int pos = editState % 3;
    int colX = 42 + pos * 18;
    u8g2.setDrawColor(0);
    char tmp[4];
    int val = (editState <= 2) ? timers[currentTimer].on[pos] : timers[currentTimer].off[pos];
    sprintf(tmp, "%02d", val);
    u8g2.drawStr(colX, rowY, tmp);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

// ==========================
// ------ Show Set RTC ------
// ==========================
void showSetRTC()
{
  static bool blink = false;
  static unsigned long blinkTime = 0;
  if (millis() - blinkTime > 400)
  {
    blink = !blink;
    blinkTime = millis();
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(28, 10, "SET RTC TIME");

  char buf[20];
  sprintf(buf, "%02d:%02d:%02d", rtcH, rtcM, rtcS);
  u8g2.setFont(u8g2_font_fub20_tr);
  u8g2.drawUTF8(8, 32, buf);

  if (blink)
  {
    int colX;
    int width;
    if (editState == 0)
    {
      colX = 8;
      width = 30;
    }
    else if (editState == 1)
    {
      colX = 8 + u8g2.getUTF8Width("00:");
      width = 30;
    }
    else
    {
      colX = 8 + u8g2.getUTF8Width("00:00:");
      width = 30;
    }

    u8g2.setDrawColor(2);
    u8g2.drawBox(colX, 10, width, 24);
    u8g2.setDrawColor(1);
  }

  u8g2.sendBuffer();
}

// ==========================
// --------- Buzzer ---------
// ==========================
void beep(unsigned int freq, unsigned int duration)
{
  tone(buzzerPin, freq, duration);
  delay(duration);
  noTone(buzzerPin);
}

void beepClick()
{
  beep(3000, 40);
}

void beepHold()
{
  beep(1800, 120);
}

void beepSave()
{
  beep(2000, 150);
  beep(2500, 100);
}
void beepRelay()
{
  beep(1000, 100);
}

void beepToggle()
{
  beep(2500, 80);
  beep(1500, 80);
}
