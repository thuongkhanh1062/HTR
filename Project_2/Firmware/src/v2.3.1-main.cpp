/**
 * @author CHITOAN
 * @date June 2024 (Revised October 2025)
 * @brief Reliable AC Detect and Relay Control System with OLED Display
 * @version v2.3.1
 */

#include <Arduino.h>

// --------------------------- CONSTANT DEFINITIONS ---------------------------
#define AC_COUNT 4
#define RELAY_COUNT 4

// --------------------------- HARDWARE PINS ---------------------------
const int ACPin[AC_COUNT] = {PC0, PC1, PC2, PA0};           // Input from AC detectors PA0 = AC4, PC0 = AC1, PC1 = AC2, PC2 = AC3
const int RelayPin[RELAY_COUNT] = {PC11, PC10, PA15, PC12}; // Relays 1–4
const int BuzzerPin = PA8;

// --------------------------- GLOBAL VARIABLES ---------------------------
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

// Buzzer
bool buzzerActive = false;
unsigned long buzzerStartTime = 0;
unsigned long buzzerDuration = 0;
int toneFreq = 2000;
unsigned long previousMillis = 0;
unsigned long elapsedMillis = 0;

// --------------------------- FUNCTION DECLARATIONS ---------------------------
void buzzerBeep(unsigned long duration);
void updateBuzzer();
void updateACDetect();
void updateRelays();
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
  pinMode(BuzzerPin, OUTPUT);
  buzzerBeep(150);
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
  updateBuzzer();
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