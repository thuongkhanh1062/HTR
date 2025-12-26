#include <Arduino.h>
#include <WiFi.h>
#include <DHT.h>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// --- CẤU HÌNH PIN ---
#define BOOT_BUTTON_PIN 0
#define RAIN_PIN 25
#define SOIL_PIN 32
#define LIGHT_PIN 33
#define DHT_PIN 14
#define DHT_TYPE DHT11

bool isPressing = false;
unsigned long pressStartTime = 0;
unsigned long lastFirebaseUpload = 0;
unsigned long lastSensorupdate = 0;

String deviceMac = "";
String deviceChipID = "";

// --- CẤU HÌNH FIREBASE ---
#define API_KEY "AIzaSyAgVge-VC6S9RocrJQibOWFqY1De7nz3eM"
#define DATABASE_URL "https://smartfarmingiot-78911-default-rtdb.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL "admin@gmail.com"
#define USER_PASS "admins"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
DHT dht(DHT_PIN, DHT_TYPE);

// Cấu trúc dữ liệu
struct SensorData
{
  float humid;
  float temp;
  float light;
  float mcu_temp;
  bool rain;
  float soil;
} liveData;

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
// --- HÀM LẤY THÔNG TIN THIẾT BỊ ---
void getDeviceInfo()
{
  deviceMac = WiFi.macAddress();
  uint64_t chipid = ESP.getEfuseMac();
  deviceChipID = String((uint32_t)(chipid >> 32), HEX);
  deviceChipID += String((uint32_t)chipid, HEX);
  deviceChipID.toUpperCase();

  Serial.println("--- THÔNG TIN THIẾT BỊ ---");
  Serial.print("MAC Address: ");
  Serial.println(deviceMac);
  Serial.print("Chip ID: ");
  Serial.println(deviceChipID);
  Serial.println("--------------------------");
}
void setup()
{
  Serial.begin(115200);
  dht.begin();

  pinMode(RAIN_PIN, INPUT);
  pinMode(LIGHT_PIN, INPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  Serial.println("[WIFI] Cấu hình WiFi...");
  setupWiFi();

  Serial.println("[FIREBASE] Cấu hình Firebase...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = "admin@gmail.com";
  auth.user.password = "admins";
  Serial.println("[FIREBASE] Đang kết nối...");
  Firebase.begin(&config, &auth);

  Serial.println("========================================");
  Serial.println("[SYSTEM] ✓ Khởi động hoàn tất!");
  Serial.printf("[SYSTEM] Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("========================================\n");
}

void loop()
{ // --- KIỂM TRA NHẤN GIỮ NÚT BOOT 5 GIÂY ---

  unsigned long now = millis();

  // 2. XỬ LÝ NÚT BẤM (SmartConfig)
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
    }
  }
  else
  {
    isPressing = false;
  }
  if (millis() - lastSensorupdate > 1000)
  {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t))
    {
      liveData.humid = h;
      liveData.temp = t;
    }

    liveData.light = map(analogRead(LIGHT_PIN), 0, 4095, 100, 0);
    liveData.soil = map(analogRead(SOIL_PIN), 0, 4095, 100, 0);
    liveData.rain = (digitalRead(RAIN_PIN) == LOW);
    liveData.mcu_temp = temperatureRead();
    Serial.printf("DHT Temp: %f C, Humid: %f %% MCU_Temp: %f light: %f soil: %f rain: %s\n", liveData.temp, liveData.humid, liveData.mcu_temp, liveData.light, liveData.soil, liveData.rain ? "YES" : "NO");
    lastSensorupdate = millis();
  }

  if (millis() - lastFirebaseUpload > 5000)
  {
    lastFirebaseUpload = millis();
    bool success = Firebase.setFloat(fbdo, "/smart_farm_iot/data/node1/node1_dht_humid", liveData.humid);
    if (success)
    {
      Serial.println("[FIREBASE]Gửi Firebase THÀNH CÔNG");
    }
    else
    {
      Serial.print("[FIREBASE]Gửi THẤT BẠI. Lý do: ");
      Serial.println(fbdo.errorReason());
    }
    Firebase.setFloat(fbdo, F("/smart_farm_iot/data/node1/node1_dht_humid"), liveData.humid);
    Firebase.setFloat(fbdo, F("/smart_farm_iot/data/node1/node1_dht_temp"), liveData.temp);
    Firebase.setFloat(fbdo, F("/smart_farm_iot/data/node1/node1_light"), liveData.light);
    Firebase.setFloat(fbdo, F("/smart_farm_iot/data/node1/node1_mcu_temp"), liveData.mcu_temp);
    Firebase.setBool(fbdo, F("/smart_farm_iot/data/node1/node1_rain"), liveData.rain);
    Firebase.setFloat(fbdo, F("/smart_farm_iot/data/node1/node1_soil"), liveData.soil);
    Firebase.setBool(fbdo, F("/smart_farm_iot/data/node1/node1_state"), true);
    Firebase.setString(fbdo, F("/smart_farm_iot/data/node1/device_mac"), deviceMac);
    Firebase.setString(fbdo, F("/smart_farm_iot/data/node1/device_chipid"), deviceChipID);
    Firebase.setString(fbdo, F("/smart_farm_iot/data/node1/node1_ssid"), WiFi.SSID());

    Serial.printf("node1_dht_temp: %f\n", Firebase.getFloat(fbdo, F("/smart_farm_iot/data/node1/node1_dht_temp")));
    lastFirebaseUpload = millis();
  }
}