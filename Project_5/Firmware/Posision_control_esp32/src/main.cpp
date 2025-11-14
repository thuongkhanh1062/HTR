/*
@Authur: CHITOAN
@date: 151125
@Version: v1.2

    in this version you can control your angle of output shaft by value of target angle with 2 button up and down
    the value of target angle from 0 to 180 degree.
*/


#include <WiFi.h>
#include <ArduinoOTA.h>
#include <U8g2lib.h>

// ----------------- WIFI CONFIG ----------------
const char *ssid = "YOUR_SSID_WIFI";
const char *password = "YOUR_PASSWORD_WIFI";

// ----------------- DEFINE IO -------------------
#define Channel_A 19
#define Channel_B 18

#define PWM_A 13
#define IN_1A 15
#define IN_2A 5

#define Button_1 25 // Increase angle
#define Button_2 33 // Decrease angle
#define Button_3 32 // Start
#define Button_4 35 // Manual homing (NO PULLUP on ESP32)

#define Limit_S 34 // Limit switch for homing

// ----------------- MOTOR & ENCODER ------------
volatile long encoderCount = 0;
const int ENCODER_PPR = 44;
const float GEAR_RATIO = 45.0;

// ----------------- PID MANUAL -----------------
float Kp = 2.0;
float Ki = 0.05;
float Kd = 0.2;

float integrator = 0.0;
float lastError = 0.0;
unsigned long lastPID = 0;

// ----------------- STATES ---------------------
bool running = false;
bool emergencyStop = false;

float targetAngle = 0;
float currentAngle = 0;

// stall detection
unsigned long lastMoveTime = 0;
long lastEncoderCheck = 0;

// ----------------- OLED -----------------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// ----------------- INTERRUPTS -----------------
void IRAM_ATTR encoderA()
{
  bool A = digitalRead(Channel_A);
  bool B = digitalRead(Channel_B);
  if (A == HIGH)
  {
    if (B == LOW)
      encoderCount++;
    else
      encoderCount--;
  }
  lastMoveTime = millis();
}

void IRAM_ATTR encoderB()
{
  bool B = digitalRead(Channel_B);
  bool A = digitalRead(Channel_A);
  if (B == HIGH)
  {
    if (A == HIGH)
      encoderCount++;
    else
      encoderCount--;
  }
  lastMoveTime = millis();
}

// ----------------- MOTOR CONTROL --------------
void setMotorDir(int dir)
{
  if (dir > 0)
  {
    digitalWrite(IN_1A, HIGH);
    digitalWrite(IN_2A, LOW);
  }
  else if (dir < 0)
  {
    digitalWrite(IN_1A, LOW);
    digitalWrite(IN_2A, HIGH);
  }
  else
  {
    digitalWrite(IN_1A, LOW);
    digitalWrite(IN_2A, LOW);
  }
}

void setPWM(int pwm)
{
  if (pwm < 0)
    pwm = 0;
  if (pwm > 255)
    pwm = 255;
  ledcWrite(0, pwm);
}

void stopMotor()
{
  setMotorDir(0);
  setPWM(0);
}

// ----------------- HOMING ----------------------
void homing()
{
  const int HOMING_PWM = 70;

  setMotorDir(-1);
  setPWM(HOMING_PWM);

  unsigned long t0 = millis();
  while (digitalRead(Limit_S) == HIGH)
  {
    if (millis() - t0 > 7000)
      break;
    delay(5);
  }

  stopMotor();

  // reset encoder
  encoderCount = 0;
}

// ----------------- PID MANUAL -------------------
void computePID()
{
  unsigned long now = millis();
  float dt = (now - lastPID) / 1000.0;
  if (dt < 0.01)
    return;

  lastPID = now;

  float error = targetAngle - currentAngle;

  integrator += error * dt;
  if (integrator > 200)
    integrator = 200;
  if (integrator < -200)
    integrator = -200;

  float derivative = (error - lastError) / dt;

  float output = Kp * error + Ki * integrator + Kd * derivative;

  lastError = error;

  int pwm = abs(output);
  if (pwm > 255)
    pwm = 255;

  if (abs(error) < 0.7)
  {
    stopMotor();
    return;
  }

  if (output > 0)
    setMotorDir(1);
  else
    setMotorDir(-1);

  setPWM(pwm);
}

// ---------------- DISPLAY ----------------------
void updateOLED()
{
  char buf[30];

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  sprintf(buf, "Cur: %.2f", currentAngle);
  u8g2.drawStr(0, 12, buf);

  sprintf(buf, "Target: %.2f", targetAngle);
  u8g2.drawStr(0, 24, buf);

  sprintf(buf, "Error: %.2f", targetAngle - currentAngle);
  u8g2.drawStr(0, 36, buf);

  sprintf(buf, "Kp %.2f Ki %.2f", Kp, Ki);
  u8g2.drawStr(0, 48, buf);

  sprintf(buf, "Kd %.2f", Kd);
  u8g2.drawStr(90, 48, buf);

  if (emergencyStop)
    u8g2.drawStr(0, 62, "!! EMERGENCY STOP !!");
  else if (running)
    u8g2.drawStr(0, 62, "Status: RUN");
  else
    u8g2.drawStr(0, 62, "Status: IDLE");

  u8g2.sendBuffer();
}

// ----------------- SETUP -----------------------
void setup()
{
  Serial.begin(115200);

  pinMode(Channel_A, INPUT);
  pinMode(Channel_B, INPUT);

  pinMode(IN_1A, OUTPUT);
  pinMode(IN_2A, OUTPUT);

  pinMode(Button_1, INPUT_PULLUP);
  pinMode(Button_2, INPUT_PULLUP);
  pinMode(Button_3, INPUT_PULLUP);
  pinMode(Button_4, INPUT); // CHÂN 35 KHÔNG CÓ PULLUP!!!

  pinMode(Limit_S, INPUT_PULLUP);

  ledcSetup(0, 20000, 8);
  ledcAttachPin(PWM_A, 0);

  attachInterrupt(Channel_A, encoderA, CHANGE);
  attachInterrupt(Channel_B, encoderB, CHANGE);

  u8g2.begin();
  // ---------------- WIFI -----------------
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // ---------------- OTA ------------------
  ArduinoOTA.setHostname("ESP32_Motor");
  ArduinoOTA.onStart([]()
                     { Serial.println("Start OTA"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd OTA"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  ArduinoOTA.begin();

  delay(300);
  homing();
  lastPID = millis();
  lastMoveTime = millis();
}

// ----------------- LOOP -------------------------
void loop()
{
  ArduinoOTA.handle();
  // BUTTON 1 — Increase target
  if (!digitalRead(Button_1))
  {
    targetAngle += 1;
    delay(150);
  }

  // BUTTON 2 — Decrease target
  if (!digitalRead(Button_2))
  {
    targetAngle -= 1;
    delay(150);
  }

  // BUTTON 4 — manual homing
  if (digitalRead(Button_4) == LOW)
  {
    homing();
    delay(300);
  }

  // update angle
  currentAngle = (float)encoderCount / ENCODER_PPR * 360.0 / GEAR_RATIO;

  // PID always active
  computePID();

  updateOLED();

  delay(5);
}
