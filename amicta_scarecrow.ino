#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// +++++ HARDWARE CONFIG
// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions - FIXED: Hindari konflik I2C
#define PIR_PIN D6
#define VIBRATION_PIN D7     
#define BUZZER_PIN D3 
#define DHT_PIN D5 
#define SOIL_SENSOR_PIN A0 
// I2C for OLED: SDA = D2, SCL = D1 (Default ESP8266)

// DHT Configuration
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ===== NETWORK CONFIGURATION =====
// const char* ssid = "Trafalgar LAW";
// const char* password = "bukanwibu";

const char* ssid = "pova6";
const char* password = "gungun82";

//const char* ssid = "Ghozali";
//const char* password = "Rt10rw04";

// const char* ssid = "Yowaimou";
// const char* password = "12345678";

// ===== BLYNK VIRTUAL PINS =====
#define VPIN_TEMPERATURE    V0
#define VPIN_HUMIDITY       V1  
#define VPIN_SOIL_MOISTURE  V2
#define VPIN_PEST_COUNT     V3
#define VPIN_PEST_DETECTED  V4
#define VPIN_CONNECTION     V5
#define VPIN_MANUAL_BUZZ    V6
#define VPIN_RESET_COUNT    V7
#define VPIN_VIBRATION_STATUS V8
#define VPIN_PIR_STATUS     V9

BlynkTimer timer;

// ===== SYSTEM VARIABLES =====
unsigned long dailyPestCount = 0;
bool pirState = false;
bool vibrationState = false;
bool lastPirState = false;
bool lastVibrationState = false;
unsigned long lastPirTrigger = 0;
unsigned long lastVibrationTrigger = 0;
unsigned long lastPestDetected = 0;
bool pestDetectedFlag = false;

// ===== OLED DISPLAY VARIABLES =====
bool oledWorking = false;  // NEW: Track OLED status
bool oledBlinking = false;
bool oledBlinkState = false;
unsigned long lastOledBlink = 0;
unsigned long oledBlinkDuration = 10000;
const unsigned long OLED_BLINK_INTERVAL = 500;
String currentSensorType = "";
float currentTemp = 0;
float currentHum = 0;
float currentSoil = 0;

// ===== ANTI-OVERLAP SYSTEM =====
enum SensorState {
  SENSOR_IDLE,
  SENSOR_VIBRATION_ACTIVE,
  SENSOR_PIR_ACTIVE,
  SENSOR_BUZZER_ACTIVE
};

SensorState currentSensorState = SENSOR_IDLE;
unsigned long sensorLockTime = 0;
const unsigned long SENSOR_LOCK_DURATION = 5000;

const unsigned long VIBRATION_DEBOUNCE = 1000;
const unsigned long PIR_DEBOUNCE = 3000;
const unsigned long MIN_INTERVAL_BETWEEN_SENSORS = 3000;

// ===== MULTI-FREQUENCY BUZZER SYSTEM =====
const int frequencies[] = {1500, 2000, 1200, 1800, 1000, 2200};
const int frequencyCount = 6;

bool buzzerActive = false;
int buzzerStep = 0;
int buzzerCycle = 0;
int currentFrequencyIndex = 0;
unsigned long buzzerTimer = 0;
int buzzerPattern = 0; 

// ===== SETUP FUNCTION =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Scarecrow - OLED Fixed System ===");
  
  // Initialize pins first
  pinMode(PIR_PIN, INPUT);
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Initialize DHT sensor
  dht.begin();
  
  // Initialize I2C with default pins and scan for devices
  Wire.begin(); // Default: SDA = D2 (GPIO4), SCL = D1 (GPIO5)
  Serial.println("Scanning I2C devices...");
  scanI2CDevices();
  
  // Initialize OLED Display with multiple address attempts
  oledWorking = initializeOLED();
  
  if (oledWorking) {
    showBootScreen();
  } else {
    Serial.println("❌ OLED Display failed - continuing without display");
  }
  
  // Test buzzer at startup
  testMultiFrequencyBuzzer();
  
  // Initialize WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (oledWorking) updateOledStatus("Connecting WiFi...", false);
  }
  Serial.println("\nWiFi Connected!");
  
  // Initialize Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  
  // Setup timers
  timer.setInterval(15000L, readAndSendSensorData);
  timer.setInterval(86400000L, resetDailyCount);
  timer.setInterval(3000L, checkSensorStatus);
  timer.setInterval(2000L, debugSensorReading);
  timer.setInterval(1000L, manageSensorState);
  timer.setInterval(5000L, updateOledDisplay);
  timer.setInterval(100L, handleOledBlink);
  
  Serial.println("Smart Scarecrow System Ready!");
  Serial.println("OLED Status: " + String(oledWorking ? "WORKING" : "FAILED"));
  
  // Send initial status
  Blynk.virtualWrite(VPIN_CONNECTION, oledWorking ? "Online - OLED OK" : "Online - OLED FAIL");
  Blynk.virtualWrite(VPIN_PEST_COUNT, dailyPestCount);
  
  if (oledWorking) updateOledStatus("System Ready!", false);
}

