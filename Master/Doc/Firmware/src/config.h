#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <LoRa.h>
#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include "RTClib.h"
#include <EEPROM.h>
#include <U8g2lib.h>
#include <WebServer.h>
#include <Preferences.h>
// --- FIRMWARE VERSION ---
#define FIRMWARE_VERSION_MAJOR    1
#define FIRMWARE_VERSION_MINOR    2
#define FIRMWARE_VERSION_PATCH    0 
#define FIRMWARE_VERSION_STRING   "1.2.0" 
#define PROJECT_NAME              "ESP-MASTER-IOT"
// --- HARDWARE PINS ---
#define ss 5
#define rst 17
#define dio0 2
// --- Define Relay pins ---
#define RL1_PIN 12
#define RL2_PIN 14
#define RL3_PIN 27
#define RL4_PIN 26
// --- Define Button pins ---
#define BT_UP_PIN 33
#define BT_SEL_PIN 32
#define BT_DWN_PIN 35
#define BT_BCK_PIN 34
#define BT_BOOT 0
#define BUZ_PIN 4
#define BUZ_CHANNEL 0
// --- display resolution ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define total_Slave 10
#define MAX_NODES 24
// --- EEPROM Configuration ---
#define EEPROM_ADDR_NODECOUNT   0
#define EEPROM_ADDR_RELAYS      1
// T1
#define EEPROM_ADDR_ONHOUR1      5
#define EEPROM_ADDR_ONMIN1       6
#define EEPROM_ADDR_OFFHOUR1     7
#define EEPROM_ADDR_OFFMIN1      8
// T2
#define EEPROM_ADDR_ONHOUR2     9
#define EEPROM_ADDR_ONMIN2      10
#define EEPROM_ADDR_OFFHOUR2    11
#define EEPROM_ADDR_OFFMIN2     12
// Node
#define EEPROM_ADDR_NODES       100
#define EEPROM_ADDR_SLAVES      900
#define MAX_LABEL_LENGTH        32
#define EEPROM_SIZE             1024
// --- NTP CONFIGURATION ---
#define NTP_SERVER              "pool.ntp.org"
#define GMT_OFFSET_SEC          25200  
#define DAYLIGHT_OFFSET_SEC     0
// --- DATA STRUCTURES ---
struct NodeData {
    uint8_t id;
    char label[MAX_LABEL_LENGTH];
};
struct MasterSchedule {
    int onHour;
    int onMinute;
    int offHour;
    int offMinute;
};
struct LoRaPacket {
    int id;
    bool data1;
    int data2;
};
extern LoRaPacket packetToSend;
struct LoRaPacketRec {
    int id;
    float data1;
    unsigned long data2;
};
extern LoRaPacketRec receivedPacket;

struct MasterStation {
    float sensorValues[6];
    String onTime1;
    String onTime2;
    String offTime1;
    String offTime2;
};
extern MasterStation master;
struct SlaveStation {
    int id;
    float temperature;
    int time;
    int sliderValue;
    bool isOn;
    bool isConnected;
    String timeString;
};
extern SlaveStation slaves[total_Slave];
struct Node
{
    int id;
    String label;
    bool relay;
    bool online;
};
extern Node nodes[MAX_NODES];
extern int nodeCount;
extern int nextNodeId;
// --- GLOBAL VARIABLES ---
extern unsigned long lastNtpSyncMillis;
extern const unsigned long NTP_SYNC_INTERVAL;
// --- GLOBAL VARIABLES ---
extern char daysOfTheWeek[7][12];
extern const int relayPins[4];
extern Preferences preferences;
extern WebServer server;
extern RTC_DS1307 rtc;
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern const unsigned char image_weather_temperature_bits[];
extern const unsigned char icon_Thermal[];
extern const unsigned char icon_connected[];
extern const unsigned char icon_disconnect[];
// --- global variable for UI/Menu ---
extern bool inMenu;
extern int cursorPos;
extern int menuLevel;
extern int menuCursor;
extern bool screenFlip;
extern bool editingTime;
extern int submenuSelected;
extern bool datascreenflag;
extern const char* mainMenu[];
extern const int mainMenuCount;
extern unsigned long lastActivity;
extern int setHour, setMinute, setSecond;
extern int setDay, setMonth, setYear;
extern const unsigned long standbyTimeout;
extern bool relayState[4];
// --- FUNCTION PROTOTYPES ---
void setupWebServer();
void loadNodeConfig();
void saveNodeConfig();
void saveLocalRelayState();
void saveMasterSchedule();
void loadMasterSchedule();
void sendLora(int ID, int stateLed, int valvePwm);
void saveToEEPROM(int slaveId, bool isOn, int sliderValue);
void print_data();
// --- Web API Handlers ---
void handleApiStatus();
void handleApiToggleNode();
void handleApiMasterToggleAll();
void handleApiLocalToggleAll();
void handleApiAddNode();
void handleApiEditNode();
void handleApiRemoveNode();
void handleApiNodePwm();
void handleGetTime();
void handleApiLocalToggle();
void handleApiWifiConfig();
// --- Task Handlers ---
void displayTask(void *pvParameters);
void loraTask(void *pvParameters);
void ioTask(void *pvParameters);
// --- UI/Display ---
void standby_screen();
void data_screen();
// --- IO/Utility ---
void buzzerBeep(int frequency, unsigned long duration);
void buzzerUpdate();
void setRelayLocal(int idx, bool on);
float readInternalTemp();
// --- FUNCTION PROTOTYPES---
void syncNTPTime();
void checkTimeSync(); 
void syncNodesWithSlaves();
#endif