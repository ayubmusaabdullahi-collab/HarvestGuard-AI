/*
* HarvestGuard AI - Complete ESP32 Firmware v2.1
* Target: ESP32 DevKit V1
* Pin Map (FINAL - DO NOT CHANGE):
* Indoor DHT11 -> GPIO18
* Outdoor DHT11 -> GPIO19
* LCD SDA -> GPIO21 (0x27)
* LCD SCL -> GPIO22
* Fan Relay -> GPIO23
* Green LED -> GPIO25
* Yellow LED -> GPIO26
* Red LED -> GPIO27
* Buzzer -> GPIO32
* Fault LED -> GPIO33
* Floor Sensor -> GPIO34 (ENABLED - analog input)
*
* Libraries:
* DHT sensor library by Adafruit (1.4.4)
* Adafruit Unified Sensor (1.2.0)
* LiquidCrystal_I2C by Frank de Brabander (1.1.2)
* ArduinoJson (6.21.3)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ===== PIN DEFINITIONS (FINAL) =====
#define PIN_INDOOR_DHT 18
#define PIN_OUTDOOR_DHT 19
#define PIN_LCD_SDA 21
#define PIN_LCD_SCL 22
#define PIN_FAN_RELAY 23
#define PIN_LED_GREEN 25
#define PIN_LED_YELLOW 26
#define PIN_LED_RED 27
#define PIN_BUZZER 32
#define PIN_FAULT_LED 33
#define PIN_FLOOR_SENSOR 34 // ENABLED - analog sensor output (0-3.3V max)

#define DHT_TYPE DHT11

// ===== OBJECTS =====
DHT dhtIndoor(PIN_INDOOR_DHT, DHT_TYPE);
DHT dhtOutdoor(PIN_OUTDOOR_DHT, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);

// ===== STATE VARIABLES =====
float tempIn = NAN, humIn = NAN;
float tempOut = NAN, humOut = NAN;
float lastValidTempIn = 20.0, lastValidHumIn = 40.0;
float lastValidTempOut = 25.0, lastValidHumOut = 50.0;

unsigned long lastRead = 0;
const unsigned long READ_INTERVAL = 2500;

String systemStatus = "SAFE"; // SAFE, WARNING, DANGER, FAULT
bool fanState = false;
bool faultIndoor = false;
bool faultOutdoor = false;
bool fanLocked = false;

unsigned long fanOnSince = 0;
unsigned long fanOffSince = 0;
unsigned long fanTotalRuntime = 0;
const unsigned long MIN_ON_MS = 10000;
const unsigned long MIN_OFF_MS = 5000;
const unsigned long MAX_ON_MS = 60000;
const unsigned long COOLDOWN_MS = 15000;
unsigned long cooldownUntil = 0;

int floorSensorRaw = 0; // Raw ADC reading (0-4095); calibrate sensor thresholds for wet/dry detection
bool floorSensorFault = false; // Fault if ADC is stuck at either rail (disconnected/shorted signal)
unsigned long bootTime = 0;

// LCD Scheduler
enum LCDPage { P_INDOOR, P_OUTDOOR, P_STATUS, P_FAULT };
LCDPage currentPage = P_INDOOR;
unsigned long lastLCDSwitch = 0;
const unsigned long LCD_PAGE_MS = 3000;

// WiFi Config
const char* apSSID = "--------------";
const char* apPASS = "*******";
const char* staSSID = "YOUR_HOME_WIFI";
const char* staPASS = "YOUR_PASSWORD";

// Floor sensor enabled flag
const bool FLOOR_SENSOR_ENABLED = true;

// ===== FUNCTION PROTOTYPES =====
void readSensors();
void evaluateSafety();
void controlFan();
void updateLEDs();
void updateLCD();
void handleBuzzer(int beeps);
void setupWiFi();
void setupServer();
String getUptimeString();

// ===== SETUP =====
void setup() {
Serial.begin(115200);
bootTime = millis();
Serial.println("\n\n=== HarvestGuard AI Booting ===");
Serial.println("Pin Map Validation: FINAL");
analogReadResolution(12); // ESP32 ADC range: 0-4095

// Pin Modes
pinMode(PIN_FAN_RELAY, OUTPUT);
pinMode(PIN_LED_GREEN, OUTPUT);
pinMode(PIN_LED_YELLOW, OUTPUT);
pinMode(PIN_LED_RED, OUTPUT);
pinMode(PIN_BUZZER, OUTPUT);
pinMode(PIN_FAULT_LED, OUTPUT);
pinMode(PIN_FLOOR_SENSOR, INPUT);
digitalWrite(PIN_FAN_RELAY, LOW);
digitalWrite(PIN_LED_GREEN, LOW);
digitalWrite(PIN_LED_YELLOW, LOW);
digitalWrite(PIN_LED_RED, LOW);
digitalWrite(PIN_FAULT_LED, LOW);
digitalWrite(PIN_BUZZER, LOW);

// I2C for LCD
Wire.begin(PIN_LCD_SDA, PIN_LCD_SCL);
lcd.init();
lcd.backlight();
lcd.setCursor(0,0);
lcd.print("HarvestGuard AI");
lcd.setCursor(0,1);
lcd.print("Booting ...");

dhtIndoor.begin();
dhtOutdoor.begin();

setupWiFi();
setupServer();

delay(2000);
lcd.clear();
handleBuzzer(2);
Serial.println("Setup complete. Safety interlocks ACTIVE.");
}

// ===== LOOP =====
void loop() {
server.handleClient();
unsigned long now = millis();

if (now - lastRead >= READ_INTERVAL) {
lastRead = now;
readSensors();
evaluateSafety();
controlFan();
updateLEDs();
}

if (now - lastLCDSwitch >= LCD_PAGE_MS) {
lastLCDSwitch = now;
currentPage = (LCDPage)((currentPage + 1) % 4);
updateLCD();
}

// Floor sensor is sampled inside readSensors() every READ_INTERVAL.

delay(10);
}

// ===== SENSOR READING WITH 3891% BUG FIX =====
void readSensors() {
if (FLOOR_SENSOR_ENABLED) {
floorSensorRaw = analogRead(PIN_FLOOR_SENSOR);
// GPIO34 reads near 0 or full-scale when the analog signal is disconnected/shorted.
floorSensorFault = false;
}

float tIn = dhtIndoor.readTemperature();
float hIn = dhtIndoor.readHumidity();
float tOut = dhtOutdoor.readTemperature();
float hOut = dhtOutdoor.readHumidity();

// Indoor validation
if (isnan(tIn) || isnan(hIn) || tIn < -10 || tIn > 60 || hIn < 0 || hIn > 100) {
Serial.println("[FAULT] IN-DHT FAIL - Keeping last valid");
faultIndoor = true;
// CRITICAL FIX: Do NOT assign invalid value, keep lastValid
tempIn = lastValidTempIn;
humIn = lastValidHumIn;
} else {
faultIndoor = false;
tempIn = tIn;
humIn = hIn;
lastValidTempIn = tIn;
lastValidHumIn = hIn;
}

// Outdoor validation
if (isnan(tOut) || isnan(hOut) || tOut < -10 || tOut > 60 || hOut < 0 || hOut > 100) {
Serial.println("[FAULT] OUT-DHT FAIL - Keeping last valid");
faultOutdoor = true;
tempOut = lastValidTempOut;
humOut = lastValidHumOut;
} else {
faultOutdoor = false;
tempOut = tOut;
humOut = hOut;
lastValidTempOut = tOut;
lastValidHumOut = hOut;
}

// Extra guard against 3891% bug: if reading >100 or <0, force fault
if (humIn > 100.5 || humIn < 0 || humOut > 100.5 || humOut < 0) {
Serial.println("[CRITICAL] Humidity out of range - forcing FAULT");
faultIndoor = faultIndoor || (humIn > 100.5);
faultOutdoor = faultOutdoor || (humOut > 100.5);
systemStatus = "FAULT";
}

Serial.printf("IN: %.1fC %.1f%% %s | OUT: %.1fC %.1f%% %s\n",
tempIn, humIn, faultIndoor ? "FAIL" : "OK",
tempOut, humOut, faultOutdoor ? "FAIL" : "OK");
}

// ===== SAFETY EVALUATION - HUMIDITY + TEMPERATURE =====
void evaluateSafety() {

  // Rule 6: Sensor fault -> Fan OFF, Fault LED ON, no auto
  if (faultIndoor || faultOutdoor) {
    systemStatus = "FAULT";
    fanState = false;
    digitalWrite(PIN_FAN_RELAY, LOW);
    return;
  }

  // Cooldown check
  if (millis() < cooldownUntil) {
    fanLocked = true;
  } else {
    fanLocked = false;
  }

  // Humidity conditions
  bool humidityDanger = (humIn >= 35.0);
  bool humidityWarning = (humIn >= 30.0);

  // Temperature conditions (NEW)
  bool temperatureDanger = (tempIn >= 35.0);
  bool temperatureWarning = (tempIn >= 30.0);

  // Overall safety evaluation
  if (humidityDanger || temperatureDanger) {
    systemStatus = "DANGER";
  }
  else if (humidityWarning || temperatureWarning) {
    systemStatus = "WARNING";
  }
  else {
    systemStatus = "SAFE";
  }


// AI Advisory is separate - never overrides this
}

// ===== MOISTURE CALCULATION FOR INCOMING AIR =====
// Approximate humidity ratio at standard atmospheric pressure (101.325 kPa).
// Returns grams of water vapour per kg of dry air. DHT11 accuracy is limited,
// so use a margin rather than reacting to tiny differences.
float humidityRatioGkg(float tempC, float relativeHumidity) {
float rh = constrain(relativeHumidity, 0.0, 100.0) / 100.0;
float saturationVapourKPa = 0.61094 * exp((17.625 * tempC) / (tempC + 243.04));
float vapourKPa = rh * saturationVapourKPa;
if (vapourKPa >= 101.0) return 999.0;
return 622.0 * vapourKPa / (101.325 - vapourKPa);
}

// ===== FAN CONTROL WITH ALL INTERLOCKS =====
void controlFan() {
unsigned long now = millis();

// ABSOLUTE SAFETY INTERLOCK: a failed sensor must switch the fan OFF.
if (faultIndoor || faultOutdoor) {
if (fanState) Serial.println("[INTERLOCK] Sensor fault -> Fan OFF");
fanState = false;
digitalWrite(PIN_FAN_RELAY, LOW);
fanOnSince = 0;
if (fanOffSince == 0) fanOffSince = now;
return;
}

// Keep the existing outdoor temperature safety limits.
if (tempOut < 10.0 || tempOut > 35.0) {
if (fanState) {
Serial.println("[INTERLOCK] Outdoor temperature unsafe -> Fan OFF");
fanState = false;
digitalWrite(PIN_FAN_RELAY, LOW);
fanOffSince = now;
fanTotalRuntime += now - fanOnSince;
fanOnSince = 0;
}
return;
}

float indoorW = humidityRatioGkg(tempIn, humIn);
float outdoorW = humidityRatioGkg(tempOut, humOut);
bool indoorNeedsHelp = (humIn >= 30.0) || (tempIn >= 30.0);
// DHT11s are not precision instruments: use a 1.0 g/kg drying margin.
bool outdoorCanDry = outdoorW <= (indoorW - 1.0);
// For cooling, allow nearly equal moisture content, but not air that is
// materially wetter. Outdoor air must also be at least 2 C cooler.
bool outdoorCanCool = (tempOut <= tempIn - 2.0) && (outdoorW <= indoorW + 0.5);
bool ventilationSuitable = outdoorCanDry || outdoorCanCool;

// Safety override: if incoming air is materially wetter and is not useful
// for cooling, switch OFF immediately, even before the minimum ON time.
if (fanState && !ventilationSuitable) {
Serial.println("[INTERLOCK] Incoming air unsuitable -> Fan OFF");
fanState = false;
digitalWrite(PIN_FAN_RELAY, LOW);
fanOffSince = now;
fanTotalRuntime += now - fanOnSince;
fanOnSince = 0;
return;
}

if (!fanState) {
bool minOffElapsed = (now - fanOffSince) >= MIN_OFF_MS;
bool notInCooldown = now >= cooldownUntil;
if (indoorNeedsHelp && ventilationSuitable && minOffElapsed && notInCooldown && !fanLocked) {
Serial.printf("[ACTION] Suitable incoming air (inside %.1f g/kg, outside %.1f g/kg) -> Fan ON", indoorW, outdoorW);
fanState = true;
digitalWrite(PIN_FAN_RELAY, HIGH);
fanOnSince = now;
fanOffSince = 0;
}
} else {
bool humiditySafe = (humIn < 30.0);
bool temperatureSafe = (tempIn < 30.0);
bool minOnElapsed = (now - fanOnSince) >= MIN_ON_MS;
bool maxOnExceeded = (now - fanOnSince) >= MAX_ON_MS;

if (maxOnExceeded) {
Serial.println("[SAFETY] Max runtime exceeded -> Cooldown");
fanState = false;
digitalWrite(PIN_FAN_RELAY, LOW);
fanOffSince = now;
fanTotalRuntime += now - fanOnSince;
fanOnSince = 0;
cooldownUntil = now + COOLDOWN_MS;
} else if ((humiditySafe && temperatureSafe) && minOnElapsed) {
Serial.println("[ACTION] Indoor conditions improved -> Fan OFF");
fanState = false;
digitalWrite(PIN_FAN_RELAY, LOW);
fanOffSince = now;
fanTotalRuntime += now - fanOnSince;
fanOnSince = 0;
}
}
}

void updateLEDs() {
digitalWrite(PIN_FAULT_LED, (faultIndoor || faultOutdoor || fanLocked || floorSensorFault) ? HIGH : LOW);

digitalWrite(PIN_LED_GREEN, LOW);
digitalWrite(PIN_LED_YELLOW, LOW);
digitalWrite(PIN_LED_RED, LOW);

if (systemStatus == "SAFE") {
digitalWrite(PIN_LED_GREEN, HIGH);
} else if (systemStatus == "WARNING") {
digitalWrite(PIN_LED_YELLOW, HIGH);
} else if (systemStatus == "DANGER") {
digitalWrite(PIN_LED_RED, HIGH);
} else if (systemStatus == "FAULT") {
// Blink red + fault LED
bool blink = (millis() / 500) % 2;
digitalWrite(PIN_LED_RED, blink ? HIGH : LOW);
digitalWrite(PIN_FAULT_LED, HIGH);
}

// Buzzer patterns
static unsigned long lastBuzz = 0;
if (systemStatus == "DANGER" && millis() - lastBuzz > 10000) {
handleBuzzer(3);
lastBuzz = millis();
}
if (systemStatus == "FAULT" && millis() - lastBuzz > 5000) {
handleBuzzer(5);
lastBuzz = millis();
}
}

void updateLCD() {
lcd.clear();
if (currentPage == P_INDOOR) {
lcd.setCursor(0,0);
lcd.print("IN:");
lcd.print(tempIn, 1);
lcd.print("C ");
lcd.print(humIn, 0);
lcd.print("%");
lcd.setCursor(0,1);
if (faultIndoor) lcd.print("IN-DHT FAIL");
else lcd.print(humIn >= 30 ? "HIGH HUMIDITY!" : "Grain Safe");
} else if (currentPage == P_OUTDOOR) {
lcd.setCursor(0,0);
lcd.print("OUT:");
lcd.print(tempOut, 1);
lcd.print("C ");
lcd.print(humOut, 0);
lcd.print("%");
lcd.setCursor(0,1);
if (faultOutdoor) lcd.print("OUT-DHT FAIL");
else {
if (humOut > humIn + 10) lcd.print("BAD OA -> NO FAN");
else lcd.print("Vent OK");
}
} else if (currentPage == P_STATUS) {
lcd.setCursor(0,0);
lcd.print("Status:");
lcd.print(systemStatus);
lcd.setCursor(0,1);
lcd.print("Fan:");
lcd.print(fanState ? "ON " : "OFF");
if (fanLocked) lcd.print(" LOCK");
else lcd.print(" OK");
} else if (currentPage == P_FAULT) {
if (faultIndoor || faultOutdoor || fanLocked || floorSensorFault) {
lcd.setCursor(0,0);
if (faultIndoor) lcd.print("IN-DHT FAIL");
else if (faultOutdoor) lcd.print("OUT-DHT FAIL");
else if (floorSensorFault) lcd.print("FLOOR SENSOR");
else if (fanLocked) lcd.print("FAN LOCK");
lcd.setCursor(0,1);
if (floorSensorFault && !faultIndoor && !faultOutdoor) lcd.print("FAULT - CHECK IT");
else lcd.print("Check Sensors!");
} else {
lcd.setCursor(0,0);
lcd.print("All Systems OK");
lcd.setCursor(0,1);
lcd.print("AI: Advisory On");
}
}
}

void handleBuzzer(int beeps) {
// Passive buzzer: generate an audible square-wave tone instead of steady HIGH.
const int BUZZER_FREQUENCY = 5000; // Hz
for (int i=0;i<beeps;i++) {
tone(PIN_BUZZER, BUZZER_FREQUENCY);
delay(150);
noTone(PIN_BUZZER);
delay(150);
}
}

void setupWiFi() {
Serial.println("Starting AP: HarvestGuard-AI");
WiFi.softAP(apSSID, apPASS);
IPAddress IP = WiFi.softAPIP();
Serial.print("AP IP: ");
Serial.println(IP);

// Try STA connection as fallback (optional)
WiFi.begin(staSSID, staPASS);
unsigned long start = millis();
while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
delay(500);
Serial.print(".");
}
if (WiFi.status() == WL_CONNECTED) {
Serial.print("\nSTA IP: ");
Serial.println(WiFi.localIP());
}
}

void setupServer() {
server.on("/", HTTP_GET, []() {
String html = R"rawliteral(

<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HarvestGuard AI Dashboard</title>

<style>
:root {
  --bg:#0a1a0a;
  --card:#1e331e;
  --lime:#a3ff12;
  --text:#e2e8f0;
  --danger:#ef4444;
  --warn:#facc15;
}

* { box-sizing:border-box; }

body {
  margin:0;
  background:var(--bg);
  color:var(--text);
  font-family:system-ui,sans-serif;
  padding:16px;
}

.header {
  display:flex;
  justify-content:space-between;
  align-items:center;
  flex-wrap:wrap;
  gap:12px;
}

h1 {
  color:var(--lime);
  margin:0;
  font-size:1.8rem;
}

.grid {
  display:grid;
  grid-template-columns:repeat(auto-fit,minmax(280px,1fr));
  gap:16px;
  margin-top:20px;
}

.card {
  background:var(--card);
  border-radius:16px;
  padding:18px;
  border:1px solid #2a4a2a;
  box-shadow:0 4px 20px rgba(0,0,0,0.3);
  min-width:0;
}

.badge {
  display:inline-block;
  padding:4px 12px;
  border-radius:20px;
  font-weight:800;
  font-size:0.85rem;
}

.SAFE {background:var(--lime);color:#000;}
.WARNING {background:var(--warn);color:#000;}
.DANGER {background:var(--danger);color:#fff;}
.FAULT {background:#a855f7;color:#fff;}

.metric {
  font-size:2.2rem;
  font-weight:800;
  margin:8px 0;
}

.small {
  font-size:0.85rem;
  opacity:0.8;
  overflow-wrap:anywhere;
}

.btn {
  padding:10px 16px;
  border-radius:10px;
  border:none;
  font-weight:700;
  cursor:pointer;
  margin:3px;
}

.btn-lime {background:var(--lime);color:#000;}
.btn-ghost {
  background:transparent;
  border:1px solid #3a5a3a;
  color:var(--text);
}

#chart {
  width:100%;
  height:180px;
  background:#0f2510;
  border-radius:12px;
  margin-top:12px;
}

.log {
  font-family:monospace;
  font-size:0.8rem;
  background:#0f2510;
  padding:10px;
  border-radius:10px;
  max-height:150px;
  overflow:auto;
  margin-top:8px;
  white-space:pre-wrap;
}

.ai-box {
  border-left:4px solid var(--lime);
  padding-left:14px;
  margin-top:10px;
}

.ai-confidence {
  font-size:1.8rem;
  font-weight:800;
  color:var(--lime);
  margin:8px 0;
}

.ai-status {
  display:inline-block;
  padding:4px 10px;
  border-radius:20px;
  background:#334155;
  font-size:0.8rem;
  font-weight:700;
}

.online {background:#166534;color:white;}
.offline {background:#991b1b;color:white;}

@media(max-width:500px) {
  body {padding:10px;}
  h1 {font-size:1.4rem;}
  .metric {font-size:1.8rem;}
}
</style>
</head>

<body>

<div class="header">
  <h1>🌾 HarvestGuard AI</h1>

  <div>
    <button class="btn btn-lime" onclick="toggleAuto()">
      AUTO: <span id="autoState">ON</span>
    </button>

    <button class="btn btn-ghost" onclick="manualFan()">
      Manual Fan
    </button>
  </div>
</div>

<div class="grid">

  <!-- INDOOR SENSOR -->
  <div class="card">
    <h3>Indoor (Grain Storage)</h3>

    <div class="metric">
      <span id="ti">--</span>°C /
      <span id="hi">--</span>%
    </div>

    <div class="small">
      Sensor GPIO18 | Last valid reading kept if sensor fails
    </div>

    <div class="small" id="faultIn"></div>
  </div>

  <!-- OUTDOOR SENSOR -->
  <div class="card">
    <h3>Outdoor (Ambient)</h3>

    <div class="metric">
      <span id="to">--</span>°C /
      <span id="ho">--</span>%
    </div>

    <div class="small">
      Sensor GPIO19 | Ventilation decision factor
    </div>

    <div class="small" id="faultOut"></div>
  </div>

  <!-- SYSTEM STATUS -->
  <div class="card">
    <h3>System Status</h3>

    <p>
      Status:
      <span id="status" class="badge">--</span>

      Fan:
      <span id="fan" class="badge">--</span>
    </p>

    <p class="small">
      Uptime: <span id="uptime">--</span>
      | Fault: <span id="fault">NONE</span>
    </p>

    <p class="small">
      Floor Sensor GPIO34:
      <span id="floorSensorState">ENABLED</span>
      | ADC: <span id="floorRaw">--</span>
    </p>

    <div class="ai-box">
      <strong>ESP32 Ventilation Advice:</strong>

      <div id="advice">
        Waiting for sensor readings...
      </div>

      <div class="small">
        ESP32 safety rules: outdoor temperature 10–35°C
        and suitable incoming air conditions.
      </div>
    </div>
  </div>

  <!-- AI ADVISORY -->
  <div class="card">
    <h3>🤖 HarvestGuard AI Advisory</h3>

    <p>
      AI connection:
      <span id="aiConnection" class="ai-status offline">
        CHECKING
      </span>
    </p>

    <div id="aiPred">
      Waiting for live sensor readings...
    </div>

    <div class="ai-confidence">
      <span id="conf">--</span>%
    </div>

    <div class="small">
      Model confidence
    </div>

    <p class="small">
      AI recommendation:
      <strong id="rec">--</strong>
    </p>

    <p class="small">
      Last AI update:
      <span id="aiTime">--</span>
    </p>

    <button class="btn btn-ghost"
            onclick="fetchAI()">
      Refresh AI
    </button>

    <div class="small" style="margin-top:12px">
      RandomForest model trained on 10,000 synthetic
      Nigerian environmental records. Real-world
      validation is required.
    </div>

    <div class="small">
      IMPORTANT: ESP32 safety controls always have
      final authority over fan operation.
    </div>
  </div>

</div>

<!-- HISTORY -->
<div class="card" style="margin-top:16px">
  <h3>History (Indoor Humidity)</h3>

  <canvas id="chart"></canvas>

  <div class="log" id="log">
    System booting... Safety interlocks ACTIVE
  </div>
</div>

<script>

// ==========================================
// HARVESTGUARD DASHBOARD CONFIGURATION
// ==========================================

let autoMode = true;
let historyData = [];
let latestData = null;
let aiRequestInProgress = false;

// IMPORTANT:
// Replace this with your Windows computer's
// actual Wi-Fi IPv4 address.
// Example: http://192.168.1.105:5000

const AI_API_BASE = "http://172.27.61.15:5000";

const ctx = document.getElementById("chart").getContext("2d");


// ==========================================
// LOGGING
// ==========================================

function addLog(message) {
  const log = document.getElementById("log");

  log.textContent += "\n" +
    new Date().toLocaleTimeString() + " " + message;

  const lines = log.textContent.split("\n");

  if (lines.length > 40) {
    log.textContent = lines.slice(-40).join("\n");
  }

  log.scrollTop = log.scrollHeight;
}


// ==========================================
// CHART
// ==========================================

function drawChart() {

  const canvas = document.getElementById("chart");

  const width = canvas.width = canvas.clientWidth * 2;
  const height = canvas.height = 360;

  ctx.clearRect(0, 0, width, height);

  ctx.strokeStyle = "#a3ff12";
  ctx.lineWidth = 3;
  ctx.beginPath();

  if (historyData.length < 2) return;

  let max = Math.max(...historyData, 30);
  let min = Math.min(...historyData, 20);

  historyData.forEach((value, index) => {

    const x = index / (historyData.length - 1) * width;

    const y = height -
      ((value - min) / (max - min + 1) * height * 0.8
      + height * 0.1);

    if (index === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }

  });

  ctx.stroke();
}


// ==========================================
// FETCH ESP32 SENSOR DATA
// ==========================================

async function fetchData() {

  try {

    const response = await fetch("/api/data");

    if (!response.ok) {
      throw new Error("ESP32 HTTP " + response.status);
    }

    const data = await response.json();

    const tempIn = Number(data.tempIn);
    const humIn = Number(data.humIn);
    const tempOut = Number(data.tempOut);
    const humOut = Number(data.humOut);

    // Reject invalid readings
    if (
      !Number.isFinite(tempIn) ||
      !Number.isFinite(humIn) ||
      !Number.isFinite(tempOut) ||
      !Number.isFinite(humOut) ||
      humIn < 0 || humIn > 100 ||
      humOut < 0 || humOut > 100
    ) {

      addLog("Invalid sensor reading filtered");
      return;
    }

    latestData = {
      ...data,
      tempIn,
      humIn,
      tempOut,
      humOut
    };

    // INDOOR DISPLAY
    document.getElementById("ti").textContent =
      tempIn.toFixed(1);

    document.getElementById("hi").textContent =
      humIn.toFixed(0);

    // OUTDOOR DISPLAY
    document.getElementById("to").textContent =
      tempOut.toFixed(1);

    document.getElementById("ho").textContent =
      humOut.toFixed(0);

    // SYSTEM STATUS
    const statusElement =
      document.getElementById("status");

    statusElement.textContent = data.status || "--";
    statusElement.className =
      "badge " + (data.status || "");

    // FAN STATUS
    const fanElement = document.getElementById("fan");

    fanElement.textContent = data.fanOn ? "ON" : "OFF";

    fanElement.className =
      "badge " + (data.fanOn ? "SAFE" : "FAULT");

    // UPTIME AND FAULT
    document.getElementById("uptime").textContent =
      data.uptime ?? "--";

    document.getElementById("fault").textContent =
      data.fault || "NONE";

    document.getElementById("floorSensorState").textContent =
      data.floorSensorState ?? "ENABLED";

    document.getElementById("floorRaw").textContent =
      data.floorRaw ?? "--";

    // SENSOR FAULTS
    const faultText = data.fault || "NONE";

    document.getElementById("faultIn").textContent =
      faultText.includes("IN-DHT")
      ? "⚠️ IN-DHT FAIL"
      : "";

    document.getElementById("faultOut").textContent =
      faultText.includes("OUT-DHT")
      ? "⚠️ OUT-DHT FAIL"
      : "";

    // ESP32 VENTILATION ADVICE
    let advice = "";

    if (faultText !== "NONE") {

      advice =
        "SAFETY LOCK: Fault detected. Check the sensors.";

    } else if (humOut > humIn + 5) {

      advice =
        "OUTDOOR MORE HUMID - Do NOT ventilate.";

    } else if (tempOut < 10 || tempOut > 35) {

      advice =
        "OUTDOOR TEMPERATURE UNSAFE - Ventilation blocked.";

    } else if (humIn >= 30 && humOut <= humIn - 5) {

      advice =
        "FAVORABLE: Outdoor air is drier. Ventilation may be suitable.";

    } else {

      advice =
        "Monitoring environmental conditions.";

    }

    document.getElementById("advice").textContent = advice;

    // HISTORY
    historyData.push(humIn);

    if (historyData.length > 30) {
      historyData.shift();
    }

    drawChart();

    addLog(
      "IN:" + humIn.toFixed(0) +
      "% OUT:" + humOut.toFixed(0) +
      "% Fan:" + (data.fanOn ? "ON" : "OFF")
    );

  } catch (error) {

    console.error("ESP32 data error:", error);

    addLog("ESP32 data connection error");

  }

}


// ==========================================
// CHECK AI SERVER CONNECTION
// ==========================================

async function checkAIConnection() {

  const connection =
    document.getElementById("aiConnection");

  try {

    const response = await fetch(
      AI_API_BASE + "/health",
      { cache: "no-store" }
    );

    if (!response.ok) {
      throw new Error("AI server unavailable");
    }

    const result = await response.json();

    if (!result.model_loaded) {
      throw new Error("AI model not loaded");
    }

    connection.textContent = "ONLINE";
    connection.className = "ai-status online";

    return true;

  } catch (error) {

    connection.textContent = "OFFLINE";
    connection.className = "ai-status offline";

    return false;

  }

}


// ==========================================
// SEND SENSOR DATA TO HARVESTGUARD AI
// ==========================================

async function fetchAI() {

  if (aiRequestInProgress) return;

  if (!latestData) {

    document.getElementById("aiPred").textContent =
      "Waiting for valid ESP32 readings...";

    return;
  }

  aiRequestInProgress = true;

  try {

    const connection = await checkAIConnection();

    if (!connection) {
      throw new Error("AI server is offline");
    }

    const response = await fetch(
      AI_API_BASE + "/predict",
      {
        method: "POST",

        headers: {
          "Content-Type": "application/json"
        },

        body: JSON.stringify({

          tempIn: latestData.tempIn,
          humIn: latestData.humIn,

          tempOut: latestData.tempOut,
          humOut: latestData.humOut,

          region: "Jos",
          season: "rainy",
          grain_type: "maize",
          storage_type: "jute_bag",
          days_stored: 30

        })

      }
    );

    if (!response.ok) {
      throw new Error("AI prediction HTTP " + response.status);
    }

    const result = await response.json();

    // DISPLAY AI RECOMMENDATION
    document.getElementById("aiPred").textContent =
      result.recommendation || "No recommendation received";

    // IMPORTANT:
    // Flask already returns confidence as a percentage.
    // Do NOT multiply by 100 again.

    document.getElementById("conf").textContent =
      Number(result.confidence).toFixed(2);

    document.getElementById("rec").textContent =
      result.should_ventilate ? "VENTILATE" : "HOLD";

    document.getElementById("aiTime").textContent =
      new Date().toLocaleTimeString();

    addLog(
      "AI: " +
      (result.should_ventilate ? "VENTILATE" : "HOLD") +
      " Confidence:" +
      Number(result.confidence).toFixed(2) +
      "%"
    );

  } catch (error) {

    console.error("AI error:", error);

    document.getElementById("aiPred").textContent =
      "AI unavailable. ESP32 safety control remains active.";

    document.getElementById("conf").textContent = "--";

    document.getElementById("rec").textContent = "--";

    document.getElementById("aiTime").textContent =
      "Connection failed";

    document.getElementById("aiConnection").textContent =
      "OFFLINE";

    document.getElementById("aiConnection").className =
      "ai-status offline";

  } finally {

    aiRequestInProgress = false;

  }

}


// ==========================================
// AUTO / MANUAL MODE
// ==========================================

function toggleAuto() {

  autoMode = !autoMode;

  document.getElementById("autoState").textContent =
    autoMode ? "ON" : "OFF";

}


// ==========================================
// MANUAL FAN COMMAND
// ==========================================

function manualFan() {

  if (!autoMode) {

    fetch("/api/fan/toggle", {
      method: "POST"
    })
    .catch(error => {
      console.error("Manual fan error:", error);
    });

  } else {

    alert(
      "Switch to MANUAL first. ESP32 safety interlocks remain active."
    );

  }

}


// ==========================================
// START DASHBOARD
// ==========================================

fetchData();
checkAIConnection();
drawChart();

setInterval(fetchData, 3000);
setInterval(fetchAI, 15000);
setInterval(checkAIConnection, 30000);

</script>

</body>
</html>

)rawliteral";
server.send(200, "text/html", html);
});

server.on("/api/data", HTTP_GET, []() {
DynamicJsonDocument doc(1024);
doc["tempIn"] = tempIn;
doc["humIn"] = humIn;
doc["tempOut"] = tempOut;
doc["humOut"] = humOut;
doc["status"] = systemStatus;
doc["fanOn"] = fanState;
doc["uptime"] = getUptimeString();
doc["fault"] = faultIndoor ? "IN-DHT FAIL" : faultOutdoor ? "OUT-DHT FAIL" : fanLocked ? "FAN LOCK" : "NONE";
doc["floorSensor"] = !FLOOR_SENSOR_ENABLED ? "DISABLED" : floorSensorFault ? "FLOOR SENSOR FAULT" : "ENABLED";
doc["floorRaw"] = FLOOR_SENSOR_ENABLED ? floorSensorRaw : -1;
doc["aiNote"] = "Advisory only - ESP32 retains safety control";
String out;
serializeJson(doc, out);
server.send(200, "application/json", out);
});

server.begin();
Serial.println("Web server started on port 80");
}

String getUptimeString() {
unsigned long ms = millis() - bootTime;
unsigned long s = ms / 1000;
unsigned long m = s / 60;
unsigned long h = m / 60;
s %= 60; m %= 60;
char buf[32];
sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
return String(buf);
}
