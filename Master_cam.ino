#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =========================
// Pin Definitions
// =========================
#define BME_SDA 21
#define BME_SCL 22
#define OLED_SDA 19
#define OLED_SCL 18

#define SOIL_PIN 34
#define MQ135_PIN 35
#define LED_STRIP_PIN 4

// New Test Modus Pins
#define TEST_PIN_13 13
#define TEST_PIN_12 12
#define TEST_PIN_14 14
#define TEST_PIN_32 32
const int testPins[4] = {TEST_PIN_13, TEST_PIN_12, TEST_PIN_14, TEST_PIN_32};
const char* testPinNames[4] = {"Pin D13", "Pin D12", "Pin D14", "Pin D32"};
bool testPinStates[4] = {false, false, false, false}; // Track state of test pins

// Rotary Encoder Pins
#define ENCODER_CLK 25
#define ENCODER_DT  26
#define ENCODER_SW  27

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
const int TOTAL_TEST_PARAMS = 5; // 4 pins + 1 "Back" button

// =========================
// Target Variables & Core Structures
// =========================
Adafruit_BME280 bme;
float targetTemp = 25.0;
float targetHumid = 50.0;
int targetSoil = 40;
int targetCO2 = 400;
bool ledStripState = false;

int lastClkState;
unsigned long lastButtonPress = 0;

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

// =========================
// Callbacks
// =========================
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingResponse, incomingData, sizeof(incomingResponse));
  Serial.print("Response received: ");
  Serial.println(incomingResponse.status);
}

