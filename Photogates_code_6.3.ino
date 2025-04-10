//State machine for Photogates control box
//Created by Michael Freeman, March 2025
//for CPR Baja

//

//converted from 5.2 for the adafruit feather M0 Express


// ===== Libraries =====
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

// ===== Pin Definitions =====
#define GATE_1_PHOTORESISTOR_PIN A0
#define GATE_2_PHOTORESISTOR_PIN A1
#define I2C_SDA 27
#define I2C_SCL 26

#define BUTTON_PIN 6
#define GATE_1_LED 10
#define GATE_2_LED 11

#define RED_POWER_LED A2
#define GREEN_POWER_LED A3
#define BLUE_POWER_LED A4

#define VBATT_PIN A7

// ===== State Machine Enum =====

bool serialPrintState = true; // if true, state changes are printed to the serial monitor

enum State { INIT, IDLE, SET, TIMING1TO2, TIMING2TO1 };
const char* stateNames[] = { "INIT", "IDLE", "SET", "TIMING1T2", "TIMING2TO1" };

// ===== Sensor Objects =====

//button knows if it is currently being pressed, was pressed on the last cycle, and handles pin assignments
class Button {
  //Button pin is on INPUT_PULLUP 
  //Button is NO
  //so digitalRead(BUTTON_PIN) returns HIGH, ie true, if pressed
  private:
    int buttonPin;
    bool isPressed;
    bool prevPressed;
    
  public:
    bool wasPressed;  //if the button was pressed the previous code cycle
  
    void  begin(int _buttonPin){
      isPressed = false;
      prevPressed = false;
      buttonPin = _buttonPin;
    }
    
    void updateAll(){
      wasPressed = !prevPressed && isPressed;
      if (wasPressed){
      }
      prevPressed = isPressed;
      isPressed = !digitalRead(buttonPin);
    }
};

//RGB can be any RGB color, and be set to on or off, and handles pin assignments
class RGB_LED {
  private:
    int RED_PIN, GREEN_PIN, BLUE_PIN;
    int currentRed, currentGreen, currentBlue;
    bool isOn;
    
  public:    
    // Assign pins and start with the LED turned off.
    void begin(int _RED_PIN, int _GREEN_PIN, int _BLUE_PIN) {
      RED_PIN = _RED_PIN;
      GREEN_PIN = _GREEN_PIN;
      BLUE_PIN = _BLUE_PIN;
      currentRed = currentGreen = currentBlue = 254;
      
      pinMode(RED_PIN, OUTPUT);
      pinMode(GREEN_PIN, OUTPUT);
      pinMode(BLUE_PIN, OUTPUT);
      
      off();
    }
    
    // Set the LED's color.
    // Parameters: true for HIGH (on) and false for LOW (off)
    
    void setColor(int R, int G, int B) {
      currentRed = R;
      currentGreen = G;
      currentBlue = B;
      if (isOn) {
        digitalWrite(RED_PIN, constrain(currentRed,0, 255));
        digitalWrite(GREEN_PIN, constrain(currentGreen,0, 255));
        digitalWrite(BLUE_PIN, constrain(currentBlue,0, 255));
      }
    }
    
    // Turn the LED on using the last set color.
    void on() {
      isOn = true;
      digitalWrite(RED_PIN, currentRed);
      digitalWrite(GREEN_PIN, currentGreen);
      digitalWrite(BLUE_PIN, currentBlue);
    }
    
    // Turn the LED off.
    void off() {
      isOn = false;
      digitalWrite(RED_PIN, 0);
      digitalWrite(GREEN_PIN, 0);
      digitalWrite(BLUE_PIN, 0);
    }
};

//PhotoGates keep track of the highest light reading. Covering the control lens is equivelant to arc flashing the lasor sensor lens ; in either case, the system may need recalibration
//PhotoGates also knows if its laser beam has been broken, and if and handels pin assignments

class PhotoGate {
  
  private:
    int neutral; //neutral should be the static photoresistor reading when no laser is on the sensor box. 
                 //if running on 5v, and using an analog reading between 0 (0V) and 1024 (5V), neutral would be around int 512
                 //if running on 3.3v, and using an analog reading between 0 (0V) and 1024 (5V), neutral would be around int 338
                 //360 was a value determined expirimentally using the photoresistor python file that exports the analog reading of the photogates to an excel file
               
