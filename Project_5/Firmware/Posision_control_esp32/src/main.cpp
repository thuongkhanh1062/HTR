/*
@Author: CHITOAN (modified)
@date: 171125 -> updated
@version: V4.0

Features added in V4.0:
 - Send HALL sensor value over WebSocket (field "hall")
 - Chart.js shows 3rd dataset for Hall value
 - New modes: MODE_POSITION (absolute position control) and MODE_HOMING (perform homing using Hall sensor)
 - Homing routine: motor moves slowly until Hall triggered, encoder reset -> switch to POSITION mode
 - API toggles cycle through modes: SPEED -> ROUND -> POSITION -> HOMING
 - Hall read uses analogRead with HALL_THRESHOLD to detect trigger

Notes:
 - HALL_S pin is analog on ESP32; threshold may need tuning per your sensor.
 - You can change HALL_THRESHOLD, HOMING_PWM, and HOMING_DIRECTION if needed.
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
const char *ssid = "NGUYEN SANG TRUOC";
const char *password = "apdkp413271a1";

// ---------------- IO ----------------
#define ENC_A 19
#define ENC_B 18

#define IN1_PIN 15
#define IN2_PIN 5
#define PWM_PIN 13

#define BUTTON_UP 25    // increase key
#define BUTTON_DOWN 33  // decrease key
#define BUTTON_ENTER 32 // run/stop, long-press to change screen
#define BUTTON_MODE 35  // change mode

#define HALL_S 34

// ---------------- Display ----------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// ---------------- Encoder / mechanics ----------------
volatile long encoderCount = 0;
volatile uint8_t lastAB = 0;
volatile unsigned long lastEncoderMillis = 0;

const int ENCODER_PPR = 44;
const float GEAR_RATIO = 45.0;
const float COUNTS_PER_OUTPUT_REV = ENCODER_PPR * GEAR_RATIO;

// ---------------- Modes ----------------
enum Mode
{
  MODE_SPEED,
  MODE_ROUND,
  MODE_POSITION,
  MODE_HOMING
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
double speedKp = 0.35, speedKi = 0.02, speedKd = 0.05;
PID pidSpeed(&speedInput, &speedOutput, &speedSetpoint, speedKp, speedKi, speedKd, DIRECT);

// ---------------- PID (Position) ----------------
double posInput = 0, posOutput = 0, posSetpoint = 0;
double posKp = 39.0, posKi = 0.06, posKd = 1.1;
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

// ---------------- Hall sensor ----------------
const int HALL_THRESHOLD = 3600;
const int HOMING_PWM = 80;
const int HOMING_DIRECTION = -1;

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
bool hallTriggered();

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

// ---------------- STOP motor --------------------------------------------
void stopMotor() { setMotorPWM(0); }

// ---------------- Speed measurement with low-pass filter ----------------
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
  double rounds = (double)cnt / COUNTS_PER_OUTPUT_REV;

  const float alpha = 0.3;
  currentRPM = alpha * rpm + (1 - alpha) * currentRPM;
  currentRounds = alpha * rounds + (1 - alpha) * currentRounds;
}

// ---------------- Hall helper ----------------
bool hallTriggered()
{
  int val = analogRead(HALL_S); // 0..4095 on default ESP32 ADC
  return val >= HALL_THRESHOLD;
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
    const char *modeStr = (mode == MODE_SPEED) ? "SPEED" : (mode == MODE_ROUND) ? "ROUND" : (mode == MODE_POSITION) ? "POSITION" : "HOMING";
    u8g2.drawStr(0, 10, modeStr);
    if (running)
      u8g2.drawStr(100, 10, "RUN");
    else
      u8g2.drawStr(100, 10, "STOP");

    u8g2.setFont(u8g2_font_6x10_tr);
    if (mode == MODE_SPEED)
    {
      snprintf(buf, sizeof(buf), "%.0f", currentRPM);
      u8g2.setFont(u8g2_font_fur30_tf);
      u8g2.drawStr(0, 47, buf);

      snprintf(buf, sizeof(buf), "%3.0fRPM", targetRPM);
      u8g2.setFont(u8g2_font_9x15_me);
      int16_t w1 = u8g2.getStrWidth(buf);
      u8g2.drawStr(128 - w1, 27, buf);

      snprintf(buf, sizeof(buf), "%4dPWM", (int)round(speedOutput));
      int16_t w2 = u8g2.getStrWidth(buf);
      u8g2.drawStr(128 - w2, 47, buf);

      snprintf(buf, sizeof(buf), "%5.2f %5.2f %5.2f", speedKp, speedKi, speedKd);
      u8g2.setFont(u8g2_font_6x12_me);
      int16_t xSpeed = (128 - u8g2.getStrWidth(buf)) / 2;
      u8g2.drawStr(xSpeed, 60, buf);
    }
    else
    {
      snprintf(buf, sizeof(buf), "%.3f", currentRounds);
      u8g2.setFont(u8g2_font_fur30_tf);
      u8g2.drawStr(0, 47, buf);

      snprintf(buf, sizeof(buf), "%3.1frd", targetRounds);
      u8g2.setFont(u8g2_font_9x15_me);
      int16_t w3 = u8g2.getStrWidth(buf);
      u8g2.drawStr(128 - w3, 27, buf);

      snprintf(buf, sizeof(buf), "%4d", (int)round(posOutput));
      int16_t w4 = u8g2.getStrWidth(buf);
      u8g2.drawStr(128 - w4, 47, buf);

      snprintf(buf, sizeof(buf), "%5.2f %5.2f %5.2f", posKp, posKi, posKd);
      u8g2.setFont(u8g2_font_6x12_me);
      int16_t xSpeed = (128 - u8g2.getStrWidth(buf)) / 2;
      u8g2.drawStr(xSpeed, 60, buf);
    }
  }
  else
  {
    // PID PAGE
    u8g2.setFont(u8g2_font_6x12_me);
    u8g2.drawStr(0, 10, "PID SPEED PARA");
    snprintf(buf, sizeof(buf), "%5.2f %5.2f %5.2f", speedKp, speedKi, speedKd);
    u8g2.setFont(u8g2_font_7x14_mf);
    int16_t xSpeed = (128 - u8g2.getStrWidth(buf)) / 2;
    u8g2.drawStr(xSpeed, 26, buf);

    u8g2.setFont(u8g2_font_6x12_me);
    u8g2.drawStr(0, 44, "PID ROUND PARA");
    snprintf(buf, sizeof(buf), "%6.1f %5.2f %5.1f", posKp, posKi, posKd);
    u8g2.setFont(u8g2_font_7x14_mf);
    int16_t xRound = (128 - u8g2.getStrWidth(buf)) / 2;
    u8g2.drawStr(xRound, 60, buf);
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
    <title>ESP32 Motor - Realtime Chart (Hall)</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;600;800&display=swap" rel="stylesheet">
    <style>
        :root { --bg:#071028; --card:#0b1220; --accent:#06b6d4; --muted:#94a3b8; }
        body{margin:0;font-family:Inter, Arial, Helvetica, sans-serif;background:linear-gradient(180deg,#071025,#03101b);color:#e6eef8}
        .container{max-width:1000px;margin:16px auto;padding:16px}
        .header{display:flex;align-items:center;gap:12px}
        .logo{width:48px;height:48px;background:linear-gradient(135deg,#2563eb,#06b6d4);border-radius:8px;display:flex;align-items:center;justify-content:center;font-weight:800;color:white}
        .title{font-size:18px;font-weight:700}
        .grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-top:14px}
        .card{background:var(--card);padding:12px;border-radius:12px;border:1px solid rgba(255,255,255,0.02)}
        .chart-wrap{height:220px;padding:8px;background:linear-gradient(0deg,rgba(255,255,255,0.01),rgba(255,255,255,0.02));border-radius:8px}
        .small{padding:8px;border-radius:8px;border:0;background:var(--accent);color:#042027;font-weight:700;cursor:pointer}
        .muted{color:var(--muted);font-size:13px}
        @media(max-width:768px){.grid{grid-template-columns:1fr}}
    </style>
</head>

<body>
    <div class="container">
        <div class="header">
            <div class="logo">M</div>
            <div>
                <div class="title">ESP32 Motor — Realtime PID Tuner & Chart (with Hall)</div>
                <div class="muted">Made by CHITOAN (modified)</div>
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
                    <div class="legend" style="margin-top:8px;display:flex;gap:8px;align-items:center;">
                        <div style="display:flex;gap:6px;align-items:center;"><div class="dot" style="width:12px;height:8px;border-radius:3px;background:#facc15"></div><div class="muted">Target</div></div>
                        <div style="display:flex;gap:6px;align-items:center;"><div class="dot" style="width:12px;height:8px;border-radius:3px;background:#22d3ee"></div><div class="muted">Current</div></div>
                        <div style="display:flex;gap:6px;align-items:center;"><div class="dot" style="width:12px;height:8px;border-radius:3px;background:#10b981"></div><div class="muted">Hall</div></div>
                    </div>
                </div>

                <div style="margin-top:12px" class="controls">
                    <div style="margin-top:12px" class="muted">Controls </div>
                    <div style="display:flex;gap:8px;">
                        <button class="small" onclick="toggleRun()">Start/Stop</button>
                        <button class="small" onclick="toggleMode()">Cycle Mode</button>
                        <button class="small" onclick="savePrefs()">Save</button>
                        <button class="small" onclick="loadPrefs()">Load</button>
                    </div>

                    <div style="display:flex;gap:8px;margin-top:8px;">
                        <input id="targetRPM" type="number" step="1" placeholder="Target RPM">
                        <button class="small" onclick="setTarget()">Set RPM</button>
                    </div>

                    <div style="display:flex;gap:8px;margin-top:8px;">
                        <input id="targetR" type="number" step="0.001" placeholder="Target Rounds/Position">
                        <button class="small" onclick="setTargetRounds()">Set Rounds</button>
                    </div>
                </div>
            </div>

            <div class="card">
                <div style="font-weight:700">PID Tuner</div>
                <div style="margin-top:8px" class="muted">Speed PID (SPEED)</div>
                <div style="display:flex;gap:8px;margin-top:8px;">
                    <input id="kp" type="number" step="0.01" placeholder="Kp">
                    <input id="ki" type="number" step="0.01" placeholder="Ki">
                    <input id="kd" type="number" step="0.01" placeholder="Kd">
                </div>
                <div style="display:flex;gap:8px;margin-top:8px;"><button class="small" onclick="setPID()">Update Speed PID</button></div>

                <div style="margin-top:12px" class="muted">Position PID (ROUND/POSITION)</div>
                <div style="display:flex;gap:8px;margin-top:8px;">
                    <input id="pkp" type="number" step="0.1" placeholder="P">
                    <input id="pki" type="number" step="0.01" placeholder="I">
                    <input id="pkd" type="number" step="0.1" placeholder="D">
                </div>
                <div style="display:flex;gap:8px;margin-top:8px;"><button class="small" onclick="setPosPID()">Update Position PID</button></div>

                <div style="margin-top:14px" class="muted">Hall / Homing</div>
                <div style="margin-top:8px">Hall value will be plotted and homing can be started by cycling modes until HOMING.</div>
            </div>
        </div>
        <div style="margin-top:12px;color:#94a3b8;font-size:12px">Contact CHITOAN - Firmware version: V4.0</div>
    </div>

    <!-- Chart.js v2 -->
    <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.9.4/Chart.min.js"></script>

    <script>
        const MAX_POINTS = 150;
        let ws;
        let datasetTarget = [];
        let datasetCurrent = [];
        let hallDataset = [];
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
                        borderDash: [6, 4],
                        fill: false,
                        lineTension: 0
                    }, {
                        label: 'Current',
                        data: datasetCurrent,
                        borderColor: '#22d3ee',
                        backgroundColor: 'rgba(34,211,238,0.06)',
                        fill: false,
                        lineTension: 0
                    }, {
                        label: 'Hall',
                        data: hallDataset,
                        borderColor: '#10b981',
                        backgroundColor: 'rgba(16,185,129,0.06)',
                        fill: false,
                        lineTension: 0
                    }]
                },
                options: {
                    animation: false,
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        xAxes: [{ display: false }],
                        yAxes: [{ ticks: { beginAtZero: true } }]
                    },
                    elements: { point: { radius: 0 } }
                }
            });
        }

        function pushPoint(target, current, hall) {
            const t = new Date().toLocaleTimeString().split(' ')[0];
            labels.push(t);
            datasetTarget.push(target);
            datasetCurrent.push(current);
            hallDataset.push(hall);
            if (labels.length > MAX_POINTS) {
                labels.shift(); datasetTarget.shift(); datasetCurrent.shift(); hallDataset.shift();
            }
            chart.update();
        }

        function connectWS() {
            const ip = location.hostname;
            const url = "ws://" + ip + ":81/";
            ws = new WebSocket(url);
            ws.onopen = () => { console.log("WS open", url); document.getElementById('ipInfo').textContent = location.hostname; };
            ws.onmessage = (evt) => {
                try {
                    const j = JSON.parse(evt.data);
                    document.getElementById('modeTitle').textContent = 'Mode: ' + j.mode;
                    currentMode = j.mode;
                    document.getElementById('runBadge').textContent = j.running ? 'RUN' : 'STOP';
                    document.getElementById('rpmBadge').textContent = (j.mode === 'SPEED' ? 'RPM: ' + (j.currentRPM||0).toFixed(1) : 'RDS: ' + (j.currentRounds||0).toFixed(3));

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
                        pushPoint(parseFloat(j.targetRPM), parseFloat(j.currentRPM), parseFloat(j.hall||0));
                        chart.data.datasets[0].borderColor = '#facc15';
                        chart.data.datasets[1].borderColor = '#22d3ee';
                        chart.data.datasets[2].borderColor = '#10b981';
                    } else {
                        pushPoint(parseFloat(j.targetRounds), parseFloat(j.currentRounds), parseFloat(j.hall||0));
                        chart.data.datasets[0].borderColor = '#facc15';
                        chart.data.datasets[1].borderColor = '#fb923c';
                        chart.data.datasets[2].borderColor = '#10b981';
                    }
                } catch (e) { console.warn('ws parse err', e); }
            };
            ws.onclose = () => { console.log("WS closed, reconnect in 1s"); setTimeout(connectWS, 1000); };
        }
        window.onload = function () {
            const d1 = document.createElement('div'); d1.style.display = 'none'; d1.id = 'curRPM'; document.body.appendChild(d1);
            const d2 = document.createElement('div'); d2.style.display = 'none'; d2.id = 'curRounds'; document.body.appendChild(d2);
            const d3 = document.createElement('div'); d3.style.display = 'none'; d3.id = 'curPWM'; document.body.appendChild(d3);
            initChart();
            connectWS();
        };

        // API functions
        function setPID() { const kp = document.getElementById('kp').value; const ki = document.getElementById('ki').value; const kd = document.getElementById('kd').value; fetch(`/api/setpid?kp=${encodeURIComponent(kp)}&ki=${encodeURIComponent(ki)}&kd=${encodeURIComponent(kd)}`); }
        function setPosPID() { const pkp = document.getElementById('pkp').value; const pki = document.getElementById('pki').value; const pkd = document.getElementById('pkd').value; fetch(`/api/setpospid?pkp=${encodeURIComponent(pkp)}&pki=${encodeURIComponent(pki)}&pkd=${encodeURIComponent(pkd)}`); }
        function setTarget() { const t = document.getElementById('targetRPM').value; fetch(`/api/settarget?t=${encodeURIComponent(t)}`); }
        function setTargetRounds() { const r = document.getElementById('targetR').value; fetch(`/api/settarget?r=${encodeURIComponent(r)}`); }
        function toggleRun() { fetch('/api/run'); }
        function toggleMode() { fetch('/api/togglemode'); }
        function savePrefs() { fetch('/api/save').then(()=>alert('Saved')); }
        function loadPrefs() { fetch('/api/load').then(()=>alert('Loaded')); }
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
  int hallValue = analogRead(HALL_S);
  String s = "{";
  s += "\"mode\":\"" + String(mode == MODE_SPEED ? "SPEED" : mode == MODE_ROUND ? "ROUND" : mode == MODE_POSITION ? "POSITION" : "HOMING") + "\",";
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
  s += "\"pwm\":" + String(pwmVal) + ",";
  s += "\"hall\":" + String(hallValue);
  s += "}";
  server.send(200, "application/json", s);
}

void handleToggleMode()
{
  // cycle modes: SPEED -> ROUND -> POSITION -> HOMING
  mode = (Mode)((mode + 1) % 4);
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
  }
}

// ---------------- Setup ----------------
void startHoming() {
    mode = MODE_HOMING;
    running = true;
}

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
  pinMode(HALL_S, INPUT);

  // PWM channel
  ledcSetup(0, 20000, 8);
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
  ArduinoOTA.onStart([](){ Serial.println("OTA start"); });
  ArduinoOTA.onEnd([](){ Serial.println("\nOTA end"); });
  ArduinoOTA.onError([](ota_error_t e){ Serial.printf("OTA err %u\n", e); });
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
  startHoming();
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
      targetRPM += 5.0; if (targetRPM > 400.0) targetRPM = 400.0;
    }
    else
    {
      targetRounds += 0.1; if (targetRounds > 1000.0) targetRounds = 1000.0;
    }
    lastBtnUp = now;
  }
  if (!digitalRead(BUTTON_DOWN) && now - lastBtnDown > DEBOUNCE_MS)
  {
    if (mode == MODE_SPEED)
    {
      targetRPM -= 5.0; if (targetRPM < 0.0) targetRPM = 0.0;
    }
    else
    {
      targetRounds -= 0.1; if (targetRounds < 0.0) targetRounds = 0.0;
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
    mode = (Mode)((mode + 1) % 4);
    lastBtnMode = now;
  }

  updateSpeedMeasurement();

  if (mode == MODE_HOMING)
  {
    int pwm = HOMING_PWM * HOMING_DIRECTION;
    setMotorPWM(pwm);
    if (hallTriggered())
    {
      stopMotor();
      encoderCount = 0;
      currentRounds = 0;
      currentRPM = 0;
      mode = MODE_POSITION;
      running = false;
      Serial.println("Homing complete: encoder reset.");
    }
    drawOLED();
  }
  else if (now - lastPidMillis >= PID_SAMPLE_MS)
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
      else if (mode == MODE_ROUND || mode == MODE_POSITION)
      {
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
    int hallValue = analogRead(HALL_S);
    String s = "{";
    s += "\"mode\":\"" + String(mode == MODE_SPEED ? "SPEED" : mode == MODE_ROUND ? "ROUND" : mode == MODE_POSITION ? "POSITION" : "HOMING") + "\",";
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
    s += "\"pwm\":" + String(pwmVal) + ",";
    s += "\"hall\":" + String(hallValue);
    s += "}";
    webSocket.broadcastTXT(s);
    Serial.printf("Hall Value: %d\n", hallValue);
  }
  delay(5);
}
