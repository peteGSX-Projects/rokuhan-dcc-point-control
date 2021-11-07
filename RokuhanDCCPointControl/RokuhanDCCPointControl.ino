/*************************************************************
Arduino Rokuhan DCC Point Controller.

This uses motor controllers to control Rokuhan points and works with either:
- The L298 based FunduMoto "buzzer" type motor shield for two sets of points
- Two L293D motor control ICs for four sets of points

Define the MOTOR_CONTROLLER variable to select the above option:
- #define FUNDUMOTO // Use this for two points with the FunduMoto shield
- #define L293D     // Use this for four points with the L293D ICs

This is based on these example sketches supplied with the NMRA DCC library:
- NmraDccAccessoryDecoder_1
- NmraDccAccessoryDecoder_Pulsed_8

See the README for the full list of features and instructions.
*************************************************************/

// Include the NMRA DCC library
#include <NmraDcc.h>

// Create our DCC objects
NmraDcc  Dcc;
DCC_MSG  Packet;

// Define the motor controller in use, uncomment the appropriate one only (not both)
//#define FUNDUMOTO                             // Use this for two points with the FunduMoto shield
#define L293D                                 // Use this for four points with the L293D ICs

// Define our global variables
#define DCC_PIN     2                         // DCC input interupt pin
const int DccAckPin = A1;                     // DCC ACK output pin
uint16_t BaseTurnoutAddress;                  // First turnout address
long switchingPulse = 25;                     // Define the 25ms pulse required to switch
long switchingDelay = 100;                    // Define 100ms delay between switching to avoid coil burnout

// Define decoder version
#define DCC_DECODER_VERSION_NUM 1

// Define number of turnouts and turnout struct based on motor controller in use
#ifdef FUNDUMOTO
  #define NUM_TURNOUTS 2
  typedef struct {
    byte directionPin;                    // pin to control direction
    byte speedPin;                        // pin to control speed
    byte currentDirection;                // current direction of the point (LOW = close, HIGH = throw)
    byte newDirection;                    // new direction of the point (LOW = close, HIGH = throw)
    unsigned long lastSwitchStartMillis;  // variable for the last time it was switched
    unsigned long lastSwitchEndMillis;    // variable for the last time it switching ended
  } point_def;
  point_def points[NUM_TURNOUTS];
#elif defined(L293D)
  #define NUM_TURNOUTS 4
  typedef struct {
    byte directionPin1;                   // pin to connect to input 1/3 on the L293D
    byte directionPin2;                   // pin to connect to input 2/4 on the L293D
    byte speedPin;                        // pin to connect to enable 1/2 on the L293D
    byte currentDirection;                // current direction of the point (LOW = close, HIGH = throw)
    byte newDirection;                    // new direction of the point (LOW = close, HIGH = throw)
    unsigned long lastSwitchStartMillis;  // variable for the last time it was switched
    unsigned long lastSwitchEndMillis;    // variable for the last time it switching ended
  } point_def;
  point_def points[NUM_TURNOUTS];
#endif

// Define the struct for CVs
struct CVPair
{
  uint16_t  CV;
  uint8_t   Value;
};

// Define the factory default address CV pair (key, value)
CVPair FactoryDefaultCVs [] =
{
  {CV_ACCESSORY_DECODER_ADDRESS_LSB, DEFAULT_ACCESSORY_DECODER_ADDRESS & 0xFF},
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, DEFAULT_ACCESSORY_DECODER_ADDRESS >> 8},
};

// Define the index in the array that holds the factory default CVs
uint8_t FactoryDefaultCVIndex = 0;

void notifyCVResetFactoryDefault()
{
  // Make FactoryDefaultCVIndex non-zero and equal to num CV's to be reset 
  // to flag to the loop() function that a reset to Factory Defaults needs to be done
  FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs)/sizeof(CVPair);
};

// This function is called by the NmraDcc library when a DCC ACK needs to be sent
// Calling this function should cause an increased 60ma current drain on the power supply for 6ms to ACK a CV Read 
void notifyCVAck(void)
{
  Serial.println("notifyCVAck");
  
  digitalWrite( DccAckPin, HIGH );
  delay( 10 );  
  digitalWrite( DccAckPin, LOW );
}

// Uncomment to print all DCC Packets
//#define NOTIFY_DCC_MSG
#ifdef  NOTIFY_DCC_MSG
void notifyDccMsg( DCC_MSG * Msg)
{
  Serial.print("notifyDccMsg: ");
  for(uint8_t i = 0; i < Msg->Size; i++)
  {
    Serial.print(Msg->Data[i], HEX);
    Serial.write(' ');
  }
  Serial.println();
}
#endif

