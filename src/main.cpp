#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Arduino.h>
#define BMP_SCK  (13)
#define BMP_MISO (12)
#define BMP_MOSI (11)
#define BMP_CS   (10)
#define BMP_SCK  (13)
#define BMP_MISO (12)
#define BMP_MOSI (11)
#define BMP_CS   (10)

Adafruit_BMP280 bmp; // I2C
//Adafruit_BMP280 bmp(BMP_CS); // hardware SPI
//Adafruit_BMP280 bmp(BMP_CS, BMP_MOSI, BMP_MISO,  BMP_SCK);






int ventileren = 0;
int water = 0;
int verwarmen = 0;
int vochtiger = 0;
int licht = 0;
#define BMES 2
#define ESPCLK 4
#define DPSDA 16
#define ROENS 26
#define MQS 17
#define BVS 27
#define heatingpad 13
#define GROWLIGHT 32
#define HUMIDIFIER 12
#define WATERPOMP 14
#define INPUT1 23
#define INPUT2 21
#define INPUT3 15
#define INPUT4 5
#define ENABLEA 22
#define ENABLEB 24

void setup() {
Serial.begin(9600);
while ( !Serial ) delay(100);   // wait for native usb
  Serial.println(F("BMP280 test"));
  unsigned status;
  //status = bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID);
  status = bmp.begin();
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    Serial.print("SensorID was: 0x"); Serial.println(bmp.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    while (1) delay(10);
  }

  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */


  pinMode( MQS, INPUT);
pinMode( DPSDA, INPUT);
pinMode( BVS, INPUT);
pinMode( MQS, INPUT);
pinMode( INPUT1, OUTPUT);
pinMode( INPUT3, OUTPUT);
}

void loop(){
    Serial.print(F("Temperature = "));
    Serial.print(bmp.readTemperature());
    Serial.println(" *C");

    Serial.print(F("Pressure = "));
    Serial.print(bmp.readPressure());
    Serial.println(" Pa");

    Serial.print(F("Approx altitude = "));
    Serial.print(bmp.readAltitude(1013.25)); /* Adjusted to local forecast! */
    Serial.println(" m");

    Serial.println();
    delay(2000);   

  if (digitalRead(MQS) == HIGH){
  digitalWrite(INPUT1, HIGH);
  digitalWrite(INPUT3, HIGH);
  }else{  
  digitalWrite(INPUT1, LOW);
  digitalWrite(INPUT3, LOW);

  }
  if (verwarmen == 1){
    digitalWrite (heatingpad, HIGH);
  }
    if (vochtiger == 1){
    digitalWrite (HUMIDIFIER, HIGH);
  }
    if (water == 1){
    digitalWrite (WATERPOMP, HIGH);
  }
  if (licht == 1){
    digitalWrite (GROWLIGHT, HIGH);
  }
  }

