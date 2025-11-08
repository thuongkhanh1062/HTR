/*
  =========================================================================================================
  @date       : 07/11/2025
  @version    : v1.5
  @author     : [Your Name or Team]
  @hardware   : ESP32 + RTC DS1307 + LCD I2C 16x2 + 4 Relay + 5 Buttons
  ---------------------------------------------------------------------------------------------------------
  @description:
    This program controls 4 relays automatically based on user-configurable ON/OFF schedules stored in EEPROM.
    The system displays real-time clock data on an LCD using a DS1307 RTC module and supports both manual time
    configuration and Wi-Fi-based NTP synchronization (future feature placeholder).

  ---------------------------------------------------------------------------------------------------------
  @FEATURES:
    1️⃣ Real-time display:
        - Display current time (HH:MM:SS) on LCD.
        - Blinking colons (":") for clock animation.

    2️⃣ Relay automation:
        - 4 independent relay timers.
        - Each timer has configurable ON and OFF times.
        - Automatically turns relay ON/OFF based on RTC time.
        - Supports overnight schedule (e.g., ON 22:00 → OFF 06:00).

    3️⃣ Menu navigation via buttons:
        - 5 buttons: [+], [Next], [-], [Exit], [BOOT].
        - Short press BOOT → Enter Timer Edit mode.
        - Long press BOOT (5s) → Enter RTC Time Config mode.
        - [+]/[-] → Increase/Decrease time values.
        - [Next] → Move between Hours, Minutes, Seconds.
        - [Exit] → Return to main screen.

    4️⃣ EEPROM storage:
        - All ON/OFF times for 4 timers saved permanently.
        - Data auto-saved after each edit.

    5️⃣ Menu timeout:
        - If no button pressed within `menuinterval` (30 seconds),
          the menu automatically exits to main screen.

  ---------------------------------------------------------------------------------------------------------
  @BUTTON FUNCTIONS SUMMARY:
    - [BOOT short press]   → Enter Timer Edit mode / Switch between Timer 1–4.
    - [BOOT long press 5s] → Enter Time Configuration mode.
    - [+]                  → Increase value.
    - [-]                  → Decrease value.
    - [Next]               → Move to next editable field.
    - [Exit]               → Exit current menu to main display.

  ---------------------------------------------------------------------------------------------------------
  @DISPLAY MODES:
    🕒 MAIN SCREEN:
        TIME
        HH:MM:SS
    ⚙️ TIMER EDIT SCREEN:
        Tn ON  HH:MM:SS
        Tn OFF HH:MM:SS
        - Blinking field indicates currently edited value.

    ⏰ TIME CONFIG SCREEN:
        SET RTC TIME
        HH:MM:SS

  ---------------------------------------------------------------------------------------------------------
  @EEPROM STRUCTURE (bytes per timer = 6 bytes):
      For each timer i (0–3):
        [0]  ON Hour
        [1]  OFF Hour
        [2]  ON Minute
        [3]  OFF Minute
        [4]  ON Second
        [5]  OFF Second

      → Total: 4 timers × 6 bytes = 24 bytes

  ---------------------------------------------------------------------------------------------------------
  @NOTES:
    - System uses millis() for non-blocking timing and button debounce.
    - All time logic based on RTC DS1307.
  =========================================================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <time.h>
#include <EEPROM.h>
#include <RTClib.h>
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
unsigned long buttonholdinterval = 5000;

// --- Menu chỉnh sửa ---
enum MenuState
{
  MENU_NONE,
  MENU_TIMER_EDIT,
  MENU_TIME_CONFIG
};
MenuState menuState = MENU_NONE;
int editState = 0;
int currentTimer = 0;

// --- LCD, RTC, WiFi ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

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
void time_config();
void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  // --- RTC ---
  if (!rtc.begin())
  {
    Serial.println("Không tìm thấy DS1307!");
    while (1)
      delay(10);
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
  // pinMode(buzzer, OUTPUT);
  // digitalWrite(buzzer, LOW);
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

  if (menuState == MENU_TIME_CONFIG)
  {
    time_config();
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
  }

  if (!state && bootPressed)
  {
    bootPressed = false;
    unsigned long duration = millis() - bootPressTime;
    // giữ 5s → Time config
    if (duration >= buttonholdinterval)
    {
      menuState = MENU_TIME_CONFIG;
    }
    else
    {
      // nhấn ngắn → bật chế độ chỉnh timer hoặc chuyển timer
      if (menuState != MENU_TIMER_EDIT)
      {
        menuState = MENU_TIMER_EDIT;
        currentTimer = 0;
        editState = 0;
      }
      else
      {
        currentTimer = (currentTimer + 1) % 4;
        editState = 0;
      }
    }
    menupreviousMillis = millis();
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
    saveeeprom();
    menupreviousMillis = millis();
  }
  // thoát menu
  if (digitalRead(buttonpin[3]) == LOW && now - lastButtonExit > buttonDelay)
  {
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
  for (int i = 0; i < 4; i++)
  {
    bool state = digitalRead(relaypin[i]);
    Serial.printf("Relay%d: %s\n", i + 1, state ? "ON" : "OFF");
  }
}

void display()
{
  static unsigned long lastBlink = 0;
  static bool blinkState = false;
  const unsigned long blinkInterval = 400;

  unsigned long nowMillis = millis();
  if (nowMillis - lastBlink > blinkInterval)
  {
    lastBlink = nowMillis;
    blinkState = !blinkState;
  }

  static int lastH = -1, lastM = -1, lastS = -1;
  static MenuState lastMenu = MENU_NONE;

  DateTime now = rtc.now();

  if (menuState != lastMenu)
  {
    lcd.clear();
    lastMenu = menuState;
  }

  if (menuState == MENU_NONE)
  {
    if (now.hour() != lastH || now.minute() != lastM || now.second() != lastS)
    {
      lastH = now.hour();
      lastM = now.minute();
      lastS = now.second();

      // --- Dòng 1: chữ TIME ---
      String timeStr = "TIME";
      int colTime = (16 - timeStr.length()) / 2;
      lcd.setCursor(colTime, 0);
      lcd.print(timeStr);

      // --- Dòng 2: giá trị giờ ---
      char clockStr[9];
      sprintf(clockStr, "%02d%s%02d%s%02d", now.hour(), blinkState ? ":" : " ", now.minute(), blinkState ? ":" : " ", now.second());
      int colClock = (16 - strlen(clockStr)) / 2;
      lcd.setCursor(colClock, 1);
      lcd.print(clockStr);
    }
  }

  else if (menuState == MENU_TIMER_EDIT)
  {
    // Khi vào menu lần đầu mới clear
    if (menuState != lastMenu)
      lcd.clear();

    // --- In tĩnh ---
    lcd.setCursor(0, 0);
    lcd.printf("T%d ON  %02d:%02d:%02d", currentTimer + 1,
               ontime[currentTimer][0], ontime[currentTimer][1], ontime[currentTimer][2]);

    lcd.setCursor(0, 1);
    lcd.printf("T%d OFF %02d:%02d:%02d", currentTimer + 1,
               offtime[currentTimer][0], offtime[currentTimer][1], offtime[currentTimer][2]);

    // --- Nhấp nháy vị trí đang chỉnh ---
    int row = (editState <= 2) ? 0 : 1;
    int pos = editState % 3;
    int colStart = 7 + pos * 3;

    if (blinkState)
    {
      lcd.setCursor(colStart, row);
      lcd.print("  ");
    }
    else
    {
      int value = (row == 0) ? ontime[currentTimer][pos] : offtime[currentTimer][pos];
      lcd.setCursor(colStart, row);
      lcd.printf("%02d", value);
    }
  }
}

void time_config()
{
  static int editPos = 0;
  static unsigned long lastButtonTime = 0;
  static unsigned long lastRefresh = 0;
  const unsigned long debounce = 200;
  const unsigned long refreshInterval = 900;
  static bool blinkState = false;

  static bool initialized = false;
  static int setH, setM, setS;

  unsigned long currentMillis = millis();

  if (!initialized)
  {
    DateTime now = rtc.now();
    setH = now.hour();
    setM = now.minute();
    setS = now.second();

    lcd.clear();
    // --- Dòng 1: chữ TIME ---
    String labeltimeStr = "SET RTC TIME";
    int colTime = (16 - labeltimeStr.length()) / 2;
    lcd.setCursor(colTime, 0);
    lcd.print(labeltimeStr);
    lcd.noBlink();
    initialized = true;
  }

  // --- Nút tăng ---
  if (digitalRead(buttonpin[0]) == LOW && currentMillis - lastButtonTime > debounce)
  {
    if (editPos == 0)
      setH = (setH + 1) % 24;
    else if (editPos == 1)
      setM = (setM + 1) % 60;
    else
      setS = (setS + 1) % 60;
    lastButtonTime = currentMillis;
    menupreviousMillis = millis();
  }

  // --- Nút giảm ---
  if (digitalRead(buttonpin[2]) == LOW && currentMillis - lastButtonTime > debounce)
  {
    if (editPos == 0)
      setH = (setH - 1 + 24) % 24;
    else if (editPos == 1)
      setM = (setM - 1 + 60) % 60;
    else
      setS = (setS - 1 + 60) % 60;
    lastButtonTime = currentMillis;
    menupreviousMillis = millis();
  }

  // --- Nút next ---
  if (digitalRead(buttonpin[1]) == LOW && currentMillis - lastButtonTime > debounce)
  {
    editPos = (editPos + 1) % 3;
    lastButtonTime = currentMillis;
    menupreviousMillis = millis();
  }

  // --- Nút exit ---
  if (digitalRead(buttonpin[3]) == LOW && currentMillis - lastButtonTime > debounce)
  {
    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), setH, setM, setS));

    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Time Updated!");
    lcd.noBlink();
    delay(800);

    menuState = MENU_NONE;
    initialized = false;
    lcd.clear();
    return;
  }

  // --- Cập nhật hiển thị theo chu kỳ ---
  if (currentMillis - lastRefresh > refreshInterval)
  {
    lastRefresh = currentMillis;
    blinkState = !blinkState;

    char buf[17];
    sprintf(buf, "%02d:%02d:%02d", setH, setM, setS);
    lcd.setCursor(3, 1);

    // Tạo nhấp nháy cho phần đang edit
    String timeStr = buf;
    if (blinkState)
    {
      int start = editPos * 3;
      timeStr.setCharAt(start, ' ');
      timeStr.setCharAt(start + 1, ' ');
    }

    lcd.print(timeStr);
  }
}
