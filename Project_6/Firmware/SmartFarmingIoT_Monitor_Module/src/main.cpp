#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include "UI/ui.h"
#include "CST820.h"
#include <HTTPClient.h>
#include <LovyanGFX.hpp>
#include <FirebaseESP32.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <ArduinoJson.h>

// --- CẤU HÌNH PIN ---
#define BOOT_BUTTON_PIN 0

// --- CẤU HÌNH FIREBASE ---
#define API_KEY "AIzaSyAgVge-VC6S9RocrJQibOWFqY1De7nz3eM"
#define DATABASE_URL "https://smartfarmingiot-78911-default-rtdb.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL "admin@gmail.com"
#define USER_PASS "admins"

// --- CẤU HÌNH WEATHER API ---
#define OPENWEATHERMAP_API_KEY "fe3f1acd362a2919de02d9a4ae0cbd40"

float current_lat = 0;
float current_lon = 0;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const char *month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// --- BIẾN TOÀN CỤC ---
const long gmtOffset_sec = 7 * 3600;

unsigned long lastStatsUpdate = 0;
unsigned long lastFirebaseUpdate = 0;
unsigned long pressStartTime = 0;
unsigned long lastTimesync = 0;
unsigned long lastDatesync = 0;
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherInterval = 1800000;
bool isPressing = false;

unsigned long button1_override_time = 0;
unsigned long button2_override_time = 0;
unsigned long button3_override_time = 0;
unsigned long button4_override_time = 0;

bool last_relay1_state = false;
bool last_relay2_state = false;
bool last_relay3_state = false;
bool last_relay4_state = false;

// --- LGFX & DISPLAY CONFIG ---
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_Parallel8 _bus_instance;

public:
  LGFX(void)
  {
    auto cfg_b = _bus_instance.config();
    cfg_b.pin_wr = 4;
    cfg_b.pin_rd = 2;
    cfg_b.pin_rs = 16;
    cfg_b.pin_d0 = 15;
    cfg_b.pin_d1 = 13;
    cfg_b.pin_d2 = 12;
    cfg_b.pin_d3 = 14;
    cfg_b.pin_d4 = 27;
    cfg_b.pin_d5 = 25;
    cfg_b.pin_d6 = 33;
    cfg_b.pin_d7 = 32;
    _bus_instance.config(cfg_b);
    _panel_instance.setBus(&_bus_instance);

    auto cfg_p = _panel_instance.config();
    cfg_p.pin_cs = 17;
    cfg_p.panel_width = 240;
    cfg_p.panel_height = 320;
    _panel_instance.config(cfg_p);
    setPanel(&_panel_instance);
  }
};

static LGFX tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[240 * 40];  // Buffer 1
static lv_color_t buf2[240 * 40]; // Buffer 2 (THÊM ĐỂ GIẢM FLICKER)
CST820 touch(21, 22, -1, -1);

// --- CALLBACKS CHO LVGL ---
void my_disp_flush(lv_disp_drv_t *d, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.pushImageDMA(area->x1, area->y1, w, h, &color_p->full);
  lv_disp_flush_ready(d);
}

void my_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
  uint16_t x, y;
  uint8_t g;
  data->state = touch.getTouch(&x, &y, &g) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  data->point.x = x;
  data->point.y = y;
}

