#include <Adafruit_SHT31.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BH1750.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>

// ==================== PIN DEFINITIONS ====================
#define A1Pin 34   // GPIO34 - Analog input (ADC1) for soil moisture
#define RL1 12     // GPIO12 - Relay 1
#define RL2 14     // GPIO14 - Relay 2
#define RL3 27     // GPIO27 - Relay 3
#define RL4 26     // GPIO26 - Relay 4
#define BUZZ 4     // GPIO4  - Buzzer (passive)
#define LED 25     // GPIO25 - WiFi status LED
#define BOOT_BTN 0 // GPIO0  - Boot button for SmartConfig

// ==================== MQTT CONFIGURATION ====================
const char *mqtt_broker = "1a4c0875d95541aca8fc573b043ab11c.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char *mqtt_username = "10minute";
const char *mqtt_password = "10Minute";
String module_define_id = "K009";
String mqtt_clientid = module_define_id;
String module_id = module_define_id;

// MQTT Topics
const char *topic_base = "farmos";

// ==================== GLOBAL VARIABLES ====================
Preferences prefs;
WiFiClientSecure espClient;
PubSubClient client(espClient);

bool relay_state[4] = {false, false, false, false};
const int relay_pins[4] = {RL1, RL2, RL3, RL4};

const unsigned long debounce_delay = 50;

int moisture_value = 0;
unsigned long last_read_time = 0;
//
const unsigned long read_interval =
    5000; // THỜI GIAN CẬP NHẬT GIÁ TRỊ LÊN MQTT (5 giây)

enum WiFiState {
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_SMARTCONFIG,
  WIFI_DISABLED
};
WiFiState wifi_state = WIFI_DISABLED;
unsigned long wifi_last_attempt = 0;
const unsigned long wifi_retry_interval = 5000;
unsigned long smartconfig_start_time = 0;
const unsigned long smartconfig_timeout = 30000;
String saved_ssid;
String saved_password;
bool smartconfig_active = false;

Adafruit_SHT31 sht31 = Adafruit_SHT31();
BH1750 lightMeter;

bool boot_button_state = HIGH;
bool boot_button_last_state = HIGH;
unsigned long boot_button_last_debounce = 0;
unsigned long boot_button_press_time = 0;
unsigned long boot_button_last_tap_time = 0;
int boot_button_tap_count = 0;
const unsigned long boot_button_long_press = 10000;
const unsigned long boot_button_tap_timeout = 2000;
const int boot_button_exit_taps = 4;

unsigned long led_blink_time = 0;
bool led_state = false;
const unsigned long led_interval = 500;

int buzzer_queue_beeps = 0;
unsigned long buzzer_beep_duration = 0;
unsigned long buzzer_gap_duration = 0;
unsigned long buzzer_next_action_time = 0;
bool buzzer_is_on = false;
unsigned long smartconfig_buzzer_time = 0;

unsigned long mqtt_last_attempt = 0;
const unsigned long mqtt_retry_interval = 5000;

// ==================== FUNCTION DECLARATIONS ====================
void load_wifi_credentials();
void save_wifi_credentials(const char *ssid, const char *pass);
void start_wifi_connection();
void start_smartconfig();
void stop_smartconfig();
void update_boot_button();
void check_wifi();
void mqtt_connect();
void mqtt_callback(char *topic, byte *payload, unsigned int length);
int read_moisture();
void control_relay(int relay_index, bool state);
String modulePrefix();
String topicRelayControl(int relay_index);
String topicRelayStatus(int relay_index);
String topicSensors();
String topicModuleId();
String topicWifiStatus();
void trigger_short_beep();
void trigger_long_beep();
void trigger_double_beep();
void update_buzzer();
void update_led_status();
void publish_selected_data();
void publish_relay_status();

String modulePrefix() {
  return String(topic_base) + "/" + module_id;
}

String topicRelayControl(int relay_index) {
  return modulePrefix() + "/relay" + String(relay_index + 1) + "/control";
}

String topicRelayStatus(int relay_index) {
  return modulePrefix() + "/relay" + String(relay_index + 1) + "/status";
}

String topicSensors() { return "farmos/sensors"; }

String topicModuleId() { return modulePrefix() + "/status/module_id"; }

