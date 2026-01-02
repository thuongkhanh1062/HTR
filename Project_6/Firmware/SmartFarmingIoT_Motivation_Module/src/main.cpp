#include <DHT.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>
#include <Arduino.h>

// --- Giữ nguyên các định nghĩa Pin và Config ---
#define BOOT_BUTTON_PIN 0
#define RL1_PIN 12
#define RL2_PIN 14
#define RL3_PIN 27
#define RL4_PIN 26
#define LED_PIN 25
#define BUZZER_PIN 4
#define FLOW_PIN 33

#define TIMEZONE_OFFSET_SEC (7 * 3600)
#define DST_OFFSET_SEC 0
#define RELAY_ACTIVE_LOW false

#define API_KEY "AIzaSyAgVge-VC6S9RocrJQibOWFqY1De7nz3eM"
#define DATABASE_URL "https://smartfarmingiot-78911-default-rtdb.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL "admin@gmail.com"
#define USER_PASS "admins"

#define SSID "FarmIot"
#define PASS "12345678"

FirebaseData fbdo_stream, fbdo_action, fbdo_cfg;
FirebaseAuth auth;
FirebaseConfig config;

const int relay_pins[4] = {RL1_PIN, RL2_PIN, RL3_PIN, RL4_PIN};
bool relay_status[4] = {false, false, false, false};

volatile unsigned long pulseCount = 0;
float currentFlowRate = 0;
unsigned long lastFlowMillis = 0;

float fbTemp = 0, fbLight = 0, fbSoil = 0;
float fbTempThresh = 0, fbLightThresh = 0, fbSoilThresh = 0;
bool fbTempEn = false, fbLightEn = false, fbSoilEn = false;
int fbTempDur = 0, fbSoilDur = 0;
int fbTempRelayIdx = 0, fbSoilRelayIdx = 1, fbLightRelayIdx = 2;

unsigned long tempEndMillis = 0;
bool tempIsRunning = false;
unsigned long soilEndMillis = 0;
bool soilIsRunning = false;

struct TimerJob
{
  String id;
  bool enabled = false;
  uint8_t relayIndex = 0;
  uint16_t startMinutes = 0;
  uint32_t durationSec = 0;
  int lastRunDay = -1;
  bool running = false;
  unsigned long endMillis = 0;
};
TimerJob timers[12];
uint8_t timerCount = 0;

unsigned long lastFirebaseUpload = 0;
unsigned long lastConfigFetch = 0;
unsigned long pressStartTime = 0;
bool isPressing = false;

void IRAM_ATTR pulseCounter() { pulseCount++; }

void startBuzzerBeep()
{
  ledcWrite(0, 128);
  delay(100);
  ledcWrite(0, 0);
}

void setRelayState(uint8_t index, bool on, bool syncFirebase = false)
{
  if (index >= 4)
    return;
  uint8_t level = on ? (RELAY_ACTIVE_LOW ? LOW : HIGH) : (RELAY_ACTIVE_LOW ? HIGH : LOW);
  if (digitalRead(relay_pins[index]) != level)
  {
    digitalWrite(relay_pins[index], level);
    relay_status[index] = on;
    startBuzzerBeep();
    if (syncFirebase && Firebase.ready())
    {
      String path = "/smart_farm_iot/data/node3/relay" + String(index + 1) + "_state";
      Firebase.setBool(fbdo_action, path, on);
    }
  }
}

// Hàm lấy cấu hình (Chỉ gọi khi cần thiết, không gọi liên tục)
void fetchControlConfig()
{
  if (!Firebase.ready())
    return;
  // Lấy dữ liệu cảm biến từ Node 1
  if (Firebase.getFloat(fbdo_cfg, "/smart_farm_iot/data/node1/node1_dht_temp"))
    fbTemp = fbdo_cfg.floatData();
  if (Firebase.getFloat(fbdo_cfg, "/smart_farm_iot/data/node1/node1_light"))
    fbLight = fbdo_cfg.floatData();
  if (Firebase.getFloat(fbdo_cfg, "/smart_farm_iot/data/node1/node1_soil"))
    fbSoil = fbdo_cfg.floatData();

  // Lấy logic điều khiển
  if (Firebase.getJSON(fbdo_cfg, "/smart_farm_iot/config/context_logic"))
  {
    FirebaseJson &json = fbdo_cfg.jsonObject();
    FirebaseJsonData data;
    json.get(data, "temp/enabled");
    fbTempEn = data.boolValue;
    json.get(data, "temp/threshold");
    fbTempThresh = data.floatValue;
    json.get(data, "temp/duration");
    fbTempDur = data.intValue;
    json.get(data, "temp/relay");
    fbTempRelayIdx = data.intValue;
    json.get(data, "soil/enabled");
    fbSoilEn = data.boolValue;
    json.get(data, "soil/threshold");
    fbSoilThresh = data.floatValue;
    json.get(data, "soil/duration");
    fbSoilDur = data.intValue;
    json.get(data, "soil/relay");
    fbSoilRelayIdx = data.intValue;
    json.get(data, "light/enabled");
    fbLightEn = data.boolValue;
    json.get(data, "light/threshold");
    fbLightThresh = data.floatValue;
    json.get(data, "light/relay");
    fbLightRelayIdx = data.intValue;
  }
}

