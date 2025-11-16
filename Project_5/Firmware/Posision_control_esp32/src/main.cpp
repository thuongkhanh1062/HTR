/*
@Author:
@date: 1711250600
@version: V3.2
   ESP32 motor control with:
   - Encoder, motor driver IN1/IN2 + PWM (ledc)
   - Speed PID (non-negative) and Position PID (bidirectional)
   - Web server (port 80) serves modern UI using Chart.js v2 (CDN)
   - WebSocket server (port 81) pushes realtime JSON (every 150 ms)
   - OLED shows status and PID page (toggle by holding ENTER 2s)
   - Preferences persistence
   - OTA
   Notes:
   - Chart.js v2 used for lightweight realtime chart
   - SPEED PID limited to 0..255 to avoid low-RPM sign flip by default
*/

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <U8g2lib.h>
#include <PID_v1.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>

// ---------------- USER CONFIG ----------------
const char *ssid = "WIFI";
const char *password = "PASSWORD";

// ---------------- IO ----------------
#define ENC_A 19
#define ENC_B 18

#define IN1_PIN 15
#define IN2_PIN 5
#define PWM_PIN 13

#define BUTTON_UP 25    // increase keuy
#define BUTTON_DOWN 33  // decrease key
#define BUTTON_ENTER 32 // run/stop, long-press to change screen
#define BUTTON_MODE 35  // change mode

// #define LIMIT_S 34 // optional

// ---------------- Display ----------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// ---------------- Encoder / mechanics ----------------
volatile long encoderCount = 0;
volatile uint8_t lastAB = 0;
volatile unsigned long lastEncoderMillis = 0;

const int ENCODER_PPR = 44;                                   // pulses per motor rev x4 counting
const float GEAR_RATIO = 45.0;                                // 45 motor rev = 1 output rev
const float COUNTS_PER_OUTPUT_REV = ENCODER_PPR * GEAR_RATIO; // 1980

// ---------------- Modes ----------------
enum Mode
{
  MODE_SPEED,
  MODE_ROUND
};
Mode mode = MODE_SPEED;

// ---------------- Runtime variables ----------------
double currentRPM = 0.0;
double currentRounds = 0.0;
double targetRPM = 60.0;
double targetRounds = 1.0;

bool running = false;

// ---------------- PID (Speed) ----------------
double speedInput = 0, speedOutput = 0, speedSetpoint = 0;
double speedKp = 1.5, speedKi = 0.25, speedKd = 0.05;
PID pidSpeed(&speedInput, &speedOutput, &speedSetpoint, speedKp, speedKi, speedKd, DIRECT);

// ---------------- PID (Position) ----------------
double posInput = 0, posOutput = 0, posSetpoint = 0;
double posKp = 150.0, posKi = 0.0, posKd = 20.0;
PID pidPos(&posInput, &posOutput, &posSetpoint, posKp, posKi, posKd, DIRECT);

// ---------------- Control params ----------------
const int PID_SAMPLE_MS = 100;
const int POS_SAMPLE_MS = 50;
int MIN_DRIVE = 60;
double RPM_DEADBAND = 2.0;
const double ROUND_DEADBAND = 0.01;

unsigned long lastPidMillis = 0;
unsigned long lastSpeedCalcMillis = 0;
long lastEncoderCountForSpeed = 0;

// ---------------- Buttons debounce & long-press ----------------
unsigned long lastBtnUp = 0, lastBtnDown = 0, lastBtnEnter = 0, lastBtnMode = 0;
const unsigned long DEBOUNCE_MS = 200;
unsigned long enterPressStart = 0;
bool enterHeld = false;
bool showPidPage = false;

// ---------------- Web server & WebSocket ----------------
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
Preferences preferences;

// ---------------- Function prototypes ----------------
void updateSpeedMeasurement();
void drawOLED();
void setMotorPWM(int pwmSigned);
void stopMotor();
void handleRoot();
void handleAPI_SetPID();
void handleAPI_SetPosPID();
void handleAPI_SetTarget();
void handleAPI_Run();
void handleAPI_Stop();
void handleAPI_Save();
void handleAPI_Load();
void handleAPI_StatusJSON();
void handleToggleMode();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

