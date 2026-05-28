#include <esp_now.h>
#include <WiFi.h>

// Transmitter MAC Address (ESP32 Dev Kit)
uint8_t transmitterAddress[] = {0xA4, 0xF0, 0x0F, 0x5E, 0xF5, 0x90};

// Structure to receive sensor data (Must match Transmitter)
typedef struct struct_message {
    float temperature;
    float humidity;
    int soilMoisture;
    int co2Level;
} struct_message;

struct_message incomingReadings;

// Structure to send response back
typedef struct struct_response {
    char status[32];
} struct_response;

struct_response replyMessage;

esp_now_peer_info_t peerInfo;

// Callback when data is received
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  
  Serial.println("\n--- New Data Received ---");
  Serial.printf("Temperature: %.2f °C\n", incomingReadings.temperature);
  Serial.printf("Air Humidity: %.2f %%\n", incomingReadings.humidity);
  Serial.printf("Soil Moisture: %d %%\n", incomingReadings.soilMoisture);
  Serial.printf("CO2 Raw Level: %d\n", incomingReadings.co2Level);
  
  // Formulate a response back to the Dev Kit
  strcpy(replyMessage.status, "S3 Cam Received OK!");
  
  esp_err_t result = esp_now_send(transmitterAddress, (uint8_t *) &replyMessage, sizeof(replyMessage));
  if (result == ESP_OK) {
    Serial.println("Reply acknowledgment sent back.");
  }
}

// FIXED: Callback signature updated to use wifi_tx_info_t for modern 3.x.x cores
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("Reply Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}
 
void setup() {
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Register callbacks
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer (Dev Kit) so S3 can reply back directly inside the callback
  memcpy(peerInfo.peer_addr, transmitterAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}
 
void loop() {
  // S3 Cam logic goes here (e.g., taking pictures, processing data)
  delay(1000);
}