String topicWifiStatus() { return modulePrefix() + "/status/wifi"; }

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  prefs.begin("wifi", false);

  // ADC pin: configure ADC resolution and attenuation for GPIO32
  analogReadResolution(12);
  analogSetPinAttenuation(A1Pin, ADC_11db);
  Serial.println("ADC initialized on GPIO32");
  for (int i = 0; i < 4; i++) {
    pinMode(relay_pins[i], OUTPUT);
    digitalWrite(relay_pins[i], LOW);
  }

  pinMode(BUZZ, OUTPUT);
  digitalWrite(BUZZ, LOW);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  pinMode(BOOT_BTN, INPUT_PULLUP);

  // Always initialize I2C and sensors
  Wire.begin();
  if (!sht31.begin(0x44)) {
    Serial.println("Warning: SHT30 not found");
  }
  if (!lightMeter.begin()) {
    Serial.println("Warning: BH1750 not found");
  }

  espClient.setInsecure();
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(mqtt_callback);

  load_wifi_credentials();
  if (saved_ssid.length() && saved_password.length()) {
    start_wifi_connection();
  } else {
    wifi_state = WIFI_DISABLED;
    smartconfig_active = false;
    Serial.println("No saved WiFi credentials; operating offline until "
                   "SmartConfig is started.");
  }
  update_buzzer();

  trigger_short_beep();
}

// ==================== LOOP ====================
void loop() {
  check_wifi();
  mqtt_connect();
  client.loop();
  update_led_status();
  update_buzzer();
  update_boot_button();

  unsigned long now = millis();
  if (millis() % 500 < 50) {
    moisture_value = read_moisture();
  }

  if (now - last_read_time >= read_interval) {
    int percentage = read_moisture();
    float temp = sht31.readTemperature();
    float hum = sht31.readHumidity();
    float lux = lightMeter.readLightLevel();
    Serial.printf(
        "moisture: %d%% (ADC=%d), temp: %.2f C, hum: %.2f %%, lux: %.2f lx\n",
        percentage, moisture_value, temp, hum, lux);
    publish_selected_data();
    last_read_time = now;
  }
  if (millis() % 500 < 100) {

    float temp = sht31.readTemperature();
    float hum = sht31.readHumidity();
    float lux = lightMeter.readLightLevel();
    float light_percentage = (lux / 65535.0) * 100.0;
    light_percentage = constrain(light_percentage, 0, 100);
    Serial.printf("moisture: %d%%, temp: %.2f C, hum: %.2f %%, lux: %.2f lx, "
                  "light_percentage: %.2f%%\n",
                  moisture_value, temp, hum, lux, light_percentage);
  }
}

// ==================== WiFi FUNCTIONS ====================
void load_wifi_credentials() {
  if (prefs.isKey("ssid") && prefs.isKey("pass")) {
    saved_ssid = prefs.getString("ssid", "");
    saved_password = prefs.getString("pass", "");
    if (saved_ssid.length()) {
      Serial.println("Loaded WiFi credentials from NVM");
    }
  }
}

void save_wifi_credentials(const char *ssid, const char *pass) {
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  saved_ssid = ssid;
  saved_password = pass;
  Serial.println("Saved WiFi credentials to NVM");
}

void start_wifi_connection() {
  Serial.printf("Connecting to saved WiFi '%s'...\n", saved_ssid.c_str());
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
  wifi_last_attempt = millis();
  wifi_state = WIFI_CONNECTING;
  smartconfig_active = false;
}

void start_smartconfig() {
  Serial.println("Starting SmartConfig...");
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_STA);
  WiFi.beginSmartConfig();
  smartconfig_start_time = millis();
  wifi_state = WIFI_SMARTCONFIG;
  smartconfig_active = true;
}

void stop_smartconfig() {
  if (smartconfig_active) {
    Serial.println("Stopping SmartConfig and staying offline.");
    WiFi.stopSmartConfig();
  }
  smartconfig_active = false;
  wifi_state = WIFI_DISABLED;
  boot_button_tap_count = 0;
  boot_button_last_tap_time = 0;
}

void update_boot_button() {
  unsigned long now = millis();
  bool reading = digitalRead(BOOT_BTN);

  if (reading != boot_button_last_state) {
    boot_button_last_debounce = now;
    boot_button_last_state = reading;
  }

  if ((now - boot_button_last_debounce) > debounce_delay) {
    if (reading != boot_button_state) {
      boot_button_state = reading;

      if (boot_button_state == LOW) {
        boot_button_press_time = now;
      } else {
        unsigned long press_duration = now - boot_button_press_time;

        if (press_duration >= boot_button_long_press) {
          start_smartconfig();
          boot_button_tap_count = 0;
          boot_button_last_tap_time = 0;
        } else if (wifi_state == WIFI_SMARTCONFIG) {
          if (boot_button_last_tap_time == 0 ||
              now - boot_button_last_tap_time <= boot_button_tap_timeout) {
            boot_button_tap_count++;
          } else {
            boot_button_tap_count = 1;
          }

          boot_button_last_tap_time = now;
          Serial.printf("Boot button tap %d/%d\n", boot_button_tap_count,
                        boot_button_exit_taps);

          if (boot_button_tap_count >= boot_button_exit_taps) {
            stop_smartconfig();
          }
        }
      }
    }
  }
}