#ifdef __cplusplus
extern "C"
{
#endif
  uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
// ---------------------- HÀM XỬ LÝ SỰ KIỆN LVGL ----------------------
void onButton1Clicked(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  bool isChecked = lv_obj_has_state(btn, LV_STATE_CHECKED);

  // ĐƯỜNG DẪN MỚI: smart_farm_iot/data/node3/relay1_state
  if (Firebase.ready())
  {
    Firebase.setBool(fbdo, F("/smart_farm_iot/data/node3/relay1_state"), isChecked);
    last_relay1_state = isChecked;
  }
}

void onButton2Clicked(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  bool isChecked = lv_obj_has_state(btn, LV_STATE_CHECKED);

  if (Firebase.ready())
  {
    Firebase.setBool(fbdo, F("/smart_farm_iot/data/node3/relay2_state"), isChecked);
    last_relay2_state = isChecked;
  }
}

void onButton3Clicked(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  bool isChecked = lv_obj_has_state(btn, LV_STATE_CHECKED);
  if (Firebase.ready())
  {
    Firebase.setBool(fbdo, F("/smart_farm_iot/data/node3/relay3_state"), isChecked);
    last_relay3_state = isChecked;
  }
}

void onButton4Clicked(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  bool isChecked = lv_obj_has_state(btn, LV_STATE_CHECKED);
  if (Firebase.ready())
  {
    Firebase.setBool(fbdo, F("/smart_farm_iot/data/node3/relay4_state"), isChecked);
    last_relay4_state = isChecked;
  }
}

String removeVietnameseAccents(String str)
{
  if (str.length() == 0)
    return "";
  String resStr = str;
  const char *s[] = {
      "àáạảãâầấậẩẫăằắặẳẵ", "a", "èéẹẻẽêềếệểễ", "e", "ìíịỉĩ", "i",
      "òóọỏõôồốộổỗơờớợởỡ", "o", "ùúụủũưừứựửữ", "u", "ỳýỵỷỹ", "y",
      "đ", "d", "ÀÁẠẢÃÂẦẤẬẨẪĂẰẮẶẲẴ", "A", "ÈÉẸẺẼÊỀẾỆỂỄ", "E",
      "ÌÍỊỈĨ", "I", "ÒÓỌỎÕÔỒỐỘỔỖƠỜỚỢỞỠ", "O", "ÙÚỤỦŨƯỪỨỰỬỮ", "U",
      "ỲÝỴỶỸ", "Y", "Đ", "D"};
  for (int i = 0; i < 28; i += 2)
  {
    String pattern = s[i];
    char replacement = s[i + 1][0];
    for (int j = 0; j < pattern.length();)
    {
      int len = 0;
      unsigned char c = pattern[j];
      if (c < 0x80)
        len = 1;
      else if ((c & 0xE0) == 0xC0)
        len = 2;
      else if ((c & 0xF0) == 0xE0)
        len = 3;
      else if ((c & 0xF8) == 0xF0)
        len = 4;

      String utf8Char = pattern.substring(j, j + len);
      resStr.replace(utf8Char, String(replacement));
      j += len;
    }
  }
  return resStr;
}

void syncRelaysFromFirebase()
{
  if (!Firebase.ready())
    return;

  if (Firebase.getString(fbdo, F("/smart_farm_iot/config/custom_names/relay1")))
  {
    String raw_name = fbdo.stringData();
    String label_button1 = removeVietnameseAccents(raw_name);
    lv_label_set_text(ui_LabelBT1, label_button1.c_str());
  }

  // Đọc và xử lý Relay 2
  if (Firebase.getString(fbdo, F("/smart_farm_iot/config/custom_names/relay2")))
  {
    String label_button2 = removeVietnameseAccents(fbdo.stringData());
    lv_label_set_text(ui_LabelBT2, label_button2.c_str());
  }

  // Đọc và xử lý Relay 3
  if (Firebase.getString(fbdo, F("/smart_farm_iot/config/custom_names/relay3")))
  {
    String label_button3 = removeVietnameseAccents(fbdo.stringData());
    lv_label_set_text(ui_LabelBT3, label_button3.c_str());
  }

  // Đọc và xử lý Relay 4
  if (Firebase.getString(fbdo, F("/smart_farm_iot/config/custom_names/relay4")))
  {
    String label_button4 = removeVietnameseAccents(fbdo.stringData());
    lv_label_set_text(ui_LabelBT4, label_button4.c_str());
  }

  if (Firebase.getJSON(fbdo, F("/smart_farm_iot/data/node3")))
  {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, fbdo.jsonString());

    // Đọc trạng thái từ JSON
    bool r1 = doc["relay1_state"];
    bool r2 = doc["relay2_state"];
    bool r3 = doc["relay3_state"];
    bool r4 = doc["relay4_state"];

    // Nếu khác với trạng thái hiện tại trên màn hình thì cập nhật
    if (r1 != last_relay1_state)
    {
      last_relay1_state = r1;
      if (r1)
        lv_obj_add_state(ui_Button1, LV_STATE_CHECKED);
      else
        lv_obj_clear_state(ui_Button1, LV_STATE_CHECKED);
    }
    // Tương tự cho r2, r3, r4...
    if (r2 != last_relay2_state)
    {
      last_relay2_state = r2;
      if (r2)
        lv_obj_add_state(ui_Button2, LV_STATE_CHECKED);
      else
        lv_obj_clear_state(ui_Button2, LV_STATE_CHECKED);
    }
    // ... (tiếp tục cho r3 và r4)
    if (r3 != last_relay3_state)
    {
      last_relay3_state = r3;
      if (r3)
        lv_obj_add_state(ui_Button3, LV_STATE_CHECKED);
      else
        lv_obj_clear_state(ui_Button3, LV_STATE_CHECKED);
    }
    // ... (tiếp tục cho r3 và r4)
    if (r4 != last_relay4_state)
    {
      last_relay4_state = r4;
      if (r4)
        lv_obj_add_state(ui_Button4, LV_STATE_CHECKED);
      else
        lv_obj_clear_state(ui_Button4, LV_STATE_CHECKED);
    }
  }
}

