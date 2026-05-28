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
// Primary I2C Pins (For BME280)
#define BME_SDA 21
#define BME_SCL 22

// Secondary I2C Pins (For OLED Display)
#define OLED_SDA 25
#define OLED_SCL 33

#define SOIL_PIN 34
#define MQ135_PIN 35

// LED Strip Data Pin
#define LED_STRIP_PIN 4

// Rotary Encoder Pins
#define ENCODER_CLK 14
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
// Sensor & UI Variables
// =========================
Adafruit_BME280 bme;

// Menu State Machine
enum MenuState { SELECT_PARAM, EDIT_PARAM };
MenuState currentState = SELECT_PARAM;

int currentMenuIndex = 0;
const int TOTAL_PARAMS = 5; // Increased to 5 for the LED Strip
const char* paramNames[TOTAL_PARAMS] = {
  "Temp Target", 
  "Humid Target", 
  "Soil Target", 
  "CO2 Target",
  "LED Strip"
};

// Target Values (Adjustable via Encoder)
float targetTemp = 25.0;
float targetHumid = 50.0;
int targetSoil = 40;
int targetCO2 = 400;
bool ledStripState = false; // False = OFF, True = ON

// Encoder Tracking
int lastClkState;
unsigned long lastButtonPress = 0;

// =========================
// Structures for ESP-NOW
// =========================
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

// Timing helper for sensor sending
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
// UI Display Function
// =========================
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.println("=== TARGET SETTINGS ===");
  display.println("");

  for (int i = 0; i < TOTAL_PARAMS; i++) {
    if (i == currentMenuIndex) {
      if (currentState == EDIT_PARAM) {
        display.print("[>] "); 
      } else {
        display.print("> ");   
      }
    } else {
      display.print("  ");
    }
    
    display.print(paramNames[i]);
    display.print(": ");
    
    // UI Rendering Logic per option
    if (i == 0) display.print(targetTemp, 1);
    else if (i == 1) display.print(targetHumid, 1);
    else if (i == 2) display.print(targetSoil);
    else if (i == 3) display.print(targetCO2);
    else if (i == 4) display.print(ledStripState ? "ON" : "OFF");
    
    display.println();
  }
  display.display();
}

// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);

  // Initialize Pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastClkState = digitalRead(ENCODER_CLK);

  // Initialize LED pin as output
  pinMode(LED_STRIP_PIN, OUTPUT);
  digitalWrite(LED_STRIP_PIN, LOW); // Start turned off

  // Start Default I2C Bus for BME280
  Wire.begin(BME_SDA, BME_SCL);

  // Start Second I2C Bus for OLED Screen
  Wire1.begin(OLED_SDA, OLED_SCL);

  // Initialize OLED using Wire1
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println("OLED failed to initialize on Wire1");
  }

  // Initialize BME280 using standard Wire
  bool status = bme.begin(0x76, &Wire);
  if (!status) status = bme.begin(0x77, &Wire);
  if (!status) {
    Serial.println("Could not find BME280 sensor!");
    while (1);
  }

  // Initialize WiFi & ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

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
    
    if (currentState == SELECT_PARAM) {
      if (clockwise) {
        currentMenuIndex = (currentMenuIndex + 1) % TOTAL_PARAMS;
      } else {
        currentMenuIndex = (currentMenuIndex - 1 + TOTAL_PARAMS) % TOTAL_PARAMS;
      }
    } 
    else if (currentState == EDIT_PARAM) {
      float increment = clockwise ? 1.0 : -1.0;
      
      switch(currentMenuIndex) {
        case 0: targetTemp += (increment * 0.5); break; 
        case 1: targetHumid += increment; break;
        case 2: targetSoil = constrain(targetSoil + (int)increment, 0, 100); break;
        case 3: targetCO2 = constrain(targetCO2 + ((int)increment * 10), 0, 2000); break; 
        case 4: 
          // Flips the boolean state when rotated in either direction
          ledStripState = !ledStripState; 
          digitalWrite(LED_STRIP_PIN, ledStripState ? HIGH : LOW);
          break;
      }
    }
    updateDisplay();
  }
  lastClkState = currentClkState;

  // 2. Read Encoder Button Click
  if (digitalRead(ENCODER_SW) == LOW) {
    if (millis() - lastButtonPress > 250) { 
      if (currentState == SELECT_PARAM) {
        currentState = EDIT_PARAM;
      } else {
        currentState = SELECT_PARAM;
      }
      updateDisplay();
      lastButtonPress = millis();
    }
  }

  // 3. Handle Sensor Reading, Serial Printing & ESP-NOW Sending
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
    Serial.print("LED Strip State: "); Serial.println(ledStripState ? "ON" : "OFF");

    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *)&sensorData, sizeof(sensorData));
    
    if (result == ESP_OK) {
      Serial.println("Sent successfully");
    } else {
      Serial.println("Error sending data");
    }
    Serial.println("-------------------------");
    Serial.println();
  }
}
