#include <DHT.h>          // Thư viện cảm biến nhiệt độ, độ ẩm
#include <WiFi.h>         // Thư viện WiFi (dùng để lấy địa chỉ MAC và thiết lập chế độ Station)
#include <esp_now.h>      // Thư viện giao thức truyền thông ESP-NOW
#include <Arduino.h>      
#include <ArduinoOTA.h>   // Thư viện cập nhật firmware qua WiFi (nếu cần)
#include <rom/rtc.h>      // Thư viện hệ thống Real-Time Clock

// --- Định nghĩa các chân IO ---
#define RL1_PIN 12        // Relay 1 nối chân 12
#define RL2_PIN 14        // Relay 2 nối chân 14
#define RL3_PIN 27        // Relay 3 nối chân 27
#define RL4_PIN 26        // Relay 4 nối chân 26
#define LED_PIN 25        // LED trạng thái nối chân 25

#define DHT_PIN 5         // Chân dữ liệu cảm biến DHT11 nối chân 5
#define DHT_TYPE DHT11    // Loại cảm biến DHT11

#define FLOW_PIN 18       // Chân tín hiệu cảm biến dòng chảy (YF-S201...)
#define FLOW_SENSOR_K_FACTOR 7.5 // Hệ số K (thường là 7.5 cho cảm biến YF-S201)

// --- Cấu hình mạng ---
uint8_t MAC_NODE2[6] = {0xF4, 0x65, 0x0B, 0xA9, 0x52, 0x4C}; // MAC của Node Master
uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Địa chỉ gửi quảng bá

DHT dht(DHT_PIN, DHT_TYPE); // Khởi tạo đối tượng DHT
const uint8_t id_node3 = 3; // Định danh của Node này là số 3

// --- Cấu trúc gói tin DỮ LIỆU GỬI ĐI (Node 3 -> Master) ---
typedef struct {
  float mcu_temp;        // Nhiệt độ CPU
  float temp;            // Nhiệt độ môi trường
  float humid;           // Độ ẩm
  int flow_rate;         // Lưu lượng nước
  uint8_t led_status;    // Trạng thái đèn LED
  uint8_t relay_1_status; // Trạng thái thực tế Relay 1
  uint8_t relay_2_status; // Trạng thái thực tế Relay 2
  uint8_t relay_3_status; // Trạng thái thực tế Relay 3
  uint8_t relay_4_status; // Trạng thái thực tế Relay 4
  uint8_t nodeID;        // ID của Node (là 3)
} esp_struct_node3_in;

// --- Cấu trúc gói tin LỆNH NHẬN VỀ (Master -> Node 3) ---
typedef struct {
  uint8_t relay_1_cmd;   // Lệnh điều khiển Relay 1 (0: Tắt, 1: Bật)
  uint8_t relay_2_cmd;   // Lệnh điều khiển Relay 2
  uint8_t relay_3_cmd;   // Lệnh điều khiển Relay 3
  uint8_t relay_4_cmd;   // Lệnh điều khiển Relay 4
} esp_struct_node3_out;

// --- Cấu trúc gói tin ĐĂNG KÝ ---
typedef struct {
  uint8_t nodeID;        // ID Node
  uint8_t type;          // Loại tin nhắn (1: Đăng ký)
} esp_struct_reg;

// --- Khai báo biến toàn cục ---
esp_struct_node3_in outgoing_data;    // Biến chứa dữ liệu gửi đi
esp_struct_node3_out incoming_command; // Biến chứa lệnh nhận về
esp_struct_reg regMsg = {id_node3, 1}; // Khởi tạo tin nhắn đăng ký

unsigned long lastRegTime = 0;        // Thời điểm đăng ký cuối
const unsigned long regInterval = 30000; // Chu kỳ đăng ký (30 giây)

unsigned long lastSendTime = 0;       // Thời điểm gửi dữ liệu cuối
const long sendInterval = 1000;       // Chu kỳ gửi dữ liệu (1 giây)

