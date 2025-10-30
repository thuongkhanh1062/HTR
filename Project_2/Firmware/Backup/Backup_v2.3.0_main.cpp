/**
 * @author CHITOAN
 * @date June 2024 (Revised October 2025)
 * @brief Reliable AC Detect and Relay Control System with OLED Display
 * @version v2.3.0
 */

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --------------------------- CONSTANT DEFINITIONS ---------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUTTON_COUNT 5
#define AC_COUNT 4
#define RELAY_COUNT 4
#define LED_COUNT 8
#define LEDCTRL_COUNT 4
#define EEPROM_SIZE 64

// --------------------------- HARDWARE PINS ---------------------------
const int ButtonPin[BUTTON_COUNT] = {PB0, PC15, PC7, PC6, PB11};
const int ACPin[AC_COUNT] = {PC0, PC1, PC2, PA0};           // Input from AC detectors PA0 = AC4, PC0 = AC1, PC1 = AC2, PC2 = AC3
const int RelayPin[RELAY_COUNT] = {PC11, PC10, PA15, PC12}; // Relays 1–4
const int LEDPin[LED_COUNT] = {PB3, PB4, PC3, PC4, PC5, PB9, PB8, PD2};
const int LEDCtrl[LEDCTRL_COUNT] = {PC14, PC13, PD0, PD1};
const int BuzzerPin = PA8;

// --------------------------- OBJECTS ---------------------------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --------------------------- GLOBAL VARIABLES ---------------------------
// Timing
unsigned long lastDisplay = 0;
const unsigned long displayInterval = 100;

// AC detect filtering
bool electricalDetect[AC_COUNT] = {false};
unsigned long lastPulseTime[AC_COUNT] = {0};
int detectCounter[AC_COUNT] = {0};
const int detectThreshold = 5;
const unsigned long lostTimeout = 100;
volatile unsigned long lastSignalTime[4] = {0};
volatile bool flag[4] = {0};

// Relay state
int relayActive = -1;
bool relayState[RELAY_COUNT] = {false};

// Button
bool lastButtonState[BUTTON_COUNT];
unsigned long lastDebounceTime[BUTTON_COUNT];
const unsigned long debounceDelay = 100;

// Buzzer
bool buzzerActive = false;
unsigned long buzzerStartTime = 0;
unsigned long buzzerDuration = 0;
int toneFreq = 2000;
unsigned long previousMillis = 0;
unsigned long elapsedMillis = 0;

int hours = 0;
int minutes = 0;
int seconds = 0;
uint32_t bootCount = 0;

// Biến long press
bool buttonPressed = false;
unsigned long pressStart = 0;
const unsigned long longPressTime = 3000; // 3s nhấn giữ

struct BootData
{
  uint32_t bootCount;
  uint8_t rebootHour;
  uint8_t rebootMinute;
  uint8_t rebootSecond;
};
BootData data;

// --------------------------- FUNCTION DECLARATIONS ---------------------------
void buzzerBeep(unsigned long duration);
void updateBuzzer();
void updateACDetect();
void updateRelays();
void updateDisplay();
void updateButtons();
void saveBootData();
void readBootData();
void AC1Interrupt();
void AC2Interrupt();
void AC3Interrupt();
void AC4Interrupt();

void AC1Interrupt()
{
  electricalDetect[0] = 1;
  lastSignalTime[0] = millis();
}
void AC2Interrupt()
{
  electricalDetect[1] = 1;
  lastSignalTime[1] = millis();
}
void AC3Interrupt()
{
  electricalDetect[2] = 1;
  lastSignalTime[2] = millis();
}
void AC4Interrupt()
{
  electricalDetect[3] = 1;
  lastSignalTime[3] = millis();
}

// ============================================================================
//                                 SETUP
// ============================================================================
void setup()
{
  Serial.begin(115200);
  Wire.begin();

  readBootData();
  data.bootCount++;
  data.rebootHour = hours;
  data.rebootMinute = minutes;
  data.rebootSecond = seconds;

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed!"));
    for (int i = 0; i < 3; i++)
      buzzerBeep(200);
  }

  display.setRotation(1);
  display.clearDisplay();
  display.display();

  // Button setup
  for (int i = 0; i < BUTTON_COUNT; i++)
  {
    pinMode(ButtonPin[i], INPUT_PULLUP);
    lastButtonState[i] = HIGH;
    lastDebounceTime[i] = 0;
  }

  // AC input setup
  for (int i = 0; i < AC_COUNT; i++)
  {
    pinMode(ACPin[i], INPUT_PULLDOWN);
    lastSignalTime[i] = millis();
  }

  // Relay setup
  for (int i = 0; i < RELAY_COUNT; i++)
  {
    pinMode(RelayPin[i], OUTPUT);
    digitalWrite(RelayPin[i], LOW);
  }

  // LED & Buzzer
  for (int i = 0; i < LED_COUNT; i++)
    pinMode(LEDPin[i], OUTPUT);
  for (int i = 0; i < LEDCTRL_COUNT; i++)
    pinMode(LEDCtrl[i], OUTPUT);
  pinMode(BuzzerPin, OUTPUT);

  buzzerBeep(150);
  Serial.println("System initialized successfully!");
  saveBootData();
  attachInterrupt(digitalPinToInterrupt(ACPin[0]), AC1Interrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(ACPin[1]), AC2Interrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(ACPin[2]), AC3Interrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(ACPin[3]), AC4Interrupt, RISING);
}

