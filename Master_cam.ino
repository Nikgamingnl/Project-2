
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>

// =========================
// Pin Definitions
// =========================
#define SDA_PIN 21
#define SCL_PIN 22

#define SOIL_PIN 34
#define MQ135_PIN 35

// =========================
// Receiver MAC Address
// =========================
uint8_t receiverAddress[] = {0xAC, 0xA7, 0x04, 0x26, 0x05, 0xB0};

// =========================
// BME280 Instance
// =========================
Adafruit_BME280 bme;

// =========================
// Structure to send data
// =========================
typedef struct struct_message {
  float temperature;
  float humidity;
  int soilMoisture;
  int co2Level;
} struct_message;

struct_message sensorData;

// =========================
// Structure to receive data
// =========================
typedef struct struct_response {
  char status[32];
} struct_response;

struct_response incomingResponse;

esp_now_peer_info_t peerInfo;

// =========================
// Callback when data is sent
// =========================
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {

  Serial.print("Last Packet Send Status: ");

  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Delivery Success");
  } else {
    Serial.println("Delivery Fail");
  }
}

// =========================
// Callback when data received
// =========================
void OnDataRecv(const esp_now_recv_info *info,
                const uint8_t *incomingData,
                int len) {

  memcpy(&incomingResponse, incomingData, sizeof(incomingResponse));

  Serial.print("Response received: ");
  Serial.println(incomingResponse.status);
}

// =========================
// Setup
// =========================
void setup() {

  Serial.begin(115200);

  // Start I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("Starting BME280...");

  // Try address 0x76 first
  bool status = bme.begin(0x76);

  // If not found, try 0x77
  if (!status) {
    status = bme.begin(0x77);
  }

  // Stop if sensor not found
  if (!status) {
    Serial.println("Could not find BME280 sensor!");
    Serial.println("Check wiring and I2C address.");
    while (1);
  }

  Serial.println("BME280 initialized successfully!");

  // =========================
  // WiFi Station Mode
  // =========================
  WiFi.mode(WIFI_STA);

  // =========================
  // Initialize ESP-NOW
  // =========================
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callbacks
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  // =========================
  // Register Peer
  // =========================
  memcpy(peerInfo.peer_addr, receiverAddress, 6);

  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("ESP-NOW Ready");
}

// =========================
// Main Loop
// =========================
void loop() {

  // Read BME280
  sensorData.temperature = bme.readTemperature();
  sensorData.humidity = bme.readHumidity();

  // Read Soil Moisture
  int rawSoil = analogRead(SOIL_PIN);

  sensorData.soilMoisture =
      map(rawSoil, 3200, 1200, 0, 100);

  sensorData.soilMoisture =
      constrain(sensorData.soilMoisture, 0, 100);

  // Read MQ135
  sensorData.co2Level = analogRead(MQ135_PIN);

  // =========================
  // Print values
  // =========================
  Serial.println("------ SENSOR DATA ------");

  Serial.print("Temperature: ");
  Serial.print(sensorData.temperature);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(sensorData.humidity);
  Serial.println(" %");

  Serial.print("Soil Moisture: ");
  Serial.print(sensorData.soilMoisture);
  Serial.println(" %");

  Serial.print("CO2 Level: ");
  Serial.println(sensorData.co2Level);

  // =========================
  // Send via ESP-NOW
  // =========================
  esp_err_t result = esp_now_send(
      receiverAddress,
      (uint8_t *)&sensorData,
      sizeof(sensorData));

  if (result == ESP_OK) {
    Serial.println("Sent successfully");
  } else {
    Serial.println("Error sending data");
  }

  Serial.println("-------------------------");
  Serial.println();

  // Send every 5 seconds
  delay(5000);
}
