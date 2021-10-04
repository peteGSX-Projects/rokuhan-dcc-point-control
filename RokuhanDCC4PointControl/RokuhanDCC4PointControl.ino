/*************************************************************
Arduino Rokuhan DCC 4 Point Controller.

This uses two L293D motor controllers to control 4 Rokuhan points.

See the README for the full list of features and instructions.
*************************************************************/

// Include NMRA DCC library.
#include "nmradcc"

// define the structure for the definition of a point setup
typedef struct {
  byte directionPin1;                   // pin to connect to input 1/3 on the L293D
  byte directionPin2;                   // pin to connect to input 2/4 on the L293D
  byte speedPin;                        // pin to connect to enable 1/2 on the L293D
  byte switchPin;                       // pin to connect the direction switch button to
  byte currentDirection;                // current direction of the point (LOW = through, HIGH = branch)
  byte newDirection;                    // new direction of the point (LOW = through, HIGH = branch)
  unsigned long lastSwitchMillis;       // variable for the last time it was switched
  Switch *switchButton;                 // reference to switch object to poll the switch button
} point_def;

point_def points[4];                    // set up an array to hold the point definitions above

// define variables in use
unsigned long currentMillis = 0;        // variable to hold the current Millis time
long switchDelay = 25;                  // delay time in ms that the Rokuhan switch motor needs to be on to switch

void setup() {
  // Set up our serial output
  Serial.begin(9600);
  Serial.println("Rokuhan 4 Point Controller");

  // define our 4 points in use
  points[0] = (point_def) {A4,A5,5,A0,LOW,LOW,0,new Switch(A0)};
  points[1] = (point_def) {4,7,6,A1,LOW,LOW,0,new Switch(A1)};
  points[2] = (point_def) {8,11,9,A2,LOW,LOW,0,new Switch(A2)};
  points[3] = (point_def) {12,13,10,A3,LOW,LOW,0,new Switch(A3)};

  // set the correct pin mode and initial values for all defined pins (Switch object sets this for the button pins)
  for (int point = 0; point < 4; point++) {
    Serial.println((String)"Configuring point " + point);
    pinMode(points[point].directionPin1, OUTPUT);
    pinMode(points[point].directionPin2, OUTPUT);
    pinMode(points[point].speedPin, OUTPUT);
    digitalWrite(points[point].directionPin1, LOW);
    digitalWrite(points[point].directionPin2, LOW);
    analogWrite(points[point].speedPin, 0);
  }
}

void loop() {
  for (int point = 0; point < 4; point++) {
    currentMillis = millis();                                                 // record the current millis time
    if (points[point].currentDirection != points[point].newDirection) {       // if our points aren't set to the new direction yet, need to process
      if ((currentMillis - points[point].lastSwitchMillis) > switchDelay) {   // if our switching time has exceeded the delay, stop to avoid burnout
        analogWrite(points[point].speedPin, 0);
        digitalWrite(points[point].directionPin1, LOW);
        digitalWrite(points[point].directionPin2, LOW);
        points[point].currentDirection = points[point].newDirection;          // our current direction now matches new
        Serial.println("Switching end");
      }
    }
    points[point].switchButton->poll();
    if (points[point].switchButton->pushed()) {
      Serial.println((String)"Button for point " + point + " pushed");
      if (points[point].currentDirection == LOW) {                            // if our current direction is low (through), set to branch
        Serial.println("Switching to branch position");
        points[point].newDirection = HIGH;
        digitalWrite(points[point].directionPin1, LOW);
        digitalWrite(points[point].directionPin2, HIGH);
      } else {
        Serial.println("Switching to through position");
        points[point].newDirection = LOW;                                     // otherwise set to high (branch)
        digitalWrite(points[point].directionPin1, HIGH);
        digitalWrite(points[point].directionPin2, LOW);
      }
      analogWrite(points[point].speedPin, 255);
      points[point].lastSwitchMillis = currentMillis;                         // record the time we started switching
    }
  }
}
