#include <DHT.h>       // Thư viện cho cảm biến nhiệt độ, độ ẩm DHT
#include <WiFi.h>      // Thư viện kết nối WiFi cho ESP32
#include <stdint.h>    // Thư viện định nghĩa các kiểu dữ liệu số nguyên (uint8_t, uint32_t...)
#include <rom/rtc.h>   // Thư viện truy cập các tính năng thời gian thực của chip
#include <esp_now.h>   // Thư viện giao thức truyền thông ESP-NOW (không cần WiFi router)
#include <esp_system.h>// Thư viện hệ thống của ESP32
 
// === Cấu hình chân kết nối Cảm biến ===
#define RAIN_PIN 25    // Chân cảm biến mưa (Digital/Analog)
#define SOIL_PIN 32    // Chân cảm biến độ ẩm đất (Analog)
#define LIGHT_PIN 33   // Chân cảm biến ánh sáng (Analog)
#define ANALOG2_PIN 34 // Chân Analog dự phòng
#define DHT_PIN 14     // Chân dữ liệu của cảm biến DHT11
#define DHT_TYPE DHT11 // Xác định loại cảm biến là DHT11
 
// === Khai báo biến thời gian và hằng số ===
unsigned long intervalSend = 1000;  // Khoảng thời gian gửi dữ liệu (1 giây)
unsigned long lastInterval = 0;     // Lưu thời điểm gửi dữ liệu cuối cùng
const float K_FACTOR = 0.1;         // Hệ số lọc Kalman đơn giản (hệ số làm mượt)
 
// Các biến lưu giá trị ước tính sau khi lọc (Kalman)
float lightEstimated = 0.0;
float soilEstimated = 0.0;
float tempEstimated = 0.0;
float humidEstimated = 0.0;
 
unsigned long lastRegTime = 0;      // Lưu thời điểm gửi gói tin đăng ký cuối cùng
const unsigned long regInterval = 30000; // Khoảng thời gian gửi đăng ký lại (30 giây)
 
uint32_t chipId = 0;   // Biến lưu ID của chip ESP32
uint8_t node_id = 1;   // Mã định danh của trạm này (Node 1)
 
// === Cấu trúc gói tin DỮ LIỆU GỬI ĐI (Tới Master/Node 2) ===
typedef struct
{
  float mcu_temp;      // Nhiệt độ bên trong chip ESP32
  float temp;          // Nhiệt độ môi trường (từ DHT)
  float humid;         // Độ ẩm môi trường (từ DHT)
  float light;         // Cường độ ánh sáng (%)
  float soil;          // Độ ẩm đất (%)
  uint8_t nodeID;      // ID của Node gửi
} esp_struct_node1;
esp_struct_node1 dataSend; // Khởi tạo biến cấu trúc dữ liệu gửi
 
// === Cấu trúc gói tin ĐĂNG KÝ (Gửi Broadcast) ===
typedef struct
{
  uint8_t nodeID;      // ID của Node
  uint8_t type;        // Loại gói tin (1: Registration)
} esp_struct_reg;
esp_struct_reg regMsg = {node_id, 1}; // Khởi tạo gói tin đăng ký mặc định
 
// === Địa chỉ MAC của các thiết bị nhận ===
uint8_t MAC_NODE2[6] = {0xF4, 0x65, 0x0B, 0xA9, 0x52, 0x4C}; // MAC của ESP32 nhận (Master)
uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Địa chỉ quảng bá cho mọi thiết bị
 
DHT dht(DHT_PIN, DHT_TYPE); // Khởi tạo đối tượng dht để đọc dữ liệu
 
// Hàm đọc nhiệt độ nội tại của chip ESP32
float readInternalTemperature()
{
  return temperatureRead(); // Hàm có sẵn của ESP32 để đo nhiệt độ CPU
}
 
// Hàm phản hồi (Callback) khi dữ liệu được gửi đi
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("Trạng thái gửi: ");
  // Kiểm tra nếu status là ESP_NOW_SEND_SUCCESS thì thành công
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    Serial.print("Thành công\t");
  }
  else
  {
    Serial.print("Thất bại\t");
  }
}
 
// Hàm lọc Kalman đơn giản để làm mượt dữ liệu cảm biến (giảm nhiễu)
float kalmanFilter(float measurement, float &estimatedState)
{
  // Công thức: Giá trị mới = K * (Giá trị đo) + (1-K) * (Giá trị cũ)
  estimatedState = K_FACTOR * measurement + (1.0 - K_FACTOR) * estimatedState;
  return estimatedState;
}
 