void check_wifi() {
  unsigned long now = millis();
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    if (wifi_state != WIFI_CONNECTED) {
      wifi_state = WIFI_CONNECTED;
      Serial.println("WiFi connected");
      if (smartconfig_active) {
        String ssid = WiFi.SSID();
        String pass = WiFi.psk();
        if (ssid.length() && pass.length()) {
          save_wifi_credentials(ssid.c_str(), pass.c_str());
        }
        trigger_double_beep();
        smartconfig_active = false;
      }
    }
    return;
  }

  if (wifi_state == WIFI_DISABLED) {
    return;
  }

  if (wifi_state == WIFI_CONNECTED) {
    wifi_state = WIFI_CONNECTING;
    wifi_last_attempt = now;
  }

  if (wifi_state == WIFI_CONNECTING) {
    if (now - wifi_last_attempt >= wifi_retry_interval) {
      wifi_last_attempt = now;
      if (saved_ssid.length() && saved_password.length()) {
        Serial.println("Retrying saved WiFi credentials...");
        WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
      } else {
        start_smartconfig();
      }
    }
  } else if (wifi_state == WIFI_SMARTCONFIG) {
    if (WiFi.smartConfigDone()) {
      Serial.println("SmartConfig completed, waiting for connection...");
      String ssid = WiFi.SSID();
      String pass = WiFi.psk();
      if (ssid.length() && pass.length()) {
        save_wifi_credentials(ssid.c_str(), pass.c_str());
      }
      wifi_state = WIFI_CONNECTING;
      wifi_last_attempt = now;
    } else if (now - smartconfig_start_time >= smartconfig_timeout) {
      Serial.println("SmartConfig timeout, exiting...");
      trigger_long_beep();
      stop_smartconfig();
    }
  }
}

// ==================== MQTT FUNCTIONS ====================
void mqtt_connect() {
  unsigned long now = millis();
  if (client.connected()) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (now - mqtt_last_attempt < mqtt_retry_interval) {
    return;
  }

  mqtt_last_attempt = now;
  Serial.println("Attempting MQTT connection...");

  if (client.connect(mqtt_clientid.c_str(), mqtt_username, mqtt_password)) {
    Serial.println("Connected to MQTT!");

    client.subscribe(topicRelayControl(0).c_str());
    client.subscribe(topicRelayControl(1).c_str());
    client.subscribe(topicRelayControl(2).c_str());
    client.subscribe(topicRelayControl(3).c_str());

    client.publish(topicWifiStatus().c_str(), "online", true);
    publish_relay_status();
  } else {
    Serial.printf("MQTT connect failed, state=%d\n", client.state());
  }
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  // Copy payload into a trimmed, upper-case string for robust comparison
  char rawMsg[length + 1];
  strncpy(rawMsg, (char *)payload, length);
  rawMsg[length] = '\0';
  String msg = String(rawMsg);
  msg.trim();
  String msgUpper = msg;
  msgUpper.toUpperCase();

  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(" -> '");
  Serial.print(msg);
  Serial.println("'");

  bool valueTrue = (msgUpper == "ON" || msgUpper == "1");
  bool valueFalse = (msgUpper == "OFF" || msgUpper == "0");

  // Only act when the topic matches a relay control topic; determine which
  // relay
  if (strcmp(topic, topicRelayControl(0).c_str()) == 0) {
    if (valueTrue || valueFalse)
      control_relay(0, valueTrue);
  } else if (strcmp(topic, topicRelayControl(1).c_str()) == 0) {
    if (valueTrue || valueFalse)
      control_relay(1, valueTrue);
  } else if (strcmp(topic, topicRelayControl(2).c_str()) == 0) {
    if (valueTrue || valueFalse)
      control_relay(2, valueTrue);
  } else if (strcmp(topic, topicRelayControl(3).c_str()) == 0) {
    if (valueTrue || valueFalse)
      control_relay(3, valueTrue);
  }
}

// ==================== RELAY FUNCTIONS ====================
void control_relay(int relay_index, bool state) {
  if (relay_index < 0 || relay_index > 3)
    return;

  if (relay_state[relay_index] != state) {
    relay_state[relay_index] = state;
    trigger_short_beep();
  }

  digitalWrite(relay_pins[relay_index], state ? HIGH : LOW);

  Serial.print("Relay ");
  Serial.print(relay_index + 1);
  Serial.print(" set to ");
  Serial.println(state ? "ON" : "OFF");

  if (client.connected()) {
    client.publish(topicRelayStatus(relay_index).c_str(), state ? "ON" : "OFF",
                   true);
  }
}