// ===== I2C AND OLED INITIALIZATION =====
void scanI2CDevices() {
  byte error, address;
  int nDevices = 0;

  Serial.println("I2C Scanner Starting...");

  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    }
  }
  
  if (nDevices == 0) {
    Serial.println("❌ No I2C devices found!");
  } else {
    Serial.println("✅ I2C scan complete. " + String(nDevices) + " device(s) found.");
  }
}

bool initializeOLED() {
  // Try common OLED addresses
  byte addresses[] = {0x3C, 0x3D};
  
  for (int i = 0; i < 2; i++) {
    Serial.println("Trying OLED at address 0x" + String(addresses[i], HEX));
    
    if(display.begin(SSD1306_SWITCHCAPVCC, addresses[i])) {
      Serial.println("✅ OLED initialized successfully at 0x" + String(addresses[i], HEX));
      display.display();
      delay(2000);
      display.clearDisplay();
      return true;
    }
  }
  
  Serial.println("❌ OLED initialization failed at all addresses");
  return false;
}

// ===== MAIN LOOP =====
void loop() {
  Blynk.run();
  timer.run();
  
  handleMultiFrequencyBuzzer();
  
  if (currentSensorState == SENSOR_IDLE) {
    checkSensorsWithPriority();
  }
  
  if (pestDetectedFlag && (millis() - lastPestDetected > oledBlinkDuration)) {
    pestDetectedFlag = false;
    oledBlinking = false;
    Blynk.virtualWrite(VPIN_PEST_DETECTED, 0);
    clearSensorStatus();
    if (oledWorking) updateOledDisplay();
  }
  
  delay(50);
}

// ===== OLED DISPLAY FUNCTIONS =====
void showBootScreen() {
  if (!oledWorking) return;
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println(F("SMART"));
  display.setCursor(10, 40);
  display.println(F("SCARECROW"));
  display.display();
  delay(2000);
}