// --- QUẢN LÝ WIFI & SMARTCONFIG ---
void startSmartConfig()
{
  Serial.println("\n[WIFI] Đang xóa cấu hình cũ...");
  WiFi.disconnect(true, true);
  delay(1000);

  WiFi.beginSmartConfig();
  Serial.println("[WIFI] Chờ SmartConfig từ điện thoại...");

  if (ui_Label7)
    lv_label_set_text(ui_Label7, "SmartConfig Mode...");

  while (!WiFi.smartConfigDone())
  {
    delay(500);
    Serial.print("#");
    lv_timer_handler();
    yield(); // Cho watchdog nghỉ
  }

  Serial.println("\n[WIFI] Đã nhận cấu hình!");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    lv_timer_handler();
    yield();
  }
  Serial.println("\n[WIFI] Kết nối thành công!");
}

void setupWiFi()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin();

  Serial.print("[WIFI] Đang kết nối");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 15)
  {
    delay(1000);
    Serial.print(".");
    timeout++;
    yield(); // Cho watchdog nghỉ
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n[WIFI] Kết nối OK!");
    Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
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

// --- CẬP NHẬT UI ĐỊNH KỲ ---
void updateSystemUI()
{
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready())
    return;

  // 1. Lấy nhiệt độ MCU nội bộ (Không liên quan Firebase)
  float temp_f = temprature_sens_read();
  float temp_c = (temp_f - 32) / 1.8;

  // 2. Đọc dữ liệu từ Firebase về các biến tạm
  if (Firebase.getJSON(fbdo, F("/smart_farm_iot/data/node1")))
  {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, fbdo.jsonString());

    float n1_temp = doc["node1_dht_temp"];
    int n1_humid = doc["node1_dht_humid"];
    int n1_soil = doc["node1_soil"];
    float n1_light = doc["node1_light"];
    bool n1_rain = doc["node1_rain"];
    float n1_mcu_temp = doc["node1_mcu_temp"];
    bool n1_state = doc["node1_state"];
    Serial.printf("n1_temp: %0.1f n1_humid: %0.1f, n1_soil: %0.1f n1_light: %0.1f\n", n1_temp, n1_humid, n1_soil, n1_light);

    // 3. Hiển thị lên UI
    char buff_mcu2[16];
    snprintf(buff_mcu2, sizeof(buff_mcu2), "%0.1f°C", temp_c);
    lv_label_set_text_fmt(ui_LabelMCU2, "%s", buff_mcu2);

    lv_arc_set_value(ui_ArcMCU2, (int)temp_c);

    // Cập nhật dữ liệu nhiệt độ MCU Node1
    char buff_n1_mcu_temp[16];
    snprintf(buff_n1_mcu_temp, sizeof(buff_n1_mcu_temp), "MCU: %0.1f °C", n1_mcu_temp);
    lv_label_set_text_fmt(ui_LabelNode1MCU, "%s", buff_n1_mcu_temp);

    // Cập nhật dữ liệu cảm biến Node1
    char buff_n1_temp[16];
    snprintf(buff_n1_temp, sizeof(buff_n1_temp), "Temperature: %0.1f °C", n1_temp);
    lv_label_set_text_fmt(ui_LabelNode1Temp, "%s", buff_n1_temp);

    // Cập nhật dữ liệu cảm biến Node1
    char buff_n1_humid[16];
    snprintf(buff_n1_humid, sizeof(buff_n1_humid), "Humidity: %d %%", n1_humid);
    lv_label_set_text_fmt(ui_LabelNode1Humid, "%s", buff_n1_humid);

    // Cập nhật dữ liệu cảm biến Node1
    char buff_n1_soil[16];
    snprintf(buff_n1_soil, sizeof(buff_n1_soil), "Soil: %d %%", n1_soil);
    lv_label_set_text_fmt(ui_LabelNode1Soil, "%s", buff_n1_soil);

    // Cập nhật dữ liệu cảm biến Node1
    char buff_n1_light[16];
    snprintf(buff_n1_light, sizeof(buff_n1_light), "Light: %0.1f lx", n1_light);
    lv_label_set_text_fmt(ui_LabelNode1Light, "%s", buff_n1_light);

    lv_arc_set_value(ui_ArcTemp, (int)n1_temp);
    char buff_n1_mcu_temp_arc[16];
    snprintf(buff_n1_mcu_temp_arc, sizeof(buff_n1_mcu_temp_arc), "%d°C", (int)n1_mcu_temp);
    lv_label_set_text_fmt(ui_LabelTemp, "%s", buff_n1_mcu_temp_arc);

    lv_arc_set_value(ui_ArcHumid, (int)n1_humid);
    char buff_n1_humid_arc[16];
    snprintf(buff_n1_humid_arc, sizeof(buff_n1_humid_arc), "%d%%", (int)n1_humid);
    lv_label_set_text_fmt(ui_LabelHumid, "%s", buff_n1_humid_arc);

    lv_arc_set_value(ui_ArcMCU1, (int)n1_mcu_temp);
    char buff_n1_mcu_arc[16];
    snprintf(buff_n1_mcu_arc, sizeof(buff_n1_mcu_arc), "%d°C", (int)n1_mcu_temp);
    lv_label_set_text_fmt(ui_LabelMCU1, "%s", buff_n1_mcu_arc);
  }

  if (Firebase.getJSON(fbdo, F("/smart_farm_iot/data/node2")))
  {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, fbdo.jsonString());

    int wther_temp = doc["weather_temp"];
    int wther_humid = doc["weather_humid"];
    int wther_wind = doc["weather_humid"];
    int wther_rain = doc["weather_humid"];
    int wther_ads = doc["weather_humid"];

    String wther_sunset = doc["weather_humid"];
    String wther_sunrise = doc["weather_humid"];
    String wther_state = doc["weather_state"];
  }

  if (Firebase.getJSON(fbdo, F("/smart_farm_iot/data/node3")))
  {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, fbdo.jsonString());

    float n3_mcu_temp = doc["node3_mcu_temp"];
    float n3_flow = doc["flowrate"];
    char buff_n3_mcu[16];
    snprintf(buff_n3_mcu, sizeof(buff_n3_mcu), "MCU: %0.1f°C", n3_mcu_temp);
    lv_label_set_text_fmt(ui_LabelNode3MCU, "%s", buff_n3_mcu);
    char buff_n3_flow[16];
    snprintf(buff_n3_flow, sizeof(buff_n3_flow), "Flow: %0.1f L/min", n3_flow);
    lv_label_set_text_fmt(ui_LabelNode3Flow, "%s", buff_n3_flow);

    // Cập nhật UI Arcs
    lv_arc_set_value(ui_ArcMCU3, (int)n3_mcu_temp);
    char buff_n3_mcu_arc[16];
    snprintf(buff_n3_mcu_arc, sizeof(buff_n3_mcu_arc), "%d°C", (int)n3_mcu_temp);
    lv_label_set_text_fmt(ui_LabelMCU3, "%s", buff_n3_mcu_arc);

    // Cập nhật Flowrate
    char buff_n3_flow_arc[16];
    snprintf(buff_n3_flow_arc, sizeof(buff_n3_flow_arc), "%d", (int)n3_flow);
    lv_label_set_text_fmt(ui_LabelPWM, "%s", buff_n3_flow_arc);
    lv_arc_set_value(ui_ArcFlowrate, (int)n3_flow);
  }
}

