#include "config.h"
#include "webportal.h"
// --- GLOBAL INSTANCES & DATA ---
LoRaPacket packetToSend;
LoRaPacketRec receivedPacket;
MasterStation master;
SlaveStation slaves[total_Slave];
Node nodes[MAX_NODES];
int nodeCount = 0;
int nextNodeId = 1;
// --- Schedule setup ---
const int frameX[] = {18, 42, 18, 42, 72, 96, 72, 96};
const int frameY[] = {18, 18, 38, 38, 18, 18, 38, 38};
// --- Time setting ---
const int frameXTime[] = {18, 42, 66, 16, 37, 58};
const int frameYTime[] = {18, 18, 18, 38, 38, 38};
const int frameWTime[] = {20, 20, 22, 18, 18, 36};
const int frameW = 20;
const int frameH = 18;
const int frameR = 3;
char daysOfTheWeek[7][12] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
// Web/Hardware
WebServer server(80);
RTC_DS1307 rtc;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
Preferences preferences;
// Local Relay Control
const int relayPins[4] = {RL1_PIN, RL2_PIN, RL3_PIN, RL4_PIN};
bool relayState[4] = {false, false, false, false};
// Menu/UI State
bool inMenu = false;
int cursorPos = 0;
int menuLevel = 0;
int menuCursor = 0;
bool screenFlip = false;
bool editingTime = false;
int submenuSelected = -1;
bool datascreenflag = true;
const char *mainMenu[] = {"Time Setting", "Node Setting", "Internet Setting", "Schedule setup"};
const int mainMenuCount = 4;
unsigned long lastActivity = 0;
int setHour = 0, setMinute = 0, setSecond = 0;
int setDay = 1, setMonth = 1, setYear = 2024;
int onHour1, onMinute1, offHour1, offMinute1;
int onHour2, onMinute2, offHour2, offMinute2;
const unsigned long standbyTimeout = 15000UL;
unsigned long selectPressStart = 0;
bool selectPressed = false;
int nodeControlCursor = 0;
bool nodeControlAdjustingPWM = false;
unsigned long nodeControlPressStart = 0;
// Buzzer State
unsigned long buzzerStart = 0;
unsigned long buzzerDuration = 0;
bool buzzerActive = false;
int buzzerFrequency = 1000;
// Task Handles
TaskHandle_t displayTaskHandle;
TaskHandle_t loraTaskHandle;
TaskHandle_t ioTaskHandle;
// Other Globals
const char *ssid = "CT32";
const char *password = "ct32iot2025";
String ip_addr;
String time_now_rtc, day_now_rtc;
int countIdSlave = 0;
// icon bit
const unsigned char image_weather_temperature_bits[] = {0x38, 0x00, 0x44, 0x40, 0xd4, 0xa0, 0x54, 0x40, 0xd4, 0x1c, 0x54, 0x06, 0xd4, 0x02, 0x54, 0x02, 0x54, 0x06, 0x92, 0x1c, 0x39, 0x01, 0x75, 0x01, 0x7d, 0x01, 0x39, 0x01, 0x82, 0x00, 0x7c, 0x00};
const unsigned char icon_Thermal[] = {0xc6, 0x01, 0x29, 0x02, 0x29, 0x00, 0x26, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x02, 0xc0, 0x01};
const unsigned char icon_connected[] = {0x7c, 0x00, 0x82, 0x00, 0x01, 0x01, 0x38, 0x00, 0x44, 0x00, 0x00, 0x00, 0x10, 0x00};
const unsigned char icon_disconnect[] = {0x00, 0x01, 0xfc, 0x00, 0xc2, 0x00, 0x21, 0x01, 0x38, 0x00, 0x4c, 0x00, 0x04, 0x00, 0x12, 0x00};
// --- GLOBAL NTP ---
unsigned long lastNtpSyncMillis = 0;
const unsigned long NTP_SYNC_INTERVAL = 7200000;
#define TIME_SYNC_TASK_PRIORITY 1
// --- UTILITY FUNCTIONS ---
extern "C" uint8_t temprature_sens_read();
float readInternalTemp()
{
    return (temprature_sens_read() - 32) / 1.8;
}

void timeSyncTask(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(10000));
    if (WiFi.status() == WL_CONNECTED)
    {
        syncNTPTime();
    }

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(NTP_SYNC_INTERVAL));

        if (WiFi.status() == WL_CONNECTED)
        {
            syncNTPTime();
        }
        else
        {
            Serial.println("Wi-Fi not connected. Skipping scheduled NTP sync.");
        }
    }
}

void syncNTPTime()
{
    Serial.println("Starting NTP time synchronization...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5000))
    {
        Serial.println("Failed to obtain NTP time.");
        return;
    }
    DateTime now(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    rtc.adjust(now);
    lastNtpSyncMillis = millis();
}

void checkTimeSync()
{
    if (lastNtpSyncMillis == 0 || millis() - lastNtpSyncMillis >= NTP_SYNC_INTERVAL)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            syncNTPTime();
        }
        else
        {
            Serial.println("Wi-Fi not connected, skipping NTP sync.");
        }
    }
}

void initBuzzer()
{
    pinMode(BUZ_PIN, OUTPUT);
    digitalWrite(BUZ_PIN, LOW);
}

void buzzerBeep(int frequency, unsigned long duration)
{
    buzzerFrequency = frequency;
    buzzerDuration = duration;
    buzzerStart = millis();
    buzzerActive = true;
    ledcAttachPin(BUZ_PIN, BUZ_CHANNEL);
    ledcSetup(BUZ_CHANNEL, frequency, 8);
    ledcWriteTone(BUZ_CHANNEL, frequency);
}

void buzzerUpdate()
{
    if (buzzerActive && millis() - buzzerStart >= buzzerDuration)
    {
        ledcWriteTone(BUZ_CHANNEL, 0);
        ledcDetachPin(BUZ_PIN);
        buzzerActive = false;
    }
}

void setRelayLocal(int idx, bool on)
{
    if (idx < 0 || idx > 3)
        return;
    relayState[idx] = on;
    digitalWrite(relayPins[idx], on ? LOW : HIGH);
    saveLocalRelayState();
    buzzerBeep(2000, 80);
}

void saveToEEPROM(int slaveId, bool isOn, int sliderValue)
{
    if (slaveId < 0 || slaveId >= total_Slave)
        return;
    int addr = EEPROM_ADDR_SLAVES + (slaveId * 8);
    EEPROM.write(addr, isOn);
    EEPROM.write(addr + 4, sliderValue);
    EEPROM.commit();
}

void saveNodeConfig()
{
    EEPROM.write(EEPROM_ADDR_NODECOUNT, nodeCount);
    for (int i = 0; i < nodeCount; i++)
    {
        NodeData data;
        data.id = nodes[i].id;
        strncpy(data.label, nodes[i].label.c_str(), MAX_LABEL_LENGTH);
        data.label[MAX_LABEL_LENGTH - 1] = '\0';
        int addr = EEPROM_ADDR_NODES + i * sizeof(NodeData);
        EEPROM.put(addr, data);
    }
    EEPROM.commit();
}

