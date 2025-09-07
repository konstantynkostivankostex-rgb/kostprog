/* Fixed full sketch
   - GPIO: ONE_WIRE_BUS = 14 (D5), FAN_PIN = 5 (D1), COMP_PIN = 4 (D2)
   - DS18B20 x2 (getTempCByIndex)
   - 24h history (288 points, step 5 min)
   - Master ON/OFF, Heating/Cooling, hysteresis, delaySeconds editable
   - Settings persisted to EEPROM
   - Fixed JS: always use document.getElementById; listeners attached explicitly
   - Serial debug messages in handlers
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "wifi.h" // WiFi credentials
#include "html.h" // Web page


// --- Pins (GPIO numbers) ---
#define ONE_WIRE_BUS 14   // D5
#define FAN_PIN 5         // D1
#define COMP_PIN 4        // D2

// --- Sensors ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// --- Web server ---
ESP8266WebServer server(80);

// --- History ---
const uint16_t HIST_N = 288;
int16_t histT1x10[HIST_N];
int16_t histT2x10[HIST_N];
uint16_t histCount = 0;
uint16_t histHead  = 0;
unsigned long nextSampleMs = 0;

// --- EEPROM config ---
#define EEPROM_SIZE 512
#define CONFIG_MAGIC 0xC0DEC0DEUL

struct Config {
  uint32_t magic;
  float airSetpoint;
  float compShutdownTemp;
  float hysteresis;
  uint16_t delaySeconds;
  uint8_t heatingMode;   // 0=cooling,1=heating
  uint8_t systemEnabled; // 0=off,1=on
  uint8_t reserved[5];
};
Config cfg;

// runtime mirrors
float airSetpoint = 30.0;
float compShutdownTemp = 60.0;
float hysteresis = 0.5;
uint16_t delaySeconds = 120;
bool heatingMode = false;
bool systemEnabled = true;

unsigned long compStopTime = 0;
bool fanState = false;
bool compState = false;

// helpers
inline void comp_on(){ digitalWrite(COMP_PIN, LOW); compState = true; Serial.println("COMP ON"); }
inline void comp_off(){ digitalWrite(COMP_PIN, HIGH); compState = false; Serial.println("COMP OFF"); }
inline bool comp_is_on(){ return compState; }

inline void fan_on(){ digitalWrite(FAN_PIN, LOW); fanState = true; Serial.println("FAN ON"); }
inline void fan_off(){ digitalWrite(FAN_PIN, HIGH); fanState = false; Serial.println("FAN OFF"); }
inline bool fan_is_on(){ return fanState; }

static inline float safeReadC(float v){
  if (v == DEVICE_DISCONNECTED_C || v < -55 || v > 125) return NAN;
  return v;
}
static inline String fmtUptime(unsigned long ms){
  unsigned long s = ms/1000;
  unsigned long m = s/60;
  unsigned long h = m/60;
  unsigned long d = h/24;
  char buf[48];
  sprintf(buf, "%lu d %02lu:%02lu:%02lu", d, h%24, m%60, s%60);
  return String(buf);
}

unsigned long waitRemainingMs(){
  unsigned long elapsed = millis() - compStopTime;
  unsigned long need = (unsigned long)delaySeconds * 1000UL;
  if (elapsed >= need) return 0;
  return need - elapsed;
}

// EEPROM
void saveConfig(){
  cfg.magic = CONFIG_MAGIC;
  cfg.airSetpoint = airSetpoint;
  cfg.compShutdownTemp = compShutdownTemp;
  cfg.hysteresis = hysteresis;
  cfg.delaySeconds = delaySeconds;
  cfg.heatingMode = heatingMode ? 1 : 0;
  cfg.systemEnabled = systemEnabled ? 1 : 0;
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, cfg);
  EEPROM.commit();
  delay(5);
  Serial.println("Config saved to EEPROM");
}
bool loadConfig(){
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, cfg);
  if (cfg.magic != CONFIG_MAGIC) return false;
  // sanity
  if (!isfinite(cfg.airSetpoint) || !isfinite(cfg.compShutdownTemp) || !isfinite(cfg.hysteresis)) return false;
  if (cfg.delaySeconds < 5 || cfg.delaySeconds > 3600) return false;
  // apply
  airSetpoint = cfg.airSetpoint;
  compShutdownTemp = cfg.compShutdownTemp;
  hysteresis = cfg.hysteresis;
  delaySeconds = cfg.delaySeconds;
  heatingMode = (cfg.heatingMode != 0);
  systemEnabled = (cfg.systemEnabled != 0);
  Serial.println("Config loaded from EEPROM");
  return true;
}

// History
void historyInit(){
  for (uint16_t i=0;i<HIST_N;i++){ histT1x10[i]=INT16_MIN; histT2x10[i]=INT16_MIN; }
  histCount=0; histHead=0;
  nextSampleMs = millis();
}
void historyAdd(float t1, float t2){
  int16_t a = isnan(t1) ? INT16_MIN : (int16_t)roundf(t1 * 10.0f);
  int16_t b = isnan(t2) ? INT16_MIN : (int16_t)roundf(t2 * 10.0f);
  histT1x10[histHead] = a;
  histT2x10[histHead] = b;
  histHead = (histHead + 1) % HIST_N;
  if (histCount < HIST_N) histCount++;
}
void historyTick(){
  unsigned long now = millis();
  while ((long)(now - nextSampleMs) >= 0){
    sensors.requestTemperatures();
    float t1 = safeReadC(sensors.getTempCByIndex(0));
    float t2 = safeReadC(sensors.getTempCByIndex(1));
    historyAdd(t1, t2);
    nextSampleMs += 300000UL; // +5 min
  }
}
void sendHistoryJSON(){
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "{\"stepMin\":5,\"capacity\":288,\"points\":[");
  for (uint16_t i=0;i<histCount;i++){
    uint16_t idx = (histHead + HIST_N - histCount + i) % HIST_N;
    if (i) server.sendContent(",");
    String obj = "{\"t1\":";
    if (histT1x10[idx]==INT16_MIN) obj += "null"; else obj += String(histT1x10[idx]/10.0f,1);
    obj += ",\"t2\":";
    if (histT2x10[idx]==INT16_MIN) obj += "null"; else obj += String(histT2x10[idx]/10.0f,1);
    obj += "}";
    server.sendContent(obj);
  }
  server.sendContent("]}");
}



// --- Handlers ---
void handleRoot(){ server.send_P(200, "text/html", PAGE); }

void handleStatus(){
  sensors.requestTemperatures();
  float rt1 = safeReadC(sensors.getTempCByIndex(0));
  float rt2 = safeReadC(sensors.getTempCByIndex(1));
  float t1 = isnan(rt1) ? -127.0f : rt1;
  float t2 = isnan(rt2) ? -127.0f : rt2;

  String compState;
  if (!systemEnabled) compState = "OFF";
  else if (comp_is_on()) compState = "ON";
  else if (waitRemainingMs() > 0) compState = "WAIT";
  else compState = "OFF";

  String json = "{";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":\"" + String(WiFi.RSSI()) + "\",";
  json += "\"uptime\":\"" + fmtUptime(millis()) + "\",";
  json += "\"system\":\"" + String(systemEnabled ? "ON" : "OFF") + "\",";
  json += "\"t1\":\"" + String(t1,1) + "\",";
  json += "\"t2\":\"" + String(t2,1) + "\",";
  json += "\"airSetpoint\":\"" + String(airSetpoint,1) + "\",";
  json += "\"compShutdown\":\"" + String(compShutdownTemp,1) + "\",";
  json += "\"hyst\":\"" + String(hysteresis,1) + "\",";
  json += "\"delay\":\"" + String(delaySeconds) + "\",";
  json += "\"countdown\":\"" + String(waitRemainingMs()/1000) + "\",";
  json += "\"fan\":\"" + String((fan_is_on() && systemEnabled) ? "ON" : "OFF") + "\",";
  json += "\"compState\":\"" + compState + "\",";
  json += "\"mode\":\"" + String(heatingMode ? "Heating" : "Cooling") + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleHistory(){ sendHistoryJSON(); }

void handleSetTemp(){ 
  if (server.hasArg("t")) {
    airSetpoint = server.arg("t").toFloat();
    cfg.airSetpoint = airSetpoint; saveConfig(); Serial.printf("Set airSetpoint=%0.1f\n", airSetpoint);
  }
  server.send(200,"text/plain","OK");
}
void handleSetCompTemp(){
  if (server.hasArg("c")) {
    compShutdownTemp = server.arg("c").toFloat();
    cfg.compShutdownTemp = compShutdownTemp; saveConfig(); Serial.printf("Set compShutdown=%0.1f\n", compShutdownTemp);
  }
  server.send(200,"text/plain","OK");
}
void handleSetHyst(){
  if (server.hasArg("h")) {
    hysteresis = server.arg("h").toFloat();
    cfg.hysteresis = hysteresis; saveConfig(); Serial.printf("Set hyst=%0.1f\n", hysteresis);
  }
  server.send(200,"text/plain","OK");
}
void handleSetDelay(){
  if (server.hasArg("d")) {
    int d = server.arg("d").toInt(); if (d < 5) d = 5; if (d > 3600) d = 3600;
    delaySeconds = (uint16_t)d; cfg.delaySeconds = delaySeconds; saveConfig(); Serial.printf("Set delay=%u\n", delaySeconds);
  }
  server.send(200,"text/plain","OK");
}

void handleToggleMode(){
  heatingMode = !heatingMode; cfg.heatingMode = heatingMode ? 1 : 0; saveConfig();
  Serial.printf("Toggle mode -> %s\n", heatingMode ? "Heating":"Cooling");
  server.send(200,"text/plain","OK");
}
void handleToggleFan(){
  Serial.println("HTTP /toggleFan");
  if (!systemEnabled){ fan_off(); server.send(200,"text/plain","OFF"); return; }
  fan_is_on() ? fan_off() : fan_on();
  server.send(200,"text/plain","OK");
}
void handleToggleComp(){
  Serial.println("HTTP /toggleComp");
  if (!systemEnabled){ comp_off(); compStopTime = millis(); server.send(200,"text/plain","OFF"); return; }
  if (comp_is_on()){ comp_off(); compStopTime = millis(); }
  else if (waitRemainingMs() == 0){ comp_on(); }
  server.send(200,"text/plain","OK");
}
void handleToggleSystem(){
  Serial.println("HTTP /toggleSystem");
  systemEnabled = !systemEnabled;
  cfg.systemEnabled = systemEnabled ? 1 : 0;
  if (!systemEnabled){ comp_off(); compStopTime = millis(); fan_off(); }
  saveConfig();
  server.send(200,"text/plain", systemEnabled? "ON":"OFF");
}

// control
void controlLoop(){
  historyTick();

  if (!systemEnabled){
    if (comp_is_on()){ comp_off(); compStopTime = millis(); }
    if (fan_is_on()) fan_off();
    return;
  }

  sensors.requestTemperatures();
  float t1 = safeReadC(sensors.getTempCByIndex(0)); // compressor
  float t2 = safeReadC(sensors.getTempCByIndex(1)); // air

  bool delayOver  = (waitRemainingMs() == 0);
  bool compTooHot = (!isnan(t1) && t1 > compShutdownTemp);

  bool wantOn = false;
  if (!isnan(t2)){
    if (heatingMode){
      if (!comp_is_on() && t2 < (airSetpoint - hysteresis)) wantOn = true;
      if (comp_is_on()  && t2 < (airSetpoint + hysteresis)) wantOn = true;
    } else {
      if (!comp_is_on() && t2 > (airSetpoint + hysteresis)) wantOn = true;
      if (comp_is_on()  && t2 > (airSetpoint - hysteresis)) wantOn = true;
    }
  }

  if (comp_is_on()){
    if (compTooHot || !wantOn){
      comp_off();
      compStopTime = millis();
    }
  } else {
    if (delayOver && wantOn && !compTooHot){
      comp_on();
    }
  }
}

// setup / loop
void setup(){
  Serial.begin(115200);
  Serial.println();
  pinMode(FAN_PIN, OUTPUT);
  pinMode(COMP_PIN, OUTPUT);
  fan_off(); comp_off();

  sensors.begin();
  historyInit();

  if (!loadConfig()){
    airSetpoint = 30.0; compShutdownTemp = 60.0; hysteresis = 0.5; delaySeconds = 120; heatingMode = false; systemEnabled = true;
    cfg.airSetpoint = airSetpoint; cfg.compShutdownTemp = compShutdownTemp; cfg.hysteresis = hysteresis; cfg.delaySeconds = delaySeconds; cfg.heatingMode = heatingMode?1:0; cfg.systemEnabled = systemEnabled?1:0;
    saveConfig();
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED){
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/history", handleHistory);
  server.on("/setTemp", handleSetTemp);
  server.on("/setCompTemp", handleSetCompTemp);
  server.on("/setHyst", handleSetHyst);
  server.on("/setDelay", handleSetDelay);
  server.on("/toggleMode", handleToggleMode);
  server.on("/toggleFan", handleToggleFan);
  server.on("/toggleComp", handleToggleComp);
  server.on("/toggleSystem", handleToggleSystem);
  server.begin();

  if (systemEnabled) fan_on(); else fan_off();
  compStopTime = millis();
}

void loop(){
  server.handleClient();
  controlLoop();
}