void updateOledStatus(String status, bool blink) {
  if (!oledWorking) return;
  
  if (blink) {
    oledBlinking = true;
    lastOledBlink = millis();
    oledBlinkState = true;
  } else {
    oledBlinking = false;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(status);
  display.display();
}

void updateOledDisplay() {
  if (!oledWorking || oledBlinking) return;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.println("=== SMART SCARECROW ===");
  
  display.setCursor(0, 10);
  display.print("State: ");
  display.println(getSensorStateShort(currentSensorState));
  
  display.setCursor(0, 20);
  display.print("Temp: ");
  display.print(currentTemp, 1);
  display.print("C  Hum: ");
  display.print(currentHum, 1);
  display.println("%");
  
  display.setCursor(0, 30);
  display.print("Soil: ");
  display.print(currentSoil, 1);
  display.println("%");
  
  display.setCursor(0, 40);
  display.print("Pest Count: ");
  display.println(dailyPestCount);
  
  display.setCursor(0, 50);
  if (WiFi.status() == WL_CONNECTED) {
    display.println("WiFi: Connected");
  } else {
    display.println("WiFi: Disconnected");
  }
  
  display.display();
}

void showPestAlert(String sensorType) {
  if (!oledWorking) return;
  
  currentSensorType = sensorType;
  oledBlinking = true;
  lastOledBlink = millis();
  oledBlinkState = true;
  
  displayPestAlertScreen();
}

void displayPestAlertScreen() {
  if (!oledWorking) return;
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(10, 5);
  display.println(F("PEST"));
  display.setCursor(5, 25);
  display.println(F("DETECTED!"));
  
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.print("Sensor: ");
  display.println(currentSensorType);
  
  display.setCursor(0, 55);
  display.print("Count: ");
  display.println(dailyPestCount);
  
  display.display();
}

void handleOledBlink() {
  if (!oledWorking || !oledBlinking) return;
  
  unsigned long currentTime = millis();
  
  if (currentTime - lastPestDetected > oledBlinkDuration) {
    oledBlinking = false;
    updateOledDisplay();
    return;
  }
  
  if (currentTime - lastOledBlink >= OLED_BLINK_INTERVAL) {
    lastOledBlink = currentTime;
    oledBlinkState = !oledBlinkState;
    
    if (oledBlinkState) {
      displayPestAlertScreen();
    } else {
      display.clearDisplay();
      display.display();
    }
  }
}

String getSensorStateShort(SensorState state) {
  switch (state) {
    case SENSOR_IDLE: return "IDLE";
    case SENSOR_VIBRATION_ACTIVE: return "VIB-LOCK";
    case SENSOR_PIR_ACTIVE: return "PIR-LOCK";
    case SENSOR_BUZZER_ACTIVE: return "BUZZ-ACT";
    default: return "UNKNOWN";
  }
}

// ===== SENSOR FUNCTIONS =====
void checkSensorsWithPriority() {
  bool currentVibState = digitalRead(VIBRATION_PIN);
  bool currentPirState = digitalRead(PIR_PIN);
  
  unsigned long currentTime = millis();
  
  if (currentVibState && !lastVibrationState && 
      (currentTime - lastVibrationTrigger > VIBRATION_DEBOUNCE) &&
      (currentTime - lastPirTrigger > MIN_INTERVAL_BETWEEN_SENSORS)) {
    
    activateSensorWithLock(SENSOR_VIBRATION_ACTIVE, "Vibration", 1);
    lastVibrationTrigger = currentTime;
    Serial.println("🔴 VIBRATION DETECTED - PRIORITY 1!");
  }
  else if (currentPirState && !lastPirState && 
           (currentTime - lastPirTrigger > PIR_DEBOUNCE) &&
           (currentTime - lastVibrationTrigger > MIN_INTERVAL_BETWEEN_SENSORS)) {
    
    activateSensorWithLock(SENSOR_PIR_ACTIVE, "PIR Motion", 2);
    lastPirTrigger = currentTime;
    Serial.println("🟡 PIR MOTION DETECTED - PRIORITY 2!");
  }
  
  lastVibrationState = currentVibState;
  lastPirState = currentPirState;
}

void activateSensorWithLock(SensorState newState, const String& sensorType, int pattern) {
  currentSensorState = newState;
  sensorLockTime = millis();
  
  Serial.println("=== PEST DETECTED - NO OVERLAP SYSTEM ===");
  Serial.println("Active Sensor: " + sensorType);
  Serial.println("Pattern: " + String(pattern));
  Serial.println("OLED Status: " + String(oledWorking ? "WORKING" : "FAILED"));
  
  handlePestDetection(sensorType, pattern);
  updateSensorStatusDisplay(pattern);
}

void handlePestDetection(const String& sensorType, int pattern) {
  dailyPestCount++;
  buzzerPattern = pattern;
  currentFrequencyIndex = 0;
  
  activateMultiFrequencyDeterrent();
  sendPestDetection(sensorType);
  
  if (oledWorking) {
    showPestAlert(sensorType);
  }
  
  currentSensorState = SENSOR_BUZZER_ACTIVE;
}

// ===== MULTI-FREQUENCY BUZZER FUNCTIONS =====
void activateMultiFrequencyDeterrent() {
  buzzerActive = true;
  buzzerStep = 0;
  buzzerCycle = 0;
  currentFrequencyIndex = 0;
  buzzerTimer = millis();
  
  Serial.println("🎵 Multi-Frequency Buzzer activated!");
  Serial.println("Pattern: " + String(buzzerPattern));
}

void handleMultiFrequencyBuzzer() {
  if (!buzzerActive) return;
  
  unsigned long currentTime = millis();
  
  int maxCycles, beepDuration, pauseDuration;
  
  switch (buzzerPattern) {
    case 1:
      maxCycles = 6;
      beepDuration = 200;
      pauseDuration = 150;
      break;
    case 2:
      maxCycles = 4;
      beepDuration = 300;
      pauseDuration = 200;
      break;
    default:
      maxCycles = 3;
      beepDuration = 250;
      pauseDuration = 200;
      break;
  }
  
  switch (buzzerStep) {
    case 0:
      if (buzzerCycle < maxCycles) {
        int currentFreq = frequencies[currentFrequencyIndex];
        tone(BUZZER_PIN, currentFreq);
        
        Serial.println("🎵 BEEP " + String(buzzerCycle + 1) + "/" + String(maxCycles) + 
                      " - Freq: " + String(currentFreq) + "Hz");
        
        buzzerTimer = currentTime;
        buzzerStep = 1;
        
        currentFrequencyIndex = (currentFrequencyIndex + 1) % frequencyCount;
      } else {
        buzzerActive = false;
        buzzerCycle = 0;
        currentFrequencyIndex = 0;
        
        currentSensorState = SENSOR_IDLE;
        sensorLockTime = millis();
        
        Serial.println("🎵 Multi-frequency sequence completed!");
      }
      break;
      
    case 1:
      if (currentTime - buzzerTimer >= beepDuration) {
        noTone(BUZZER_PIN);
        buzzerTimer = currentTime;
        buzzerStep = 2;
      }
      break;
      
    case 2:
      if (currentTime - buzzerTimer >= pauseDuration) {
        buzzerCycle++;
        buzzerStep = 0;
      }
      break;
  }
}

void testMultiFrequencyBuzzer() {
  Serial.println("🎵 Testing Multi-Frequency Buzzer...");
  
  for (int i = 0; i < frequencyCount; i++) {
    Serial.println("Testing Frequency " + String(i + 1) + "/" + String(frequencyCount) + 
                   ": " + String(frequencies[i]) + "Hz");
    tone(BUZZER_PIN, frequencies[i], 250);
    delay(400);
  }
  
  Serial.println("🎵 Multi-Frequency Buzzer test complete!");
}

// ===== SENSOR DATA AND MANAGEMENT =====
void readAndSendSensorData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  int soilRaw = analogRead(SOIL_SENSOR_PIN);
  float soilMoisture = map(soilRaw, 1023, 0, 0, 100);
  soilMoisture = constrain(soilMoisture, 0.0, 100.0);
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("DHT sensor error!");
    humidity = 0;
    temperature = 0;
  }
  
  currentTemp = temperature;
  currentHum = humidity;
  currentSoil = soilMoisture;
  
  Blynk.virtualWrite(VPIN_TEMPERATURE, temperature);
  Blynk.virtualWrite(VPIN_HUMIDITY, humidity);
  Blynk.virtualWrite(VPIN_SOIL_MOISTURE, soilMoisture);
  Blynk.virtualWrite(VPIN_PEST_COUNT, dailyPestCount);
  
  Serial.println("===== Sensor Data & System Status =====");
  Serial.println("Temperature: " + String(temperature, 1) + "°C");
  Serial.println("Humidity: " + String(humidity, 1) + "%");
  Serial.println("Soil Moisture: " + String(soilMoisture, 1) + "%");
  Serial.println("Daily Pest Count: " + String(dailyPestCount));
  Serial.println("System State: " + getSensorStateName(currentSensorState));
  Serial.println("OLED Status: " + String(oledWorking ? "WORKING" : "FAILED"));
  Serial.println("========================================");
}