// --- CẬP NHẬT FIREBASE ---
void updateFirebase()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[FIREBASE] WiFi chưa kết nối!");
    return;
  }

  if (!Firebase.ready())
  {
    Serial.println("[FIREBASE] Chưa sẵn sàng!");
    return;
  }

  float temp_f = temprature_sens_read();
  float temp_c = (temp_f - 32) / 1.8;

  Serial.printf("[FIREBASE] Đang gửi nhiệt độ: %.2f °C\n", temp_c);

  if (Firebase.setFloat(fbdo, F("/smart_farm_iot/data/node2/node2_mcu_temp"), temp_c))
  {
    Serial.println("[FIREBASE] ✓ Cập nhật thành công!");
  }
  else
  {
    Serial.println("[FIREBASE] ✗ Lỗi: " + fbdo.errorReason());
  }

  if (Firebase.setString(fbdo, F("/smart_farm_iot/data/node2/node2_ssid"), WiFi.SSID()))
  {
    Serial.println("[FIREBASE] ✓ Cập nhật SSID thành công!");
  }
  else
  {
    Serial.println("[FIREBASE] ✗ Lỗi: " + fbdo.errorReason());
  }
  if (Firebase.setString(fbdo, F("/smart_farm_iot/data/node2/node2_ip"), WiFi.localIP().toString()))
  {
    Serial.println("[FIREBASE] ✓ Cập nhật IP thành công!");
  }
  else
  {
    Serial.println("[FIREBASE] ✗ Lỗi: " + fbdo.errorReason());
  }
  char buff_ip[32];
  snprintf(buff_ip, sizeof(buff_ip), "IP:%s\n.%s.%s.%s", String(WiFi.localIP()[0]).c_str(), String(WiFi.localIP()[1]).c_str(), String(WiFi.localIP()[2]).c_str(), String(WiFi.localIP()[3]).c_str());
  lv_label_set_text(ui_Label7, buff_ip);
}

