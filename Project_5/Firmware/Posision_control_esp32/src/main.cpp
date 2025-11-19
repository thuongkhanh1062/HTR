/*
@Author: CHITOAN (modified)
@date: 171125 -> updated
@version: V5.0

Features added in V5.0:
 - MODE_ANGLE: Control output shaft position in 360 degrees (0-360°)
 - BT1/BT2 increase/decrease angle by 5° increments
 - Angle display on OLED
 - Automatic conversion between encoder counts and degrees
*/


#include <Arduino.h>
#include <WiFi.h>
#include <U8g2lib.h>

// ---------------- IO ----------------
#define ENC_A 19
#define ENC_B 18
#define IN1_PIN 15
#define IN2_PIN 5
#define PWM_PIN 13

#define BUTTON_UP 25
#define BUTTON_DOWN 33
#define BUTTON_NEXT 32

#define HALL_S 39

// ---------------- ENCODER ----------------
const float PPR = 990.0; // CHỈNH PPR THẬT (sau khi test)
const float DEG_PER_PULSE = 360.0 / PPR;

volatile long encoderCount = 0;
volatile int lastA = 0;

// ---------------- PID ----------------
float Kp = 1.2;
float Ki = 0.03;
float Kd = 0.05;

float pid_i = 0;
float last_error = 0;
int pidIndex = 0;

// ---------------- OLED ----------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// ---------------- STATE ----------------
float targetAngle = 0;
float currentAngle = 0;

// ---------------- FILTER ----------------
float angleFiltered = 0;
float errorFiltered = 0;

// ========================= WRAP (-180 → +180) =========================
float wrapAngle(float x)
{
  while (x > 180)
    x -= 360;
  while (x < -180)
    x += 360;
  return x;
}

// ========================= ENCODER ISR (CHUẨN NHẤT) =========================
void IRAM_ATTR encoderISR()
{
  int A = digitalRead(ENC_A);
  int B = digitalRead(ENC_B);

  if (A != lastA)
  {
    if (A == B)
      encoderCount--;
    else
      encoderCount++;
    lastA = A;
  }
}

// ========================= MOTOR CONTROL =========================
void motorDrive(float pwm)
{
  pwm = constrain(pwm, -255, 255);

  if (pwm > 0)
  {
    digitalWrite(IN1_PIN, HIGH);
    digitalWrite(IN2_PIN, LOW);
    ledcWrite(0, pwm);
  }
  else
  {
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, HIGH);
    ledcWrite(0, -pwm);
  }
}

// ========================= HOMING CHUẨN XÁC =========================
void homing()
{
  Serial.println("Homing start...");

  // 1) Quay về trái cho đến khi chạm cảm biến
  motorDrive(50);
  while (analogRead(HALL_S) < 3600)
  {
    delay(2);
  }

  // 2) Dừng
  motorDrive(0);
  delay(150);

  // 3) Reset encoder
  encoderCount = 0;
  angleFiltered = 0;

  Serial.println("Homing OK");
}

// ========================= SETUP =========================
void setup()
{
  Serial.begin(115200);

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(PWM_PIN, OUTPUT);

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_NEXT, INPUT_PULLUP);

  pinMode(HALL_S, INPUT);

  ledcAttachPin(PWM_PIN, 0);
  ledcSetup(0, 20000, 8);

  // Encoder — chỉ dùng 1 interrupt
  attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR, CHANGE);

  u8g2.begin();
  homing();
}

// ========================= LOOP =========================
void loop()
{

  // ------------------- UP/DOWN Điều khiển góc -------------------
  if (!digitalRead(BUTTON_UP))
  {
    targetAngle += 2;
    if (targetAngle > 180)
      targetAngle = 180;
    delay(120);
  }

  if (!digitalRead(BUTTON_DOWN))
  {
    targetAngle -= 2;
    if (targetAngle < -180)
      targetAngle = -180;
    delay(120);
  }

  // ------------------- PID Adjust Buttons -------------------
  if (!digitalRead(BUTTON_NEXT))
  {
    pidIndex = (pidIndex + 1) % 3;
    delay(200);
  }

  // ------------------- ENCODER to Angle -------------------
  float rawAngle = encoderCount * DEG_PER_PULSE;
  angleFiltered = angleFiltered * 0.85 + rawAngle * 0.15;

  currentAngle = wrapAngle(angleFiltered);

  // ------------------- PID -------------------
  float error = wrapAngle(targetAngle - currentAngle);

  errorFiltered = errorFiltered * 0.6 + error * 0.4;

  pid_i += errorFiltered;
  pid_i = constrain(pid_i, -300, 300);

  float d = errorFiltered - last_error;
  last_error = errorFiltered;

  float output = Kp * errorFiltered + Ki * pid_i + Kd * d;

  motorDrive(output);

  // ------------------- OLED -------------------
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);

  u8g2.setCursor(0, 12);
  u8g2.printf("Target: %.1f", targetAngle);
  u8g2.setCursor(0, 26);
  u8g2.printf("Pos   : %.1f | %.0f", currentAngle, analogRead(HALL_S));
  u8g2.setCursor(0, 40);
  u8g2.printf("Err   : %.1f", errorFiltered);

  u8g2.setCursor(0, 58);
  u8g2.printf("p:%.2f i:%.3f d:%.2f", Kp, Ki, Kd);
  Serial.println(analogRead(HALL_S));
  u8g2.sendBuffer();
}