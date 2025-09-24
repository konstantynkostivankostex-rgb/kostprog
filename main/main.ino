#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "wifi.h"   // ssid, password
#include "html.h"   // веб-сторінка
#include "config.h" // конфігурація

// --- Helpers ---
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

// --- EEPROM ---
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
  Serial.println("Config saved");
}
bool loadConfig(){
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, cfg);
  if (cfg.magic != CONFIG_MAGIC) return false;
  if (!isfinite(cfg.airSetpoint) || !isfinite(cfg.compShutdownTemp) || !isfinite(cfg.hysteresis)) return false;
  if (cfg.delaySeconds < 5 || cfg.delaySeconds > 3600) return false;
  airSetpoint = cfg.airSetpoint;
  compShutdownTemp = cfg.compShutdownTemp;
  hysteresis = cfg.hysteresis;
  delaySeconds = cfg.delaySeconds;
  heatingMode = (cfg.heatingMode != 0);
  systemEnabled = (cfg.systemEnabled != 0);
  return true;
}

// --- History ---
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

  String compS;
  if (!systemEnabled) compS = "OFF";
  else if (comp_is_on()) compS = "ON";
  else if (waitRemainingMs() > 0) compS = "WAIT";
  else compS = "OFF";

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
  json += "\"compState\":\"" + compS + "\",";
  json += "\"mode\":\"" + String(heatingMode ? "Heating" : "Cooling") + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleHistory(){ sendHistoryJSON(); }

void handleSetTemp(){ if (server.hasArg("t")) { airSetpoint = server.arg("t").toFloat(); cfg.airSetpoint = airSetpoint; saveConfig(); } server.send(200,"text/plain","OK"); }
void handleSetCompTemp(){ if (server.hasArg("c")) { compShutdownTemp = server.arg("c").toFloat(); cfg.compShutdownTemp = compShutdownTemp; saveConfig(); } server.send(200,"text/plain","OK"); }
void handleSetHyst(){ if (server.hasArg("h")) { hysteresis = server.arg("h").toFloat(); cfg.hysteresis = hysteresis; saveConfig(); } server.send(200,"text/plain","OK"); }
void handleSetDelay(){ if (server.hasArg("d")) { int d = server.arg("d").toInt(); if (d<5) d=5; if (d>3600) d=3600; delaySeconds=d; cfg.delaySeconds=d; saveConfig(); } server.send(200,"text/plain","OK"); }

void handleToggleMode(){ heatingMode = !heatingMode; cfg.heatingMode = heatingMode ? 1:0; saveConfig(); server.send(200,"text/plain","OK"); }
void handleToggleFan(){ if (!systemEnabled){ fan_off(); server.send(200,"text/plain","OFF"); return; } fan_is_on()?fan_off():fan_on(); server.send(200,"text/plain","OK"); }
void handleToggleComp(){
  if (!systemEnabled){ comp_off(); compStopTime = millis(); server.send(200,"text/plain","OFF"); return; }
  if (comp_is_on()){ comp_off(); compStopTime = millis(); }
  else if (waitRemainingMs()==0){ comp_on(); }
  server.send(200,"text/plain","OK");
}
void handleToggleSystem(){
  systemEnabled = !systemEnabled;
  cfg.systemEnabled = systemEnabled ? 1:0;
  if (!systemEnabled){ comp_off(); compStopTime = millis(); fan_off(); }
  saveConfig();
  server.send(200,"text/plain", systemEnabled? "ON":"OFF");
}

// --- Control Loop ---
void controlLoop(){
  historyTick();

  if (!systemEnabled){
    if (comp_is_on()){ comp_off(); compStopTime = millis(); }
    if (fan_is_on()) fan_off();
    return;
  }

  sensors.requestTemperatures();
  float t1 = safeReadC(sensors.getTempCByIndex(0));
  float t2 = safeReadC(sensors.getTempCByIndex(1));

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
      compStopTime = millis();   // 🔥 запуск таймера при кожному виключенні
    }
  } else {
    if (delayOver && wantOn && !compTooHot){
      comp_on();
    }
  }
}

// --- Setup / Loop ---
void setup(){
  Serial.begin(115200);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(COMP_PIN, OUTPUT);
  fan_off(); comp_off();

  sensors.begin();
  historyInit();

  if (!loadConfig()){
    airSetpoint=30; compShutdownTemp=60; hysteresis=0.5; delaySeconds=120; heatingMode=false; systemEnabled=true;
    cfg.airSetpoint=airSetpoint; cfg.compShutdownTemp=compShutdownTemp; cfg.hysteresis=hysteresis; cfg.delaySeconds=delaySeconds; cfg.heatingMode=0; cfg.systemEnabled=1;
    saveConfig();
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED){ delay(300); }

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
