/*
@date: 071125
@Version: v1.0
@feature: 
    Display time using RTC DS1307 and sync time using NTP get time from internet. set timer for 4 Relay automatics on and off.
    To connect to wifi press and hold boot button in 5s esp create wifi with name "ESP_TIMER" and password is "12345678".
    use smartphone connect to this wifi and it phone is automatics popup website or not user can access to 192.168.4.1 by any web browser.
    on this website user can choose any wifi esp32 can reached (2.4Ghz WiFi only). after enter main WiFi and password the timer is going to
    main screen show time. to edit timer short-press boot button to jump into editing timer menu. using "+" and "-" to increase/ decrease H/M/S
    and use "next" to switch beetwen H/M/S short-press boot button one more time to switched beetwen timer1, timer2, timer3, timer4.
    finally user press "exit" to back to main screen.
    to config Wifi again or remote Wifi data press and hold boot button in 5s to enter website and chosse "info", at bottom of website have 
    button called "REMOVED WIFI INFOR".
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <EEPROM.h>
#include <RTClib.h>
#include <WiFiManager.h>
#include <LiquidCrystal_I2C.h>

// --- Khai báo IO ---
const int relaypin[4] = {12, 14, 27, 26};
const int buttonpin[5] = {33, 32, 35, 34, 0}; // + / next / - / exit / BOOT
const int buzzer = 4;

// --- Thời gian 4 bộ timer ---
int ontime[4][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
int offtime[4][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}};

// --- Timer chính ---
unsigned long previousMillis = 0;
unsigned long Maininterval = 1000;
unsigned long menupreviousMillis = 0;
unsigned long menuinterval = 30000;

// --- NTP ---
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

// --- WiFi configuration ---
const char *ssid = "ESP_TIMER";
const char *password = "12345678";

// --- Menu chỉnh sửa ---
enum MenuState
{
  MENU_NONE,
  MENU_TIMER_EDIT,
  MENU_WIFI_CONFIG
};
MenuState menuState = MENU_NONE;
int editState = 0;
int currentTimer = 0;

// --- LCD, RTC, WiFi ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;
WiFiManager wm;

// Biến thời gian và chống dội nút nhấn
unsigned long lastButtonUpTime = 0;
unsigned long lastButtonDownTime = 0;
unsigned long lastButtonNextTime = 0;
unsigned long lastButtonExit = 0;
const unsigned long buttonDelay = 200;

// --- Nguyên mẫu ---
void checkBootButton();
void editTimer();
void loadeeprom();
void saveeeprom();
void checktime();
void relay();
void display();

void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi...");

  bool res = wm.autoConnect(ssid, password);
  if (!res)
  {
    lcd.clear();
    String WIFIstr = "WiFi Failed!";
    int colTime = (16 - WIFIstr.length()) / 2;
    lcd.setCursor(colTime, 0);
    lcd.print(WIFIstr);

    String ESPstr = "ESP Restarting...";
    int colESPstr = (16 - ESPstr.length()) / 2;
    lcd.setCursor(colESPstr, 1);
    lcd.print(ESPstr);
    delay(1000);
    ESP.restart();
  }
  else
  {
    lcd.clear();
    String WIFIstr = "WiFi OK";
    int colTime = (16 - WIFIstr.length()) / 2;
    lcd.setCursor(colTime, 0);
    lcd.print(WIFIstr);

    char IPstr[9];
    sprintf(IPstr, "%s", WiFi.localIP());
    int colClock = (16 - strlen(IPstr)) / 2;
    lcd.setCursor(colClock, 1);
    lcd.print(IPstr);
    delay(1500);
  }

  // --- NTP ---
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // --- RTC ---
  if (!rtc.begin())
  {
    Serial.println("Không tìm thấy DS1307!");
    while (1)
      delay(10);
  }
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    lcd.clear();
    lcd.print("Time synced!");
    Serial.println("Đã đồng bộ NTP!");
  }
  else
  {
    lcd.clear();
    lcd.print("Sync NTP fail!");
    Serial.println("Không thể lấy thời gian NTP!");
  }

  // --- EEPROM ---
  EEPROM.begin(128);
  loadeeprom();

  // --- Relay và nút ---
  for (int i = 0; i < 4; i++)
  {
    pinMode(relaypin[i], OUTPUT);
    digitalWrite(relaypin[i], LOW);
  }
  for (int i = 0; i < 5; i++)
  {
    pinMode(buttonpin[i], INPUT_PULLUP);
  }
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
}

void loop()
{
  unsigned long now = millis();

  if (now - previousMillis >= Maininterval)
  {
    previousMillis = now;
    checktime();
    relay();
    display();
  }
  if (millis() - menupreviousMillis >= menuinterval)
  {
    menuState = MENU_NONE;
  }

  checkBootButton();
  editTimer();

  if (menuState == MENU_WIFI_CONFIG)
  {
    lcd.clear();
    lcd.print("WiFi Config Mode");
    delay(500);
    wm.startConfigPortal(ssid, password);
    ESP.restart();
  }
}

// ==============================
//        HÀM PHỤ
// ==============================

void checkBootButton()
{
  bool state = (digitalRead(buttonpin[4]) == LOW);
  static bool bootPressed = false;
  static unsigned long bootPressTime = 0;

  if (state && !bootPressed)
  {
    bootPressed = true;
    bootPressTime = millis();
    menupreviousMillis = millis();
  }

  if (!state && bootPressed)
  {
    bootPressed = false;
    unsigned long duration = millis() - bootPressTime;
    // giữ 5s → WiFi config
    if (duration >= 5000)
    {
      menuState = MENU_WIFI_CONFIG;
    }
    else
    {
      // nhấn ngắn → bật chế độ chỉnh timer hoặc chuyển timer
      if (menuState != MENU_TIMER_EDIT)
      {
        menuState = MENU_TIMER_EDIT;
        menupreviousMillis = millis();
        currentTimer = 0;
        editState = 0;
      }
      else
      {
        currentTimer = (currentTimer + 1) % 4;
        editState = 0;
      }
    }
  }
}

void editTimer()
{
  if (menuState != MENU_TIMER_EDIT)
    return;

  int *targetArray = (editState <= 2) ? ontime[currentTimer] : offtime[currentTimer];
  int pos = editState % 3;
  unsigned long now = millis();

  // Tăng
  if (digitalRead(buttonpin[0]) == LOW && now - lastButtonUpTime > buttonDelay)
  {
    targetArray[pos] = (targetArray[pos] + 1) % ((pos == 0) ? 24 : 60);
    lastButtonUpTime = now;
    menupreviousMillis = millis();
  }

  // Giảm
  if (digitalRead(buttonpin[2]) == LOW && now - lastButtonDownTime > buttonDelay)
  {
    targetArray[pos] = (targetArray[pos] - 1 + ((pos == 0) ? 24 : 60)) % ((pos == 0) ? 24 : 60);
    lastButtonDownTime = now;
    menupreviousMillis = millis();
  }

  // Nhảy đơn vị tiếp theo
  if (digitalRead(buttonpin[1]) == LOW && now - lastButtonNextTime > buttonDelay)
  {
    editState++;
    if (editState == 3)
      editState = 3;
    if (editState > 5)
      editState = 0;
    lastButtonNextTime = now;
    saveeeprom(); // lưu vào eeprom
    menupreviousMillis = millis();
  }
  // thoát menu
  if (digitalRead(buttonpin[3]) == LOW && now - lastButtonExit > buttonDelay)
  {
    // nếu nhấn nút back thì quay về màn hình chính
    menuState = MENU_NONE;
  }
}

void loadeeprom()
{
  int addr = 0;
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      ontime[i][j] = EEPROM.read(addr++);
      offtime[i][j] = EEPROM.read(addr++);
      // Đảm bảo thời gian hợp lệ
      if (ontime[i][0] > 23)
        ontime[i][0] = 23;
      if (ontime[i][1] > 59)
        ontime[i][1] = 59;
      if (ontime[i][2] > 59)
        ontime[i][2] = 59;
      if (offtime[i][0] > 23)
        offtime[i][0] = 23;
      if (offtime[i][1] > 59)
        offtime[i][1] = 59;
      if (offtime[i][2] > 59)
        offtime[i][2] = 59;
    }
  }
}

void saveeeprom()
{
  int addr = 0;
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      EEPROM.write(addr++, ontime[i][j]);
      EEPROM.write(addr++, offtime[i][j]);
    }
  }
  EEPROM.commit();
}

void checktime()
{
  DateTime now = rtc.now();
  int curH = now.hour();
  int curM = now.minute();
  int curS = now.second();

  // Kiểm tra từng relay
  for (int i = 0; i < 4; i++)
  {
    bool onFlag = false;

    // So sánh thời gian hiện tại với ontime[i] và offtime[i]
    int tonH = ontime[i][0], tonM = ontime[i][1], tonS = ontime[i][2];
    int toffH = offtime[i][0], toffM = offtime[i][1], toffS = offtime[i][2];

    // Nếu thời gian on < off (trong cùng ngày)
    if ((curH > tonH || (curH == tonH && curM > tonM) || (curH == tonH && curM == tonM && curS >= tonS)) &&
        (curH < toffH || (curH == toffH && curM < toffM) || (curH == toffH && curM == toffM && curS < toffS)))
    {
      onFlag = true;
    }

    // Nếu thời gian on > off (qua đêm)
    if ((tonH > toffH) || (tonH == toffH && tonM > toffM) || (tonH == toffH && tonM == toffM && tonS > toffS))
    {
      if ((curH > tonH || (curH == tonH && curM > tonM) || (curH == tonH && curM == tonM && curS >= tonS)) ||
          (curH < toffH || (curH == toffH && curM < toffM) || (curH == toffH && curM == toffM && curS < toffS)))
      {
        onFlag = true;
      }
    }

    // Lưu trạng thái bật relay
    digitalWrite(relaypin[i], onFlag ? HIGH : LOW);
  }
}

void relay()
{
  // Nếu muốn tách riêng, có thể dùng relay() để hiển thị trạng thái
  for (int i = 0; i < 4; i++)
  {
    bool state = digitalRead(relaypin[i]);
    Serial.printf("Relay%d: %s\n", i + 1, state ? "ON" : "OFF");
  }
}

void display()
{
  DateTime now = rtc.now();

  lcd.clear();

  if (menuState == MENU_NONE)
  {
    // --- String căn giữa cho Chữ Time ở dòng 1
    String timeStr = "TIME";
    int colTime = (16 - timeStr.length()) / 2;
    lcd.setCursor(colTime, 0);
    lcd.print(timeStr);

    // --- String căn giữa cho giá trị thời gian ở dòng 2
    char clockStr[9];
    sprintf(clockStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    int colClock = (16 - strlen(clockStr)) / 2;
    lcd.setCursor(colClock, 1);
    lcd.print(clockStr);
  }
  else if (menuState == MENU_TIMER_EDIT)
  {
    // --- Hiển thị Ton ---
    lcd.setCursor(0, 0);
    lcd.printf("Timer%d %02d:%02d:%02d", currentTimer + 1,
               ontime[currentTimer][0], ontime[currentTimer][1], ontime[currentTimer][2]);
    // --- Hiển thị Toff ---
    lcd.setCursor(0, 1);
    lcd.printf("Timer%d %02d:%02d:%02d", currentTimer + 1,
               offtime[currentTimer][0], offtime[currentTimer][1], offtime[currentTimer][2]);

    // --- Tính vị trí highlight ---
    int row = (editState <= 2) ? 0 : 1;
    int pos = editState % 3;
    int startCol = 7;
    if (row == 1)
      startCol = 7;

    int cursorCol = startCol + pos * 3;

    // --- Highlight bằng custom char block (invert màu) ---
    lcd.setCursor(cursorCol, row);
    lcd.blink();
  }
}