// ==================== SENSOR FUNCTIONS ====================
int read_moisture() {
  moisture_value = analogRead(A1Pin);
  if (moisture_value <= 0) {
    Serial.println("Warning: ADC value is 0. Check sensor wiring, A1Pin "
                   "connection, and module mode.");
  }

  // Convert ADC reading to percentage based on expected wet/dry range.
  // Typical soil sensors: 0 = fully wet, 4095 = fully dry.
  int percentage = map(moisture_value, 4095, 0, 0, 100);
  percentage = constrain(percentage, 0, 100);
  return percentage;
}

void publish_selected_data() {
  if (!client.connected()) {
    return;
  }

  client.publish(topicModuleId().c_str(), module_id.c_str(), true);

  // Read moisture raw and convert to percentage float (0.0 to 100.0)
  moisture_value = analogRead(A1Pin);
  float moisture_pct = (float)(4095 - moisture_value) * 100.0 / 4095.0;
  moisture_pct = constrain(moisture_pct, 0.0, 100.0);

  float temperature = sht31.readTemperature();
  float humidity = sht31.readHumidity();
  float light = lightMeter.readLightLevel();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read SHT30 sensor");
    temperature = 0.0;
    humidity = 0.0;
  }
  if (isnan(light)) {
    Serial.println("Failed to read BH1750 sensor");
    light = 0.0;
  }

  // Convert lux to percentage (0-65535 lux -> 0-100%)
  float light_percentage = (light / 65535.0) * 100.0;
  light_percentage = constrain(light_percentage, 0.0, 100.0);

  StaticJsonDocument<256> doc;
  doc["module_id"] = module_id;
  doc["moisture_percentage"] = String(moisture_pct, 1);
  doc["temperature"] = String(temperature, 1);
  doc["humidity"] = String(humidity, 1);
  doc["lux"] = String(light_percentage, 1);

  char buffer[256];
  serializeJson(doc, buffer);
  client.publish(topicSensors().c_str(), buffer);
}

// ==================== BUZZER FUNCTIONS ====================
void trigger_short_beep() {
  buzzer_queue_beeps = 1;
  buzzer_beep_duration = 100;
  buzzer_gap_duration = 100;
  buzzer_next_action_time = millis();
}

void trigger_long_beep() {
  buzzer_queue_beeps = 1;
  buzzer_beep_duration = 1000;
  buzzer_gap_duration = 100;
  buzzer_next_action_time = millis();
}

void trigger_double_beep() {
  buzzer_queue_beeps = 2;
  buzzer_beep_duration = 150;
  buzzer_gap_duration = 150;
  buzzer_next_action_time = millis();
}

void update_buzzer() {
  unsigned long now = millis();

  // Handle active beep duration
  if (buzzer_is_on) {
    if (now >= buzzer_next_action_time) {
      noTone(BUZZ);
      buzzer_is_on = false;
      if (buzzer_queue_beeps > 0) {
        buzzer_next_action_time = now + buzzer_gap_duration;
      }
    }
  } else {
    // If we have queued beeps and gap has passed
    if (buzzer_queue_beeps > 0 && now >= buzzer_next_action_time) {
      tone(BUZZ, 2000);
      buzzer_is_on = true;
      buzzer_next_action_time = now + buzzer_beep_duration;
      buzzer_queue_beeps--;
    }
    // Periodic smartconfig chirps (twice per second -> every 500ms)
    else if (wifi_state == WIFI_SMARTCONFIG) {
      if (now - smartconfig_buzzer_time >= 500) {
        if (buzzer_queue_beeps == 0 && !buzzer_is_on) {
          tone(BUZZ, 2000);
          buzzer_is_on = true;
          buzzer_next_action_time = now + 50; // short 50ms chirp
        }
        smartconfig_buzzer_time = now;
      }
    }
  }
}

// ==================== LED FUNCTIONS ====================
void update_led_status() {
  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED, HIGH);
  } else if (wifi_state == WIFI_SMARTCONFIG) {
    // Blink 3 times per second (period ~333ms, half-period ~166ms)
    if (now - led_blink_time >= 166) {
      led_state = !led_state;
      digitalWrite(LED, led_state ? HIGH : LOW);
      led_blink_time = now;
    }
  } else {
    // Wifi disconnected: Blink at 1Hz (period 1000ms, half-period 500ms)
    if (now - led_blink_time >= 500) {
      led_state = !led_state;
      digitalWrite(LED, led_state ? HIGH : LOW);
      led_blink_time = now;
    }
  }
}

// ==================== STATUS FUNCTIONS ====================
void publish_relay_status() {
  if (client.connected()) {
    for (int i = 0; i < 4; i++) {
      client.publish(topicRelayStatus(i).c_str(), relay_state[i] ? "ON" : "OFF",
                     true);
    }
  }
}