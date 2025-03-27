//State machine for Photogates control box
//Created by Michael Freeman, March 2025
//for CPR Baja

//

//updates from V3


//timer object 


// ===== Libraries =====
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

// ===== Pin Definitions =====
#define GATE_1_PR_PIN A0
#define GATE_2_PR_PIN A1
#define I2C_SDA A4
#define I2C_SCL A5
#define BUTTON_PIN 2
#define GATE_1_LED 3
#define GATE_2_LED 4
#define POWER_LED 6

// ===== State Machine Enum =====

bool serialPrintState = false; // if true, state changes are printed to the serial monitor

enum State { INIT, IDLE, SET, TIMING1TO2, TIMING2TO1 };
const char* stateNames[] = { "INIT", "IDLE", "SET", "TIMING1T2", "TIMING2TO1" };

// ===== Button Object =====
class Button {
  //Button pin is on INPUT_PULLUP 
  //so digitalRead(BUTTON_PIN) returns LOW, ie false, if pressed
  private:
  int buttonPin;
  bool isPressed;
  bool prevPressed;
  public:
  bool wasPressed;
  
  begin(int _buttonPin){
    isPressed = false;
    prevPressed = false;
    buttonPin = _buttonPin;
  }
  updateAll(){
    wasPressed = !prevPressed && isPressed;
    if (wasPressed){
    }
    prevPressed = isPressed;
    isPressed = !digitalRead(buttonPin);
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

// ===== Photogate objects =====
class PhotoGate {
  
  private:
    int neutral;
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
        Serial.print("new high!");
      }

      // Update mid (tune this)
      middle = (neutral + highest) / 2;
      // Check if blocked
      broken = light > middle;
    }
};

// ===== Function Prototypes =====
void initState();
void idleState();
void setState();
void timingState();
// ===== Setup  =====

Adafruit_7segment seven_segment_disp_matrix = Adafruit_7segment();
Times timesData;
PhotoGate Gate1;
PhotoGate Gate2;
State currentState = INIT;
Button setButton;

void setup() {
  Serial.begin(9600);
  Serial.println("Serial connected");
  seven_segment_disp_matrix.begin(0x70);
  
  Gate1.begin(GATE_1_PR_PIN);
  Gate2.begin(GATE_2_PR_PIN);
  setButton.begin(BUTTON_PIN);
  
  pinMode(I2C_SDA, OUTPUT);         //A4 sda
  pinMode(I2C_SCL, OUTPUT);         //A5 scl
  pinMode(BUTTON_PIN, INPUT_PULLUP);//D2
  pinMode(GATE_1_LED, OUTPUT);       //D3
  pinMode(GATE_2_LED, OUTPUT);      //D4
  pinMode(POWER_LED, OUTPUT);       //D6
}

void loop() {
  //heart beat blue LED lets the user know the code is running
  if ((millis() / 1000) % 2 == 0) { // Check if seconds are even
    digitalWrite(POWER_LED, HIGH);
  } else {
    digitalWrite(POWER_LED, LOW);
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