void saveLocalRelayState()
{
    for (int i = 0; i < 4; i++)
    {
        EEPROM.write(EEPROM_ADDR_RELAYS + i, relayState[i]);
    }
    EEPROM.commit();
}

void saveMasterSchedule()
{
    EEPROM.write(EEPROM_ADDR_ONHOUR1, onHour1);
    EEPROM.write(EEPROM_ADDR_ONMIN1, onMinute1);
    EEPROM.write(EEPROM_ADDR_OFFHOUR1, offHour1);
    EEPROM.write(EEPROM_ADDR_OFFMIN1, offMinute1);
    EEPROM.write(EEPROM_ADDR_ONHOUR2, onHour2);
    EEPROM.write(EEPROM_ADDR_ONMIN2, onMinute2);
    EEPROM.write(EEPROM_ADDR_OFFHOUR2, offHour2);
    EEPROM.write(EEPROM_ADDR_OFFMIN2, offMinute2);
    EEPROM.commit();

    char onTime1Str[6], offTime1Str[6], onTime2Str[6], offTime2Str[6];
    sprintf(onTime1Str, "%02d:%02d", onHour1, onMinute1);
    sprintf(onTime2Str, "%02d:%02d", onHour2, onMinute2);
    sprintf(offTime1Str, "%02d:%02d", offHour1, offMinute1);
    sprintf(offTime2Str, "%02d:%02d", offHour2, offMinute2);
    master.onTime1 = String(onTime1Str);
    master.onTime2 = String(onTime2Str);
    master.offTime1 = String(offTime1Str);
    master.offTime2 = String(offTime2Str);
}

void loadMasterSchedule()
{
    onHour1 = EEPROM.read(EEPROM_ADDR_ONHOUR1);
    onMinute1 = EEPROM.read(EEPROM_ADDR_ONMIN1);
    offHour1 = EEPROM.read(EEPROM_ADDR_OFFHOUR1);
    offMinute1 = EEPROM.read(EEPROM_ADDR_OFFMIN1);

    onHour2 = EEPROM.read(EEPROM_ADDR_ONHOUR2);
    onMinute2 = EEPROM.read(EEPROM_ADDR_ONMIN2);
    offHour2 = EEPROM.read(EEPROM_ADDR_OFFHOUR2);
    offMinute2 = EEPROM.read(EEPROM_ADDR_OFFMIN2);

    char onTime1Str[6], offTime1Str[6], onTime2Str[6], offTime2Str[6];
    sprintf(onTime1Str, "%02d:%02d", onHour1, onMinute1);
    sprintf(onTime2Str, "%02d:%02d", onHour2, onMinute2);
    sprintf(offTime1Str, "%02d:%02d", offHour1, offMinute1);
    sprintf(offTime2Str, "%02d:%02d", offHour2, offMinute2);
    master.onTime1 = String(onTime1Str);
    master.onTime2 = String(onTime2Str);
    master.offTime1 = String(offTime1Str);
    master.offTime2 = String(offTime2Str);
}

void loadLocalRelayState()
{
    for (int i = 0; i < 4; i++)
    {
        relayState[i] = EEPROM.read(EEPROM_ADDR_RELAYS + i);
        setRelayLocal(i, relayState[i]);
    }
}

void loadNodeConfig()
{
    uint8_t count = EEPROM.read(EEPROM_ADDR_NODECOUNT);
    if (count == 0xFF || count > MAX_NODES)
    {
        nodeCount = 0;
        EEPROM.write(EEPROM_ADDR_NODECOUNT, (uint8_t)0);
        EEPROM.commit();
        return;
    }
    nodeCount = (int)count;
    for (int i = 0; i < nodeCount; i++)
    {
        NodeData data;
        int addr = EEPROM_ADDR_NODES + i * sizeof(NodeData);
        EEPROM.get(addr, data);
        nodes[i].id = data.id;
        nodes[i].label = String(data.label);
        nodes[i].relay = false;
        nodes[i].online = false;
    }
}

void loadSlavesFromEEPROM()
{
    for (int i = 0; i < total_Slave; i++)
    {
        int addr = EEPROM_ADDR_SLAVES + (i * 8);
        slaves[i].isOn = EEPROM.read(addr);
        slaves[i].sliderValue = EEPROM.read(addr + 4);
    }
}

void sendLora(int ID, int stateLed, int valvePwm)
{
    saveToEEPROM(ID - 1, slaves[ID - 1].isOn, slaves[ID - 1].sliderValue);
    packetToSend.id = ID;
    packetToSend.data1 = stateLed;
    packetToSend.data2 = valvePwm;
    LoRa.beginPacket();
    LoRa.write((uint8_t *)&packetToSend, sizeof(packetToSend));
    LoRa.endPacket();
}

void print_data()
{
    Serial.println("=========================================");
    for (int i = 0; i < total_Slave; i++)
    {
        Serial.print("id: ");
        Serial.print(slaves[i].id);
        Serial.print("      temperature: ");
        Serial.print(slaves[i].temperature, 1);
        Serial.print("      slider: ");
        Serial.print(slaves[i].sliderValue);
        Serial.print("   connection: ");
        Serial.print(slaves[i].isConnected);
        Serial.print("   relay: ");
        Serial.print(slaves[i].isOn);
        Serial.print("   time: ");
        Serial.println(slaves[i].timeString);
    }
    Serial.println("--- Gateway Sensor Values (MOCK) ---");
    Serial.printf("V1: %.2f V, V2: %.2f V, V3: %.2f V\n", master.sensorValues[3], master.sensorValues[4], master.sensorValues[5]);
    Serial.printf("I1: %.2f A, I2: %.2f A, I3: %.2f A\n", master.sensorValues[0], master.sensorValues[1], master.sensorValues[2]);
    Serial.println("=========================================");
}

void handleGetTime()
{
    DateTime now = rtc.now();
    String json = "{";
    json += "\"year\":" + String(now.year()) + ",";
    json += "\"month\":" + String(now.month()) + ",";
    json += "\"day\":" + String(now.day()) + ",";
    json += "\"hour\":" + String(now.hour()) + ",";
    json += "\"minute\":" + String(now.minute()) + ",";
    json += "\"second\":" + String(now.second());
    json += "}";
    server.send(200, "application/json", json);
}