// --- Biến đo lưu lượng dòng chảy ---
volatile unsigned long pulseCount = 0; // Biến đếm xung (volatile vì dùng trong ngắt)
unsigned long flowCalcLastTime = 0;    // Thời điểm tính lưu lượng cuối
float currentFlowRate = 0;             // Giá trị lưu lượng hiện tại (L/phút)

// --- Quản lý Relay ---
volatile uint8_t relay_status[4] = {0, 0, 0, 0}; // Mảng lưu trạng thái mong muốn
const int relay_pins[4] = {RL1_PIN, RL2_PIN, RL3_PIN, RL4_PIN}; // Mảng chân vật lý

// =========================================================================
//                            CÁC HÀM CHỨC NĂNG
// =========================================================================

// Hàm ngắt: Tự động cộng xung khi cảm biến dòng chảy quay
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

/**
 * Hàm tính lưu lượng (Lít/phút)
 * Công thức: Flow = (Tần số xung) / K_FACTOR
 */
float calculateFlowRate() {
  unsigned long currentTime = millis();
  unsigned long timeDelta = currentTime - flowCalcLastTime;
  
  if (timeDelta > 100) { // Chỉ tính toán nếu đã qua ít nhất 100ms
    detachInterrupt(FLOW_PIN); // Tạm dừng ngắt để tính toán chính xác
    
    // Tần số (Hz) = Số xung / Thời gian (giây)
    float frequency = (float)pulseCount * 1000.0 / timeDelta;
    currentFlowRate = frequency / FLOW_SENSOR_K_FACTOR;
    
    pulseCount = 0;             // Reset đếm xung cho chu kỳ mới
    flowCalcLastTime = currentTime;
    attachInterrupt(FLOW_PIN, pulseCounter, FALLING); // Kích hoạt lại ngắt
  }
  return currentFlowRate;
}

// Đọc nhiệt độ bên trong chip ESP32
float readInternalTemperature() {
  return temperatureRead();
}

// Hàm cập nhật tất cả dữ liệu cảm biến vào cấu trúc gửi đi
void readSensorData() {
  outgoing_data.mcu_temp = readInternalTemperature();
  outgoing_data.temp = dht.readTemperature();
  outgoing_data.humid = dht.readHumidity();
  outgoing_data.flow_rate = (int)calculateFlowRate(); // Ép kiểu về int
  outgoing_data.led_status = digitalRead(LED_PIN);
  outgoing_data.nodeID = id_node3;

  // Đọc trạng thái thực tế từ các chân Relay
  outgoing_data.relay_1_status = digitalRead(RL1_PIN);
  outgoing_data.relay_2_status = digitalRead(RL2_PIN);
  outgoing_data.relay_3_status = digitalRead(RL3_PIN);
  outgoing_data.relay_4_status = digitalRead(RL4_PIN);

  Serial.printf("MCU: %.1f\tTemp: %.1f\tHumid: %.1f\tFlow: %.1f\n", 
                outgoing_data.mcu_temp, outgoing_data.temp, 
                outgoing_data.humid, currentFlowRate);
}

// Hàm thực thi lệnh điều khiển Relay nhận được từ Master
void controlRelays() {
  // Relay 1: Nếu lệnh khác trạng thái hiện tại thì cập nhật
  if (incoming_command.relay_1_cmd != relay_status[0]) {
    relay_status[0] = incoming_command.relay_1_cmd;
    digitalWrite(relay_pins[0], !relay_status[0]); // Đảo mức logic vì Relay kích mức LOW
    Serial.printf("Relay 1 set to: %d\n", relay_status[0]);
  }
  // Tương tự cho Relay 2, 3, 4...
  if (incoming_command.relay_2_cmd != relay_status[1]) {
    relay_status[1] = incoming_command.relay_2_cmd;
    digitalWrite(relay_pins[1], !relay_status[1]);
    Serial.printf("Relay 2 set to: %d\n", relay_status[1]);
  }
  if (incoming_command.relay_3_cmd != relay_status[2]) {
    relay_status[2] = incoming_command.relay_3_cmd;
    digitalWrite(relay_pins[2], !relay_status[2]);
    Serial.printf("Relay 3 set to: %d\n", relay_status[2]);
  }
  if (incoming_command.relay_4_cmd != relay_status[3]) {
    relay_status[3] = incoming_command.relay_4_cmd;
    digitalWrite(relay_pins[3], !relay_status[3]);
    Serial.printf("Relay 4 set to: %d\n", relay_status[3]);
  }
}