    bool broken; //broken when light is low
    int light; //current light in volts (.5 to 4.5)
    int highest;
    int middle;
    byte Apin;
    
  public:
    bool isBroken() {
      return broken;
    }

    void begin(byte _Apin, int _neutral = 360) {
      Apin = _Apin;
      neutral = _neutral;
      pinMode(Apin, INPUT);
      highest = 0;
    }
    
    void updateAll() {
      light = analogRead(Apin);

      if (light > highest) {
        highest = light;
        Serial.print("new high! ");
      }

      // Update mid (tune this)
      middle = (neutral + highest) / 2;
      // Check if blocked
      broken = light < middle;
    }
};

// ===== Time Storage =====

#define MAX_HISTORY 100

class Times {
  private:
    float history[MAX_HISTORY];
    int count;
  public:
    float recentTime;
    float startTime;

    // Constructor to initialize values
    Times() : recentTime(0), count(0), startTime(0)  {
      for (int i = 0; i < MAX_HISTORY; i++) {
        history[i] = 0;
      }
    }

    // Function to add a new time
    void addTime(float newTime) {
      // Shift history to make space for new recentTime
      for (int i = MAX_HISTORY - 1; i > 0; i--) {
        history[i] = history[i - 1];
      }

      // Store recent time in history[0]
      if (count < MAX_HISTORY) {
        count++;
      }
      recentTime = newTime;
      history[0] = recentTime;
    }
    // Function to print stored times
    //prints out all stored non zero times, as many as 100 before memory rolls over
    void printlnHistory() {
      for (auto time : history) {
        if (time != 0) {
          Serial.println(time);
        }
      }
    }
};

class Power {
  private:
    float VBatt;
    int VBattPin;

    enum Charge { FULL, MEDIUM, EMPTY };
    Charge charge;

  public:
    void begin(int _VBattPin) {
      VBattPin = _VBattPin;
    }

    void update() {
      int raw = analogRead(VBattPin);
      VBatt = raw * (3.3 / 1023.0) *2 ;  // Convert ADC value to voltage - multiply by 2 to account for voltage divider

      if (VBatt > 3.90) {
        charge = FULL;
      } else if (VBatt < 3.0) {
        charge = EMPTY;
      } else {
        charge = MEDIUM;
      }
    }

    float getVoltage() {
      return VBatt;
    }

    int getChargeLevel() {
      switch (charge) {
        case FULL: return 0;
        case MEDIUM: return 1;
        case EMPTY: return 2;
        default: return 3;
      }
    }
};

// ===== Setup  =====
Adafruit_7segment seven_segment_disp_matrix = Adafruit_7segment();
Times timesData;
PhotoGate Gate1;
PhotoGate Gate2;
State currentState = INIT;
Button setButton;
RGB_LED power_LED;
Power power;

void setup() {
  
  seven_segment_disp_matrix.begin(0x70);
  
  Gate1.begin(GATE_1_PHOTORESISTOR_PIN);
  Gate2.begin(GATE_2_PHOTORESISTOR_PIN);
  setButton.begin(BUTTON_PIN);
  power_LED.begin(RED_POWER_LED, GREEN_POWER_LED, BLUE_POWER_LED);
  power.begin(VBATT_PIN);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);//D2
  pinMode(GATE_1_LED, OUTPUT);       //D3
  pinMode(GATE_2_LED, OUTPUT);      //D4

  Serial.begin(9600);
  for (uint8_t digit = 0; digit < 5; digit++) {  // Digits 0–4
    for (uint8_t i = 0; i < 8; i++) {            // Segments A–G + DP
      uint8_t seg = 1 << i;  // Bitmask for current segment
      seven_segment_disp_matrix.clear();
      seven_segment_disp_matrix.writeDigitRaw(digit, seg);  // Normal digit write
      seven_segment_disp_matrix.writeDisplay();

      // ⏱ Skip delay if this is digit 2 and no visible colon segment is on
      if (!(digit == 2 && (seg != 0b00000010))) {
        delay(60);  // Only delay if a visible segment is active
      }
    }
  }
  Serial.println("Serial connected");
}

