#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define CLK_PIN 33
#define DT_PIN 32
#define SW_PIN 14

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

volatile int encoderPosition = 0;
volatile int lastCLK = HIGH;
volatile bool encoderUpdated = false;

void IRAM_ATTR handleEncoder() {
  int clk = digitalRead(CLK_PIN);
  int dt = digitalRead(DT_PIN);

  if (clk != lastCLK) {
    if (dt != clk) {
      encoderPosition++; // clockwise
    } else {
      encoderPosition--; // counter-clockwise
    }
    lastCLK = clk;
    encoderUpdated = true;
  }
}

void displayPosition(int position) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 16);
  display.print(F("Encoder:"));
  display.setCursor(0, 40);
  display.print(position);
  display.display();
}

void setup() {
  Serial.begin(115200);

  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  pinMode(SW_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Rotary encoder"));
  display.display();

  attachInterrupt(digitalPinToInterrupt(CLK_PIN), handleEncoder, CHANGE);
}

void loop() {
  if (encoderUpdated) {
    noInterrupts();
    int position = encoderPosition;
    encoderUpdated = false;
    interrupts();

    displayPosition(position);
    Serial.print(F("Encoder position: "));
    Serial.println(position);
  }
