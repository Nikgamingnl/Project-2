#include <Arduino.h>
#define BMES 2
#define ESPCLK 4
#define DPSDA 16
#define ROENS 17
#define MQS 26
#define BVS 27
#define HEATING PAD 13
#define GROW LIGHT 32
#define HUMIDIFIER 12
#define WATERPOMP 14
#define INPUT1 23
#define INPUT2 21
#define INPUT3 19
#define INPUT4 5
#define ENABLEA 22
#define ENABLEB 24

void setup{
 Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  display.clearDisplay();
}