void fetchTimers()
{
  if (!Firebase.ready())
    return;
  if (Firebase.getJSON(fbdo_cfg, "/smart_farm_iot/config/timers"))
  {
    FirebaseJson &json = fbdo_cfg.jsonObject();
    size_t len = json.iteratorBegin();
    String key, value;
    int type;
    timerCount = 0;
    for (size_t i = 0; i < len && timerCount < 12; i++)
    {
      json.iteratorGet(i, type, key, value);
      if (type == FirebaseJson::JSON_OBJECT)
      {
        FirebaseJson timerObj;
        timerObj.setJsonData(value);
        FirebaseJsonData data;
        timers[timerCount].id = key;
        timerObj.get(data, "enabled");
        timers[timerCount].enabled = data.boolValue;
        timerObj.get(data, "duration");
        timers[timerCount].durationSec = data.intValue;
        timerObj.get(data, "relayKey");
        String rk = data.stringValue;
        timers[timerCount].relayIndex = (rk == "RL1") ? 0 : (rk == "RL2") ? 1
                                                        : (rk == "RL3")   ? 2
                                                                          : 3;
        timerObj.get(data, "time");
        String ts = data.stringValue;
        int colon = ts.indexOf(':');
        if (colon > 0)
          timers[timerCount].startMinutes = ts.substring(0, colon).toInt() * 60 + ts.substring(colon + 1).toInt();
        timerCount++;
      }
    }
    json.iteratorEnd();
  }
}

// --- HÀM SMARTCONFIG WIFI ---
void startSmartConfig()
{
  Serial.println("\n[WIFI] Đang xóa cấu hình cũ...");
  WiFi.disconnect(true, true);
  delay(1000);

  WiFi.beginSmartConfig();
  Serial.println("[WIFI] Chờ SmartConfig từ điện thoại...");

  while (!WiFi.smartConfigDone())
  {
    delay(500);
    Serial.print("#");
    yield(); // Cho watchdog nghỉ
  }

  Serial.println("\n[WIFI] Đã nhận cấu hình!");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    yield();
  }
  Serial.println("\n[WIFI] Kết nối thành công!");
}

// --- HÀM THIẾT LẬP WIFI & LƯU THÔNG TIN ---
void setupWiFi()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);

  Serial.println("Kiểm tra wifi đã lưu Flash...");

  WiFi.begin();

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20)
  {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n[THÀNH CÔNG] Đã kết nối WiFi");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\n[KHÔNG TÌM THẤY] Bắt đầu chế độ SmartConfig...");
    WiFi.beginSmartConfig();

    while (!WiFi.smartConfigDone())
    {
      delay(500);
      Serial.print("#");
    }
    Serial.println("\n[OK] SmartConfig nhận được thông tin!");
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\n[Đã lưu] WiFi đã được thiết lập vào Flash.");
  }
}

void setup()
{
  
  Serial.begin(115200);
  for (int i = 0; i < 4; i++)
  {
    pinMode(relay_pins[i], OUTPUT);
    digitalWrite(relay_pins[i], RELAY_ACTIVE_LOW ? HIGH : LOW);
  }
  pinMode(LED_PIN, OUTPUT);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);
  ledcSetup(0, 2000, 8);
  ledcAttachPin(BUZZER_PIN, 0);

  Serial.println("[WIFI] Cấu hình WiFi...");
  setupWiFi();

  configTime(TIMEZONE_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASS;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.ready())
  {
    // Bắt đầu luồng Stream để lắng nghe lệnh điều khiển Relay
    Firebase.beginStream(fbdo_stream, "/smart_farm_iot/data/node3");
    fetchControlConfig();
    fetchTimers();
  }
  startBuzzerBeep();
}

