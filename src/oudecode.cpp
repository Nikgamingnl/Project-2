#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// IR Receiver pins

#define KY040_CLK 33
#define KY040_DT 32
#define KY040_SW 14
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define CLK_PIN 33  // CLK or A pin of the encoder
#define DT_PIN 32
// Pin definitions


int right2 = 1;
int left2 = 0;

int right = 1;
int left = 0;
int ja = 0;




Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// 'Selectie_Standoff', 128x64px

                                                      	

// State machine
enum State {
  SETUP,
  ALIVE,
  HIT,
  DEAD,
  IDLE,
  LEFT,
  RIGHT,
  SHOOTING
};
volatile State thisState = IDLE;
int prevCLK = HIGH;  // Initial state assumption

// IR receiver interrupt handler: records pulse durations
void IRAM_ATTR handleIrChange() {
	const uint32_t now = micros();
	const uint8_t state = digitalRead(IR_PIN);
	const uint32_t duration = (lastChangeUs == 0) ? 0 : (now - lastChangeUs);
	lastChangeUs = now;

	const uint8_t nextIndex = static_cast<uint8_t>((writeIndex + 1) % BUF_SIZE);
	if (nextIndex != readIndex) {  // Drop sample if buffer is full.
		pulseDurationUs[writeIndex] = duration;
		pulseState[writeIndex] = state;
		writeIndex = nextIndex;
	}
}

// Interrupt service routine to handle encoder changes
void IRAM_ATTR handleEncoder() {
  int clk = digitalRead(CLK_PIN);
  int dt = digitalRead(DT_PIN);

  if (clk != prevCLK) {
    // Detect direction based on phase difference
    if (dt != clk) {
      // Clockwise rotation (right)
      right2 = 1;
      left2 = 0;
      thisState = RIGHT;
    } else {
      left2 = 1;
      right2 = 0;
      thisState = LEFT;
    }
    prevCLK = clk;
  }
}

State currentState = SETUP;

// Button debouncing variables
bool prevTrigger = HIGH;  
bool prevReset   = HIGH;
bool prevHit     = HIGH;



// Function to update life LEDs
void updateLifeLEDs(int lives) {
  digitalWrite(PIN_LED1, lives >= 1 ? HIGH : LOW);
  digitalWrite(PIN_LED2, lives >= 2 ? HIGH : LOW);
  digitalWrite(PIN_LED3, lives >= 3 ? HIGH : LOW);
} 

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  display.clearDisplay();
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);
  // Configure IR receiver pins
  pinMode(IR_PIN, INPUT);  // IR receivers should not have pull-up
  pinMode(IR_STATUS_LED, OUTPUT);
  digitalWrite(IR_STATUS_LED, LOW);
  pinMode(5,OUTPUT);
  // Configure 38 kHz PWM for the IR LED using new ESP32 3.x LEDC API
  ledcAttach(IR_TX_PIN, LEDC_FREQ, LEDC_RES_BITS);
  ledcWrite(IR_TX_PIN, 0);
  
  // Attach interrupt to IR pin (both edges to capture demodulated pulses)
  attachInterrupt(digitalPinToInterrupt(IR_PIN), handleIrChange, CHANGE);
  
  // Configure pins
  pinMode(KY040_CLK, INPUT);
  pinMode(KY040_DT, INPUT);
  pinMode(KY040_SW, INPUT_PULLUP);
  
  pinMode(PIN_MAG, INPUT_PULLUP);
  pinMode(PIN_TRIGGER, INPUT_PULLUP);
  
  // PIN_HIT removed - now using IR_PIN (pin 16) for hit detection
  
  // Initialize hit state to current sensor reading to prevent false trigger at startup
  prevHitState = digitalRead(IR_PIN);
  Serial.print("Initial IR_PIN (hit sensor) state: ");
  Serial.println(prevHitState);
  
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_LED3, OUTPUT);
	pinMode(vibratePin, OUTPUT);
  digitalWrite(vibratePin, LOW);
	pinMode(vibratepin2, OUTPUT);
  digitalWrite(vibratepin2, LOW);
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
      display.clearDisplay();
      display.drawBitmap(0, 0, selectie[1] , 128,64,1);
      display.display();
  // Attach interrupt to CLK pin (falling edge for detection)
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), handleEncoder, CHANGE);

  Serial.println("Rotary Encoder State Machine Initialized");
  Serial.println("IR receiver ready. Waiting for remote signals...");
  thisState = IDLE;
  // Attach interrupt for hit sensor
  
  
  startMillis = millis();

 


}