// ============================================================================
//                                 LOOP
// ============================================================================
void loop()
{
  updateACDetect();
  updateRelays();
  updateButtons();
  updateBuzzer();
  unsigned long now = millis();
  if (now - lastDisplay >= displayInterval)
  {
    lastDisplay = now;
    updateDisplay();
  }

  if (now - previousMillis >= 1000)
  {
    previousMillis = now;
    seconds++;
    if (seconds >= 60)
    {
      seconds = 0;
      minutes++;
    }
    if (minutes >= 60)
    {
      minutes = 0;
      hours++;
    }
    if (hours >= 24)
    {
      hours = 0;
    }
  }
  if (seconds % 30 == 0 && seconds != 0)
  {
    saveBootData();
  }

  int buttonState = digitalRead(ButtonPin[0]);

  if (buttonState == LOW && !buttonPressed)
  {
    buttonPressed = true;
    pressStart = now;
  }

  if (buttonState == HIGH && buttonPressed)
  {
    buttonPressed = false;
  }

  if (buttonPressed && (now - pressStart >= longPressTime))
  {
    data.bootCount = 0;
    saveBootData();
    buzzerBeep(150);
    buttonPressed = false;
  }
}

// ============================================================================
//                          FUNCTION IMPLEMENTATIONS
// ============================================================================

// --------------------------- BUZZER ---------------------------
void buzzerBeep(unsigned long duration)
{
  if (!buzzerActive)
  {
    buzzerActive = true;
    buzzerStartTime = millis();
    buzzerDuration = duration;
    tone(BuzzerPin, toneFreq);
  }
}

void updateBuzzer()
{
  if (buzzerActive && millis() - buzzerStartTime >= buzzerDuration)
  {
    noTone(BuzzerPin);
    buzzerActive = false;
  }
}

// --------------------------- AC DETECT WITH FILTER ---------------------------
void updateACDetect()
{
  unsigned long now = millis();

  for (int i = 0; i < AC_COUNT; i++)
  {
    if (millis() - lastSignalTime[i] > lostTimeout)
    {
      electricalDetect[i] = 0;
    }
  }
}

// --------------------------- RELAY CONTROL LOGIC ---------------------------
void updateRelays()
{
  // Turn OFF all first
  for (int i = 0; i < RELAY_COUNT; i++)
    relayState[i] = false;

  // Channel 4: Independent (direct)
  relayState[3] = electricalDetect[3];

  // Channels 1–3: priority logic
  if (electricalDetect[0] && electricalDetect[1] && electricalDetect[2])
  {
    relayState[0] = true;
    relayActive = 0;
  }
  else if (relayActive == -1)
  {
    for (int i = 0; i < 3; i++)
    {
      if (electricalDetect[i])
      {
        relayState[i] = true;
        relayActive = i;
        buzzerBeep(100);
        break;
      }
    }
  }
  else if (electricalDetect[relayActive])
  {
    relayState[relayActive] = true;
  }
  else
  {
    relayActive = -1;
  }

  // Apply relay states
  for (int i = 0; i < RELAY_COUNT; i++)
  {
    digitalWrite(RelayPin[i], relayState[i] ? HIGH : LOW);
  }
}

// --------------------------- DISPLAY UPDATE ---------------------------
void updateDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.printf("RL1:%s\nRL2:%s\n", relayState[0] ? "ON " : "OFF", relayState[1] ? "ON " : "OFF");
  display.printf("RL3:%s\nRL4:%s\n", relayState[2] ? "ON " : "OFF", relayState[3] ? "ON " : "OFF");

  display.println();
  for (int i = 0; i < AC_COUNT; i++)
  {
    display.printf("CH%d: %s\n", i + 1, electricalDetect[i] ? "PWR" : "UPWR");
  }
  display.println();
  display.printf("%02d:%02d:%02d\n", hours, minutes, seconds);
  display.println();
  display.printf("btime:%d\n%02d:%02d:%02d\n", data.bootCount, data.rebootHour, data.rebootMinute, data.rebootSecond);
  display.display();
}

// --------------------------- BUTTON UPDATE ---------------------------
void updateButtons()
{
  for (int i = 0; i < BUTTON_COUNT; i++)
  {
    bool reading = digitalRead(ButtonPin[i]);

    if (reading != lastButtonState[i])
    {
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay)
    {
      if (reading == LOW && lastButtonState[i] == HIGH)
      {
        buzzerBeep(150);
        Serial.printf("Button %d pressed\n", i + 1);
      }
    }
    lastButtonState[i] = reading;
  }
}

void saveBootData()
{
  EEPROM.put(0, data);
}

void readBootData()
{
  EEPROM.get(0, data);
}