void loop()
{
  unsigned long now = millis();

  // 1. Xử lý Stream (Nhận lệnh relay tức thời - KHÔNG NGHẼN)
  if (Firebase.ready() && Firebase.readStream(fbdo_stream))
  {
    if (fbdo_stream.streamTimeout())
      Serial.println("Stream timeout, resuming...");

    // Kiểm tra nếu dữ liệu trả về là trạng thái các relay
    String path = fbdo_stream.dataPath();
    if (path.indexOf("relay1_state") >= 0)
      setRelayState(0, fbdo_stream.boolData());
    else if (path.indexOf("relay2_state") >= 0)
      setRelayState(1, fbdo_stream.boolData());
    else if (path.indexOf("relay3_state") >= 0)
      setRelayState(2, fbdo_stream.boolData());
    else if (path.indexOf("relay4_state") >= 0)
      setRelayState(3, fbdo_stream.boolData());
  }

  // 2. Cập nhật cấu hình định kỳ (15 giây một lần để tránh nghẽn SSL)
  if (now - lastConfigFetch > 15000)
  {
    fetchControlConfig();
    lastConfigFetch = now;
  }

  // 3. Sensor Flow (1 giây)
  if (now - lastFlowMillis > 1000)
  {
    currentFlowRate = (pulseCount / 7.5f);
    pulseCount = 0;
    lastFlowMillis = now;
  }

  // 4. Logic Temp/Soil/Light (Giữ nguyên logic của bạn)
  if (fbTempEn)
  {
    if (fbTemp > fbTempThresh && !tempIsRunning)
    {
      tempIsRunning = true;
      tempEndMillis = now + (fbTempDur * 1000);
      setRelayState(fbTempRelayIdx, true, true);
    }
    if (tempIsRunning && now >= tempEndMillis)
    {
      tempIsRunning = false;
      setRelayState(fbTempRelayIdx, false, true);
    }
  }
  if (fbSoilEn)
  {
    if (fbSoil < fbSoilThresh && !soilIsRunning)
    {
      soilIsRunning = true;
      soilEndMillis = now + (fbSoilDur * 1000);
      setRelayState(fbSoilRelayIdx, true, true);
    }
    if (soilIsRunning && now >= soilEndMillis)
    {
      soilIsRunning = false;
      setRelayState(fbSoilRelayIdx, false, true);
    }
  }
  if (fbLightEn)
  {
    bool shouldLightOn = (fbLight > fbLightThresh);
    if (relay_status[fbLightRelayIdx] != shouldLightOn)
      setRelayState(fbLightRelayIdx, shouldLightOn, true);
  }

  // 5. Timer (Giữ nguyên logic)
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    int curMin = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    for (int i = 0; i < timerCount; i++)
    {
      TimerJob &t = timers[i];
      if (!t.enabled)
        continue;
      if (!t.running && t.lastRunDay != timeinfo.tm_yday && curMin == t.startMinutes)
      {
        t.running = true;
        t.lastRunDay = timeinfo.tm_yday;
        t.endMillis = now + (t.durationSec * 1000);
        setRelayState(t.relayIndex, true, true);
      }
      if (t.running && now >= t.endMillis)
      {
        t.running = false;
        setRelayState(t.relayIndex, false, true);
      }
    }
  }

  // 6. Upload dữ liệu (30 giây)
  if (now - lastFirebaseUpload > 30000)
  {
    if (Firebase.ready())
    {
      FirebaseJson update;
      update.set("flowrate", currentFlowRate);
      update.set("node3_mcu_temp", temperatureRead());
      update.set("node3_state", true);
      Firebase.updateNode(fbdo_action, "/smart_farm_iot/data/node3", update);
      fetchTimers(); // Cập nhật timer mỗi 30s
    }
    lastFirebaseUpload = now;
  }

  // 7. Nút BOOT (Giữ nguyên)

  // Xác định trạng thái WiFi
  bool isSmartConfig = false;
  static bool smartConfigStarted = false;
  static bool wasConnected = true;
  static unsigned long smartConfigStartTime = 0;
  if (digitalRead(BOOT_BUTTON_PIN) == LOW)
  {
    if (!isPressing)
    {
      pressStartTime = now;
      isPressing = true;
    }
    if (now - pressStartTime > 5000)
    {
      Serial.println("[BOOT] Khởi động SmartConfig!");
      startSmartConfig();
      isPressing = false;
      smartConfigStarted = true;
      smartConfigStartTime = millis();
    }
  }
  else
  {
    isPressing = false;
  }
}