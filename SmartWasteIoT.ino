#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// ----- CONFIG -----
const char* ssid = "ROBOT1"; // your WiFi SSID
const char* password = "12345678"; // your WiFi password
const char* thingspeakApiKey = "YOUR_THINGSPEAK_API_KEY"; // replace with real key or keep as ""
// Hardware pins (NodeMCU / ESP8266 Dn names)
const uint8_t TRIG_PIN = D5; // HC-SR04 TRIG
const uint8_t ECHO_PIN = D6; // HC-SR04 ECHO (USE VOLTAGE DIVIDER: HC-SR04 echo is 5V!)
const uint8_t BUZZER_PIN = D4; // Buzzer (active HIGH)
const uint8_t I2C_SDA = D2; // SDA
const uint8_t I2C_SCL = D1; // SCL
// Bin geometry & thresholds
const int BIN_HEIGHT_CM = 18; // physical inside height of bin in cm
const int FULL_THRESHOLD = 80; // percent above which we say "FULL" and buzz
const unsigned long THINGSPEAK_INTERVAL = 20000UL; // 20 seconds between ThingSpeak updates
// ----- GLOBALS -----
LiquidCrystal_I2C lcd(0x27, 16, 2);
ESP8266WebServer server(80);
WiFiClient client;
unsigned long lastThingSpeakMillis = 0;
unsigned long lastMeasurementMillis = 0;
const unsigned long MEASURE_INTERVAL = 1500UL; // measure every 1.5s
float lastDistanceCm = 0.0;
int lastLevelPercent = 0;
// ---------- Helper: measure distance (HC-SR04) ----------
float measureDistanceCM() {
// Ensure trigger low
digitalWrite(TRIG_PIN, LOW);
delayMicroseconds(2);
// Trigger 10us pulse
digitalWrite(TRIG_PIN, HIGH);
delayMicroseconds(10);
digitalWrite(TRIG_PIN, LOW);
// Read echo (reduced timeout to prevent blocking)
// 12000us ~ 2m, more than enough for a dustbin
unsigned long duration = pulseIn(ECHO_PIN, HIGH, 12000UL);
if (duration == 0) {
// no echo (timeout) -> return beyond bin height
return (float)BIN_HEIGHT_CM + 5.0;
}
// distance in cm = (duration * 0.0343) / 2
float distance = (duration * 0.0343f) / 2.0f;
return distance;
}
// ---------- Helper: compute filled percentage ----------
int distanceToPercent(float distanceCm) {
if (distanceCm >= BIN_HEIGHT_CM) return 0;
if (distanceCm <= 0) return 100;
float filled = ((float)BIN_HEIGHT_CM - distanceCm) / (float)BIN_HEIGHT_CM * 100.0f;
if (filled < 0.0f) filled = 0.0f;
if (filled > 100.0f) filled = 100.0f;
return (int)round(filled);
}
// ---------- Send data to ThingSpeak (simple HTTP GET) ----------
void sendToThingSpeak(int percent, float distanceCm) {
if (strlen(thingspeakApiKey) == 0) return; // no key provided
if (!client.connect("api.thingspeak.com", 80)) {
Serial.println("ThingSpeak connect failed");
client.stop();
return;
}
String url = "/update?api_key=";
url += thingspeakApiKey;
url += "&field1=" + String(percent);
url += "&field2=" + String(distanceCm, 2);
client.print(String("GET ") + url + " HTTP/1.1\r\n" +
"Host: api.thingspeak.com\r\n" +
"Connection: close\r\n\r\n");
// read & discard response (optional)
unsigned long start = millis();
while (client.connected() && millis() - start < 1500) {
while (client.available()) {
client.read(); // discard
}
yield();
}
client.stop();
Serial.println("ThingSpeak: update sent");
}
// ---------- /status JSON endpoint (NO CACHE) ----------
void handleStatus() {
String statusText = (lastLevelPercent >= FULL_THRESHOLD) ? "FULL" : "OK";
String json = "{";
json += "\"level\":" + String(lastLevelPercent) + ",";
json += "\"distance\":" + String(lastDistanceCm, 2) + ",";
json += "\"status\":\"" + statusText + "\",";
json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
json += "}";
server.sendHeader("Access-Control-Allow-Origin", "*");
server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
server.sendHeader("Pragma", "no-cache");
server.sendHeader("Expires", "0");
server.send(200, "application/json", json);
}
// ---------- Live web page (polls /status) ----------
// REPLACE your handleRoot() with this professional UI version
// (Works with the /status JSON endpoint we added earlier)
void handleRoot() {
String html = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>Smart Dustbin (ESP8266) — Real-time</title>
<style>
:root{
--bg1:#0ea5e9; /* sky */
--bg2:#22c55e; /* green */
--card:#ffffff;
--text:#0f172a; /* slate-900 */
--muted:#64748b; /* slate-500 */
--ok:#22c55e;
--warn:#f59e0b;
--full:#ef4444;
--shadow: 0 16px 40px rgba(2,6,23,.18);
--radius: 18px;
}
*{box-sizing:border-box}
body{
margin:0;
font-family: ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, Arial, "Noto Sans", "Apple
Color Emoji","Segoe UI Emoji";
color:var(--text);
background: radial-gradient(1200px 700px at 10% 10%, rgba(255,255,255,.35), transparent 60%),
linear-gradient(135deg, var(--bg1), var(--bg2));
min-height:100vh;
display:flex;
align-items:center;
justify-content:center;
padding:18px;
}
.wrap{
width:min(920px, 100%);
display:grid;
gap:14px;
}
.topbar{
display:flex;
justify-content:space-between;
align-items:center;
gap:12px;
color:white;
padding:4px 2px;
}
.brand{
display:flex;
align-items:center;
gap:10px;
}
.logo{
width:44px;height:44px;border-radius:14px;
background: rgba(255,255,255,.2);
backdrop-filter: blur(10px);
box-shadow: 0 12px 30px rgba(2,6,23,.18);
display:grid;place-items:center;
border:1px solid rgba(255,255,255,.25);
}
.title{
line-height:1.1;
}
.title h1{
margin:0;
font-size:18px;
font-weight:800;
letter-spacing:.2px;
}
.title p{
margin:2px 0 0 0;
font-size:12px;
opacity:.9;
}
.pill{
display:flex; align-items:center; gap:8px;
padding:10px 12px;
border-radius:999px;
background: rgba(255,255,255,.18);
border:1px solid rgba(255,255,255,.22);
backdrop-filter: blur(10px);
color:white;
font-size:13px;
}
.dot{
width:10px;height:10px;border-radius:50%;
background:#ffffff;
opacity:.9;
box-shadow: 0 0 0 4px rgba(255,255,255,.15);
}
.dot.ok{background: #bbf7d0;}
.dot.bad{background: #fecaca;}
.card{
background: var(--card);
border-radius: var(--radius);
box-shadow: var(--shadow);
padding:18px;
border: 1px solid rgba(2,6,23,.06);
}
.grid{
display:grid;
grid-template-columns: 1.05fr .95fr;
gap:14px;
}
@media (max-width: 840px){
.grid{grid-template-columns: 1fr}
}
/* Left: Dustbin visualization */
.binArea{
display:grid;
grid-template-columns: 1fr;
gap:14px;
align-items:center;
justify-items:center;
padding:10px;
}
.binWrap{
width: min(340px, 100%);
display:grid;
justify-items:center;
gap:12px;
}
.binTitle{
display:flex;
align-items:center;
justify-content:space-between;
width:100%;
gap:10px;
}
.statusBadge{
display:inline-flex; align-items:center; gap:8px;
padding:8px 12px;
border-radius: 999px;
font-weight:700;
font-size:12px;
letter-spacing:.2px;
border: 1px solid rgba(2,6,23,.08);
background: #f8fafc;
color: var(--text);
}
.badgeDot{
width:10px;height:10px;border-radius:50%;
background: var(--ok);
}
.bin{
width: 210px;
height: 280px;
border-radius: 24px;
position: relative;
background: #f8fafc;
border: 3px solid rgba(15,23,42,.4);
overflow:hidden;
box-shadow: inset 0 0 0 8px rgba(2,6,23,.03);
}
.lid{
position:absolute;
top:-18px;
left:50%;
transform:translateX(-50%);
width: 240px;
height: 34px;
border-radius: 16px;
background: rgba(15,23,42,.55);
box-shadow: 0 10px 20px rgba(2,6,23,.22);
}
.fill{
position:absolute;
left:0; bottom:0;
width:100%;
height:0%;
background: linear-gradient(180deg, rgba(255,255,255,.22), transparent 25%),
linear-gradient(180deg, #4ade80, #22c55e);
transition: height .55s ease, filter .55s ease, background .55s ease;
}
.stripes{
position:absolute; inset:0;
background: repeating-linear-gradient(
0deg,
rgba(255,255,255,.18),
rgba(255,255,255,.18) 2px,
transparent 2px,
transparent 32px
);
mix-blend-mode: overlay;
pointer-events:none;
opacity:.7;
}
.bigPercent{
font-size: 46px;
font-weight: 900;
margin:0;
letter-spacing:-1px;
}
.subText{
margin:0;
color:var(--muted);
font-size:13px;
line-height:1.4;
}
/* Right: Metrics */
.metrics{
display:grid;
gap:14px;
}
.metricGrid{
display:grid;
grid-template-columns: 1fr 1fr;
gap:12px;
}
@media (max-width: 520px){
.metricGrid{grid-template-columns:1fr}
}
.metric{
padding:14px;
border-radius: 16px;
background: #f8fafc;
border: 1px solid rgba(2,6,23,.07);
display:flex;
gap:12px;
align-items:center;
}
.icon{
width:44px;height:44px;border-radius:14px;
background: rgba(14,165,233,.12);
display:grid; place-items:center;
border: 1px solid rgba(14,165,233,.18);
flex: 0 0 auto;
}
.metric:nth-child(2) .icon{ background: rgba(34,197,94,.12); border-color: rgba(34,197,94,.18); }
.metric:nth-child(3) .icon{ background: rgba(245,158,11,.14); border-color: rgba(245,158,11,.18); }
.metric:nth-child(4) .icon{ background: rgba(148,163,184,.18); border-color:
rgba(148,163,184,.22); }
.mLabel{
font-size:12px;
color: var(--muted);
margin:0;
}
.mValue{
font-size:18px;
font-weight:900;
margin:2px 0 0 0;
letter-spacing:.2px;
}
.footer{
display:flex;
justify-content:space-between;
align-items:center;
gap:10px;
color: var(--muted);
font-size:12px;
padding-top: 6px;
border-top: 1px dashed rgba(2,6,23,.12);
}
.note{
color: var(--muted);
font-size:12px;
margin-top:-4px;
}
.barWrap{
width:100%;
background:#e2e8f0;
height:12px;
border-radius:999px;
overflow:hidden;
border: 1px solid rgba(2,6,23,.08);
}
.bar{
height:100%;
width:0%;
background: linear-gradient(90deg, #22c55e, #f59e0b, #ef4444);
transition: width .5s ease;
}
.miniRow{
display:flex;
justify-content:space-between;
width:100%;
font-size:12px;
color: var(--muted);
margin-top:6px;
}
</style>
</head>
<body>
<div class="wrap">
<div class="topbar">
<div class="brand">
<div class="logo" aria-hidden="true">
<!-- simple bin icon -->
<svg width="22" height="22" viewBox="0 0 24 24" fill="none">
<path d="M9 3h6" stroke="white" stroke-width="2" stroke-linecap="round"/>
<path d="M4 6h16" stroke="white" stroke-width="2" stroke-linecap="round"/>
<path d="M7 6l1 15h8l1-15" stroke="white" stroke-width="2" stroke-linecap="round" strokelinejoin="round"/>
</svg>
</div>
<div class="title">
<h1>Smart Dustbin (ESP8266)</h1>
<p>Real-time monitoring dashboard</p>
</div>
</div>
<div class="pill" title="Connection status">
<span id="dot" class="dot bad"></span>
<span id="connText">Connecting…</span>
</div>
</div>
<div class="card">
<div class="grid">
<!-- Left -->
<div class="binArea">
<div class="binWrap">
<div class="binTitle">
<div class="statusBadge">
<span class="badgeDot" id="badgeDot"></span>
<span>Status: </span><span id="status">--</span>
</div>
<div class="note" id="lastSeen">Last update: --</div>
</div>
<div class="bin" aria-label="Dustbin level">
<div class="lid"></div>
<div id="fill" class="fill"></div>
<div class="stripes"></div>
</div>
<p class="bigPercent"><span id="levelMain">--</span>%</p>
<div class="barWrap" aria-hidden="true"><div id="bar" class="bar"></div></div>
<div class="miniRow">
<span>0% (Empty)</span>
<span>100% (Full)</span>
</div>
<p class="subText">
When level ≥ <b>80%</b>, bin turns <b style="color:#ef4444">red</b> and buzzer sounds.
</p>
</div>
</div>
<!-- Right -->
<div class="metrics">
<div class="metricGrid">
<div class="metric">
<div class="icon" aria-hidden="true">
<!-- gauge icon -->
<svg width="22" height="22" viewBox="0 0 24 24" fill="none">
<path d="M21 12a9 9 0 10-18 0" stroke="#0f172a" stroke-width="2" strokelinecap="round"/>
<path d="M12 12l4-4" stroke="#0f172a" stroke-width="2" stroke-linecap="round"/>
<path d="M7 16h10" stroke="#0f172a" stroke-width="2" stroke-linecap="round"/>
</svg>
</div>
<div>
<p class="mLabel">Garbage Level</p>
<p class="mValue"><span id="level">--</span>%</p>
</div>
</div>
<div class="metric">
<div class="icon" aria-hidden="true">
<!-- ruler icon -->
<svg width="22" height="22" viewBox="0 0 24 24" fill="none">
<path d="M5 19V5h14v14H5z" stroke="#0f172a" stroke-width="2"/>
<path d="M8 8h3M8 11h6M8 14h3M8 17h6" stroke="#0f172a" stroke-width="2" strokelinecap="round"/>
</svg>
</div>
<div>
<p class="mLabel">Distance (Ultrasonic)</p>
<p class="mValue"><span id="distance">--</span> cm</p>
</div>
</div>
<div class="metric">
<div class="icon" aria-hidden="true">
<!-- alert icon -->
<svg width="22" height="22" viewBox="0 0 24 24" fill="none">
<path d="M12 9v4" stroke="#0f172a" stroke-width="2" stroke-linecap="round"/>
<path d="M12 17h.01" stroke="#0f172a" stroke-width="3" stroke-linecap="round"/>
<path d="M10.3 4.3h3.4L22 20H2L10.3 4.3z" stroke="#0f172a" stroke-width="2" strokelinejoin="round"/>
</svg>
</div>
<div>
<p class="mLabel">Bin State</p>
<p class="mValue" id="stateText">--</p>
</div>
</div>
<div class="metric">
<div class="icon" aria-hidden="true">
<!-- wifi icon -->
<svg width="22" height="22" viewBox="0 0 24 24" fill="none">
<path d="M2 8c5-4 15-4 20 0" stroke="#0f172a" stroke-width="2" strokelinecap="round"/>
<path d="M5 12c3.5-3 10.5-3 14 0" stroke="#0f172a" stroke-width="2" strokelinecap="round"/>
<path d="M8 16c2-2 6-2 8 0" stroke="#0f172a" stroke-width="2" stroke-linecap="round"/>
<path d="M12 20h.01" stroke="#0f172a" stroke-width="3" stroke-linecap="round"/>
</svg>
</div>
<div>
<p class="mLabel">Device IP</p>
<p class="mValue"><span id="ip">--</span></p>
</div>
</div>
</div>
<div class="footer">
<span>Auto refresh: <b>1 sec</b></span>
<span id="hint">Tip: keep bin height set correctly.</span>
</div>
</div>
</div>
</div>
</div>
<script>
const FULL_THRESHOLD = 80;
const el = {
fill: document.getElementById('fill'),
levelMain: document.getElementById('levelMain'),
level: document.getElementById('level'),
distance: document.getElementById('distance'),
status: document.getElementById('status'),
stateText: document.getElementById('stateText'),
ip: document.getElementById('ip'),
dot: document.getElementById('dot'),
connText: document.getElementById('connText'),
badgeDot: document.getElementById('badgeDot'),
lastSeen: document.getElementById('lastSeen'),
bar: document.getElementById('bar')
};
function setConnected(ok){
el.dot.className = 'dot ' + (ok ? 'ok' : 'bad');
el.connText.textContent = ok ? 'Live' : 'Offline';
}
function setColors(level){
// Fill gradient based on level
if(level >= FULL_THRESHOLD){
el.fill.style.background =
'linear-gradient(180deg, rgba(255,255,255,.18), transparent 25%), linear-gradient(180deg,
#fecaca, #ef4444)';
el.badgeDot.style.background = '#ef4444';
}else if(level >= 50){
el.fill.style.background =
'linear-gradient(180deg, rgba(255,255,255,.18), transparent 25%), linear-gradient(180deg,
#fde68a, #f59e0b)';
el.badgeDot.style.background = '#f59e0b';
}else{
el.fill.style.background =
'linear-gradient(180deg, rgba(255,255,255,.22), transparent 25%), linear-gradient(180deg,
#4ade80, #22c55e)';
el.badgeDot.style.background = '#22c55e';
}
}
async function updateStatus(){
try{
const res = await fetch('/status?t=' + Date.now(), {cache:'no-store'});
if(!res.ok) throw new Error('HTTP ' + res.status);
const data = await res.json();
const level = Number(data.level);
const distance = Number(data.distance);
// update text
el.status.textContent = data.status ?? '--';
el.levelMain.textContent = isFinite(level) ? level : '--';
el.level.textContent = isFinite(level) ? level : '--';
el.distance.textContent = isFinite(distance) ? distance.toFixed(2) : '--';
el.ip.textContent = data.ip ?? '--';
// state text
el.stateText.textContent = (level >= FULL_THRESHOLD) ? 'FULL (Alert)' : 'OK (Normal)';
// update fill & bar
if(isFinite(level)){
el.fill.style.height = Math.max(0, Math.min(100, level)) + '%';
el.bar.style.width = Math.max(0, Math.min(100, level)) + '%';
setColors(level);
}
// connection + last update
setConnected(true);
const now = new Date();
el.lastSeen.textContent = 'Last update: ' + now.toLocaleTimeString();
}catch(e){
// if ESP busy / WiFi drop
setConnected(false);
console.log('status error', e);
}
}
setInterval(updateStatus, 1000);
updateStatus();
</script>
</body>
</html>
)rawliteral";
// Disable caching for the HTML itself
server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
server.sendHeader("Pragma", "no-cache");
server.sendHeader("Expires", "0");
server.send(200, "text/html", html);
}
// ---------- Setup ----------
void setup() {
Serial.begin(115200);
delay(100);
// pins
pinMode(TRIG_PIN, OUTPUT);
pinMode(ECHO_PIN, INPUT);
pinMode(BUZZER_PIN, OUTPUT);
digitalWrite(BUZZER_PIN, LOW);
// I2C for LCD
Wire.begin(I2C_SDA, I2C_SCL);
lcd.begin();
lcd.backlight();
lcd.clear();
lcd.setCursor(0,0);
lcd.print(" Smart Waste IoT ");
lcd.setCursor(0,1);
lcd.print(" Connecting WiFi ");
// Connect WiFi
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
delay(2000);
lcd.clear();
unsigned long start = millis();
while (WiFi.status() != WL_CONNECTED) {
delay(300);
yield();
lcd.setCursor(0,1);
lcd.print("WiFi Connecting...");
if (millis() - start > 15000UL) {
start = millis();
}
}
lcd.clear();
lcd.setCursor(0,0);
lcd.print("WiFi Connected");
lcd.setCursor(0,1);
lcd.print(WiFi.localIP().toString());
Serial.print("IP: ");
Serial.println(WiFi.localIP());
delay(6000);
// Web server routes
server.on("/", HTTP_GET, handleRoot);
server.on("/status", HTTP_GET, handleStatus);
server.begin();
Serial.println("Web server started");
// initial measure
lastDistanceCm = measureDistanceCM();
lastLevelPercent = distanceToPercent(lastDistanceCm);
lastThingSpeakMillis = millis() - THINGSPEAK_INTERVAL; // send immediately
lastMeasurementMillis = millis() - MEASURE_INTERVAL; // measure immediately
lcd.clear();
}
// ---------- Main loop ----------
void loop() {
server.handleClient();
if (millis() - lastMeasurementMillis >= MEASURE_INTERVAL) {
lastMeasurementMillis = millis();
// measure
float distance = measureDistanceCM();
int percent = distanceToPercent(distance);
lastDistanceCm = distance;
lastLevelPercent = percent;
// Serial log
Serial.print("Distance(cm): ");
Serial.print(distance, 2);
Serial.print(" Level(%): ");
Serial.println(percent);
// Update LCD (avoid heavy delay)
lcd.setCursor(0,0);
lcd.print("Level:");
lcd.setCursor(6,0);
lcd.print(String(percent) + "% ");
lcd.setCursor(10, 0);
lcd.print("D:"); // clear old data
lcd.setCursor(12, 0);
lcd.print(lastDistanceCm, 1);
lcd.print("cm");
lcd.setCursor(0,1);
if (percent >= FULL_THRESHOLD) {
lcd.print("Status:Full B:ON");
digitalWrite(BUZZER_PIN, HIGH);
} else {
lcd.print("Status:ok B:off");
digitalWrite(BUZZER_PIN, LOW);
}
// Send to ThingSpeak periodically
if (millis() - lastThingSpeakMillis >= THINGSPEAK_INTERVAL) {
lastThingSpeakMillis = millis();
if (String(thingspeakApiKey) != "") {
sendToThingSpeak(percent, distance);
}
}
}
yield();
}