// ---------------- Encoder ISR ----------------
IRAM_ATTR void encoderISR_A()
{
  uint8_t A = digitalRead(ENC_A);
  uint8_t B = digitalRead(ENC_B);
  uint8_t ab = (A << 1) | B;
  uint8_t last = lastAB;
  if (last == 0)
  {
    if (ab == 1)
      encoderCount++;
    else if (ab == 2)
      encoderCount--;
  }
  else if (last == 1)
  {
    if (ab == 3)
      encoderCount++;
    else if (ab == 0)
      encoderCount--;
  }
  else if (last == 3)
  {
    if (ab == 2)
      encoderCount++;
    else if (ab == 1)
      encoderCount--;
  }
  else if (last == 2)
  {
    if (ab == 0)
      encoderCount++;
    else if (ab == 3)
      encoderCount--;
  }
  lastAB = ab;
  lastEncoderMillis = millis();
}
IRAM_ATTR void encoderISR_B() { encoderISR_A(); }

// ---------------- Motor driver ----------------
void setMotorPWM(int pwmSigned)
{
  int pwm = abs(pwmSigned);
  if (pwm > 255)
    pwm = 255;
  if (pwm < 0)
    pwm = 0;
  ledcWrite(0, pwm);

  if (pwmSigned > 0)
  {
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, HIGH);
  }
  else if (pwmSigned < 0)
  {
    digitalWrite(IN1_PIN, HIGH);
    digitalWrite(IN2_PIN, LOW);
  }
  else
  {
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);
  }
}
void stopMotor() { setMotorPWM(0); }

// ---------------- Speed measurement ----------------
void updateSpeedMeasurement()
{
  unsigned long now = millis();
  unsigned long dt = now - lastSpeedCalcMillis;
  if (dt < 20)
    return;
  long cnt = encoderCount;
  long delta = cnt - lastEncoderCountForSpeed;
  lastEncoderCountForSpeed = cnt;
  lastSpeedCalcMillis = now;
  double revsOutput = (double)delta / COUNTS_PER_OUTPUT_REV;
  double minutes = (double)dt / 60000.0;
  double rpm = 0.0;
  if (minutes > 0.0)
    rpm = revsOutput / minutes;
  currentRPM = rpm;
  currentRounds = (double)cnt / COUNTS_PER_OUTPUT_REV;
}

// ---------------- OLED drawing ----------------
void drawOLED()
{
  char buf[64];
  u8g2.clearBuffer();

  if (!showPidPage)
  {
    // Main page
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 10, mode == MODE_SPEED ? "MODE: SPEED PID" : "MODE: ROUND PID");
    if (running)
      u8g2.drawStr(90, 10, "RUN");
    else
      u8g2.drawStr(90, 10, "STOP");

    u8g2.setFont(u8g2_font_6x10_tr);
    if (mode == MODE_SPEED)
    {
      snprintf(buf, sizeof(buf), "RPM: %6.1f", currentRPM);
      u8g2.drawStr(0, 28, buf);
      snprintf(buf, sizeof(buf), "Tar: %6.1f RPM", targetRPM);
      u8g2.drawStr(0, 40, buf);
      snprintf(buf, sizeof(buf), "PWM: %4d", (int)round(speedOutput));
      u8g2.drawStr(0, 52, buf);
    }
    else
    {
      snprintf(buf, sizeof(buf), "Rds: %6.3f", currentRounds);
      u8g2.drawStr(0, 28, buf);
      snprintf(buf, sizeof(buf), "Tar: %6.3f rd", targetRounds);
      u8g2.drawStr(0, 40, buf);
      snprintf(buf, sizeof(buf), "PWM: %4d", (int)round(posOutput));
      u8g2.drawStr(0, 52, buf);
    }
  }
  else
  {
    // PID page
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "SPD PID:");
    snprintf(buf, sizeof(buf), "Kp:%5.2f I:%5.2f D:%5.2f", speedKp, speedKi, speedKd);
    u8g2.drawStr(0, 24, buf);
    u8g2.drawStr(0, 36, "POS PID:");
    snprintf(buf, sizeof(buf), "Kp:%6.1f I:%5.2f D:%5.1f", posKp, posKi, posKd);
    u8g2.drawStr(0, 50, buf);
  }

  u8g2.sendBuffer();
}