void loop() {
  digitalWrite(5,HIGH);
	// Example: Print the current state (you can replace this with your logic)
  switch (thisState) {
    case IDLE:
      // Do nothing or handle idle state
      break;
    case LEFT:
      Serial.println("State: LEFT");
      left = 1;
      right = 0;
      if (ja == 0){
      display.clearDisplay();
      display.drawBitmap(0, 0, selectie[0] , 128,64,1);
      display.display();
      }
      thisState = IDLE;


      break;
    case RIGHT:
      Serial.println("State: RIGHT");
      right = 1;
      left =0;
     if (ja == 0){
      display.clearDisplay();
      display.drawBitmap(0, 0, selectie[1] , 128,64,1);
      display.display();
      }
      thisState = IDLE;
      break;
  }
 
 
 
  currentMillis = millis();
  if (isVibrating && currentMillis - vibrationStartTime >= 400){
    digitalWrite(vibratePin, LOW);
    isVibrating = false;
  }
   if (isVibrating2 && currentMillis - vibration2StartTime >= 600) {
    digitalWrite(vibratepin2, LOW);
    isVibrating2 = false;
  }
	// Read all inputs
  
  bool reloadButton = digitalRead(PIN_MAG);
  bool trigger = digitalRead(PIN_TRIGGER);

// Update magazine status with non-blocking debounce delay
  bool currentMagazineState = !reloadButton;
  
  // Detect magazine state change
  if (currentMagazineState != magazineInserted) {
    if (magazineChangeTime == 0) {
      // First detection of change, record the time
      magazineChangeTime = millis();
    } else if (millis() - magazineChangeTime >= MAGAZINE_DEBOUNCE_MS) {
      // Debounce period has passed, apply the change
      magazineInserted = currentMagazineState;
      magazineChangeTime = 0;
    }
  } else {
    // No change detected, reset the timer
    magazineChangeTime = 0;
  }
  
  // Detect button presses (edges)
  bool triggerPressed = (trigger == LOW && prevTrigger == HIGH);
  
  // Update previous states
 
  prevTrigger = trigger;
  
  
  // Check magazine insertion for reloading
  if (magazineInserted && !prevMagazineInserted && ammo < 6 && dood == 0) {
    ammo = 6;
  }
  if (!magazineInserted && prevMagazineInserted && dood == 0) {
    ammo = 0;
  }
  prevMagazineInserted = magazineInserted;
  // State machine
  switch (currentState) {
    case SETUP: {
      if (right == 1 && !digitalRead(KY040_SW) && ja == 0 && dood == 0) {
      lives = 3;
      ja = 1;
      ammo = 0;
      display.clearDisplay();
      display.drawBitmap(0, 0, leven[2] , 128,64,1);
      display.display();
        currentState = ALIVE;
      } else if (left == 1 && !digitalRead(KY040_SW) && ja == 0 && dood == 0) {
        lives = 1;
      ja = 1;
      ammo = 0;
      display.clearDisplay();
      display.drawBitmap(0, 0, leven[3] , 128,64,1);
      display.display();
      currentState = ALIVE;
      }
      break;
    }
    
    case ALIVE: {
      // Hit detection is now handled in the IR pulse processing section below
      // (removed duplicate hit detection code to prevent double-counting hits)
      
      if (lives <= 0) {
    
        currentState = DEAD;
      } else if (triggerPressed && ammo > 0 && magazineInserted) {
        ammo--;
        Serial.println(ammo);
				isVibrating = true;
        vibrationStartTime = millis();
        digitalWrite(vibratePin, HIGH);

        
        // Send IR signal mimicking NEC protocol for better detection
        Serial.println("Sending IR signal!");
        
        // NEC protocol header
        ledcWrite(IR_TX_PIN, LEDC_DUTY);
        delayMicroseconds(9000);  // 9ms header burst
        ledcWrite(IR_TX_PIN, 0);
        delayMicroseconds(4500);  // 4.5ms space
        
        // Send data bits (32 bits total to mimic complete NEC frame)
        for (int i = 0; i < 32; i++) {
          ledcWrite(IR_TX_PIN, LEDC_DUTY);
          delayMicroseconds(560);   // 560μs burst for each bit
          ledcWrite(IR_TX_PIN, 0);
          // Alternate between logical 0 and 1 timing
          delayMicroseconds((i % 2) ? 1690 : 560);
        }
        
        // Final stop bit
        ledcWrite(IR_TX_PIN, LEDC_DUTY);
        delayMicroseconds(560);
        ledcWrite(IR_TX_PIN, 0);
        
        startMillis = currentMillis;
        currentState = SHOOTING;
      }
      break;
    }

    case HIT: {
      if (currentMillis - startMillis >= HIT_DURATION) {
       hitsec = 0;
        currentState = ALIVE;
      }
      // Else remain in HIT
      break;
    }

    case SHOOTING: {
      if (currentMillis - startMillis >= RELOAD_DURATION_PARTIAL) {
        currentState = ALIVE;
      }
      break;
    }
    
    case DEAD: {
      // Dead state - do nothing
      break;
    }
}
      if(ja == 1 && lives != previousLives && dood == 0 || ja == 1 && right == 1 && right2 == 1 && dood == 0){
      switch(lives) {
      case 0:
      right2 = 0;
      left2 = 0;
      display.clearDisplay();
      display.drawBitmap(0, 0, leven[3] , 128,64,1);
      display.display();
      dood = 1;
      break;

      case 1:
      display.clearDisplay();
      display.drawBitmap(0, 0, leven[0] , 128,64,1);
      display.display();
      break;

      case 2:
      display.clearDisplay();
      display.drawBitmap(0, 0, leven[1] , 128,64,1);
      display.display();
      break;

      case 3:
      display.clearDisplay();
      display.drawBitmap(0, 0, leven[2] , 128,64,1);
      display.display();
      break;
      }
      previousLives = lives;
    }
    if(left == 1 && ja == 1 && hitsec == 0 && dood == 0){
      switch(ammo) {

      case 0:
      display.clearDisplay();
      display.drawBitmap(0, 0, kogel[6] , 128,64,1);
      display.display();
      break;

      case 1:
      display.clearDisplay();
      display.drawBitmap(0, 0, kogel[5] , 128,64,1);
      display.display();
      break;

      case 2:
      display.clearDisplay();
      display.drawBitmap(0, 0, kogel[4] , 128,64,1);
      display.display();
      break;

      case 3:
      display.clearDisplay();
      display.drawBitmap(0, 0, kogel[3] , 128,64,1);
      display.display();
      break;

      case 4:
      display.clearDisplay();
      display.drawBitmap(0, 0, kogel[2] , 128,64,1);
      display.display();
      break;

      case 5:
      display.clearDisplay();
      display.drawBitmap(0, 0, kogel[1] , 128,64,1);
      display.display();
      break;

      case 6:
      display.clearDisplay();
      display.drawBitmap(0, 0, kogel[0] , 128,64,1);
      display.display();
      break;
      }
    }
  
  // Process IR signals - move captured pulses from ISR buffer to main loop
  static uint32_t lastIrEdgeReportMs = 0;
  
  while (readIndex != writeIndex) {
    noInterrupts();
    const uint32_t duration = pulseDurationUs[readIndex];
    const uint8_t state = pulseState[readIndex];
    readIndex = static_cast<uint8_t>((readIndex + 1) % BUF_SIZE);
    interrupts();

    // Print IR pulse data for debugging
    Serial.print("IR State: ");
    Serial.print(state ? "HIGH" : "LOW");
    Serial.print("  duration(us): ");
    Serial.println(duration);

    // SIMPLIFIED: Detect ANY IR burst activity
    bool irSignalDetected = false;
    
    // Detect any IR burst (HIGH state with duration > 300us)
    if (state && duration > 300 && duration < 15000) {
      irSignalDetected = true;
      Serial.println("IR burst detected!");
    }
    
    if (irSignalDetected) {
      // Turn on status LED
      if (!irLedOn) {
        irLedOn = true;
        irLedOnTimeMs = currentMillis;
        digitalWrite(IR_STATUS_LED, HIGH);
        Serial.println("IR signal received - LED on!");
      }
      
      // Register a HIT immediately when IR burst is detected
      // Allow hits in ALIVE, SHOOTING, or HIT states (not just ALIVE)
      if (ja == 1 && (currentState == ALIVE || currentState == SHOOTING || currentState == HIT) && dood == 0) {
        if ((currentMillis - lastHitTime) >= HIT_DEBOUNCE_MS) {
          lives--;
          lastHitTime = currentMillis;
          Serial.print("HIT detected via IR burst! Lives remaining: ");
          Serial.println(lives);
          startMillis = currentMillis;
          hitsec = 1;
          right2 = 1;
          left2 = 1;
					isVibrating2 = true;
        vibration2StartTime = millis();
        digitalWrite(vibratepin2, HIGH);
          currentState = HIT;
        } else {
          Serial.println("IR hit ignored - too soon after last hit");
        }
      }
    }
  }

  // Handle IR status LED timing
  if (irLedOn) {
    if (currentMillis - irLedOnTimeMs >= IR_LED_DURATION_MS) {
      irLedOn = false;
      digitalWrite(IR_STATUS_LED, LOW);
      Serial.println("IR LED off");
    }
  }

  // If nothing has happened for 2 seconds, print idle status
  if (currentMillis - lastIrEdgeReportMs >= 2000) {
    lastIrEdgeReportMs = currentMillis;
    Serial.print("IR idle level: ");
    Serial.println(digitalRead(IR_PIN) ? "HIGH" : "LOW");
  }
  
  // Update life LEDs
  updateLifeLEDs(lives);

}
