#include <DHT.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <rom/rtc.h>

// Định nghĩa các chân IO
#define RL1_PIN 12
#define RL2_PIN 14
#define RL3_PIN 27
#define RL4_PIN 26
#define LED_PIN 25

#define DHT_PIN 5
#define DHT_TYPE DHT11

#define FLOW_PIN 18
#define FLOW_SENSOR_K_FACTOR 7.5

// Địa chỉ MAC của Node 2 (Master)
uint8_t MAC_NODE2[6] = {0xF4, 0x65, 0x0B, 0xA9, 0x52, 0x4C};
uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

DHT dht(DHT_PIN, DHT_TYPE);

const uint8_t id_node3 = 3;

// --- Cấu trúc gói tin ---
typedef struct
{
  float mcu_temp;
  float temp;
  float humid;
  int flow_rate;
  uint8_t led_status;
  uint8_t relay_1_status;
  uint8_t relay_2_status;
  uint8_t relay_3_status;
  uint8_t relay_4_status;
  uint8_t nodeID;
} esp_struct_node3_in;

typedef struct
{
  uint8_t relay_1_cmd;
  uint8_t relay_2_cmd;
  uint8_t relay_3_cmd;
  uint8_t relay_4_cmd;
} esp_struct_node3_out;

typedef struct
{
  uint8_t nodeID;
  uint8_t type;
} esp_struct_reg;

// --- Khai báo biến ---
esp_struct_node3_in outgoing_data;
esp_struct_node3_out incoming_command;
esp_struct_reg regMsg = {id_node3, 1};

unsigned long lastRegTime = 0;
const unsigned long regInterval = 30000;

unsigned long lastSendTime = 0;
const long sendInterval = 1000;

// Biến đo lưu lượng
volatile unsigned long pulseCount = 0;
unsigned long flowCalcLastTime = 0;
float currentFlowRate = 0;

// Trạng thái hiện tại của Relay
volatile uint8_t relay_status[4] = {0, 0, 0, 0};
const int relay_pins[4] = {RL1_PIN, RL2_PIN, RL3_PIN, RL4_PIN};

// =========================================================================
//                             FUNCTIONS
// =========================================================================

// Hàm ngắt ngoài
void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

/**
 * @brief Tính toán lưu lượng dòng chảy dựa trên số xung đếm được.
 * Lưu lượng (L/phút) = (Xung * 1000 / Thời gian (ms)) / K_FACTOR
 */
float calculateFlowRate()
{
  unsigned long currentTime = millis();
  unsigned long timeDelta = currentTime - flowCalcLastTime;
  if (timeDelta > 100)
  {
    detachInterrupt(FLOW_PIN);
    float frequency = (float)pulseCount * 1000.0 / timeDelta;
    currentFlowRate = frequency / FLOW_SENSOR_K_FACTOR;
    pulseCount = 0;
    flowCalcLastTime = currentTime;
    attachInterrupt(FLOW_PIN, pulseCounter, FALLING);
  }
  return currentFlowRate;
}

// Hàm đọc nhiệt độ internal MCU
float readInternalTemperature()
{
  return temperatureRead();
}

// Hàm giả lập dữ liệu
void readSensorData()
{
  outgoing_data.mcu_temp = readInternalTemperature();
  outgoing_data.temp = dht.readTemperature();
  outgoing_data.humid = dht.readHumidity();
  outgoing_data.flow_rate = calculateFlowRate();
  outgoing_data.led_status = digitalRead(LED_PIN);
  outgoing_data.nodeID = id_node3;

  outgoing_data.relay_1_status = digitalRead(RL1_PIN);
  outgoing_data.relay_2_status = digitalRead(RL2_PIN);
  outgoing_data.relay_3_status = digitalRead(RL3_PIN);
  outgoing_data.relay_4_status = digitalRead(RL4_PIN);

  Serial.printf("MCU: %.1f\tTemp: %.1f\tHumid: %.1f\tFlow: %.1f\n", outgoing_data.mcu_temp, outgoing_data.temp, outgoing_data.humid, outgoing_data.flow_rate);
}

