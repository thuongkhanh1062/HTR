#include <DHT.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>
#include <Arduino.h>

// --- Định nghĩa chân IO ---
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
#define NTP_MAX_RETRY 20

#define DHT_PIN 5
#define DHT_TYPE DHT11
#define RELAY_ACTIVE_LOW false

// --- Thông số cấu hình ---
#define API_KEY "AIzaSyAgVge-VC6S9RocrJQibOWFqY1De7nz3eM"
#define DATABASE_URL "https://smartfarmingiot-78911-default-rtdb.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL "admin@gmail.com"
#define USER_PASS "admins"

// --- Đối tượng Firebase & Biến hệ thống ---
FirebaseData fbdo_stream;
FirebaseData fbdo_action;
FirebaseAuth auth;
FirebaseConfig config;

const int relay_pins[4] = {RL1_PIN, RL2_PIN, RL3_PIN, RL4_PIN};
uint8_t relay_status[4] = {0, 0, 0, 0}; // Trạng thái thực tế trên chân Pin

volatile unsigned long pulseCount = 0;
float currentFlowRate = 0;
unsigned long lastFlowMillis = 0;
unsigned long lastFirebaseUpload = 0;
unsigned long lastTimerFetch = 0;
unsigned long pressStartTime = 0;
bool isPressing = false;
bool smartConfigActive = false;

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

// =========================================================================
//                                FUNCTIONS
// =========================================================================

void IRAM_ATTR pulseCounter() { pulseCount++; }

void startBuzzerBeep()
{
  ledcWrite(0, 128);
  delay(100);
  ledcWrite(0, 0);
}

void syncTimeNTP()
{
  configTime(TIMEZONE_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < NTP_MAX_RETRY)
  {
    delay(500);
    retry++;
  }
  if (retry >= NTP_MAX_RETRY)
  {
    Serial.println("[TIME] NTP sync failed");
  }
  else
  {
    char buf[32];
    strftime(buf, sizeof(buf), "%F %T", &timeinfo);
    Serial.printf("[TIME] Synced: %s\n", buf);
  }
}

void setRelayState(uint8_t index, bool on, bool syncFirebase = false)
{
  if (index >= 4)
    return;
  uint8_t desired = on ? (RELAY_ACTIVE_LOW ? LOW : HIGH) : (RELAY_ACTIVE_LOW ? HIGH : LOW);

  if (digitalRead(relay_pins[index]) != desired)
  {
    digitalWrite(relay_pins[index], desired);
    relay_status[index] = desired;
    startBuzzerBeep();

    if (syncFirebase)
    {
      String path = "/smart_farm_iot/data/node3/relay" + String(index + 1) + "_state";
      Firebase.setBool(fbdo_action, path, on);
    }
  }
}

void fetchTimers()
{
  if (!Firebase.ready())
    return;
  if (Firebase.getJSON(fbdo_action, "/smart_farm_iot/config/timers"))
  {
    FirebaseJson &json = fbdo_action.jsonObject();
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
        timers[timerCount].running = false;
        timerCount++;
      }
    }
    json.iteratorEnd();
    Serial.printf("[TIMER] Loaded %d jobs\n", timerCount);
  }
}

void startSmartConfig()
{
  smartConfigActive = true;
  WiFi.disconnect(true, true);
  delay(1000);
  WiFi.beginSmartConfig();
  Serial.println("[WIFI] SmartConfig Started...");
  while (!WiFi.smartConfigDone())
  {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(300);
  }
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
  }
  smartConfigActive = false;
  digitalWrite(LED_PIN, HIGH);
  Serial.println("[WIFI] Connected via SmartConfig");
}

void streamCallback(StreamData data)
{
  String path = data.dataPath();
  if (path.indexOf("relay") != -1)
  {
    int idx = (path.indexOf("1") != -1) ? 0 : (path.indexOf("2") != -1) ? 1
                                          : (path.indexOf("3") != -1)   ? 2
                                          : (path.indexOf("4") != -1)   ? 3
                                                                        : -1;
    if (idx != -1)
    {
      bool val = data.boolData();
      bool isLocked = false;
      for (int i = 0; i < timerCount; i++)
        if (timers[i].running && timers[i].relayIndex == idx)
          isLocked = true;
      if (!isLocked)
        setRelayState(idx, val);
    }
  }
}

// =========================================================================
//                               MAIN SETUP
// =========================================================================

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

  // WiFi & NTP
  WiFi.begin();
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20)
  {
    delay(500);
    timeout++;
  }
  if (WiFi.status() == WL_CONNECTED)
    digitalWrite(LED_PIN, HIGH);

  syncTimeNTP();

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASS;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.ready())
  {
    Firebase.beginStream(fbdo_stream, "/smart_farm_iot/data/node3");
    Firebase.setStreamCallback(fbdo_stream, streamCallback, [](bool) {});
    fetchTimers();
  }
}

// =========================================================================
//                               MAIN LOOP
// =========================================================================

void loop()
{
  unsigned long now = millis();

  // 1. Kiểm tra nút BOOT (SmartConfig)
  if (digitalRead(BOOT_BUTTON_PIN) == LOW)
  {
    if (!isPressing)
    {
      pressStartTime = now;
      isPressing = true;
    }
    if (now - pressStartTime > 5000)
    {
      startSmartConfig();
      isPressing = false;
    }
  }
  else
    isPressing = false;

  // 2. Tính lưu lượng (mỗi 1s)
  if (now - lastFlowMillis > 1000)
  {
    detachInterrupt(FLOW_PIN);
    unsigned long pulses = pulseCount;
    pulseCount = 0;
    lastFlowMillis = now;
    attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);

    currentFlowRate = (pulses / 7.5f);
    Serial.printf("[FLOW] pulses=%lu, rate=%.2f L/min\n", pulses, currentFlowRate);
    Serial.print();
  }

  // 3. Xử lý Timer
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    int curMin = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    for (int i = 0; i < timerCount; i++)
    {
      TimerJob &t = timers[i];
      if (!t.enabled)
        continue;
      // Bắt đầu
      if (!t.running && t.lastRunDay != timeinfo.tm_yday && curMin == t.startMinutes)
      {
        t.running = true;
        t.lastRunDay = timeinfo.tm_yday;
        t.endMillis = now + (t.durationSec * 1000);
        setRelayState(t.relayIndex, true, true);
      }
      // Kết thúc
      if (t.running && now >= t.endMillis)
      {
        t.running = false;
        setRelayState(t.relayIndex, false, true);
      }
    }
  }

  // 4. Upload dữ liệu & Sync Timer
  if (now - lastFirebaseUpload > 30000)
  {
    if (Firebase.ready())
    {
      FirebaseJson update;
      update.set("flowrate", currentFlowRate);
      update.set("node3_mcu_temp", temperatureRead());
      update.set("node3_state", true);
      Firebase.updateNode(fbdo_action, "/smart_farm_iot/data/node3", update);
      fetchTimers();
    }
    lastFirebaseUpload = now;
  }
}