void handleApiStatus()
{
    String json = "{\"ok\":true, \"master\":{\"onTime\":\"" + master.onTime1 + "\", \"offTime\":\"" + master.offTime1 + "\"}, \"localRelays\":[";
    for (int i = 0; i < 4; i++)
    {
        if (i > 0)
            json += ",";
        json += (relayState[i] ? "true" : "false");
    }
    json += "], \"wifiConnected\":";
    json += (WiFi.status() == WL_CONNECTED ? "true" : "false");
    json += ", \"nodes\":[";

    int renderedNodes = 0;
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodes[i].id > 0 && nodes[i].id <= total_Slave)
        {
            int slaveIdx = nodes[i].id - 1;
            if (renderedNodes > 0)
                json += ",";

            json += "{";
            json += "\"id\":" + String(nodes[i].id) + ",";
            json += "\"label\":\"" + nodes[i].label + "\",";

            json += "\"relay\":";
            json += (slaves[slaveIdx].isOn ? "true" : "false");
            json += ",";

            json += "\"online\":";
            json += (slaves[slaveIdx].isConnected ? "true" : "false");
            json += ",";

            json += "\"temperature\":" + String(slaves[slaveIdx].temperature, 1) + ",";
            json += "\"sliderValue\":" + String(slaves[slaveIdx].sliderValue) + ",";
            json += "\"timeString\":\"" + String(slaves[slaveIdx].timeString) + "\"";
            json += "}";
            renderedNodes++;
        }
    }

    json += "], \"firmwareVersion\":\"" + String(FIRMWARE_VERSION_STRING) + "\"";
    json += "}";

    server.send(200, "application/json", json);
}

void handleApiToggleNode()
{
    if (!server.hasArg("node") || !server.hasArg("state"))
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Missing parameters\"}");
        return;
    }
    int id = server.arg("node").toInt();
    String stateStr = server.arg("state");
    if (id < 1 || id > total_Slave)
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Invalid node ID\"}");
        return;
    }
    int idx = id - 1;
    bool newState = (stateStr == "on");
    slaves[idx].isOn = newState;
    slaves[idx].sliderValue = newState ? 80 : 0;
    int pwmvalue = map(slaves[idx].sliderValue, 0, 100, 0, 255);
    sendLora(id, slaves[idx].isOn, pwmvalue);
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleApiMasterToggleAll()
{
    if (!server.hasArg("state"))
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Missing state parameter\"}");
        return;
    }
    bool newState = (server.arg("state") == "on");
    for (int i = 0; i < total_Slave; i++)
    {
        slaves[i].isOn = newState;
        slaves[i].sliderValue = newState ? 80 : 0;
        sendLora(slaves[i].id, slaves[i].isOn, slaves[i].sliderValue);
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleApiLocalToggleAll()
{
    bool newState;
    if (server.hasArg("state"))
    {
        String s = server.arg("state");
        s.toLowerCase();
        if (s == "on")
            newState = true;
        else if (s == "off")
            newState = false;
        else
        {
            server.send(400, "application/json", "{\"ok\":false, \"err\":\"Invalid state value\"}");
            return;
        }
    }
    else
    {
        newState = !relayState[0];
    }

    for (int i = 0; i < 4; i++)
    {
        relayState[i] = newState;
        digitalWrite(relayPins[i], newState ? LOW : HIGH);
    }
    saveLocalRelayState();
    buzzerBeep(1500, 80);
    String resp = String("{\"ok\":true, \"state\":\"") + (newState ? "on" : "off") + String("\"}");
    server.send(200, "application/json", resp);
}

void handleApiNodePwm()
{
    String idStr;
    if (server.hasArg("node"))
        idStr = server.arg("node");
    else if (server.hasArg("id"))
        idStr = server.arg("id");
    else
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Missing node parameter\"}");
        return;
    }

    if (!server.hasArg("value"))
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Missing value parameter\"}");
        return;
    }

    int id = idStr.toInt();
    int value = server.arg("value").toInt();

    if (id < 1 || id > total_Slave || value < 0 || value > 100)
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Invalid node ID or PWM value\"}");
        return;
    }

    int idx = id - 1;
    slaves[idx].sliderValue = value;
    slaves[idx].isOn = (value > 0);
    int pwm255 = map(value, 0, 100, 0, 255);

    sendLora(id, slaves[idx].isOn, pwm255);
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleApiAddNode()
{
    if (!server.hasArg("id") || !server.hasArg("label"))
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Missing parameters\"}");
        return;
    }
    int id = server.arg("id").toInt();
    String label = server.arg("label");
    if (id < 1 || id > total_Slave)
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Node ID must be between 1 and %TOTAL_SLAVE%\"}");
        return;
    }
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodes[i].id == id)
        {
            server.send(400, "application/json", "{\"ok\":false, \"err\":\"Node ID already exists\"}");
            return;
        }
    }
    if (nodeCount >= MAX_NODES)
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Max nodes limit reached\"}");
        return;
    }
    nodes[nodeCount].id = id;
    nodes[nodeCount].label = label;
    nodes[nodeCount].relay = slaves[id - 1].isOn;
    nodes[nodeCount].online = slaves[id - 1].isConnected;
    nodeCount++;
    server.send(200, "application/json", "{\"ok\":true}");
    saveNodeConfig();
}

void handleApiEditNode()
{
    if (!server.hasArg("node") || !server.hasArg("name"))
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Missing parameters\"}");
        return;
    }
    int id = server.arg("node").toInt();
    String newLabel = server.arg("name");
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodes[i].id == id)
        {
            nodes[i].label = newLabel;
            saveNodeConfig();
            server.send(200, "application/json", "{\"ok\":true}");
            return;
        }
    }
    server.send(400, "application/json", "{\"ok\":false, \"err\":\"Node not found\"}");
}

void handleApiRemoveNode()
{
    if (!server.hasArg("node"))
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Missing node parameter\"}");
        return;
    }
    int id = server.arg("node").toInt();
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodes[i].id == id)
        {
            for (int j = i; j < nodeCount - 1; j++)
            {
                nodes[j] = nodes[j + 1];
            }
            nodeCount--;
            saveNodeConfig();
            server.send(200, "application/json", "{\"ok\":true}");
            return;
        }
    }
    server.send(400, "application/json", "{\"ok\":false, \"err\":\"Node not found\"}");
}

void setupWebServer()
{
    server.on("/", HTTP_GET, []()
              {
        String page = html;
        page.replace("%TOTAL_SLAVE%", String(total_Slave));
        server.send(200, "text/html", page); });

    server.on("/getTime", HTTP_GET, handleGetTime);
    server.on("/api/status", HTTP_GET, handleApiStatus);
    server.on("/api/node/pwm", HTTP_GET, handleApiNodePwm);
    server.on("/api/node/add", HTTP_POST, handleApiAddNode);
    server.on("/api/node/edit", HTTP_POST, handleApiEditNode);
    server.on("/api/node/toggle", HTTP_GET, handleApiToggleNode);
    server.on("/api/node/remove", HTTP_POST, handleApiRemoveNode);
    server.on("/api/wifi/config", HTTP_POST, handleApiWifiConfig);
    server.on("/api/local/toggle", HTTP_GET, handleApiLocalToggle);
    server.on("/api/local/toggleAll", HTTP_GET, handleApiLocalToggleAll);
    server.on("/api/master/toggleAll", HTTP_GET, handleApiMasterToggleAll);
    server.begin();
}

void handleApiLocalToggle()
{
    if (!server.hasArg("relay") || !server.hasArg("state"))
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Missing parameters\"}");
        return;
    }
    int relay = server.arg("relay").toInt();
    bool state = (server.arg("state") == "on");

    if (relay >= 1 && relay <= 4)
    {
        setRelayLocal(relay - 1, state);
        saveLocalRelayState();
        server.send(200, "application/json", "{\"ok\":true}");
    }
    else
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Invalid relay ID\"}");
    }
}