// Hàm điều khiển Relay
void controlRelays()
{
  // Relay 1
  if (incoming_command.relay_1_cmd != relay_status[0])
  {
    relay_status[0] = incoming_command.relay_1_cmd;
    digitalWrite(relay_pins[0], !relay_status[0]); // Giả sử Relay kích mức LOW
    Serial.printf("Relay 1 set to: %d\n", relay_status[0]);
  }

  // Relay 2
  if (incoming_command.relay_2_cmd != relay_status[1])
  {
    relay_status[1] = incoming_command.relay_2_cmd;
    digitalWrite(relay_pins[1], !relay_status[1]);
    Serial.printf("Relay 2 set to: %d\n", relay_status[1]);
  }

  // Relay 3
  if (incoming_command.relay_3_cmd != relay_status[2])
  {
    relay_status[2] = incoming_command.relay_3_cmd;
    digitalWrite(relay_pins[2], !relay_status[2]);
    Serial.printf("Relay 3 set to: %d\n", relay_status[2]);
  }

  // Relay 4
  if (incoming_command.relay_4_cmd != relay_status[3])
  {
    relay_status[3] = incoming_command.relay_4_cmd;
    digitalWrite(relay_pins[3], !relay_status[3]);
    Serial.printf("Relay 4 set to: %d\n", relay_status[3]);
  }
}

// Hàm gửi dữ liệu đến Node 2
void sendData()
{
  readSensorData(); // Lấy dữ liệu cảm biến và trạng thái hiện tại

  esp_err_t result = esp_now_send(MAC_NODE2, (uint8_t *)&outgoing_data, sizeof(outgoing_data));

  if (result == ESP_OK)
  {
    Serial.println("Send data to Node 2 successful.");
  }
  else
  {
    Serial.println("Error sending data to Node 2.");
  }
}

// Hàm xử lý gói tin nhận được
void onReceive(const uint8_t *mac, const uint8_t *data, int len)
{
  if (len == sizeof(incoming_command))
  {
    memcpy(&incoming_command, data, sizeof(incoming_command));
    Serial.println("Received command from Node 2:");
    Serial.printf(" R1_CMD: %d, R2_CMD: %d, R3_CMD: %d, R4_CMD: %d\n",
                  incoming_command.relay_1_cmd, incoming_command.relay_2_cmd,
                  incoming_command.relay_3_cmd, incoming_command.relay_4_cmd);
    controlRelays();
  }
}

// Hàm xử lý gói tin gửi đi
void onSend(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("\nLast Packet Send Status: ");
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    Serial.println("Delivery Success");
  }
  else
  {
    Serial.println("Delivery Fail");
  }
}

// Hàm đăng ký
void sendRegistration()
{
  esp_now_send(BROADCAST_MAC, (uint8_t *)&regMsg, sizeof(regMsg));
  Serial.printf("Node 3: Sent registration (ID: %d) to Broadcast.\n", id_node3);
}

// =========================================================================
//                             SETUP & LOOP
// =========================================================================
// Hàm setup wifi
void setupWiFi()
{
  WiFi.mode(WIFI_STA);
  Serial.print("Node 3 MAC Address: ");
  Serial.println(WiFi.macAddress());
}

// Hàm setup ESP-NOW
void setupESP_NOW()
{
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onReceive);
  esp_now_register_send_cb(onSend);

  // Thêm peer Node 2
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, MAC_NODE2, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Thêm Broadcast Peer để gửi gói đăng ký
  esp_now_peer_info_t peer_bcast = {};
  memcpy(peer_bcast.peer_addr, BROADCAST_MAC, 6);
  peer_bcast.channel = 0;
  peer_bcast.encrypt = false;
  esp_now_add_peer(&peer_bcast);

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer Node 2");
    return;
  }
}

void setup()
{
  Serial.begin(115200);
  dht.begin();

  // Khởi tạo chân Relay và LED
  pinMode(RL1_PIN, OUTPUT);
  pinMode(RL2_PIN, OUTPUT);
  pinMode(RL3_PIN, OUTPUT);
  pinMode(RL4_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Khởi tạo trạng thái ban đầu của Relay là OFF
  digitalWrite(RL1_PIN, LOW);
  digitalWrite(RL2_PIN, LOW);
  digitalWrite(RL3_PIN, LOW);
  digitalWrite(RL4_PIN, LOW);

  // LED Status ON
  digitalWrite(LED_PIN, HIGH);

  pinMode(FLOW_PIN, INPUT_PULLUP);

  setupWiFi();
  setupESP_NOW();

  Serial.println("Node 3 (Slave) Initialized.");

  // --- Cấu hình Ngắt Ngoài cho Flow Sensor ---
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);
  flowCalcLastTime = millis();
}

void loop()
{
  if (millis() - lastSendTime > sendInterval)
  {
    lastSendTime = millis();
    sendData();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  if (millis() - lastRegTime >= regInterval)
  {
    sendRegistration();
    lastRegTime = millis();
  }
}