// --- LẤY VỊ TRÍ TỪ IP-API ---
String getLocation()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return "WiFi not connected";
  }

  HTTPClient http;
  // API miễn phí của ip-api (trả về: thành phố, vùng/tỉnh, tên quốc gia)
  String url = "http://ip-api.com/json/?fields=status,country,regionName,city";

  http.begin(url);
  int httpCode = http.GET();

  String result = "Unknown Location";

  if (httpCode > 0)
  {
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();

      // Cấp phát bộ nhớ để phân tích JSON
      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error)
      {
        if (String(doc["status"] == "success"))
        {
          const char *city = doc["city"];
          const char *province = doc["regionName"];
          const char *country = doc["country"];

          // Định dạng trả về: Province/city, country
          result = String(province) + ", " + String(country);
        }
      }
    }
  }
  else
  {
    result = "HTTP Error: " + String(http.errorToString(httpCode).c_str());
  }

  http.end();
  return result;
}

// Forward declaration for AQI fetch function
void fetchAQIData(float lat, float lon);

void fetchFullWeatherData(float lat, float lon)
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;
  // URL lấy dữ liệu thời tiết hiện tại
  String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(lat, 4) +
               "&lon=" + String(lon, 4) + "&appid=" + OPENWEATHERMAP_API_KEY + "&units=metric&lang=en";

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString();
    // Cấp phát đủ bộ nhớ cho JSON lớn
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);

    // 1. Nhiệt độ & Độ ẩm
    float temp = doc["main"]["temp"];
    int humid = doc["main"]["humidity"];

    // 2. Tốc độ gió (m/s)
    float windSpeed = doc["wind"]["speed"];

    // 3. Bình minh & Hoàng hôn (Unix timestamp)
    long sunrise_raw = doc["sys"]["sunrise"];
    long sunset_raw = doc["sys"]["sunset"];

    // Chuyển đổi Unix sang định dạng HH:mm
    char sunrise_str[6], sunset_str[6];
    struct tm *timeptr;

    timeptr = localtime(&sunrise_raw);
    strftime(sunrise_str, sizeof(sunrise_str), "%H:%M", timeptr);

    timeptr = localtime(&sunset_raw);
    strftime(sunset_str, sizeof(sunset_str), "%H:%M", timeptr);

    // 4. Trạng thái thời tiết (Lấy mô tả tiếng Việt)
    const char *weather_desc = doc["weather"][0]["description"];
    const char *weather_main = doc["weather"][0]["main"];

    int cloud_pct = doc["clouds"]["all"];
    float rain_1h = 0;
    if (doc.containsKey("rain"))
    {
      rain_1h = doc["rain"]["1h"];
    }

    // --- GỬI DỮ LIỆU LÊN FIREBASE (Khớp cấu trúc node2 của bạn) ---
    if (Firebase.ready())
    {
      String path = "/smart_farm_iot/data/node2";
      Firebase.setFloat(fbdo, path + "/weather_temp", temp);
      Firebase.setInt(fbdo, path + "/weather_humid", humid);
      Firebase.setFloat(fbdo, path + "/weather_windspeed", windSpeed);
      Firebase.setString(fbdo, path + "/sunrise", String(sunrise_str));
      Firebase.setString(fbdo, path + "/sunset", String(sunset_str));
      Firebase.setString(fbdo, path + "/Weather_state", String(weather_desc));
      Firebase.setInt(fbdo, path + "/weather_rain", (rain_1h > 0) ? 100 : cloud_pct);

      Serial.println("[OWM] Đã cập nhật dữ liệu lên Firebase");
      Serial.printf("[OWM] Temp: %.1f °C, Humid: %d %% sunrise: %s, sunset: %s air quality: %s\n", temp, humid, sunrise_str, sunset_str);
    }

    // --- CẬP NHẬT LÊN UI  ---
    lv_label_set_text(ui_LabelWeatherState, weather_desc);
    lv_label_set_text_fmt(ui_LabelLocalTempHumid, "%d°C\n%d%%", (int)temp, humid);
    
    char buff_wind[16];
    snprintf(buff_wind, sizeof(buff_wind), "%0.1f m/s", windSpeed);
    lv_label_set_text(ui_LabelWindspeed, buff_wind);
    lv_label_set_text(ui_LabelSunrise, String(sunrise_str).c_str());
    lv_label_set_text(ui_LabelSunset, String(sunset_str).c_str());

    lv_label_set_text_fmt(ui_LabelRain, "%d%%", (rain_1h > 0) ? 100 : cloud_pct);
  }
  else
  {
    Serial.printf("[OWM] Lỗi HTTP: %d\n", httpCode);
  }
  http.end();
}