void handleApiWifiConfig()
{
    if (!server.hasArg("ssid"))
    {
        server.send(400, "application/json", "{\"ok\":false, \"err\":\"Missing SSID\"}");
        return;
    }
    String newSsid = server.arg("ssid");
    String newPassword = server.arg("password");
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", newSsid);
    preferences.putString("password", newPassword);
    preferences.end();
    server.send(200, "application/json", "{\"ok\":true, \"msg\":\"Rebooting to apply settings. Please connect to the new network and access the IP address displayed on the screen.\"}\n");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    ESP.restart();
}

void syncNodesWithSlaves()
{
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodes[i].id > 0 && nodes[i].id <= total_Slave)
        {
            int slaveIdx = nodes[i].id - 1;
            nodes[i].relay = slaves[slaveIdx].isOn;
            nodes[i].online = slaves[slaveIdx].isConnected;
        }
    }
}
// --- SETUP ---
void setup()
{
    Serial.begin(115250);
    EEPROM.begin(EEPROM_SIZE);
    Serial.println(PROJECT_NAME);
    Serial.print("Firmware Version: ");
    Serial.println(FIRMWARE_VERSION_STRING);
    Serial.printf("Version (Major.Minor.Patch): %d.%d.%d\n",
                  FIRMWARE_VERSION_MAJOR,
                  FIRMWARE_VERSION_MINOR,
                  FIRMWARE_VERSION_PATCH);
    initBuzzer();
    loadNodeConfig();
    loadSlavesFromEEPROM();
    loadLocalRelayState();
    loadMasterSchedule();
    for (int i = 0; i < 4; i++)
    {
        pinMode(relayPins[i], OUTPUT);
        setRelayLocal(i, true);
    }
    if (!rtc.begin())
    {
        for (int i = 0; i < 3; i++)
        {
            buzzerBeep(2000, 100);
            vTaskDelay(150 / portTICK_PERIOD_MS);
        }
    }
    u8g2.begin();
    String storedSsid = "";
    String storedPassword = "";
    IPAddress local_IP(192, 168, 10, 1);
    IPAddress gateway(192, 168, 10, 1);
    IPAddress subnet(255, 255, 255, 0);
    preferences.begin("wifi-config", true);
    storedSsid = preferences.getString("ssid", "");
    storedPassword = preferences.getString("password", "");
    preferences.end();

    if (storedSsid.length() > 0)
    {
        Serial.printf("Connecting to Wi-Fi: %s\n", storedSsid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(storedSsid.c_str(), storedPassword.c_str());
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 40)
        {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            ip_addr = WiFi.localIP().toString();
            Serial.printf("\nConnected to Wi-Fi! Access IP: %s\n", ip_addr.c_str());
            syncNTPTime();
        }
        else
        {
            Serial.println("\nFailed to connect to configured Wi-Fi. Falling back to Access Point Mode.");
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAPConfig(local_IP, gateway, subnet);
            WiFi.softAP(ssid, password);
            ip_addr = WiFi.softAPIP().toString();
            Serial.printf("Failed to connect, using AP Mode at %s\n", ip_addr.c_str());
        }
    }
    else
    {
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(local_IP, gateway, subnet);
        WiFi.softAP(ssid, password);
        ip_addr = WiFi.softAPIP().toString();
        Serial.printf("ESP32 Access Point Started at %s (No Wi-Fi Config)\n", ip_addr.c_str());
    }
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ssid, password);
    ip_addr = WiFi.softAPIP().toString();
    Serial.printf("ESP32 Access Point Started at %s\n", ip_addr.c_str());
    LoRa.setPins(ss, rst, dio0);
    if (!LoRa.begin(433E6))
    {
        for (int i = 0; i < 5; i++)
        {
            buzzerBeep(2000, 100);
            vTaskDelay(150 / portTICK_PERIOD_MS);
            Serial.println("LoRa init failed. Check your connections.");
        }   
    }
    LoRa.setSyncWord(0xF3);
    for (int i = 0; i < 6; i++)
    {
        master.sensorValues[i] = random(100, 250) / 10.0;
    }
    for (int i = 0; i < total_Slave; i++)
    {
        slaves[i] = {i + 1, random(20, 35) / 1.0f, 0, 50, false, false};
    }
    syncNodesWithSlaves();
    setupWebServer();
    xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, &displayTaskHandle, 0);
    xTaskCreatePinnedToCore(loraTask, "LoRaTask", 4096, NULL, 1, &loraTaskHandle, 1);
    xTaskCreatePinnedToCore(ioTask, "IOTask", 8192, NULL, 1, &ioTaskHandle, 1);
    xTaskCreate(timeSyncTask, "TimeSyncTask", 3072, NULL, TIME_SYNC_TASK_PRIORITY, NULL);
}
// --- LOOP ---
void loop() {}
// ---------------- TASK --------------------------
void standby_screen()
{
    DateTime now = rtc.now();
    char tbuf[16];
    char dbuf[16];
    sprintf(dbuf, "%02d/%02d/%04d", now.day(), now.month(), now.year());
    sprintf(tbuf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    u8g2.setFont(u8g2_font_ncenB18_te);
    u8g2.setCursor(18, 38);
    u8g2.print(tbuf);
    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.setCursor(34, 50);
    u8g2.print(dbuf);
}

void data_screen()
{
    u8g2.setCursor(52, 33);
    u8g2.drawXBM(36, 17, 16, 16, image_weather_temperature_bits);
    u8g2.setFont(u8g2_font_profont12_tr);
    u8g2.printf("%.1f", readInternalTemp());
    u8g2.drawXBM(80, 25, 10, 8, icon_Thermal);
    DateTime now = rtc.now();
    char tbuf[16];
    sprintf(tbuf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    u8g2.setFont(u8g2_font_12x6LED_mn);
    u8g2.setCursor(33, 12);
    u8g2.print(tbuf);
    u8g2.drawLine(0, 15, 127, 15);
    u8g2.drawLine(34, 15, 34, 63);
    u8g2.drawLine(92, 15, 92, 63);
    u8g2.drawLine(34, 40, 0, 40);
    u8g2.drawLine(92, 40, 127, 40);

    u8g2.setFont(u8g2_font_6x13_tr);
    u8g2.setColorIndex(digitalRead(RL1_PIN) == HIGH ? 0 : 1);
    u8g2.drawStr(5, 34, "RL1");
    u8g2.setColorIndex(digitalRead(RL2_PIN) == HIGH ? 0 : 1);
    u8g2.drawStr(5, 58, "RL2");
    u8g2.setColorIndex(digitalRead(RL3_PIN) == HIGH ? 0 : 1);
    u8g2.drawStr(99, 34, "RL3");
    u8g2.setColorIndex(digitalRead(RL4_PIN) == HIGH ? 0 : 1);
    u8g2.drawStr(99, 58, "RL4");
    u8g2.setColorIndex(1);
    if (WiFi.status())
    {
        u8g2.drawXBM(115, 4, 9, 7, icon_connected);
    }
    else
    {
        u8g2.drawXBM(115, 4, 9, 7, icon_disconnect);
    }
    static unsigned long lastSwitch = 0;
    static int displayIndex = 0;
    int validCount = 0;
    for (int i = 0; i < nodeCount; i++)
        if (nodes[i].id > 0)
            validCount++;
    if (validCount > 0)
    {
        if (millis() - lastSwitch >= 3000)
        {
            displayIndex = (displayIndex + 1) % validCount;
            lastSwitch = millis();
        }
        int found = -1;
        int cnt = 0;
        for (int i = 0; i < nodeCount; i++)
        {
            if (nodes[i].id <= 0)
                continue;
            if (cnt == displayIndex)
            {
                found = i;
                break;
            }
            cnt++;
        }
        if (found >= 0)
        {
            int nid = nodes[found].id;
            int slider = 0;
            bool s_conn = false;
            int si = nid - 1;
            if (nid > 0 && nid <= total_Slave)
            {
                slider = slaves[si].sliderValue;
                s_conn = slaves[si].isConnected;
            }
            char buf[32];
            char buf1[32];
            snprintf(buf, sizeof(buf), "ID:%d", nid);
            snprintf(buf1, sizeof(buf1), "Tag: %s", nodes[found].label.c_str());
            u8g2.setFont(u8g2_font_5x7_mf);
            u8g2.setCursor(36, 43);
            u8g2.printf(buf);
            u8g2.setCursor(36, 51);
            u8g2.printf(buf1);
            char buf2[64];
            snprintf(buf2, sizeof(buf2), "%d %s %s", slider, nodes[found].relay ? "ON" : "OFF", s_conn ? "C" : "D");
            u8g2.setCursor(36, 60);
            u8g2.print(buf2);
        }
    }
}

void displayTask(void *pvParameters)
{
    (void)pvParameters;
    u8g2.begin();

    u8g2.setFont(u8g2_font_unifont_t_symbols);
    u8g2.setCursor(0, 60);
    u8g2.print("Ver: ");
    u8g2.print(FIRMWARE_VERSION_STRING);
    while (1)
    {
        u8g2.clearBuffer();
        if (!inMenu)
        {
            if (millis() - lastActivity >= standbyTimeout)
            {
                datascreenflag = false;
                standby_screen();
            }
            else
            {
                datascreenflag = true;
                data_screen();
            }
        }
        else
        {
            if (menuLevel == 1)
            {
                u8g2.setFont(u8g2_font_ncenB08_tr);
                for (int i = 0; i < mainMenuCount; i++)
                {
                    if (i == menuCursor)
                        u8g2.drawRFrame(0, i * 15, 128, 15, 3);
                    u8g2.setCursor(6, (i + 1) * 15 - 3);
                    u8g2.print(mainMenu[i]);
                }
            }
            else if (menuLevel == 2)
            {
                if (submenuSelected == 0) // Time Setup
                {
                    u8g2.setFont(u8g2_font_ncenB08_tr);
                    u8g2.drawStr(6, 14, "Time Setup");
                    char buf[20];
                    char bufdate[20];
                    sprintf(bufdate, "%02d/%02d/%04d", setDay, setMonth, setYear);
                    sprintf(buf, "%02d:%02d:%02d", setHour, setMinute, setSecond);
                    u8g2.setFont(u8g2_font_ncenB12_tr);
                    u8g2.setCursor(18, 32);
                    u8g2.print(buf);
                    u8g2.setFont(u8g2_font_ncenB10_tr);
                    u8g2.setCursor(18, 52);
                    u8g2.print(bufdate);
                    if (editingTime)
                    {
                        u8g2.drawRFrame(
                            frameXTime[cursorPos],
                            frameYTime[cursorPos],
                            frameWTime[cursorPos],
                            frameH,
                            frameR);
                        u8g2.setFont(u8g2_font_6x10_tr);
                    }
                }
                else if (submenuSelected == 1) // Node Control
                {
                    u8g2.setFont(u8g2_font_ncenB08_tr);
                    u8g2.drawStr(6, 10, "Node Control");
                    int validCount = 0;
                    for (int i = 0; i < nodeCount; i++)
                    {
                        if (nodes[i].id > 0)
                            validCount++;
                    }
                    if (validCount == 0)
                    {
                        u8g2.setFont(u8g2_font_6x10_tr);
                        u8g2.setCursor(6, 30);
                        u8g2.print("No nodes configured");
                        u8g2.setCursor(6, 42);
                        u8g2.print("Add via web portal");
                    }
                    else
                    {
                        u8g2.setFont(u8g2_font_6x10_tr);
                        int startIdx = nodeControlCursor;
                        if (startIdx > validCount - 3 && validCount >= 3)
                        {
                            startIdx = validCount - 3;
                        }
                        int displayedCount = 0;
                        int actualNodeIdx = 0;
                        for (int i = 0; i < nodeCount && displayedCount < 3; i++)
                        {
                            if (nodes[i].id <= 0)
                                continue;
                            if (actualNodeIdx >= startIdx)
                            {
                                int yPos = 22 + (displayedCount * 14);
                                if (actualNodeIdx == nodeControlCursor)
                                {
                                    u8g2.drawRFrame(2, yPos - 10, 124, 12, 2);
                                }
                                int slaveIdx = nodes[i].id - 1;
                                bool isOn = (slaveIdx >= 0 && slaveIdx < total_Slave) ? slaves[slaveIdx].isOn : false;
                                bool isConnected = (slaveIdx >= 0 && slaveIdx < total_Slave) ? slaves[slaveIdx].isConnected : false;
                                int pwmVal = (slaveIdx >= 0 && slaveIdx < total_Slave) ? slaves[slaveIdx].sliderValue : 0;
                                u8g2.setCursor(4, yPos);
                                char displayBuf[32];
                                char shortLabel[10];
                                strncpy(shortLabel, nodes[i].label.c_str(), 6);
                                shortLabel[6] = '\0';
                                snprintf(displayBuf, sizeof(displayBuf), "%d.%s %s %s %d%%",
                                         nodes[i].id,
                                         shortLabel,
                                         isOn ? "ON" : "OFF",
                                         isConnected ? "C" : "D",
                                         pwmVal);
                                u8g2.print(displayBuf);
                                if (actualNodeIdx == nodeControlCursor && nodeControlAdjustingPWM)
                                {
                                    u8g2.drawStr(118, yPos, "*");
                                }

                                displayedCount++;
                            }
                            actualNodeIdx++;
                        }
                        if (nodeControlCursor > 0)
                        {
                            u8g2.drawStr(118, 20, "^");
                        }
                        if (nodeControlCursor < validCount - 1)
                        {
                            u8g2.drawStr(118, 60, "v");
                        }
                        u8g2.setFont(u8g2_font_5x7_tr);
                        char countStr[16];
                        snprintf(countStr, sizeof(countStr), "%d/%d", nodeControlCursor + 1, validCount);
                        u8g2.drawStr(100, 10, countStr);
                    }
                }
                else if (submenuSelected == 2) // Wi-Fi Status
                {
                    u8g2.setFont(u8g2_font_ncenB08_tr);
                    u8g2.drawStr(6, 14, "Internet");
                    u8g2.setFont(u8g2_font_6x10_tr);

                    if (WiFi.status() == WL_CONNECTED)
                    {
                        u8g2.setCursor(6, 26);
                        u8g2.printf("SSID: %s", WiFi.SSID().c_str());
                        u8g2.setCursor(6, 38);
                        u8g2.printf("IP: %s", WiFi.localIP().toString().c_str());
                        u8g2.setCursor(6, 50);
                        u8g2.print("Status: Connected (STA)");
                    }
                    else if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
                    {
                        preferences.begin("wifi-config", true);
                        String storedSsid = preferences.getString("ssid", "ESP32_AP");
                        preferences.end();

                        u8g2.setCursor(6, 26);
                        u8g2.printf("SSID: %s", storedSsid.c_str());
                        u8g2.setCursor(6, 38);
                        u8g2.printf("IP: %s", WiFi.softAPIP().toString().c_str());
                        u8g2.setCursor(6, 50);
                        u8g2.print("Status: AP Mode");
                    }
                    else
                    {
                        u8g2.setCursor(6, 26);
                        u8g2.print("Status: Disconnected");
                        u8g2.setCursor(6, 38);
                        u8g2.print("No Network.");
                    }
                }
                else if (submenuSelected == 3) // Schedules Setup
                {
                    u8g2.setFont(u8g2_font_ncenB08_tr);
                    u8g2.drawStr(6, 14, "Schedules Setup");
                    char bufon1[20];
                    char bufon2[20];
                    char bufoff2[20];
                    char bufoff1[20];
                    sprintf(bufon1, "%02d:%02d", onHour1, onMinute1);
                    sprintf(bufon2, "%02d:%02d", onHour2, onMinute2);
                    sprintf(bufoff1, "%02d:%02d", offHour1, offMinute1);
                    sprintf(bufoff2, "%02d:%02d", offHour2, offMinute2);
                    u8g2.setFont(u8g2_font_ncenB12_tr);
                    u8g2.setCursor(18, 32);
                    u8g2.print(bufon1);
                    u8g2.setCursor(72, 32);
                    u8g2.print(bufon2);
                    u8g2.setCursor(18, 52);
                    u8g2.print(bufoff1);
                    u8g2.setCursor(72, 52);
                    u8g2.print(bufoff2);
                    if (editingTime && cursorPos >= 0 && cursorPos < 8)
                    {
                        u8g2.drawRFrame(
                            frameX[cursorPos],
                            frameY[cursorPos],
                            frameW,
                            frameH,
                            frameR);
                        u8g2.setFont(u8g2_font_6x10_tr);
                    }
                }
            }
        }
        u8g2.sendBuffer();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void loraTask(void *pvParameters)
{
    (void)pvParameters;
    unsigned long lastSend = 0;
    while (1)
    {
        server.handleClient();
        int packetSize = LoRa.parsePacket();
        if (packetSize == sizeof(LoRaPacketRec))
        {
            LoRa.readBytes((uint8_t *)&receivedPacket, sizeof(receivedPacket));
            if (receivedPacket.id >= 1 && receivedPacket.id <= total_Slave)
            {
                slaves[receivedPacket.id - 1].temperature = receivedPacket.data1;

                int totalSeconds = receivedPacket.data2;
                int hours = totalSeconds / 3600;
                int minutes = (totalSeconds % 3600) / 60;
                int seconds = totalSeconds % 60;

                char timeBuffer[9];
                sprintf(timeBuffer, "%02d:%02d:%02d", hours, minutes, seconds);
                slaves[receivedPacket.id - 1].timeString = String(timeBuffer);
                slaves[receivedPacket.id - 1].isConnected = true;
                int nid = receivedPacket.id;
                for (int i = 0; i < nodeCount; i++)
                {
                    if (nodes[i].id == nid)
                    {
                        nodes[i].relay = slaves[nid - 1].isOn;
                        nodes[i].online = true;
                        break;
                    }
                }
            }
        }
        if ((unsigned long)(millis() - lastSend) > 500)
        {
            countIdSlave++;
            if (countIdSlave > total_Slave)
            {
                countIdSlave = 1;
            }
            sendLora(countIdSlave, slaves[countIdSlave - 1].isOn, slaves[countIdSlave - 1].sliderValue);
            DateTime now = rtc.now();
            day_now_rtc = String(daysOfTheWeek[now.dayOfTheWeek()]) + " - " + String(now.day()) + "/" + String(now.month()) + "/" + String(now.year());
            char timeBuffer_rtc[6];
            sprintf(timeBuffer_rtc, "%02d:%02d", now.hour(), now.minute());
            time_now_rtc = String(timeBuffer_rtc);

            lastSend = millis();
        }

        if (random(0, 100) < 5)
        {
            for (int i = 0; i < 6; i++)
            {
                master.sensorValues[i] += (random(-5, 6) / 100.0f);
                if (master.sensorValues[i] < 10.0f)
                    master.sensorValues[i] = 10.0f;
                if (master.sensorValues[i] > 30.0f)
                    master.sensorValues[i] = 30.0f;
            }
            print_data();
        }
        DateTime now = rtc.now();
        char current_time_buffer[6];
        sprintf(current_time_buffer, "%02d:%02d", now.hour(), now.minute());
        String current_time = String(current_time_buffer);

        if (master.onTime1.length() > 0 && master.onTime1 == current_time || master.onTime2.length() > 0 && master.onTime2 == current_time)
        {
            Serial.println("Scheduled ON Triggered!");
            master.onTime1 = "";
            master.onTime2 = "";
            for (int i = 0; i < total_Slave; i++)
            {
                slaves[i].isOn = true;
                slaves[i].sliderValue = 80;
                sendLora(slaves[i].id, slaves[i].isOn, slaves[i].sliderValue);
            }
        }

        if (master.offTime1.length() > 0 && master.offTime1 == current_time || master.offTime2.length() > 0 && master.offTime2 == current_time)
        {
            Serial.println("Scheduled OFF Triggered!");
            master.offTime1 = "";
            master.offTime2 = "";
            for (int i = 0; i < total_Slave; i++)
            {
                slaves[i].isOn = false;
                slaves[i].sliderValue = 0;
                sendLora(slaves[i].id, slaves[i].isOn, slaves[i].sliderValue);
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void ioTask(void *pvParameters)
{
    (void)pvParameters;
    pinMode(BT_UP_PIN, INPUT_PULLUP);
    pinMode(BT_SEL_PIN, INPUT_PULLUP);
    pinMode(BT_DWN_PIN, INPUT_PULLUP);
    pinMode(BT_BCK_PIN, INPUT_PULLUP);
    pinMode(BT_BOOT, INPUT_PULLUP);
    lastActivity = millis();
    unsigned long pressStart[5] = {0, 0, 0, 0, 0};
    bool pressed[5] = {false, false, false, false, false};
    while (1)
    {
        buzzerUpdate();
        int states[5] = {
            digitalRead(BT_UP_PIN),
            digitalRead(BT_SEL_PIN),
            digitalRead(BT_DWN_PIN),
            digitalRead(BT_BCK_PIN),
            digitalRead(BT_BOOT)};
        if (!inMenu)
        {
            for (int b = 0; b < 5; b++)
            {
                if (states[b] == LOW)
                {
                    if (!pressed[b])
                    {
                        pressed[b] = true;
                        lastActivity = millis();
                        pressStart[b] = millis();
                    }
                    else
                    {
                        unsigned long held = millis() - pressStart[b];
                        if (b == 4 && held >= 2000)
                        {
                            inMenu = true;
                            menuLevel = 1;
                            menuCursor = 0;
                            submenuSelected = -1;
                            buzzerBeep(1500, 120);
                            pressed[b] = false;
                        }
                        else if ((b == 0 || b == 1 || b == 2 || b == 3) && (held >= 300))
                        {
                            setRelayLocal(b, !relayState[b]);
                            saveLocalRelayState();
                            buzzerBeep(1500, 120);
                            pressed[b] = false;
                        }
                    }
                }
                else
                {
                    pressed[b] = false;
                }
            }
        }
        if (inMenu)
        {
            if (menuLevel == 2 && submenuSelected == 0) // Time Setup
            {
                if (digitalRead(BT_UP_PIN) == LOW)
                {
                    buzzerBeep(1200, 60);
                    if (cursorPos == 0)
                        setHour = (setHour + 1) % 24;
                    else if (cursorPos == 1)
                        setMinute = (setMinute + 1) % 60;
                    else if (cursorPos == 2)
                        setSecond = (setSecond + 1) % 60;
                    else if (cursorPos == 3)
                        setDay = (setDay % 31) + 1;
                    else if (cursorPos == 4)
                        setMonth = (setMonth % 12) + 1;
                    else if (cursorPos == 5)
                        setYear = (setYear < 2099) ? (setYear + 1) : 2023;
                    while (digitalRead(BT_UP_PIN) == LOW)
                        vTaskDelay(10);
                }
                if (digitalRead(BT_DWN_PIN) == LOW)
                {
                    buzzerBeep(1200, 60);
                    if (cursorPos == 0)
                        setHour = (setHour + 23) % 24;
                    else if (cursorPos == 1)
                        setMinute = (setMinute + 59) % 60;
                    else if (cursorPos == 2)
                        setSecond = (setSecond + 59) % 60;
                    else if (cursorPos == 3)
                        setDay = (setDay == 1) ? 31 : (setDay - 1);
                    else if (cursorPos == 4)
                        setMonth = (setMonth == 1) ? 12 : (setMonth - 1);
                    else if (cursorPos == 5)
                        setYear = (setYear > 2023) ? (setYear - 1) : 2099;
                    while (digitalRead(BT_DWN_PIN) == LOW)
                        vTaskDelay(10);
                }
                if (digitalRead(BT_SEL_PIN) == LOW)
                {
                    buzzerBeep(1000, 60);
                    cursorPos = (cursorPos + 1) % 6;
                    editingTime = true;
                    while (digitalRead(BT_SEL_PIN) == LOW)
                        vTaskDelay(10);
                }
                if (digitalRead(BT_BCK_PIN) == LOW)
                {
                    buzzerBeep(900, 80);
                    DateTime now = rtc.now();
                    rtc.adjust(DateTime(setYear, setMonth, setDay, setHour, setMinute, setSecond));
                    menuLevel = 1;
                    submenuSelected = -1;
                    editingTime = false;
                    cursorPos = 0;
                    while (digitalRead(BT_BCK_PIN) == LOW)
                        vTaskDelay(10);
                }
            }
            else if (menuLevel == 2 && submenuSelected == 1) // Node Control
            {
                int validCount = 0;
                for (int i = 0; i < nodeCount; i++)
                {
                    if (nodes[i].id > 0)
                        validCount++;
                }

                if (validCount > 0)
                {
                    int findNodeIdx = -1;
                    int actualIdx = 0;
                    for (int i = 0; i < nodeCount; i++)
                    {
                        if (nodes[i].id <= 0)
                            continue;
                        if (actualIdx == nodeControlCursor)
                        {
                            findNodeIdx = i;
                            break;
                        }
                        actualIdx++;
                    }
                    int nodeId = (findNodeIdx >= 0) ? nodes[findNodeIdx].id : -1;
                    int slaveIdx = nodeId - 1;

                    // ===== UP BUTTON =====
                    if (digitalRead(BT_UP_PIN) == LOW)
                    {
                        buzzerBeep(1000, 50);
                        nodeControlCursor = (nodeControlCursor - 1 + validCount) % validCount;
                        while (digitalRead(BT_UP_PIN) == LOW)
                            vTaskDelay(10);
                    }
                    // ===== DOWN BUTTON =====
                    if (digitalRead(BT_DWN_PIN) == LOW)
                    {
                        buzzerBeep(1000, 50);
                        nodeControlCursor = (nodeControlCursor + 1) % validCount;
                        while (digitalRead(BT_DWN_PIN) == LOW)
                            vTaskDelay(10);
                    }
                    // ===== SELECT BUTTON =====
                    if (digitalRead(BT_SEL_PIN) == LOW)
                    {
                        if (!selectPressed)
                        {
                            selectPressed = true;
                            selectPressStart = millis();
                        }
                        unsigned long pressDuration = millis() - selectPressStart;
                        if (pressDuration >= 500 && slaveIdx >= 0 && slaveIdx < total_Slave)
                        {
                            int currentPWM = slaves[slaveIdx].sliderValue;
                            if (currentPWM >= 80 || slaves[slaveIdx].isOn)
                            {
                                slaves[slaveIdx].sliderValue -= 5;
                                if (slaves[slaveIdx].sliderValue < 0)
                                    slaves[slaveIdx].sliderValue = 0;
                            }
                            else
                            {
                                slaves[slaveIdx].sliderValue += 5;
                                if (slaves[slaveIdx].sliderValue > 100)
                                    slaves[slaveIdx].sliderValue = 100;
                            }
                            slaves[slaveIdx].isOn = (slaves[slaveIdx].sliderValue > 0);
                            int pwmValue = map(slaves[slaveIdx].sliderValue, 0, 100, 0, 255);
                            sendLora(nodeId, slaves[slaveIdx].isOn, pwmValue);
                            buzzerBeep(1300, 30);
                            vTaskDelay(100 / portTICK_PERIOD_MS);
                        }
                    }
                    else
                    {
                        if (selectPressed)
                        {
                            unsigned long pressDuration = millis() - selectPressStart;
                            if (pressDuration < 500 && slaveIdx >= 0 && slaveIdx < total_Slave)
                            {
                                buzzerBeep(1200, 80);
                                slaves[slaveIdx].isOn = !slaves[slaveIdx].isOn;
                                slaves[slaveIdx].sliderValue = slaves[slaveIdx].isOn ? 80 : 0;
                                int pwmValue = map(slaves[slaveIdx].sliderValue, 0, 100, 0, 255);
                                sendLora(nodeId, slaves[slaveIdx].isOn, pwmValue);
                            }
                            selectPressed = false;
                            selectPressStart = 0;
                        }
                    }
                }
                // ===== BACK BUTTON =====
                if (digitalRead(BT_BCK_PIN) == LOW)
                {
                    buzzerBeep(900, 80);
                    menuLevel = 1;
                    submenuSelected = -1;
                    nodeControlCursor = 0;
                    selectPressed = false;
                    selectPressStart = 0;
                    while (digitalRead(BT_BCK_PIN) == LOW)
                        vTaskDelay(10);
                }
            }
            else if (menuLevel == 2 && submenuSelected == 3) // Schedules Setup
            {
                if (digitalRead(BT_UP_PIN) == LOW)
                {
                    buzzerBeep(1200, 60);
                    if (cursorPos == 0)
                        onHour1 = (onHour1 + 1) % 24;
                    else if (cursorPos == 1)
                        onMinute1 = (onMinute1 + 1) % 60;
                    else if (cursorPos == 2)
                        offHour1 = (offHour1 + 1) % 24;
                    else if (cursorPos == 3)
                        offMinute1 = (offMinute1 + 1) % 60;
                    if (cursorPos == 4)
                        onHour2 = (onHour2 + 1) % 24;
                    else if (cursorPos == 5)
                        onMinute2 = (onMinute2 + 1) % 60;
                    else if (cursorPos == 6)
                        offHour2 = (offHour2 + 1) % 24;
                    else if (cursorPos == 7)
                        offMinute2 = (offMinute2 + 1) % 60;
                    while (digitalRead(BT_UP_PIN) == LOW)
                        vTaskDelay(10);
                }
                if (digitalRead(BT_DWN_PIN) == LOW)
                {
                    buzzerBeep(1200, 60);
                    if (cursorPos == 0)
                        onHour1 = (onHour1 + 23) % 24;
                    else if (cursorPos == 1)
                        onMinute1 = (onMinute1 + 59) % 60;
                    else if (cursorPos == 2)
                        offHour1 = (offHour1 + 23) % 24;
                    else if (cursorPos == 3)
                        offMinute1 = (offMinute1 + 59) % 60;
                    if (cursorPos == 4)
                        onHour2 = (onHour2 + 23) % 24;
                    else if (cursorPos == 5)
                        onMinute2 = (onMinute2 + 59) % 60;
                    else if (cursorPos == 6)
                        offHour2 = (offHour2 + 23) % 24;
                    else if (cursorPos == 7)
                        offMinute2 = (offMinute2 + 59) % 60;
                    while (digitalRead(BT_DWN_PIN) == LOW)
                        vTaskDelay(10);
                }
                if (digitalRead(BT_SEL_PIN) == LOW)
                {
                    buzzerBeep(1000, 60);
                    cursorPos = (cursorPos + 1) % 8;
                    editingTime = true;
                    while (digitalRead(BT_SEL_PIN) == LOW)
                        vTaskDelay(10);
                }
                if (digitalRead(BT_BCK_PIN) == LOW)
                {
                    buzzerBeep(900, 80);
                    saveMasterSchedule();
                    EEPROM.write(EEPROM_ADDR_ONHOUR1, onHour1);
                    EEPROM.write(EEPROM_ADDR_ONMIN1, onMinute1);
                    EEPROM.write(EEPROM_ADDR_OFFHOUR1, offHour1);
                    EEPROM.write(EEPROM_ADDR_OFFMIN1, offMinute1);

                    EEPROM.write(EEPROM_ADDR_ONHOUR2, onHour2);
                    EEPROM.write(EEPROM_ADDR_ONMIN2, onMinute2);
                    EEPROM.write(EEPROM_ADDR_OFFHOUR2, offHour2);
                    EEPROM.write(EEPROM_ADDR_OFFMIN2, offMinute2);
                    EEPROM.commit();
                    menuLevel = 1;
                    submenuSelected = -1;
                    editingTime = false;
                    cursorPos = 0;
                    while (digitalRead(BT_BCK_PIN) == LOW)
                        vTaskDelay(10);
                }
            }
            else
            {
                if (digitalRead(BT_UP_PIN) == LOW)
                {
                    buzzerBeep(1000, 50);
                    menuCursor = (menuCursor - 1 + mainMenuCount) % mainMenuCount;
                    while (digitalRead(BT_UP_PIN) == LOW)
                        vTaskDelay(10);
                }
                if (digitalRead(BT_DWN_PIN) == LOW)
                {
                    buzzerBeep(1000, 50);
                    menuCursor = (menuCursor + 1) % mainMenuCount;
                    while (digitalRead(BT_DWN_PIN) == LOW)
                        vTaskDelay(10);
                }
                if (digitalRead(BT_SEL_PIN) == LOW)
                {
                    buzzerBeep(1000, 80);
                    if (menuLevel == 1)
                    {
                        submenuSelected = menuCursor;
                        menuLevel = 2;
                        menuCursor = 0;
                        if (submenuSelected == 0)
                        {
                            DateTime now = rtc.now();
                            setHour = now.hour();
                            setMinute = now.minute();
                            setSecond = now.second();
                            setDay = now.day();
                            setMonth = now.month();
                            setYear = now.year();
                            editingTime = true;
                        }
                        if (submenuSelected == 3)
                        {
                            onHour1 = EEPROM.read(EEPROM_ADDR_ONHOUR1);
                            onMinute1 = EEPROM.read(EEPROM_ADDR_ONMIN1);
                            offHour1 = EEPROM.read(EEPROM_ADDR_OFFHOUR1);
                            offMinute1 = EEPROM.read(EEPROM_ADDR_OFFMIN1);

                            onHour2 = EEPROM.read(EEPROM_ADDR_ONHOUR2);
                            onMinute2 = EEPROM.read(EEPROM_ADDR_ONMIN2);
                            offHour2 = EEPROM.read(EEPROM_ADDR_OFFHOUR2);
                            offMinute2 = EEPROM.read(EEPROM_ADDR_OFFMIN2);
                            EEPROM.commit();
                            editingTime = true;
                        }
                    }
                    while (digitalRead(BT_SEL_PIN) == LOW)
                        vTaskDelay(10);
                }

                if (digitalRead(BT_BCK_PIN) == LOW)
                {
                    buzzerBeep(1000, 80);
                    if (menuLevel == 2)
                    {
                        menuLevel = 1;
                        submenuSelected = -1;
                    }
                    else
                    {
                        inMenu = false;
                        menuLevel = 0;
                        submenuSelected = -1;
                    }
                    while (digitalRead(BT_BCK_PIN) == LOW)
                        vTaskDelay(10);
                }
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}