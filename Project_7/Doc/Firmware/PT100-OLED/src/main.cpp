#include <U8g2lib.h>
#include <Arduino.h>

#define sensorPin 35
#define FILTER_SIZE 25

const int NUM_POINTS = 10;
float voltageBuffer[FILTER_SIZE];
int filterIndex = 0;
bool filterFilled = false;

float voltTable[NUM_POINTS] = {0.032, 0.25, 0.46, 0.66, 0.87, 1.06, 1.25, 1.44, 1.62, 1.8};
float tempTable[NUM_POINTS] = {-50, 0, 50, 100, 150, 200, 250, 300, 350, 400};

float adcVoltage = 0;
float voltage_offset = 0.0f;
unsigned long prevUpdateMillis = 0;

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

float interpolateTemp(float voltage);
float getFilteredVoltage(float newVal);
void showup();

void setup()
{
  Serial.begin(115200);
  u8g2.begin();
  pinMode(sensorPin, INPUT);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}

void loop() { showup(); }

float getFilteredVoltage(float newVal)
{
  voltageBuffer[filterIndex] = newVal;
  filterIndex = (filterIndex + 1) % FILTER_SIZE;
  if (filterIndex == 0)
    filterFilled = true;

  float sum = 0;
  int count = filterFilled ? FILTER_SIZE : filterIndex;
  for (int i = 0; i < count; i++)
    sum += voltageBuffer[i];

  return sum / count;
}

float interpolateTemp(float voltage)
{
  if (voltage <= voltTable[0])
    return tempTable[0];
  if (voltage >= voltTable[NUM_POINTS - 1])
    return tempTable[NUM_POINTS - 1];

  for (int i = 0; i < NUM_POINTS - 1; i++)
    if (voltage >= voltTable[i] && voltage <= voltTable[i + 1])
      return tempTable[i] + (tempTable[i + 1] - tempTable[i]) * (voltage - voltTable[i]) / (voltTable[i + 1] - voltTable[i]);

  return -999;
}

void showup()
{
  int raw = analogRead(sensorPin);
  float voltage = raw * 3.3f / 4095.0f;
  adcVoltage = adcVoltage * 0.9 + voltage * 0.1;
  float filteredVoltage = getFilteredVoltage(adcVoltage + voltage_offset);
  float Temp = interpolateTemp(filteredVoltage);

  if (millis() - prevUpdateMillis >= 400)
  {
    prevUpdateMillis = millis();
    Serial.printf("ADC=%d  Volt=%.3fV  Temp=%.2fC\n", raw, filteredVoltage, Temp);

    char line1[32], line2[32];
    snprintf(line1, sizeof(line1), "ADC:%d  %.3fV", raw, filteredVoltage);
    snprintf(line2, sizeof(line2), "Temp: %.2f C", Temp);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(0, 12, line1);
    u8g2.drawStr(0, 28, line2);
    u8g2.sendBuffer();
  }
}