// ---------------- HTML ----------------
String pageHtml()
{
  String s = R"rawliteral(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Motor - Realtime Chart</title>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;600;800&display=swap" rel="stylesheet">
<style>
:root{--bg:#071028;--card:#0b1220;--accent:#06b6d4;--muted:#94a3b8;--target:#facc15;--current:#22d3ee;--alt:#fb923c;}
body{margin:0;font-family:Inter,Arial,Helvetica,sans-serif;background:linear-gradient(180deg,#071025,#03101b);color:#e6eef8;}
.container{max-width:1000px;margin:16px auto;padding:16px;}
.header{display:flex;align-items:center;gap:12px;}
.logo{width:48px;height:48px;background:linear-gradient(135deg,#2563eb,#06b6d4);border-radius:8px;display:flex;align-items:center;justify-content:center;font-weight:800;color:white;}
.title{font-size:18px;font-weight:700;}
.grid{display:grid;grid-template-columns:1fr 360px;gap:14px;margin-top:14px;}
.card{background:var(--card);padding:12px;border-radius:12px;border:1px solid rgba(255,255,255,0.02);}
.controls{display:flex;flex-direction:column;gap:8px;}
.controls input{padding:8px;border-radius:8px;border:1px solid rgba(255,255,255,0.04);background:transparent;color:inherit;}
.row{display:flex;gap:8px;align-items:center;}
.small{padding:8px;border-radius:8px;border:0;background:var(--accent);color:#042027;font-weight:700;cursor:pointer;}
.muted{color:var(--muted);font-size:13px;}
.chart-wrap{height:220px;padding:8px;background:linear-gradient(0deg, rgba(255,255,255,0.01), rgba(255,255,255,0.02));border-radius:8px;}
.legend{display:flex;gap:8px;margin-top:8px;align-items:center;}
.legend .item{display:flex;gap:6px;align-items:center;}
.dot{width:12px;height:8px;border-radius:3px;}
.footer{margin-top:12px;color:var(--muted);font-size:12px;}
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="logo">M</div>
    <div>
      <div class="title">ESP32 Motor — Realtime PID Tuner & Chart</div>
      <div class="muted">Realtime via WebSocket • Chart.js v2</div>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <div style="display:flex;justify-content:space-between;align-items:center;">
        <div>
          <div id="modeTitle" style="font-weight:700">Mode: SPEED</div>
          <div id="ipInfo" class="muted">Connecting...</div>
        </div>
        <div style="text-align:right">
          <div id="runBadge" style="padding:6px 10px;border-radius:999px;background:rgba(255,255,255,0.03)">STOP</div>
          <div style="height:6px"></div>
          <div id="rpmBadge" style="padding:6px 10px;border-radius:999px;background:rgba(255,255,255,0.03)">RPM: 0.0</div>
        </div>
      </div>

      <div style="margin-top:12px" class="chart-wrap">
        <canvas id="chartCanvas"></canvas>
        <div class="legend">
          <div class="item"><div class="dot" id="dotTarget" style="background:#facc15"></div><div class="muted">Target</div></div>
          <div class="item"><div class="dot" id="dotCurrent" style="background:#22d3ee"></div><div class="muted">Current</div></div>
        </div>
      </div>

      <div style="margin-top:12px" class="controls">
        <div class="row">
          <button class="small" onclick="toggleRun()">Start/Stop</button>
          <button class="small" onclick="toggleMode()">Toggle Mode</button>
          <button class="small" onclick="savePrefs()">Save</button>
          <button class="small" onclick="loadPrefs()">Load</button>
        </div>

        <div class="row">
          <input id="targetRPM" type="number" step="1" placeholder="Target RPM">
          <button class="small" onclick="setTarget()">Set RPM</button>
        </div>

        <div class="row">
          <input id="targetR" type="number" step="0.1" placeholder="Target Rounds">
          <button class="small" onclick="setTargetRounds()">Set Rounds</button>
        </div>
      </div>
    </div>

    <div class="card">
      <div style="font-weight:700">PID Tuner</div>
      <div style="margin-top:8px" class="muted">Speed PID (SPEED)</div>
      <div style="display:flex;gap:8px;margin-top:6px">
        <input id="kp" type="number" step="0.01" placeholder="Kp">
        <input id="ki" type="number" step="0.01" placeholder="Ki">
        <input id="kd" type="number" step="0.01" placeholder="Kd">
      </div>
      <div style="display:flex;gap:8px;margin-top:8px;">
        <button class="small" onclick="setPID()">Update Speed PID</button>
      </div>

      <div style="margin-top:12px" class="muted">Position PID (ROUND)</div>
      <div style="display:flex;gap:8px;margin-top:6px">
        <input id="pkp" type="number" step="0.1" placeholder="P">
        <input id="pki" type="number" step="0.01" placeholder="I">
        <input id="pkd" type="number" step="0.1" placeholder="D">
      </div>
      <div style="display:flex;gap:8px;margin-top:8px;">
        <button class="small" onclick="setPosPID()">Update Position PID</button>
      </div>

      <div class="footer">JSON: <code>/api/status</code> • WebSocket port 81</div>
    </div>
  </div>
</div>

<!-- Chart.js v2 -->
<script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.9.4/Chart.min.js"></script>

<script>
const MAX_POINTS = 150;
let ws;
let datasetTarget = [];
let datasetCurrent = [];
let labels = [];
let chart;
let currentMode = "SPEED";

function initChart() {
  const ctx = document.getElementById('chartCanvas').getContext('2d');
  chart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: labels,
      datasets: [{
        label: 'Target',
        data: datasetTarget,
        borderColor: '#facc15',
        backgroundColor: 'rgba(250,204,21,0.07)',
        borderDash: [6,4],
        fill:false,
        lineTension:0
      },{
        label: 'Current',
        data: datasetCurrent,
        borderColor: '#22d3ee',
        backgroundColor: 'rgba(34,211,238,0.06)',
        fill:false,
        lineTension:0
      }]
    },
    options: {
      animation:false,
      responsive:true,
      maintainAspectRatio:false,
      scales: {
        xAxes:[{display:false}],
        yAxes:[{ticks:{beginAtZero:true}}]
      },
      elements:{point:{radius:0}}
    }
  });
}

function pushPoint(target, current) {
  const t = new Date().toLocaleTimeString().split(' ')[0];
  labels.push(t);
  datasetTarget.push(target);
  datasetCurrent.push(current);
  if (labels.length > MAX_POINTS) {
    labels.shift();
    datasetTarget.shift();
    datasetCurrent.shift();
  }
  chart.update();
}

function connectWS() {
  const ip = location.hostname;
  const url = "ws://" + ip + ":81/";
  ws = new WebSocket(url);
  ws.onopen = () => {
    console.log("WS open", url);
    document.getElementById('ipInfo').textContent = location.hostname;
  };
  ws.onmessage = (evt) => {
    try {
      const j = JSON.parse(evt.data);
      // update UI elements
      document.getElementById('curRPM').textContent = j.currentRPM ? j.currentRPM.toFixed(1) : '0.0';
      document.getElementById('curRounds').textContent = j.currentRounds ? j.currentRounds.toFixed(3) : '0.000';
      document.getElementById('curPWM').textContent = j.pwm | 0;
      document.getElementById('modeTitle').textContent = 'Mode: ' + j.mode;
      currentMode = j.mode;
      document.getElementById('runBadge').textContent = j.running ? 'RUN' : 'STOP';
      document.getElementById('rpmBadge').textContent = (j.mode === 'SPEED' ? 'RPM: ' + j.currentRPM.toFixed(1) : 'RDS: ' + j.currentRounds.toFixed(3));
      // fill pid inputs if empty
      if (!document.getElementById('kp').value) document.getElementById('kp').value = j.kp;
      if (!document.getElementById('ki').value) document.getElementById('ki').value = j.ki;
      if (!document.getElementById('kd').value) document.getElementById('kd').value = j.kd;
      if (!document.getElementById('pkp').value) document.getElementById('pkp').value = j.pos_kp;
      if (!document.getElementById('pki').value) document.getElementById('pki').value = j.pos_ki;
      if (!document.getElementById('pkd').value) document.getElementById('pkd').value = j.pos_kd;
      if (!document.getElementById('targetRPM').value) document.getElementById('targetRPM').value = j.targetRPM;
      if (!document.getElementById('targetR').value) document.getElementById('targetR').value = j.targetRounds;

      // choose plotted values depending on mode
      if (j.mode === 'SPEED') {
        // target RPM vs current RPM
        pushPoint(parseFloat(j.targetRPM), parseFloat(j.currentRPM));
        // color adjustments
        chart.data.datasets[0].borderColor = '#facc15';
        chart.data.datasets[1].borderColor = '#22d3ee';
      } else {
        // target rounds vs current rounds
        pushPoint(parseFloat(j.targetRounds), parseFloat(j.currentRounds));
        chart.data.datasets[0].borderColor = '#facc15';
        chart.data.datasets[1].borderColor = '#fb923c';
      }
    } catch(e) { console.warn('ws parse err', e); }
  };
  ws.onclose = () => { console.log("WS closed, reconnect in 1s"); setTimeout(connectWS,1000); };
}
window.onload = function(){
  // create invisible placeholders for curRPM/curRounds/curPWM to avoid missing elements in DOM
  const d1 = document.createElement('div'); d1.style.display='none'; d1.id='curRPM'; document.body.appendChild(d1);
  const d2 = document.createElement('div'); d2.style.display='none'; d2.id='curRounds'; document.body.appendChild(d2);
  const d3 = document.createElement('div'); d3.style.display='none'; d3.id='curPWM'; document.body.appendChild(d3);
  initChart();
  connectWS();
};

// API functions
function setPID(){
  const kp = document.getElementById('kp').value;
  const ki = document.getElementById('ki').value;
  const kd = document.getElementById('kd').value;
  fetch(`/api/setpid?kp=${encodeURIComponent(kp)}&ki=${encodeURIComponent(ki)}&kd=${encodeURIComponent(kd)}`);
}
function setPosPID(){
  const pkp = document.getElementById('pkp').value;
  const pki = document.getElementById('pki').value;
  const pkd = document.getElementById('pkd').value;
  fetch(`/api/setpospid?pkp=${encodeURIComponent(pkp)}&pki=${encodeURIComponent(pki)}&pkd=${encodeURIComponent(pkd)}`);
}
function setTarget(){
  const t = document.getElementById('targetRPM').value;
  fetch(`/api/settarget?t=${encodeURIComponent(t)}`);
}
function setTargetRounds(){
  const r = document.getElementById('targetR').value;
  fetch(`/api/settarget?r=${encodeURIComponent(r)}`);
}
function toggleRun(){ fetch('/api/run'); }
function toggleMode(){ fetch('/api/togglemode'); }
function savePrefs(){ fetch('/api/save').then(()=>alert('Saved')); }
function loadPrefs(){ fetch('/api/load').then(()=>alert('Loaded')); }
</script>
</body>
</html>
)rawliteral";
  return s;
}

// ---------------- API Handlers ----------------
void handleRoot()
{
  server.send(200, "text/html", pageHtml());
}

void handleAPI_SetPID()
{
  if (server.hasArg("kp"))
    speedKp = server.arg("kp").toFloat();
  if (server.hasArg("ki"))
    speedKi = server.arg("ki").toFloat();
  if (server.hasArg("kd"))
    speedKd = server.arg("kd").toFloat();
  pidSpeed.SetTunings(speedKp, speedKi, speedKd);
  server.send(200, "text/plain", "OK");
}

void handleAPI_SetPosPID()
{
  if (server.hasArg("pkp"))
    posKp = server.arg("pkp").toFloat();
  if (server.hasArg("pki"))
    posKi = server.arg("pki").toFloat();
  if (server.hasArg("pkd"))
    posKd = server.arg("pkd").toFloat();
  pidPos.SetTunings(posKp, posKi, posKd);
  server.send(200, "text/plain", "OK");
}

void handleAPI_SetTarget()
{
  if (server.hasArg("t"))
    targetRPM = server.arg("t").toFloat();
  if (server.hasArg("r"))
    targetRounds = server.arg("r").toFloat();
  server.send(200, "text/plain", "OK");
}

void handleAPI_Run()
{
  running = !running;
  server.send(200, "text/plain", running ? "RUN" : "STOP");
}

void handleAPI_Stop()
{
  running = false;
  stopMotor();
  server.send(200, "text/plain", "STOPPED");
}

void handleAPI_Save()
{
  preferences.begin("motor", false);
  preferences.putDouble("speedKp", speedKp);
  preferences.putDouble("speedKi", speedKi);
  preferences.putDouble("speedKd", speedKd);
  preferences.putDouble("posKp", posKp);
  preferences.putDouble("posKi", posKi);
  preferences.putDouble("posKd", posKd);
  preferences.putDouble("targetRPM", targetRPM);
  preferences.putDouble("targetRounds", targetRounds);
  preferences.putInt("MIN_DRIVE", MIN_DRIVE);
  preferences.putDouble("RPM_DEADBAND", RPM_DEADBAND);
  preferences.end();
  server.send(200, "text/plain", "SAVED");
}

void handleAPI_Load()
{
  preferences.begin("motor", true);
  if (preferences.isKey("speedKp"))
  {
    speedKp = preferences.getDouble("speedKp", speedKp);
    speedKi = preferences.getDouble("speedKi", speedKi);
    speedKd = preferences.getDouble("speedKd", speedKd);
    posKp = preferences.getDouble("posKp", posKp);
    posKi = preferences.getDouble("posKi", posKi);
    posKd = preferences.getDouble("posKd", posKd);
    targetRPM = preferences.getDouble("targetRPM", targetRPM);
    targetRounds = preferences.getDouble("targetRounds", targetRounds);
    MIN_DRIVE = preferences.getInt("MIN_DRIVE", MIN_DRIVE);
    RPM_DEADBAND = preferences.getDouble("RPM_DEADBAND", RPM_DEADBAND);
    pidSpeed.SetTunings(speedKp, speedKi, speedKd);
    pidPos.SetTunings(posKp, posKi, posKd);
  }
  preferences.end();
  server.send(200, "text/plain", "LOADED");
}

void handleAPI_StatusJSON()
{
  String s = "{";
  s += "\"mode\":\"" + String(mode == MODE_SPEED ? "SPEED" : "ROUND") + "\",";
  s += "\"running\":" + String(running ? "true" : "false") + ",";
  s += "\"currentRPM\":" + String(currentRPM, 2) + ",";
  s += "\"currentRounds\":" + String(currentRounds, 4) + ",";
  s += "\"targetRPM\":" + String(targetRPM, 2) + ",";
  s += "\"targetRounds\":" + String(targetRounds, 4) + ",";
  s += "\"kp\":" + String(speedKp, 6) + ",";
  s += "\"ki\":" + String(speedKi, 6) + ",";
  s += "\"kd\":" + String(speedKd, 6) + ",";
  s += "\"pos_kp\":" + String(posKp, 6) + ",";
  s += "\"pos_ki\":" + String(posKi, 6) + ",";
  s += "\"pos_kd\":" + String(posKd, 6) + ",";
  int pwmVal = (mode == MODE_SPEED) ? (int)round(speedOutput) : (int)round(posOutput);
  s += "\"pwm\":" + String(pwmVal);
  s += "}";
  server.send(200, "application/json", s);
}

void handleToggleMode()
{
  mode = (mode == MODE_SPEED) ? MODE_ROUND : MODE_SPEED;
  server.send(200, "text/plain", "MODE_TOGGLED");
}

// ---------------- WebSocket event ----------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  if (type == WStype_TEXT)
  {
    String msg = String((char *)payload);
    Serial.println("WS msg: " + msg);
  }
  else if (type == WStype_DISCONNECTED)
  {
    Serial.printf("WS[%u] Disconnected\n", num);
  }
  else if (type == WStype_CONNECTED)
  {
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("WS[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
  }
}

// ---------------- Setup ----------------
void setup()
{
  Serial.begin(115200);
  delay(10);

  // IO
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_ENTER, INPUT_PULLUP);
  pinMode(BUTTON_MODE, INPUT);
  // pinMode(LIMIT_S, INPUT_PULLUP);

  // PWM channel
  ledcSetup(0, 20000, 8); // 20kHz, 8-bit
  ledcAttachPin(PWM_PIN, 0);

  // Encoder interrupts
  lastAB = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);
  attachInterrupt(digitalPinToInterrupt(ENC_A), encoderISR_A, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), encoderISR_B, CHANGE);

  // Display
  u8g2.begin();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(300);
    Serial.print(".");
    if (millis() - wifiStart > 20000)
      break;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }
  else
  {
    Serial.println("WiFi not connected, continue offline.");
  }

  // OTA
  ArduinoOTA.setHostname("ESP32_Motor");
  ArduinoOTA.onStart([]()
                     { Serial.println("OTA start"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nOTA end"); });
  ArduinoOTA.onError([](ota_error_t e)
                     { Serial.printf("OTA err %u\n", e); });
  ArduinoOTA.begin();

  // PID initial config
  pidSpeed.SetMode(AUTOMATIC);
  pidSpeed.SetSampleTime(PID_SAMPLE_MS);
  pidSpeed.SetOutputLimits(0, 255);
  pidSpeed.SetTunings(speedKp, speedKi, speedKd);

  pidPos.SetMode(AUTOMATIC);
  pidPos.SetSampleTime(POS_SAMPLE_MS);
  pidPos.SetOutputLimits(-255, 255); // -255 - 0 CCW, 0-255 CW
  pidPos.SetTunings(posKp, posKi, posKd);

  lastPidMillis = millis();
  lastSpeedCalcMillis = millis();
  lastEncoderCountForSpeed = encoderCount;

  // Web routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/setpid", HTTP_GET, handleAPI_SetPID);
  server.on("/api/setpospid", HTTP_GET, handleAPI_SetPosPID);
  server.on("/api/settarget", HTTP_GET, handleAPI_SetTarget);
  server.on("/api/run", HTTP_GET, handleAPI_Run);
  server.on("/api/stop", HTTP_GET, handleAPI_Stop);
  server.on("/api/save", HTTP_GET, handleAPI_Save);
  server.on("/api/load", HTTP_GET, handleAPI_Load);
  server.on("/api/status", HTTP_GET, handleAPI_StatusJSON);
  server.on("/api/togglemode", HTTP_GET, handleToggleMode);
  server.begin();
  Serial.println("HTTP server started");

  // WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started");

  drawOLED();
}

// ---------------- Main Loop ----------------
unsigned long lastPush = 0;
const unsigned long PUSH_INTERVAL = 150;

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();
  webSocket.loop();

  unsigned long now = millis();

  if (!digitalRead(BUTTON_UP) && now - lastBtnUp > DEBOUNCE_MS)
  {
    if (mode == MODE_SPEED)
    {
      targetRPM += 5.0;
      if (targetRPM > 400.0)
        targetRPM = 400.0;
    }
    else
    {
      targetRounds += 0.1;
      if (targetRounds > 1000.0)
        targetRounds = 1000.0;
    }
    lastBtnUp = now;
  }
  if (!digitalRead(BUTTON_DOWN) && now - lastBtnDown > DEBOUNCE_MS)
  {
    if (mode == MODE_SPEED)
    {
      targetRPM -= 5.0;
      if (targetRPM < 0.0)
        targetRPM = 0.0;
    }
    else
    {
      targetRounds -= 0.1;
      if (targetRounds < 0.0)
        targetRounds = 0.0;
    }
    lastBtnDown = now;
  }

  if (!digitalRead(BUTTON_ENTER))
  {
    if (!enterHeld)
    {
      enterHeld = true;
      enterPressStart = now;
    }
    else
    {
      if (enterHeld && (now - enterPressStart >= 2000))
      {
        showPidPage = !showPidPage;
        enterHeld = false;
        delay(200);
      }
    }
  }
  else
  {
    if (enterHeld)
    {
      if (now - enterPressStart < 2000)
      {
        running = !running;
      }
      enterHeld = false;
    }
  }

  if (digitalRead(BUTTON_MODE) == LOW && now - lastBtnMode > DEBOUNCE_MS)
  {
    mode = (mode == MODE_SPEED) ? MODE_ROUND : MODE_SPEED;
    lastBtnMode = now;
  }

  updateSpeedMeasurement();
  if (now - lastPidMillis >= PID_SAMPLE_MS)
  {
    lastPidMillis = now;

    if (!running)
    {
      stopMotor();
      drawOLED();
    }
    else
    {
      if (mode == MODE_SPEED)
      {
        speedInput = currentRPM;
        speedSetpoint = targetRPM;
        pidSpeed.Compute();

        if (fabs(speedInput) < 3.0)
          speedOutput = fabs(speedOutput);

        int pwm = (int)round(speedOutput);
        if (fabs(speedSetpoint - speedInput) > RPM_DEADBAND && pwm < MIN_DRIVE)
          pwm = MIN_DRIVE;
        if (fabs(speedSetpoint - speedInput) <= RPM_DEADBAND || speedSetpoint == 0.0)
        {
          stopMotor();
        }
        else
        {
          setMotorPWM(pwm);
        }
      }
      else
      {
        // round control
        posInput = currentRounds;
        posSetpoint = targetRounds;
        pidPos.Compute();
        int pwm = (int)round(posOutput);
        if (fabs(posSetpoint - posInput) > ROUND_DEADBAND && abs(pwm) < MIN_DRIVE)
          pwm = (pwm >= 0) ? MIN_DRIVE : -MIN_DRIVE;
        if (fabs(posSetpoint - posInput) <= ROUND_DEADBAND)
        {
          stopMotor();
          running = false;
        }
        else
        {
          setMotorPWM(pwm);
        }
      }
      drawOLED();
    }
  }

  if (now - lastPush >= PUSH_INTERVAL)
  {
    lastPush = now;
    String s = "{";
    s += "\"mode\":\"" + String(mode == MODE_SPEED ? "SPEED" : "ROUND") + "\",";
    s += "\"running\":" + String(running ? "true" : "false") + ",";
    s += "\"currentRPM\":" + String(currentRPM, 2) + ",";
    s += "\"currentRounds\":" + String(currentRounds, 4) + ",";
    s += "\"targetRPM\":" + String(targetRPM, 2) + ",";
    s += "\"targetRounds\":" + String(targetRounds, 4) + ",";
    s += "\"kp\":" + String(speedKp, 6) + ",";
    s += "\"ki\":" + String(speedKi, 6) + ",";
    s += "\"kd\":" + String(speedKd, 6) + ",";
    s += "\"pos_kp\":" + String(posKp, 6) + ",";
    s += "\"pos_ki\":" + String(posKi, 6) + ",";
    s += "\"pos_kd\":" + String(posKd, 6) + ",";
    int pwmVal = (mode == MODE_SPEED) ? (int)round(speedOutput) : (int)round(posOutput);
    s += "\"pwm\":" + String(pwmVal);
    s += "}";
    webSocket.broadcastTXT(s);
  }

  delay(5);
}
