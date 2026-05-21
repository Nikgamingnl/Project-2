#include <Arduino.h>
int ventileren = 0;
int water = 0;
int verwarmen = 0;
int vochtiger = 0;
int licht = 0;
#define BMES 2
#define ESPCLK 4
#define DPSDA 16
#define ROENS 17
#define MQS 26
#define BVS 27
#define heatingpad 13
#define GROWLIGHT 32
#define HUMIDIFIER 12
#define WATERPOMP 14
#define INPUT1 23
#define INPUT2 21
#define INPUT3 19
#define INPUT4 5
#define ENABLEA 22
#define ENABLEB 24

void setup() {

}

void loop(){
  if (ventileren == 1){
  digitalWrite(INPUT1, HIGH);
  digitalWrite(INPUT3, HIGH);
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
