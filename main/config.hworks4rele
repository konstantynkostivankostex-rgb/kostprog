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