void loop() {
  //heart beat blue LED lets the user know the code is running
  power.update();
  Serial.println(power.getChargeLevel());
  Serial.println(analogRead(VBATT_PIN));
  if ((millis() / 1000) % 2 == 0) { // Check if seconds are even
     switch (power.getChargeLevel()) {
        case 0: power_LED.setColor(0,255,0); break;
        case 1: power_LED.setColor(255,200,0); break;
        case 2: power_LED.setColor(255,0,0); break;
        case 3: power_LED.setColor(0,0,255); break;
     }
     power_LED.on();
   
  } else {
    power_LED.off();
  }
  
  Gate1.updateAll();
  Gate2.updateAll();
  setButton.updateAll();
  
  digitalWrite(GATE_1_LED, Gate1.isBroken() ? HIGH : LOW);
  digitalWrite(GATE_2_LED, Gate2.isBroken() ? HIGH : LOW);

  switch (currentState) {
    case INIT: initState(); break;
    case IDLE: idleState(); break;
    case SET: setState(); break;
    case TIMING1TO2: timing1to2State(); break;
    case TIMING2TO1: timing2to1State(); break;
  }
}

// ==== State Functions ====
void initState() {
  Serial.println("Initializing...");

  currentState = IDLE;
  if(serialPrintState){
      Serial.println(stateNames[currentState]);
  }
}
void idleState() {
  seven_segment_disp_matrix.clear();
  seven_segment_disp_matrix.println(timesData.recentTime, 3);
  seven_segment_disp_matrix.writeDisplay();

  if (setButton.wasPressed) {
    currentState = SET;
    if(serialPrintState){
      Serial.println(stateNames[currentState]);
    }
  }
}

void setState() {
  if ((millis() / 130 % 2) == 0) { // Check if seconds are even
    seven_segment_disp_matrix.clear();
    seven_segment_disp_matrix.println(0.000, 3);
    seven_segment_disp_matrix.writeDisplay();
  } else {
    seven_segment_disp_matrix.clear();
    seven_segment_disp_matrix.writeDisplay();
  }
  if (setButton.wasPressed) {
    timesData.printlnHistory();
    currentState = IDLE;
    if(serialPrintState){
      Serial.println(stateNames[currentState]);
    }
  }

  if (Gate1.isBroken()) {
    timesData.startTime = millis()/1000.0;
    currentState = TIMING1TO2;
    if(serialPrintState){
      Serial.println(stateNames[currentState]);
    }

  }
  if (Gate2.isBroken()) {
    timesData.startTime = millis()/1000.0;
    currentState = TIMING2TO1;
    if(serialPrintState){
      Serial.println(stateNames[currentState]);
    }
  }
}

void timing1to2State() {
  seven_segment_disp_matrix.clear();
  seven_segment_disp_matrix.println((millis() / 1000.0 - timesData.startTime), 3);
  seven_segment_disp_matrix.writeDisplay();

  if (Gate2.isBroken()) {
    timesData.addTime(millis() / 1000.0 - timesData.startTime);
    Serial.println("Finished in " + String(timesData.recentTime) + "sec");
    currentState = IDLE;
    if(serialPrintState){
      Serial.println(stateNames[currentState]);
    }

  }
  if (setButton.wasPressed) {
    currentState = IDLE;
    if(serialPrintState){
      Serial.println(stateNames[currentState]);
    }
  }
}

void timing2to1State() {
  seven_segment_disp_matrix.clear();
  seven_segment_disp_matrix.println((millis() / 1000.0 - timesData.startTime), 3);
  seven_segment_disp_matrix.writeDisplay();

  if (Gate1.isBroken()) {
    timesData.addTime(millis() / 1000.0 - timesData.startTime);
    Serial.println("Finished in " + String(timesData.recentTime) + " sec");
    currentState = IDLE;
    if(serialPrintState){
      Serial.println(stateNames[currentState]);
    }
  }
  if (setButton.wasPressed) {
    currentState = IDLE;
    if(serialPrintState){
      Serial.println(stateNames[currentState]);
    }
  }
}