// =========================
// UI Render Manager
// =========================
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  // ----------------------------------------------------
  // VIEW: MAIN TOP LEVEL MENU
  // ----------------------------------------------------
  if (currentView == MAIN_MENU) {
    display.println("===== MAIN MENU =====");
    display.println("");
    for (int i = 0; i < TOTAL_MAIN_ITEMS; i++) {
      display.print(i == mainMenusIndex ? "> " : "  ");
      display.println(mainMenuItems[i]);
    }
  }
  // ----------------------------------------------------
  // VIEW: TARGET SETTINGS MODUS
  // ----------------------------------------------------
  else if (currentView == TARGET_SETTINGS_VIEW) {
    display.println("=== TARGET SETTINGS ===");
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
    // Add a virtual back button hint
    display.println("");
    display.print(targetMenuIndex == TOTAL_TARGET_PARAMS ? "> [Back to Main]" : "  [Back to Main]");
  }
  // ----------------------------------------------------
  // VIEW: HARDWARE TEST MODUS
  // ----------------------------------------------------
  else if (currentView == TEST_MODUS_VIEW) {
    display.println("==== TEST MODUS ====");
    display.println("");
    for (int i = 0; i < 4; i++) {
      if (i == testMenuIndex) {
        display.print(currentEditState == EDIT_ITEM ? "[>] " : "> ");
      } else {
        display.print("  ");
      }
      display.print(testPinNames[i]); display.print(": ");
      display.println(testPinStates[i] ? "HIGH (ON)" : "LOW (OFF)");
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

  // Initialize Input Pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastClkState = digitalRead(ENCODER_CLK);

  // Initialize Output Pins
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
  if (!status) {
    Serial.println("Could not find BME280 sensor!");
    while (1);
  }

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { return; }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  updateDisplay();
}

// =========================
// Main Loop
// =========================
void loop() {
  
  // ----------------------------------------------------
  // 1. Read Encoder Rotation
  // ----------------------------------------------------
  int currentClkState = digitalRead(ENCODER_CLK);
  
  if (currentClkState != lastClkState && currentClkState == LOW) {
    bool clockwise = (digitalRead(ENCODER_DT) != currentClkState);
    float increment = clockwise ? 1.0 : -1.0;
    
    // --- SCROLLING/EDITING LOGIC IN MAIN MENU ---
    if (currentView == MAIN_MENU) {
      if (clockwise) mainMenusIndex = (mainMenusIndex + 1) % TOTAL_MAIN_ITEMS;
      else mainMenusIndex = (mainMenusIndex - 1 + TOTAL_MAIN_ITEMS) % TOTAL_MAIN_ITEMS;
    } 
    // --- SCROLLING/EDITING LOGIC IN TARGET SETTINGS ---
    else if (currentView == TARGET_SETTINGS_VIEW) {
      if (currentEditState == SELECT_ITEM) {
        if (clockwise) targetMenuIndex = (targetMenuIndex + 1) % (TOTAL_TARGET_PARAMS + 1); // +1 for Back button
        else targetMenuIndex = (targetMenuIndex - 1 + (TOTAL_TARGET_PARAMS + 1)) % (TOTAL_TARGET_PARAMS + 1);
      } else {
        switch(targetMenuIndex) {
          case 0: targetTemp += (increment * 0.5); break; 
          case 1: targetHumid += increment; break;
          case 2: targetSoil = constrain(targetSoil + (int)increment, 0, 100); break;
          case 3: targetCO2 = constrain(targetCO2 + ((int)increment * 10), 0, 2000); break; 
          case 4: ledStripState = !ledStripState; digitalWrite(LED_STRIP_PIN, ledStripState ? HIGH : LOW); break;
        }
      }
    }
    // --- SCROLLING/EDITING LOGIC IN TEST MODUS ---
    else if (currentView == TEST_MODUS_VIEW) {
      if (currentEditState == SELECT_ITEM) {
        if (clockwise) testMenuIndex = (testMenuIndex + 1) % TOTAL_TEST_PARAMS;
        else testMenuIndex = (testMenuIndex - 1 + TOTAL_TEST_PARAMS) % TOTAL_TEST_PARAMS;
      } else {
        if (testMenuIndex >= 0 && testMenuIndex <= 3) {
          testPinStates[testMenuIndex] = !testPinStates[testMenuIndex];
          digitalWrite(testPins[testMenuIndex], testPinStates[testMenuIndex] ? HIGH : LOW);
        }
      }
    }
    updateDisplay();
  }
  lastClkState = currentClkState;

  // ----------------------------------------------------
  // 2. Read Encoder Button Click
  // ----------------------------------------------------
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
        if (targetMenuIndex == TOTAL_TARGET_PARAMS) { // Clicked "[Back to Main]"
          currentView = MAIN_MENU;
        } else {
          currentEditState = (currentEditState == SELECT_ITEM) ? EDIT_ITEM : SELECT_ITEM;
        }
      } 
      else if (currentView == TEST_MODUS_VIEW) {
        if (testMenuIndex == 4) { // Clicked "[Back to Main]"
          currentView = MAIN_MENU;
        } else {
          currentEditState = (currentEditState == SELECT_ITEM) ? EDIT_ITEM : SELECT_ITEM;
        }
      }
      
      updateDisplay();
      lastButtonPress = millis();
    }
  }

  // ----------------------------------------------------
  // 3. Handle Sensor Reading & ESP-NOW Sending
  // ----------------------------------------------------
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    sensorData.temperature = bme.readTemperature();
    sensorData.humidity = bme.readHumidity();

    int rawSoil = analogRead(SOIL_PIN);
    sensorData.soilMoisture = map(rawSoil, 3200, 1200, 0, 100);
    sensorData.soilMoisture = constrain(sensorData.soilMoisture, 0, 100);

    sensorData.co2Level = analogRead(MQ135_PIN);

    Serial.println("------ SENSOR DATA ------");
    Serial.print("Temperature: "); Serial.print(sensorData.temperature); Serial.println(" °C");
    Serial.print("Humidity: ");    Serial.print(sensorData.humidity);    Serial.println(" %");
    Serial.print("Soil Moisture: "); Serial.print(sensorData.soilMoisture); Serial.println(" %");
    Serial.print("CO2 Level: ");    Serial.println(sensorData.co2Level);

    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *)&sensorData, sizeof(sensorData));
    if (result == ESP_OK) { Serial.println("Sent successfully"); }
    else { Serial.println("Error sending data"); }
    Serial.println("-------------------------");
    Serial.println();
  }
}