// Hàm gửi gói dữ liệu chính tới Node 2
void sendData() {
  readSensorData(); // Cập nhật dữ liệu mới nhất trước khi gửi
  esp_err_t result = esp_now_send(MAC_NODE2, (uint8_t *)&outgoing_data, sizeof(outgoing_data));
  
  if (result == ESP_OK) Serial.println("Send data to Node 2 successful.");
  else Serial.println("Error sending data to Node 2.");
}

// Hàm Callback: Tự động chạy khi nhận được dữ liệu từ thiết bị khác
void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  // Kiểm tra nếu kích thước gói tin khớp với cấu trúc lệnh
  if (len == sizeof(incoming_command)) {
    memcpy(&incoming_command, data, sizeof(incoming_command)); // Copy dữ liệu vào biến
    Serial.println("Received command from Node 2:");
    controlRelays(); // Thực thi lệnh bật/tắt relay
  }
}

// Hàm Callback: Tự động chạy sau khi gửi dữ liệu để báo trạng thái thành công/thất bại
void onSend(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\nLast Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Hàm gửi gói tin quảng bá để Master biết Node 3 đang hoạt động
void sendRegistration() {
  esp_now_send(BROADCAST_MAC, (uint8_t *)&regMsg, sizeof(regMsg));
  Serial.printf("Node 3: Sent registration (ID: %d) to Broadcast.\n", id_node3);
}

// =========================================================================
//                            SETUP & LOOP
// =========================================================================

void setupWiFi() {
  WiFi.mode(WIFI_STA); // Thiết lập chế độ Station (bắt buộc cho ESP-NOW)
  Serial.print("Node 3 MAC Address: ");
  Serial.println(WiFi.macAddress());
}

void setupESP_NOW() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Đăng ký các hàm xử lý sự kiện nhận và gửi
  esp_now_register_recv_cb(onReceive);
  esp_now_register_send_cb(onSend);

  // Thiết lập thông tin Peer (Master)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, MAC_NODE2, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Thiết lập thông tin Peer (Broadcast)
  esp_now_peer_info_t peer_bcast = {};
  memcpy(peer_bcast.peer_addr, BROADCAST_MAC, 6);
  peer_bcast.channel = 0;
  peer_bcast.encrypt = false;

  // Thêm các Peer vào danh sách quản lý của ESP
  esp_now_add_peer(&peer_bcast);
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer Node 2");
    return;
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Khởi tạo chân Relay/LED là Output
  pinMode(RL1_PIN, OUTPUT);
  pinMode(RL2_PIN, OUTPUT);
  pinMode(RL3_PIN, OUTPUT);
  pinMode(RL4_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Trạng thái ban đầu: Relay Tắt (LOW), LED Bật (HIGH)
  digitalWrite(RL1_PIN, LOW);
  digitalWrite(RL2_PIN, LOW);
  digitalWrite(RL3_PIN, LOW);
  digitalWrite(RL4_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);

  pinMode(FLOW_PIN, INPUT_PULLUP); // Cảm biến dòng chảy dùng điện trở kéo lên

  setupWiFi();
  setupESP_NOW();

  // Thiết lập ngắt cho cảm biến lưu lượng (khi chân FLOW_PIN chuyển từ cao xuống thấp)
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);
  flowCalcLastTime = millis();
}

void loop() {
  // Chu kỳ 1 giây: Gửi dữ liệu cảm biến
  if (millis() - lastSendTime > sendInterval) {
    lastSendTime = millis();
    sendData();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Nháy LED để báo hiệu đang chạy
  }

  // Chu kỳ 30 giây: Gửi gói tin đăng ký
  if (millis() - lastRegTime >= regInterval) {
    sendRegistration();
    lastRegTime = millis();
  }
}