// This function is called whenever a normal DCC Turnout Packet is received and we're in Output Addressing Mode
void notifyDccAccTurnoutOutput( uint16_t Addr, uint8_t Direction, uint8_t OutputPower )
{
  Serial.print("notifyDccAccTurnoutOutput: ");
  Serial.print(Addr,DEC);
  Serial.print(',');
  Serial.print(Direction,DEC);
  Serial.print(',');
  Serial.println(OutputPower, HEX);
  // If the address of the DCC turnout is one of these, act on it
  if(( Addr >= BaseTurnoutAddress ) && ( Addr < (BaseTurnoutAddress + NUM_TURNOUTS )) && OutputPower ) {
    uint8_t point = Addr - BaseTurnoutAddress;    // Calculate which array reference to use
    unsigned long currentDccMillis = millis();
    // Flag the change if our turnout's new and current position match, and if we've exceeded the delay to the next switching time
    if (points[point].currentDirection == points[point].newDirection && currentDccMillis - points[point].lastSwitchEndMillis > switchingDelay) {
      // Only proceed if the new direction is different to the current direction
      if ((points[point].currentDirection == LOW && Direction == 1) || (points[point].currentDirection == HIGH && Direction == 0)) {
        // If new direction is 0 (close) but our current direction is HIGH (throw), flag the change and record the time for pulsing
        if (Direction == 0 && points[point].currentDirection == HIGH) {
          points[point].newDirection = LOW;
        // If new direction is 1 (throw) but our current direction is LOW (close), flag the change and record the time for pulsing
        } else if (Direction == 1 && points[point].currentDirection == LOW) {
          points[point].newDirection = HIGH;
        }
        Serial.println((String)"Setting turnout at address " + Addr + " (point " + point + ") to " + Direction);
        // Perform our pin digital write depending on motor config
        #ifdef FUNDUMOTO
          digitalWrite(points[point].directionPin, points[point].newDirection);
        #elif defined(L293D)
          if (points[point].newDirection == LOW) {
            digitalWrite(points[point].directionPin1, HIGH);
            digitalWrite(points[point].directionPin2, LOW);
          } else if (points[point].newDirection == HIGH) {
            digitalWrite(points[point].directionPin1, LOW);
            digitalWrite(points[point].directionPin2, HIGH);
          }
        #endif
        // Set speed to start switching and record when it started so we can pulse correctly
        analogWrite(points[point].speedPin, 255);
        points[point].lastSwitchStartMillis = currentDccMillis;
      }
    }
  }
}

void initTurnouts() {
  // Function to initialise the turnout pins etc.
  #ifdef FUNDUMOTO
    Serial.println((String)"Initialising FunduMoto turnouts");
    points[0] = (point_def) {12,10,LOW,LOW,0,0};
    points[1] = (point_def) {13,11,LOW,LOW,0,0};
    for (int point = 0; point < NUM_TURNOUTS; point++) {
      pinMode(points[point].directionPin, OUTPUT);
      pinMode(points[point].speedPin, OUTPUT);
      digitalWrite(points[point].directionPin, LOW);
      analogWrite(points[point].speedPin, 0);
    }
  #elif defined(L293D)
    Serial.println((String)"Initialising L293D turnouts");
    points[0] = (point_def) {A4,A5,5,LOW,LOW,0,0};
    points[1] = (point_def) {4,7,6,LOW,LOW,0,0};
    points[2] = (point_def) {8,11,9,LOW,LOW,0,0};
    points[3] = (point_def) {12,13,10,LOW,LOW,0,0};
    for (int point = 0; point < NUM_TURNOUTS; point++) {
      pinMode(points[point].directionPin1, OUTPUT);
      pinMode(points[point].directionPin2, OUTPUT);
      pinMode(points[point].speedPin, OUTPUT);
      digitalWrite(points[point].directionPin1, HIGH);
      digitalWrite(points[point].directionPin2, LOW);
      analogWrite(points[point].speedPin, 0);
    }
  #endif
}

void processTurnouts() {
  // Function to process the turnouts and stop the switching when pulse time is done
  unsigned long processMillis = millis();
  for (uint8_t point = 0; point < (NUM_TURNOUTS); point++) {
    if (points[point].currentDirection != points[point].newDirection && processMillis - points[point].lastSwitchStartMillis > switchingPulse) {
      // Set speed to stop switching, update current direction, and record when switching finished to calculate delay for next switching
      analogWrite(points[point].speedPin, 0);
      points[point].currentDirection = points[point].newDirection;
      points[point].lastSwitchEndMillis = processMillis;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  
  // Configure the DCC CV Programing ACK pin for an output
  pinMode( DccAckPin, OUTPUT );
  Serial.println((String)"NMRA DCC Rokuhan Turnout Controller version " + DCC_DECODER_VERSION_NUM);
  #ifdef FUNDUMOTO
    Serial.println((String)"Configured for FunduMoto controller to control " + NUM_TURNOUTS + " turnouts");
  #elif defined(L293D)
    Serial.println((String)"Configured for L293D controller to control " + NUM_TURNOUTS + " turnouts");
  #endif
  
  // Setup which External Interrupt, the Pin it's associated with that we're using and enable the Pull-Up
  // Many Arduino Cores now support the digitalPinToInterrupt() function that makes it easier to figure out the
  // Interrupt Number for the Arduino Pin number, which reduces confusion. 
#ifdef digitalPinToInterrupt
  Dcc.pin(DCC_PIN, 0);
#else
  Dcc.pin(0, DCC_PIN, 1);
#endif
  
  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE, 0 );

  //BaseTurnoutAddress = (((Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB) * 64) + Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB) - 1) * 4) + 1  ;
  BaseTurnoutAddress = 1;
  // Initialise our turnouts
  initTurnouts();
  uint8_t lastTurnout = BaseTurnoutAddress + NUM_TURNOUTS - 1;
  Serial.print((String)"Init Done, base turnout address is: ");
  Serial.print(BaseTurnoutAddress, DEC);
  Serial.print(" and last turnout address is ");
  Serial.println(lastTurnout, DEC);
}

void loop()
{
  // Process turnouts to ensure switching happens, and do this before checking for DCC operations to ensure switching finishes in time
  processTurnouts();
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();
  if( FactoryDefaultCVIndex && Dcc.isSetCVReady())
  {
    FactoryDefaultCVIndex--; // Decrement first as initially it is the size of the array 
    Dcc.setCV( FactoryDefaultCVs[FactoryDefaultCVIndex].CV, FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
  }
}