void manageSensorState() {
  unsigned long currentTime = millis();
  
  if (currentSensorState != SENSOR_IDLE && 
      (currentTime - sensorLockTime > SENSOR_LOCK_DURATION) &&
      !buzzerActive) {
    
    Serial.println("🔓 Auto-unlock: Sensor system released");
    currentSensorState = SENSOR_IDLE;
    clearSensorStatus();
  }
}

void sendPestDetection(const String& sensorType) {
  pestDetectedFlag = true;
  lastPestDetected = millis();
  
  Blynk.virtualWrite(VPIN_PEST_DETECTED, 1);
  Blynk.virtualWrite(VPIN_PEST_COUNT, dailyPestCount);
  
  String notifMsg = "🚨 Pest detected! " + sensorType + " | Total: " + String(dailyPestCount) + 
                   " | OLED: " + String(oledWorking ? "OK" : "FAIL");
  Blynk.logEvent("pest_detected", notifMsg);
  
  Serial.println("Detection sent: " + notifMsg);
}

void checkSensorStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    String status = "Online - " + String(oledWorking ? "OLED OK" : "OLED FAIL");
    Blynk.virtualWrite(VPIN_CONNECTION, status);
  } else {
    Blynk.virtualWrite(VPIN_CONNECTION, "WiFi Disconnected");
  }
}

