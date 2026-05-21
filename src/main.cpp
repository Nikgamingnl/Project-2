#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Arduino.h>
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C
//Adafruit_BME280 bme(BME_CS); // hardware SPI
//Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI

unsigned long delayTime;

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
    Serial.println(F("BME280 test"));

    if (! bme.begin(0x77, &Wire)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1);
    }

    Serial.println("-- Default Test --");
    Serial.println("normal mode, 16x oversampling for all, filter off,");
    Serial.println("0.5ms standby period");
    delayTime = 5000;
    
   

    Serial.println();
  pinMode( MQS, INPUT);
pinMode( DPSDA, INPUT);
pinMode( BVS, INPUT);
pinMode( MQS, INPUT);
pinMode( INPUT1, OUTPUT);
pinMode( INPUT3, OUTPUT);
}

void loop(){
      
  printValues();
  delay(delayTime);
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







void printValues() {
    Serial.print("Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" *C");

    Serial.print("Pressure = ");

    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" hPa");

    Serial.print("Approx. Altitude = ");
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(" m");

    Serial.print("Humidity = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");

    Serial.println();
}