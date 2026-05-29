#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =========================
// Pin Definitions
// =========================
// Primary I2C Pins (For BME280)
#define BME_SDA 21
#define BME_SCL 22

// Secondary I2C Pins (For OLED Display)
#define OLED_SDA 19
#define OLED_SCL 18

#define SOIL_PIN 34
#define MQ135_PIN 35

// LED Strip Data Pin
#define LED_STRIP_PIN 4

// Automation Target Pins
#define PIN_TEMP  13 
#define PIN_HUMID 12 
#define PIN_SOIL  14 
#define PIN_CO2   32 
const int testPins[4] = {PIN_TEMP, PIN_HUMID, PIN_SOIL, PIN_CO2};

// CLEANED OLED DISPLAY STRINGS per your requirement
const char* testPinNames[4] = {"D13_Temp", "D12_Humid", "D14_Soil", "D32_CO2"};
bool testPinStates[4] = {false, false, false, false};

// Rotary Encoder Pins
#define ENCODER_CLK 27
#define ENCODER_DT  26
#define ENCODER_SW  25

// OLED Screen Dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

// =========================
// Receiver MAC Address
// =========================
uint8_t receiverAddress[] = {0xAC, 0xA7, 0x04, 0x26, 0x05, 0xB0};

// =========================
// System State Machines
// =========================
enum SystemView { MAIN_MENU, TARGET_SETTINGS_VIEW, TEST_MODUS_VIEW };
SystemView currentView = MAIN_MENU;

enum EditState { SELECT_ITEM, EDIT_ITEM };
EditState currentEditState = SELECT_ITEM;

// Menu Index Trackers
int mainMenusIndex = 0;
const int TOTAL_MAIN_ITEMS = 2;
const char* mainMenuItems[TOTAL_MAIN_ITEMS] = {"1. Target Settings", "2. Test Modus"};

int targetMenuIndex = 0;
const int TOTAL_TARGET_PARAMS = 5; 
const char* targetParamNames[TOTAL_TARGET_PARAMS] = {"Temp Target", "Humid Target", "Soil Target", "CO2 Target", "LED Strip"};

int testMenuIndex = 0;
const int TOTAL_TEST_PARAMS = 4; // 4 pins total (Back button is virtual index 4)

// Target Values
Adafruit_BME280 bme;
float targetTemp = 25.0;
float targetHumid = 50.0;
int targetSoil = 40;
int targetCO2 = 400;
bool ledStripState = false;

int lastClkState;
unsigned long lastButtonPress = 0;

// ESP-NOW Structs
typedef struct struct_message {
  float temperature;
  float humidity;
  int soilMoisture;
  int co2Level;
} struct_message;
struct_message sensorData;

typedef struct struct_response {
  char status[32];
} struct_response;
struct_response incomingResponse;
esp_now_peer_info_t peerInfo;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000; 

// Callbacks
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingResponse, incomingData, sizeof(incomingResponse));
}

// =========================
// UI Display Function
// =========================
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  // 1. RENDER MAIN MENU VIEW
  if (currentView == MAIN_MENU) {
    display.setTextSize(1);
    display.println("===== MAIN MENU =====");
    display.println("");
    for (int i = 0; i < TOTAL_MAIN_ITEMS; i++) {
      display.print(i == mainMenusIndex ? "> " : "  ");
      display.println(mainMenuItems[i]);
    }
  }
  // 2. RENDER TARGET SETTINGS VIEW
  else if (currentView == TARGET_SETTINGS_VIEW) {
    display.setTextSize(1);
    display.println("TARGET SETTINGS");
    display.println("");
    for (int i = 0; i < TOTAL_TARGET_PARAMS; i++) {
      if (i == targetMenuIndex) {
        display.print(currentEditState == EDIT_ITEM ? "[>] " : "> ");
      } else {
        display.print("  ");
      }
      display.print(targetParamNames[i]); display.print(": ");
      
      if (i == 0) display.print(targetTemp, 1);
      else if (i == 1) display.print(targetHumid, 1);
      else if (i == 2) display.print(targetSoil);
      else if (i == 3) display.print(targetCO2);
      else if (i == 4) display.print(ledStripState ? "ON" : "OFF");
      display.println();
    }
    display.println("");
    display.print(targetMenuIndex == TOTAL_TARGET_PARAMS ? "> [Back to Main]" : "  [Back to Main]");
  }
  // 3. RENDER HARDWARE TEST MODUS VIEW (Optimized size & layout)
  else if (currentView == TEST_MODUS_VIEW) {
    display.setTextSize(1); // Set to base layer font resolution
    display.println("==== TEST MODUS ====");
    
    // Looping through output item rows without extra carriage returns to optimize space
    for (int i = 0; i < 4; i++) {
      if (i == testMenuIndex) {
        display.print(currentEditState == EDIT_ITEM ? "[>] " : "> ");
      } else {
        display.print("  ");
      }
      display.print(testPinNames[i]); display.print(": ");
      display.println(testPinStates[i] ? "ON" : "OFF");
    }
    
    display.println("");
    display.print(testMenuIndex == 4 ? "> [Back to Main]" : "  [Back to Main]");
  }
  
  display.display();
}

// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);

  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastClkState = digitalRead(ENCODER_CLK);

  pinMode(LED_STRIP_PIN, OUTPUT);
  digitalWrite(LED_STRIP_PIN, LOW);
  
  for(int i = 0; i < 4; i++) {
    pinMode(testPins[i], OUTPUT);
    digitalWrite(testPins[i], LOW);
  }

  Wire.begin(BME_SDA, BME_SCL);
  Wire1.begin(OLED_SDA, OLED_SCL);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println("OLED failed to initialize");
  }

  bool status = bme.begin(0x76, &Wire);
  if (!status) status = bme.begin(0x77, &Wire);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Take initial reading
  sensorData.temperature = bme.readTemperature();
  sensorData.humidity = bme.readHumidity();
  int rawSoil = analogRead(SOIL_PIN);
  sensorData.soilMoisture = constrain(map(rawSoil, 3200, 1200, 0, 100), 0, 100);
  sensorData.co2Level = analogRead(MQ135_PIN);

  updateDisplay();
}

// =========================
// Main Loop
// =========================
void loop() {
  
  // 1. Read Encoder Rotation
  int currentClkState = digitalRead(ENCODER_CLK);
  
  if (currentClkState != lastClkState && currentClkState == LOW) {
    bool clockwise = (digitalRead(ENCODER_DT) != currentClkState);
    
    // --- MODE: MAIN MENU ROTATION ---
    if (currentView == MAIN_MENU) {
      if (clockwise) mainMenusIndex = (mainMenusIndex + 1) % TOTAL_MAIN_ITEMS;
      else mainMenusIndex = (mainMenusIndex - 1 + TOTAL_MAIN_ITEMS) % TOTAL_MAIN_ITEMS;
    } 
    // --- MODE: TARGET SETTINGS ROTATION ---
    else if (currentView == TARGET_SETTINGS_VIEW) {
      if (currentEditState == SELECT_ITEM) {
        if (clockwise) targetMenuIndex = (targetMenuIndex + 1) % (TOTAL_TARGET_PARAMS + 1);
        else targetMenuIndex = (targetMenuIndex - 1 + (TOTAL_TARGET_PARAMS + 1)) % (TOTAL_TARGET_PARAMS + 1);
      } else {
        float increment = clockwise ? 1.0 : -1.0;
        switch(targetMenuIndex) {
          case 0: targetTemp += (increment * 0.5); break; 
          case 1: targetHumid += increment; break;
          case 2: targetSoil = constrain(targetSoil + (int)increment, 0, 100); break;
          case 3: targetCO2 = constrain(targetCO2 + ((int)increment * 10), 0, 2000); break; 
          case 4: 
            ledStripState = clockwise; 
            digitalWrite(LED_STRIP_PIN, ledStripState ? HIGH : LOW); 
            break;
        }
      }
    }
    // --- MODE: TEST MODUS ROTATION ---
    else if (currentView == TEST_MODUS_VIEW) {
      if (currentEditState == SELECT_ITEM) {
        if (clockwise) testMenuIndex = (testMenuIndex + 1) % (TOTAL_TEST_PARAMS + 1);
        else testMenuIndex = (testMenuIndex - 1 + (TOTAL_TEST_PARAMS + 1)) % (TOTAL_TEST_PARAMS + 1);
      } else {
        if (testMenuIndex >= 0 && testMenuIndex <= 3) {
          testPinStates[testMenuIndex] = clockwise;
          digitalWrite(testPins[testMenuIndex], testPinStates[testMenuIndex] ? HIGH : LOW);
        }
      }
    }
    updateDisplay();
  }
  lastClkState = currentClkState;

  // 2. Read Encoder Button Click
  if (digitalRead(ENCODER_SW) == LOW) {
    if (millis() - lastButtonPress > 250) { 
      
      if (currentView == MAIN_MENU) {
        if (mainMenusIndex == 0) {
          currentView = TARGET_SETTINGS_VIEW;
          targetMenuIndex = 0;
        } else {
          currentView = TEST_MODUS_VIEW;
          testMenuIndex = 0;
        }
        currentEditState = SELECT_ITEM;
      } 
      else if (currentView == TARGET_SETTINGS_VIEW) {
        if (targetMenuIndex == TOTAL_TARGET_PARAMS) { 
          currentView = MAIN_MENU;
        } else {
          currentEditState = (currentEditState == SELECT_ITEM) ? EDIT_ITEM : SELECT_ITEM;
        }
      } 
      else if (currentView == TEST_MODUS_VIEW) {
        if (testMenuIndex == 4) { 
          currentView = MAIN_MENU;
        } else {
          currentEditState = (currentEditState == SELECT_ITEM) ? EDIT_ITEM : SELECT_ITEM;
        }
      }
      
      updateDisplay();
      lastButtonPress = millis();
    }
  }

  // 3. Handle Sensor Reading & ESP-NOW Sending & Serial Printing
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    sensorData.temperature = bme.readTemperature();
    sensorData.humidity = bme.readHumidity();

    int rawSoil = analogRead(SOIL_PIN);
    sensorData.soilMoisture = map(rawSoil, 3200, 1200, 0, 100);
    sensorData.soilMoisture = constrain(sensorData.soilMoisture, 0, 100);
    sensorData.co2Level = analogRead(MQ135_PIN);

    // SERIAL MONITOR READOUT
    Serial.println("------ SENSOR DATA ------");
    Serial.print("Temperature: "); Serial.print(sensorData.temperature); Serial.println(" °C");
    Serial.print("Humidity: ");    Serial.print(sensorData.humidity);    Serial.println(" %");
    Serial.print("Soil Moisture: "); Serial.print(sensorData.soilMoisture); Serial.println(" %");
    Serial.print("CO2 Level (Raw ADC): "); Serial.println(sensorData.co2Level);
    Serial.print("LED Strip State: "); Serial.println(ledStripState ? "ON" : "OFF");
    Serial.print("Automation Status: "); Serial.println(currentView == TEST_MODUS_VIEW ? "PAUSED (Manual Mode)" : "RUNNING");
    Serial.print("Pin States -> D13: "); Serial.print(testPinStates[0] ? "ON" : "OFF");
    Serial.print(" | D12: "); Serial.print(testPinStates[1] ? "ON" : "OFF");
    Serial.print(" | D14: "); Serial.print(testPinStates[2] ? "ON" : "OFF");
    Serial.print(" | D32: "); Serial.println(testPinStates[3] ? "ON" : "OFF");

    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *)&sensorData, sizeof(sensorData));
    if (result == ESP_OK) {
      Serial.println("Sent successfully");
    } else {
      Serial.println("Error sending data");
    }
    Serial.println("-------------------------");
    Serial.println();
    
    if (currentView == TEST_MODUS_VIEW && currentEditState == SELECT_ITEM) {
      updateDisplay();
    }
  }

  // 4. AUTOMATION LOGIC
  if (currentView != TEST_MODUS_VIEW) {
    
    // --- Temperature Control (D13_Temp) ---
    if (sensorData.temperature < targetTemp) {
      testPinStates[0] = true;
    } else {
      testPinStates[0] = false;
    }
    digitalWrite(PIN_TEMP, testPinStates[0] ? HIGH : LOW);

    // --- Air Humidity Control (D12_Humid) ---
    if (sensorData.humidity < targetHumid) {
      testPinStates[1] = true;
    } else {
      testPinStates[1] = false;
    }
    digitalWrite(PIN_HUMID, testPinStates[1] ? HIGH : LOW);

    // --- Soil Moisture Control (D14_Soil) ---
    if (sensorData.soilMoisture < targetSoil) {
      testPinStates[2] = true;
    } else {
      testPinStates[2] = false;
    }
    digitalWrite(PIN_SOIL, testPinStates[2] ? HIGH : LOW);

    // --- CO2 Level Control (D32_CO2) ---
    if (sensorData.co2Level > targetCO2) {
      testPinStates[3] = true;
    } else {
      testPinStates[3] = false;
    }
    digitalWrite(PIN_CO2, testPinStates[3] ? HIGH : LOW);
  }
}
