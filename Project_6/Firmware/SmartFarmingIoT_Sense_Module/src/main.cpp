#include <DHT.h>
#include <WiFi.h>
#include <stdint.h>
#include <rom/rtc.h>
#include <esp_now.h>
#include <esp_system.h>

// Cấu hình Cảm biến
#define RAIN_PIN 25
#define SOIL_PIN 32
#define LIGHT_PIN 33
#define ANALOG2_PIN 34
#define DHT_PIN 14
#define DHT_TYPE DHT11

unsigned long intervalSend = 1000;
unsigned long lastInterval = 0;
const float K_FACTOR = 0.1;

float lightEstimated = 0.0;
float soilEstimated = 0.0;
float tempEstimated = 0.0;
float humidEstimated = 0.0;

unsigned long lastRegTime = 0;
const unsigned long regInterval = 30000;

uint32_t chipId = 0;
uint8_t node_id = 1;

// === Cấu trúc gói tin GỬI ĐI ===
typedef struct
{
  float mcu_temp;
  float temp;
  float humid;
  float light;
  float soil;
  uint8_t nodeID;
} esp_struct_node1;
esp_struct_node1 dataSend;

// === Cấu trúc gói tin ĐĂNG KÝ ===
typedef struct
{
  uint8_t nodeID;
  uint8_t type; // 1: Registration
} esp_struct_reg;
esp_struct_reg regMsg = {node_id, 1};
uint8_t MAC_NODE2[6] = {0x30, 0xAE, 0xA4, 0xF8, 0x08, 0x08};
uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
DHT dht(DHT_PIN, DHT_TYPE);

float readInternalTemperature()
{
  return temperatureRead();
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("Trạng thái gửi: ");
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    Serial.print("Thành công\t");
  }
  else
  {
    Serial.print("Thất bại\t");
  }
}

float kalmanFilter(float measurement, float &estimatedState)
{
  estimatedState = K_FACTOR * measurement + (1.0 - K_FACTOR) * estimatedState;
  return estimatedState;
}

// ************************ HÀM GỬI ĐĂNG KÝ ************************
void sendRegistration()
{
  esp_now_send(BROADCAST_MAC, (uint8_t *)&regMsg, sizeof(regMsg));
  Serial.printf("Node 1: Sent registration (ID: %d) to Broadcast.\n", node_id);
}

void setup()
{
  Serial.begin(115200);
  dht.begin();
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Lỗi khởi tạo ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peer2 = {};
  memcpy(peer2.peer_addr, MAC_NODE2, 6);
  peer2.channel = 0;
  peer2.encrypt = false;
  if (esp_now_add_peer(&peer2) != ESP_OK)
  {
    Serial.println("Thêm peer Master thất bại");
  }

  esp_now_peer_info_t peer_bcast = {};
  memcpy(peer_bcast.peer_addr, BROADCAST_MAC, 6);
  peer_bcast.channel = 0;
  peer_bcast.encrypt = false;
  esp_now_add_peer(&peer_bcast);

  pinMode(LIGHT_PIN, INPUT);
  pinMode(SOIL_PIN, INPUT);
  lightEstimated = (float)analogRead(LIGHT_PIN);
  soilEstimated = (float)analogRead(SOIL_PIN);
  tempEstimated = dht.readTemperature();
  humidEstimated = dht.readHumidity();

  sendRegistration();
  lastRegTime = millis();
}

void loop()
{
  if (millis() - lastInterval >= intervalSend)
  {
    int rawLight = analogRead(LIGHT_PIN);
    int rawSoil = analogRead(SOIL_PIN);

    dataSend.temp = dht.readTemperature();
    dataSend.humid = dht.readHumidity();
    dataSend.light = map(rawLight, 0, 4095, 100, 0);
    dataSend.soil = map(rawSoil, 0, 4095, 0, 100);
    dataSend.mcu_temp = readInternalTemperature();

    dataSend.nodeID = node_id;

    Serial.printf("mcu: %.1f\ttemp: %.1f\thumid: %.1f\tChipID:%u\n",
                  dataSend.mcu_temp, dataSend.temp, dataSend.humid, dataSend.nodeID);

    esp_now_send(MAC_NODE2, (uint8_t *)&dataSend, sizeof(dataSend));
    lastInterval = millis();
  }

  if (millis() - lastRegTime >= regInterval)
  {
    sendRegistration();
    lastRegTime = millis();
  }
}