// Hàm gửi gói tin đăng ký sự hiện diện của Node vào mạng
void sendRegistration()
{
  // Gửi gói tin regMsg tới địa chỉ Broadcast
  esp_now_send(BROADCAST_MAC, (uint8_t *)&regMsg, sizeof(regMsg));
  Serial.printf("Node 1: Sent registration (ID: %d) to Broadcast.\n", node_id);
}
 
void setup()
{
  Serial.begin(115200); // Khởi tạo giao tiếp Serial để debug
  dht.begin();          // Khởi động cảm biến DHT
  WiFi.mode(WIFI_STA);  // Thiết lập WiFi ở chế độ Station (bắt buộc cho ESP-NOW)
 
  // Khởi tạo giao thức ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Lỗi khởi tạo ESP-NOW");
    return;
  }
 
  // Đăng ký hàm callback để theo dõi kết quả gửi dữ liệu
  esp_now_register_send_cb(OnDataSent);
 
  // --- Cấu hình kết nối với Node 2 (Master) ---
  esp_now_peer_info_t peer2 = {};
  memcpy(peer2.peer_addr, MAC_NODE2, 6); // Copy địa chỉ MAC vào cấu hình peer
  peer2.channel = 0;                     // Sử dụng kênh WiFi mặc định
  peer2.encrypt = false;                 // Không mã hóa
  if (esp_now_add_peer(&peer2) != ESP_OK)
  {
    Serial.println("Thêm peer Master thất bại");
  }
 
  // --- Cấu hình kết nối Broadcast (Quảng bá) ---
  esp_now_peer_info_t peer_bcast = {};
  memcpy(peer_bcast.peer_addr, BROADCAST_MAC, 6);
  peer_bcast.channel = 0;
  peer_bcast.encrypt = false;
  esp_now_add_peer(&peer_bcast);
 
  // Thiết lập chế độ chân Input cho cảm biến
  pinMode(LIGHT_PIN, INPUT);
  pinMode(SOIL_PIN, INPUT);
  
  // Đọc giá trị ban đầu để khởi tạo các biến ước tính lọc Kalman
  lightEstimated = (float)analogRead(LIGHT_PIN);
  soilEstimated = (float)analogRead(SOIL_PIN);
  tempEstimated = dht.readTemperature();
  humidEstimated = dht.readHumidity();
 
  sendRegistration();     // Gửi đăng ký lần đầu khi khởi động
  lastRegTime = millis(); // Lưu mốc thời gian đăng ký
}
 
void loop()
{
  // Khối lệnh thực hiện theo chu kỳ gửi dữ liệu (mỗi 1 giây)
  if (millis() - lastInterval >= intervalSend)
  {
    // Đọc giá trị thô (0 - 4095) từ cảm biến Analog
    int rawLight = analogRead(LIGHT_PIN);
    int rawSoil = analogRead(SOIL_PIN);
 
    // Đọc dữ liệu từ DHT
    dataSend.temp = dht.readTemperature();
    dataSend.humid = dht.readHumidity();
    
    // Chuyển đổi giá trị Analog sang phần trăm (%)
    // map(giá trị, min_cũ, max_cũ, min_mới, max_mới)
    dataSend.light = map(rawLight, 0, 4095, 100, 0); // Ánh sáng: 0 là tối (100%), 4095 là sáng (0%)
    dataSend.soil = map(rawSoil, 0, 4095, 0, 100);   // Độ ẩm đất: 4095 khô (100%), 0 ướt (0%) -> Có thể cần chỉnh lại logic map tùy loại cảm biến
    
    // Đọc nhiệt độ CPU
    dataSend.mcu_temp = readInternalTemperature();
    dataSend.nodeID = node_id; // Gán ID node
 
    // In dữ liệu ra màn hình Serial để theo dõi
    Serial.printf("mcu: %.1f\ttemp: %.1f\thumid: %.1f\tChipID:%u\n",
                  dataSend.mcu_temp, dataSend.temp, dataSend.humid, dataSend.nodeID);
 
    // Gửi gói dữ liệu đo được tới Node 2
    esp_now_send(MAC_NODE2, (uint8_t *)&dataSend, sizeof(dataSend));
    lastInterval = millis(); // Cập nhật mốc thời gian gửi cuối
  }
 
  // Khối lệnh thực hiện gửi tin nhắn đăng ký theo chu kỳ (mỗi 30 giây)
  if (millis() - lastRegTime >= regInterval)
  {
    sendRegistration();     // Gọi hàm gửi đăng ký
    lastRegTime = millis(); // Cập nhật mốc thời gian đăng ký cuối
  }
}