#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include <FirebaseESP32.h>
#include <HTTPClient.h>
#include <time.h>
#include "UI/ui.h"
#include "CST820.h"
#include <LovyanGFX.hpp>

// ---------------------- CẤU HÌNH ----------------------
const char *ssid = "NGUYEN SANG TRUOC";
const char *password = "apdkp413271a1";

#define API_KEY "AIzaSyAgVge-VC6S9RocrJQibOWFqY1De7nz3eM"
#define DATABASE_URL "https://smartfarmingiot-78911-default-rtdb.asia-southeast1.firebasedatabase.app"
#define OPENWEATHER_API_KEY "fe3f1acd362a2919de02d9a4ae0cbd40"

// ---------------------- BIẾN TOÀN CỤC ----------------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool node1State = false, node3State = false;
bool relay1_state = false, relay2_state = false, relay3_state = false, relay4_state = false;
float latIP = 0, lonIP = 0;
String cityName = "Loading...";

unsigned long lastWeatherUpdate = 0;
unsigned long lastFirebaseSync = 0;
unsigned long lastUartCheck = 0;

// ---------------------- LGFX & LVGL CONFIG ----------------------
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
static lv_color_t buf[240 * 40];
CST820 touch(21, 22, -1, -1);

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

// ---------------------- HÀM XỬ LÝ DỮ LIỆU & UI ----------------------

void sendCommandToMaster()
{
  JsonDocument doc;
  JsonArray r = doc["cmd_relay"].to<JsonArray>();
  r.add(relay1_state);
  r.add(relay2_state);
  r.add(relay3_state);
  r.add(relay4_state);
  doc["reset_fault"] = false;
  serializeJson(doc, Serial);
  Serial.println();
}

void updateUIWithSensorData(JsonDocument &doc)
{
  node1State = doc["sensor"]["online"];
  node3State = doc["relay"]["online"];

  if (node1State)
  {
    lv_label_set_text_fmt(ui_LabelNode1Temp, "%.1f°C", (float)doc["sensor"]["temp"]);
    lv_label_set_text_fmt(ui_LabelNode1Humid, "%d%%", (int)doc["sensor"]["hum"]);
    lv_label_set_text_fmt(ui_LabelNode1Soil, "%d%%", (int)doc["sensor"]["soil"]);
    lv_arc_set_value(ui_ArcTemp, (int)doc["sensor"]["temp"]);
    lv_arc_set_value(ui_ArcHumid, (int)doc["sensor"]["hum"]);
  }

  if (node3State)
  {
    lv_label_set_text_fmt(ui_LabelNode3Flow, "%.1f L/m", (float)doc["relay"]["flow_rate"]);
    lv_arc_set_value(ui_ArcMCU3, (int)doc["relay"]["mcu_temp"]);
  }
}

// ---------------------- FIREBASE SYNC ----------------------

void syncFirebase()
{
  if (!Firebase.ready())
    return;
  String path = "smart_farm_iot/user_data/user_1/data/";

  // Đọc trạng thái Relay từ App (Cloud -> ESP32)
  if (Firebase.getBool(fbdo, path + "relay1_status"))
    relay1_state = fbdo.boolData();
  if (Firebase.getBool(fbdo, path + "relay2_status"))
    relay2_state = fbdo.boolData();
  if (Firebase.getBool(fbdo, path + "relay3_status"))
    relay3_state = fbdo.boolData();
  if (Firebase.getBool(fbdo, path + "relay4_status"))
    relay4_state = fbdo.boolData();

  // Cập nhật giao diện nút bấm
  relay1_state ? lv_obj_add_state(ui_Button1, LV_STATE_CHECKED) : lv_obj_clear_state(ui_Button1, LV_STATE_CHECKED);
  relay2_state ? lv_obj_add_state(ui_Button2, LV_STATE_CHECKED) : lv_obj_clear_state(ui_Button2, LV_STATE_CHECKED);
  relay3_state ? lv_obj_add_state(ui_Button3, LV_STATE_CHECKED) : lv_obj_clear_state(ui_Button3, LV_STATE_CHECKED);
  relay4_state ? lv_obj_add_state(ui_Button4, LV_STATE_CHECKED) : lv_obj_clear_state(ui_Button4, LV_STATE_CHECKED);
}

// ---------------------- CALLBACK BUTTON ----------------------

void onButtonGeneric(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  bool state = lv_obj_has_state(btn, LV_STATE_CHECKED);
  if (btn == ui_Button1)
    relay1_state = state;
  else if (btn == ui_Button2)
    relay2_state = state;
  else if (btn == ui_Button3)
    relay3_state = state;
  else if (btn == ui_Button4)
    relay4_state = state;

  sendCommandToMaster(); // Gửi ngay lệnh xuống Master khi bấm nút
}

// ---------------------- SETUP ----------------------

void setup()
{
  Serial.begin(115200);
  tft.init();
  touch.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
    delay(500);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, 240 * 40);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 240;
  disp_drv.ver_res = 320;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  ui_init();

  // Đăng ký sự kiện cho các nút
  lv_obj_add_event_cb(ui_Button1, onButtonGeneric, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_Button2, onButtonGeneric, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_Button3, onButtonGeneric, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_Button4, onButtonGeneric, LV_EVENT_VALUE_CHANGED, NULL);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = "admin@gmail.com";
  auth.user.password = "admins";
  Firebase.begin(&config, &auth);

  configTime(7 * 3600, 0, "pool.ntp.org");
}

// ---------------------- LOOP ----------------------

void loop()
{
  lv_timer_handler();
  // 1. Đọc dữ liệu JSON từ UART (Nhận từ Master Node)
  if (Serial.available())
  {
    String line = Serial.readStringUntil('\n');
    JsonDocument doc;
    if (!deserializeJson(doc, line))
    {
      updateUIWithSensorData(doc);
    }
  }
  // 2. Cập nhật thời gian
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate > 1000)
  {
    struct tm ti;
    if (getLocalTime(&ti))
    {
      char buf[10];
      sprintf(buf, "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
      lv_label_set_text(ui_LabelTimeScreen1, buf);
    }
    lastTimeUpdate = millis();
  }
  // 3. Đồng bộ Firebase
  if (millis() - lastFirebaseSync > 2000)
  {
    syncFirebase();
    lastFirebaseSync = millis();
  }
}