void resetDailyCount() {
  dailyPestCount = 0;
  Blynk.virtualWrite(VPIN_PEST_COUNT, dailyPestCount);
  Serial.println("Daily pest count reset at: " + String(millis()));
}

void debugSensorReading() {
  bool vibStatus = digitalRead(VIBRATION_PIN);
  bool pirStatus = digitalRead(PIR_PIN);
  
  Serial.println("--- Debug Sensor (Anti-Overlap + OLED) ---");
  Serial.println("Current State: " + getSensorStateName(currentSensorState));
  Serial.println("Vibration (D0): " + String(vibStatus));
  Serial.println("PIR (D8): " + String(pirStatus));
  Serial.println("OLED Working: " + String(oledWorking ? "YES" : "NO"));
  Serial.println("Can Accept New Detection: " + String(currentSensorState == SENSOR_IDLE ? "YES" : "NO"));
  Serial.println("OLED Blinking: " + String(oledBlinking ? "ACTIVE" : "IDLE"));
  Serial.println("--------------------------------------------");
}

// ===== HELPER FUNCTIONS =====
String getSensorStateName(SensorState state) {
  switch (state) {
    case SENSOR_IDLE: return "All Sensors Idle";
    case SENSOR_VIBRATION_ACTIVE: return "VIBRATION LOCKED";
    case SENSOR_PIR_ACTIVE: return "PIR LOCKED";
    case SENSOR_BUZZER_ACTIVE: return "BUZZER ACTIVE";
    default: return "Unknown State";
  }
}

void updateSensorStatusDisplay(int pattern) {
  if (pattern == 1) {
    Blynk.virtualWrite(VPIN_VIBRATION_STATUS, 1);
    Blynk.virtualWrite(VPIN_PIR_STATUS, 0);
  } else if (pattern == 2) {
    Blynk.virtualWrite(VPIN_VIBRATION_STATUS, 0);
    Blynk.virtualWrite(VPIN_PIR_STATUS, 1);
  }
}

void clearSensorStatus() {
  Blynk.virtualWrite(VPIN_VIBRATION_STATUS, 0);
  Blynk.virtualWrite(VPIN_PIR_STATUS, 0);
}

// ===== BLYNK VIRTUAL PIN HANDLERS =====
BLYNK_WRITE(VPIN_MANUAL_BUZZ) {
  int value = param.asInt();
  if (value == 1 && currentSensorState == SENSOR_IDLE) {
    Serial.println("🎵 Manual multi-frequency test from Blynk + OLED Alert");
    buzzerPattern = 0;
    currentSensorState = SENSOR_BUZZER_ACTIVE;
    sensorLockTime = millis();
    activateMultiFrequencyDeterrent();
    
    if (oledWorking) {
      showPestAlert("Manual Test");
    }
    dailyPestCount++;
    pestDetectedFlag = true;
    lastPestDetected = millis();
    
  } else if (value == 1 && currentSensorState != SENSOR_IDLE) {
    Serial.println("❌ Manual buzzer blocked - System locked in state: " + getSensorStateName(currentSensorState));
  }
}

BLYNK_WRITE(VPIN_RESET_COUNT) {
  int value = param.asInt();
  if (value == 1) {
    dailyPestCount = 0;
    Blynk.virtualWrite(VPIN_PEST_COUNT, dailyPestCount);
    
    oledBlinking = false;
    pestDetectedFlag = false;
    if (oledWorking) updateOledDisplay();
    
    Serial.println("Pest count reset from Blynk + OLED refreshed");
  }
}

BLYNK_CONNECTED() {
  Serial.println("Blynk connected successfully!");
  String status = "Online - " + String(oledWorking ? "OLED OK" : "OLED FAIL");
  Blynk.virtualWrite(VPIN_CONNECTION, status);
  
  Blynk.syncVirtual(VPIN_PEST_COUNT);
  readAndSendSensorData();
  
  if (oledWorking) {
    updateOledStatus("Blynk Connected!", false);
    delay(2000);
    updateOledDisplay();
  }
}

BLYNK_DISCONNECTED() {
  Serial.println("Blynk disconnected!");
  if (oledWorking) updateOledStatus("Blynk Offline", false);
}