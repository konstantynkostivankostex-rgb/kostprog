#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- WiFi ---
const char* ssid = "MyWiFi";         // замінити на свою мережу
const char* password = "MyPass";

// --- HTTP авторизація ---
const char* www_username = "Xia";
const char* www_password = "poiuytrewqasd";

// --- Піни ---
#define RELAY1 D1
#define RELAY2 D2
#define RELAY3 D5
#define RELAY4 D6
#define ONE_WIRE_BUS D3   // DS18B20 підключені сюди

// --- Температурні датчики ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// --- Сервер ---
ESP8266WebServer server(80);

// --- Стан реле ---
bool relayState[4] = {false, false, false, false};

// --- HTML ---
String buildPage(float t1, float t2) {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;background:#f2f2f2;}";
  html += "h2{color:#333;}";
  html += ".card{background:white;padding:20px;margin:20px auto;border-radius:15px;box-shadow:0 4px 8px rgba(0,0,0,0.2);max-width:400px;}";
  html += "button{padding:12px 20px;margin:5px;border:none;border-radius:10px;font-size:16px;cursor:pointer;}";
  html += ".on{background:#4CAF50;color:white;}";
  html += ".off{background:#f44336;color:white;}";
  html += "</style></head><body>";
  html += "<h2>ESP8266 Панель керування</h2>";
  html += "<div class='card'>";
  html += "<p>Температура 1: " + String(t1) + " °C</p>";
  html += "<p>Температура 2: " + String(t2) + " °C</p>";
  html += "</div>";

  for (int i = 0; i < 4; i++) {
    html += "<div class='card'>";
    html += "<h3>Реле " + String(i+1) + "</h3>";
    html += "<p>Стан: " + String(relayState[i] ? "ON" : "OFF") + "</p>";
    html += "<a href='/relay" + String(i+1) + "/on'><button class='on'>ON</button></a>";
    html += "<a href='/relay" + String(i+1) + "/off'><button class='off'>OFF</button></a>";
    html += "</div>";
  }

  html += "</body></html>";
  return html;
}

// --- Перевірка логіну ---
bool is_auth() {
  if (!server.authenticate(www_username, www_password)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

// --- Обробники ---
void handleRoot() {
  if (!is_auth()) return;
  sensors.requestTemperatures();
  float temp1 = sensors.getTempCByIndex(0);
  float temp2 = sensors.getTempCByIndex(1);
  server.send(200, "text/html", buildPage(temp1, temp2));
}

void setRelay(int n, bool state) {
  if (n < 1 || n > 4) return;
  digitalWrite((n==1)?RELAY1:(n==2)?RELAY2:(n==3)?RELAY3:RELAY4, state ? LOW : HIGH);
  relayState[n-1] = state;
  handleRoot();
}

void handleRelay1On(){ if (is_auth()) setRelay(1, true); }
void handleRelay1Off(){ if (is_auth()) setRelay(1, false); }
void handleRelay2On(){ if (is_auth()) setRelay(2, true); }
void handleRelay2Off(){ if (is_auth()) setRelay(2, false); }
void handleRelay3On(){ if (is_auth()) setRelay(3, true); }
void handleRelay3Off(){ if (is_auth()) setRelay(3, false); }
void handleRelay4On(){ if (is_auth()) setRelay(4, true); }
void handleRelay4Off(){ if (is_auth()) setRelay(4, false); }

void setup() {
  Serial.begin(115200);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);

  sensors.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP:");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);

  server.on("/relay1/on", handleRelay1On);
  server.on("/relay1/off", handleRelay1Off);
  server.on("/relay2/on", handleRelay2On);
  server.on("/relay2/off", handleRelay2Off);
  server.on("/relay3/on", handleRelay3On);
  server.on("/relay3/off", handleRelay3Of