void updateWeatherAndAQI()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;
  // Bước A: Lấy Lat/Lon từ IP
  http.begin("http://ip-api.com/json/");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);

    current_lat = doc["lat"];
    current_lon = doc["lon"];
    String city = doc["city"];

    Serial.printf("[Weather] Vị trí: %s (%.2f, %.2f)\n", city.c_str(), current_lat, current_lon);

    // Sau khi có tọa độ, gọi tiếp 2 hàm lấy dữ liệu
    fetchFullWeatherData(current_lat, current_lon);
    fetchAQIData(current_lat, current_lon);
  }
  http.end();
}

void fetchAQIData(float lat, float lon)
{
  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/air_pollution?lat=" + String(lat) +
               "&lon=" + String(lon) + "&appid=" + OPENWEATHERMAP_API_KEY;

  http.begin(url);
  if (http.GET() == HTTP_CODE_OK)
  {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, http.getString());

    // AQI từ OWM: 1=Tốt, 2=Khá, 3=Trung bình, 4=Kém, 5=Rất kém
    int aqi = doc["list"][0]["main"]["aqi"];

    // Chuyển đổi số AQI sang chữ để hiển thị
    String aqi_text;
    switch (aqi)
    {
    case 1:
      aqi_text = "Good";
      break;
    case 2:
      aqi_text = "Fair";
      break;
    case 3:
      aqi_text = "Moderate";
      break;
    case 4:
      aqi_text = "Poor";
      break;
    case 5:
      aqi_text = "Very Poor";
      break;
    default:
      aqi_text = "N/A";
    }

    if (Firebase.ready())
    {
      Firebase.setInt(fbdo, F("/smart_farm_iot/data/node2/weather_aql"), aqi);
      Firebase.setString(fbdo, F("/smart_farm_iot/data/node2/weather_ads"), aqi_text);
      Serial.println("[Weather Air quality] đã cập nhật chất lượng không khí.");
    }
    lv_label_set_text(ui_LabelAirQuality, aqi_text.c_str());
  }
  http.end();
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("[SYSTEM] Bắt đầu khởi động...");
  Serial.printf("[SYSTEM] Free Heap: %d bytes\n", ESP.getFreeHeap());

  // 1. Khởi tạo display
  Serial.println("[DISPLAY] Khởi tạo TFT...");
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  Serial.println("[DISPLAY] TFT OK!");

  Serial.println("[DISPLAY] Khởi tạo Touch...");
  touch.begin();
  Serial.println("[DISPLAY] Touch OK!");

  // 2. Khởi tạo LVGL
  Serial.println("[LVGL] Khởi tạo LVGL...");
  lv_init();

  // Dùng double buffer để tránh flicker
  lv_disp_draw_buf_init(&draw_buf, buf, buf2, 240 * 40);
  Serial.println("[LVGL] Draw buffer OK!");

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 240;
  disp_drv.ver_res = 320;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  Serial.println("[LVGL] Display driver OK!");

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);
  Serial.println("[LVGL] Input driver OK!");

  // 3. Khởi tạo UI từ SquareLine Studio
  Serial.println("[UI] Khởi tạo UI...");
  Serial.printf("[SYSTEM] Free Heap trước UI: %d bytes\n", ESP.getFreeHeap());

  ui_init();

  Serial.println("[NTP] Cấu hình thời gian NTP...");
  configTime(gmtOffset_sec, 0, "pool.ntp.org", "time.nist.gov", "asia.pool.ntp.org");

  Serial.println("[UI] UI initialized!");
  Serial.printf("[SYSTEM] Free Heap sau UI: %d bytes\n", ESP.getFreeHeap());

  // Vẽ UI lần đầu
  lv_timer_handler();
  Serial.println("[UI] First render OK!");

  // 4. Cấu hình Nút bấm & WiFi
  Serial.println("[WIFI] Cấu hình WiFi...");
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  setupWiFi();

  // 5. Cấu hình Firebase
  Serial.println("[FIREBASE] Cấu hình Firebase...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = "admin@gmail.com";
  auth.user.password = "admins";
  Serial.println("[FIREBASE] Đang kết nối...");
  Firebase.begin(&config, &auth);
  Firebase.setFloatDigits(1);

  Serial.println("========================================");
  Serial.println("[SYSTEM] ✓ Khởi động hoàn tất!");
  Serial.printf("[SYSTEM] Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("========================================\n");

  // Thiết lập Event Callback cho Buttons
  lv_obj_add_flag(ui_Button1, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_add_event_cb(ui_Button1, onButton1Clicked, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_flag(ui_Button2, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_add_event_cb(ui_Button2, onButton2Clicked, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_flag(ui_Button3, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_add_event_cb(ui_Button3, onButton3Clicked, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_flag(ui_Button4, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_add_event_cb(ui_Button4, onButton4Clicked, LV_EVENT_VALUE_CHANGED, NULL);
}

void loop()
{
  // 1. ƯU TIÊN HÀNG ĐẦU: Cập nhật giao diện LVGL
  lv_timer_handler();
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
  // 3. CẬP NHẬT THỜI GIAN
  if (now - lastTimesync > 1000)
  {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
      // Cập nhật Giờ
      lv_label_set_text_fmt(ui_LabelTimeScreen1, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      lv_label_set_text_fmt(ui_LabelTimeScreen2, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

      // Cập nhật Ngày & Vị trí
      // lastDatesync khởi tạo là 0 nên sẽ chạy ngay khi có WiFi
      if (lastDatesync == 0 || now - lastDatesync > 3600000)
      {
        // Hiển thị Ngày
        const char *month_str = month_names[timeinfo.tm_mon];
        lv_label_set_text_fmt(ui_LabelDate, "%s %02d, %04d", month_str, timeinfo.tm_mday, timeinfo.tm_year + 1900);

        // Hiển thị Vị trí
        if (WiFi.status() == WL_CONNECTED)
        {
          String loc = getLocation();
          lv_label_set_text(ui_LabelLocation, loc.c_str());
          lastDatesync = now;
        }
      }
    }
    lastTimesync = now;
  }
  // 4. CẬP NHẬT CÁC CHỈ SỐ HỆ THỐNG TRÊN UI
  if (now - lastStatsUpdate > 2000)
  {
    updateSystemUI();
    syncRelaysFromFirebase();
    lastStatsUpdate = now;
  }
  // 5. ĐẨY DỮ LIỆU LÊN FIREBASE
  if (now - lastFirebaseUpdate > 10000)
  {
    if (WiFi.status() == WL_CONNECTED && Firebase.ready())
    {
      updateFirebase();
    }
    lastFirebaseUpdate = now;
  }
  // 6. LẤY DỮ LIỆU THỜI TIẾT MỖI 30 PHÚT
  if (now - lastWeatherUpdate > weatherInterval || lastWeatherUpdate == 0)
  {
    updateWeatherAndAQI();
    lastWeatherUpdate = now;
  }
  delay(5);
  yield();
}