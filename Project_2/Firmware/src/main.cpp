/*
@ChitoanRepo
@author: Chitoan
@date: 2023-10-01
@description: AC Power Detection for STM32 using Arduino Framework
@Version: 2.4.0
Sapling Speed: 50ms - 20Hz | CH4 Check: 5ms - 200Hz
*/

#include <Arduino.h>
HardwareSerial Serial1(PA10, PA9); // RX, TX

#define CH1_PIN PC0
#define CH2_PIN PC1
#define CH3_PIN PC2
#define CH4_PIN PA0
#define BuzzerPin PA8

#define RELAY_COUNT 4
const int RelayPin[RELAY_COUNT] = {PC11, PC10, PA15, PC12};

int relayActive = -1;
bool relayState[RELAY_COUNT] = {false};

// Buzzer
bool buzzerActive = false;
unsigned long buzzerStartTime = 0;
unsigned long buzzerDuration = 0;
unsigned long lastBuzzerToggle = 0;
bool buzzerPinState = LOW;
int buzzerPeriod_us = 250;

// Relay state
bool lastRelayState[RELAY_COUNT] = {false, false, false, false};

// AC detection
volatile bool validSignal[4] = {false, false, false, false};
volatile uint32_t lastTime_us[4] = {0, 0, 0, 0};

#define N 10
uint8_t samples[N] = {0};
uint8_t sampleIndex = 0;
bool lastCH4State = LOW;
unsigned long lastCH4EdgeTime = 0;
unsigned long lastValidEdgeTime = 0;

static uint32_t prevMillis = 0;
static uint32_t prevCH4CheckMillis = 0;

// ================= ISR ==================
void zeroCrossISR_CH1()
{
  validSignal[0] = true;
  lastTime_us[0] = micros();
}

void zeroCrossISR_CH2()
{
  validSignal[1] = true;
  lastTime_us[1] = micros();
}

void zeroCrossISR_CH3()
{
  validSignal[2] = true;
  lastTime_us[2] = micros();
}

// --------------------------- FUNCTION DECLARATIONS ---------------------------
void buzzerBeep(unsigned long duration);
void updateBuzzer();
void acdetect();
bool detectCH4();
void update_sample(uint8_t sample);
bool majority();

void setup()
{
  Serial1.begin(115200);

  pinMode(CH1_PIN, INPUT_PULLUP);
  pinMode(CH2_PIN, INPUT_PULLUP);
  pinMode(CH3_PIN, INPUT_PULLUP);
  pinMode(CH4_PIN, INPUT_PULLUP);
  pinMode(BuzzerPin, OUTPUT);

  for (int i = 0; i < RELAY_COUNT; i++)
  {
    pinMode(RelayPin[i], OUTPUT);
    digitalWrite(RelayPin[i], LOW);
  }

  attachInterrupt(digitalPinToInterrupt(CH1_PIN), zeroCrossISR_CH1, RISING);
  attachInterrupt(digitalPinToInterrupt(CH2_PIN), zeroCrossISR_CH2, RISING);
  attachInterrupt(digitalPinToInterrupt(CH3_PIN), zeroCrossISR_CH3, RISING);
  buzzerBeep(200);
}

/*
============================ MAIN LOOP ============================
*/
void loop()
{
  updateBuzzer();
  acdetect();
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
    buzzerPinState = LOW;
    lastBuzzerToggle = micros();
  }
}

void updateBuzzer()
{
  if (buzzerActive)
  {
    if (millis() - buzzerStartTime >= buzzerDuration)
    {
      digitalWrite(BuzzerPin, LOW);
      buzzerActive = false;
      return;
    }

    unsigned long now = micros();
    if (now - lastBuzzerToggle >= buzzerPeriod_us)
    {
      buzzerPinState = !buzzerPinState;
      digitalWrite(BuzzerPin, buzzerPinState);
      lastBuzzerToggle = now;
    }
  }
}

void acdetect()
{
  if (millis() - prevCH4CheckMillis >= 5)
  {
    prevCH4CheckMillis = millis();
    validSignal[3] = detectCH4();
  }

  if (millis() - prevMillis > 50)
  {
    prevMillis = millis();

    for (int i = 0; i < 3; i++)
    {
      bool signal;
      uint32_t last_us;

      noInterrupts();
      signal = validSignal[i];
      last_us = lastTime_us[i];
      interrupts();

      if (micros() - last_us > 500000)
      {
        signal = false;
        validSignal[i] = false;
      }
    }

    if (validSignal[0] == true && validSignal[1] == true && validSignal[2] == true)
    {
      digitalWrite(RelayPin[0], HIGH);
      digitalWrite(RelayPin[1], LOW);
      digitalWrite(RelayPin[2], LOW);
    }
    else if (validSignal[0] == false)
    {
      if (validSignal[1] == true && validSignal[2] == true)
      {
        digitalWrite(RelayPin[0], LOW);
        digitalWrite(RelayPin[1], HIGH);
        digitalWrite(RelayPin[2], LOW);
      }
      else if (validSignal[1] == false && validSignal[2] == true)
      {
        digitalWrite(RelayPin[0], LOW);
        digitalWrite(RelayPin[1], LOW);
        digitalWrite(RelayPin[2], HIGH);
      }
      else
      {
        digitalWrite(RelayPin[0], LOW);
        digitalWrite(RelayPin[1], LOW);
        digitalWrite(RelayPin[2], LOW);
      }
    }

    digitalWrite(RelayPin[3], validSignal[3] ? HIGH : LOW);

    for (int i = 0; i < RELAY_COUNT; i++)
    {
      bool currentState = digitalRead(RelayPin[i]);
      if (currentState != lastRelayState[i])
      {
        lastRelayState[i] = currentState;
        buzzerBeep(200);
        break;
      }
    };

    Serial1.printf("CH1: %s\t CH2: %s\t CH3: %s\t CH4: %s\t RL1: %s\t RL2: %s\t RL3: %s\t RL4: %s\n",
                   validSignal[0] ? "Powered" : "Nopower",
                   validSignal[1] ? "Powered" : "Nopower",
                   validSignal[2] ? "Powered" : "Nopower",
                   validSignal[3] ? "Powered" : "Nopower",
                   digitalRead(RelayPin[0]) ? "ON" : "OFF",
                   digitalRead(RelayPin[1]) ? "ON" : "OFF",
                   digitalRead(RelayPin[2]) ? "ON" : "OFF",
                   digitalRead(RelayPin[3]) ? "ON" : "OFF");
  }
}

bool detectCH4()
{
  unsigned long now = micros();
  bool currentState = digitalRead(CH4_PIN);

  if (currentState == HIGH && lastCH4State == LOW)
  {
    unsigned long timeSinceLastEdge = now - lastCH4EdgeTime;
    lastCH4EdgeTime = now;
    if (timeSinceLastEdge >= 8000 && timeSinceLastEdge <= 25000)
    {
      update_sample(1);
      lastValidEdgeTime = now;
    }
    else
    {
      update_sample(0);
    }
  }
  else if (now - lastCH4EdgeTime > 50000)
  {
    update_sample(0);
  }
  lastCH4State = currentState;
  bool AC4_flag = majority();
  if (now - lastValidEdgeTime > 500000)
  {
    AC4_flag = false;
  }

  return AC4_flag;
}

void update_sample(uint8_t sample)
{
  samples[sampleIndex] = sample;
  sampleIndex++;
  if (sampleIndex >= N)
  {
    sampleIndex = 0;
  }
}

bool majority()
{
  uint8_t count = 0;
  for (int i = 0; i < N; i++)
  {
    count += samples[i];
  }
  return (count >= 7);
}