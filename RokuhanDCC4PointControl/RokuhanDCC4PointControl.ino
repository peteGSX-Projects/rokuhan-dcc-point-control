/*************************************************************
Arduino Rokuhan DCC 4 Point Controller.

This uses two L293D motor controllers to control 4 Rokuhan points.

See the README for the full list of features and instructions.
*************************************************************/

// Include NMRA DCC library.
#include <NmraDcc.h>

// Print every turnout call-back message
#define NOTIFY_TURNOUT_MSG

//#define NOTIFY_DCC_MSG

// Print debug messages
#define DEBUG_MSG

// If below is uncommented, it will force CVs to be written to the factory defaults
//#define FORCE_RESET_FACTORY_DEFAULT_CV

// Enable DCC ACK for programming and CV reading via port A1 (D15)
#define ENABLE_DCC_ACK 15

// Define the DCC input pin
#define DCC_PIN 2

// Define the number of turnouts, we need 4
#define NUM_TURNOUTS 4              // Set Number of Turnouts

// DCC decoder version
#define DCC_DECODER_VERSION_NUM 1

// Define the CV pair struct
struct CVPair
{
  uint16_t  CV;
  uint8_t   Value;
};

// To set the Turnout Addresses for this board you need to change the CV values for CV1 (CV_ACCESSORY_DECODER_ADDRESS_LSB) and 
// CV9 (CV_ACCESSORY_DECODER_ADDRESS_MSB) in the FactoryDefaultCVs structure below. The Turnout Addresses are defined as: 
// Base Turnout Address is: ((((CV9 * 64) + CV1) - 1) * 4) + 1 
// With NUM_TURNOUTS 8 (above) a CV1 = 1 and CV9 = 0, the Turnout Addresses will be 1..8, for CV1 = 2 the Turnout Address is 5..12

CVPair FactoryDefaultCVs [] =
{
  {CV_ACCESSORY_DECODER_ADDRESS_LSB, DEFAULT_ACCESSORY_DECODER_ADDRESS & 0xFF},
  {CV_ACCESSORY_DECODER_ADDRESS_MSB, DEFAULT_ACCESSORY_DECODER_ADDRESS >> 8},
};

// Define factory default CV index
uint8_t FactoryDefaultCVIndex = 0;

// Define our NMRA DCC objects and variables
NmraDcc  Dcc ;
DCC_MSG  Packet ;
uint16_t BaseTurnoutAddress;

// This function is called whenever a normal DCC Turnout Packet is received
void notifyDccAccTurnoutOutput( uint16_t Addr, uint8_t Direction, uint8_t OutputPower )
{
#ifdef  NOTIFY_TURNOUT_MSG
  Serial.print("notifyDccAccTurnoutOutput: Turnout: ") ;
  Serial.print(Addr,DEC) ;
  Serial.print(" Direction: ");
  Serial.print(Direction ? "Closed" : "Thrown") ;
  Serial.print(" Output: ");
  Serial.print(OutputPower ? "On" : "Off") ;
#endif
  if(( Addr >= BaseTurnoutAddress ) && ( Addr < (BaseTurnoutAddress + NUM_TURNOUTS )) && OutputPower )
  {
    uint16_t pinIndex = ( (Addr - BaseTurnoutAddress) << 1 ) + Direction ;
    // This line below may be where I need to call the switching function
    // pinPulser.addPin(outputs[pinIndex]);
#ifdef  NOTIFY_TURNOUT_MSG
    Serial.print(" Pin Index: ");
    Serial.print(pinIndex,DEC);
    // Commenting out these as they refer to the pulser pins
    //Serial.print(" Pin: ");
    //Serial.print(outputs[pinIndex],DEC);
#endif
  }
#ifdef  NOTIFY_TURNOUT_MSG
  Serial.println();
#endif
}

// define the structure for the definition of a point setup
typedef struct {
  byte directionPin1;                   // pin to connect to input 1/3 on the L293D
  byte directionPin2;                   // pin to connect to input 2/4 on the L293D
  byte speedPin;                        // pin to connect to enable 1/2 on the L293D
  byte currentDirection;                // current direction of the point (LOW = closed, HIGH = thrown)
  byte newDirection;                    // new direction of the point (LOW = closed, HIGH = thrown)
  unsigned long lastSwitchStartMillis;  // variable for the last time it was switched
  unsigned long lastSwitchEndMillis;    // variable for the last time switching ended
} point_def;

point_def points[4];                    // set up an array to hold the point definitions above

// define variables in use
unsigned long currentMillis = 0;        // variable to hold the current Millis time
long switchingTime = 25;                // delay time in ms that the Rokuhan switch motor needs to be on to switch
long switchingDelay = 100;              // delay time in ms between switching to avoid motor burnout

void initTurnoutPins() {
  // define our 4 points in use
  points[0] = (point_def) {A4,A5,5,LOW,LOW,0,0};
  points[1] = (point_def) {4,7,6,LOW,LOW,0,0};
  points[2] = (point_def) {8,11,9,LOW,LOW,0,0};
  points[3] = (point_def) {12,13,10,LOW,LOW,0,0};

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

void processTurnouts() {
  /* This is the original loop from the 4 point controller code
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
  */
}

void setup() {
  // Set up our serial output
  Serial.begin(115200);
  Serial.println("Rokuhan DCC 4 Point Controller");

  // Setup which External Interrupt, the Pin it's associated with that we're using and enable the Pull-Up
  // Many Arduino Cores now support the digitalPinToInterrupt() function that makes it easier to figure out the
  // Interrupt Number for the Arduino Pin number, which reduces confusion. 
#ifdef digitalPinToInterrupt
  Dcc.pin(DCC_PIN, 0);
#else
  Dcc.pin(0, DCC_PIN, 1);
#endif
  
  // Call the main DCC Init function to enable the DCC Receiver
  Dcc.init( MAN_ID_DIY, DCC_DECODER_VERSION_NUM, CV29_ACCESSORY_DECODER, 0 );

#ifdef DEBUG_MSG
  Serial.print("\nNMRA DCC Rokuhan Turnout Decoder. Ver: "); Serial.println(DCC_DECODER_VERSION_NUM,DEC);
#endif  

#ifdef FORCE_RESET_FACTORY_DEFAULT_CV
  Serial.println("Resetting CVs to Factory Defaults");
  notifyCVResetFactoryDefault(); 
#endif

  if( FactoryDefaultCVIndex == 0)  // Not forcing a reset CV Reset to Factory Defaults so initTurnoutPins
    initTurnoutPins();
}

void loop() {
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();

  // Process our turnouts to make sure they're set correctly
  processTurnouts();
  
  if( FactoryDefaultCVIndex && Dcc.isSetCVReady())
  {
    FactoryDefaultCVIndex--; // Decrement first as initially it is the size of the array
    uint16_t cv = FactoryDefaultCVs[FactoryDefaultCVIndex].CV;
    uint8_t val = FactoryDefaultCVs[FactoryDefaultCVIndex].Value;
#ifdef DEBUG_MSG
    Serial.print("loop: Write Default CV: "); Serial.print(cv,DEC); Serial.print(" Value: "); Serial.println(val,DEC);
#endif     
    Dcc.setCV( cv, val );
  }
}

void notifyCVChange(uint16_t CV, uint8_t Value)
{
#ifdef DEBUG_MSG
  Serial.print("notifyCVChange: CV: ") ;
  Serial.print(CV,DEC) ;
  Serial.print(" Value: ") ;
  Serial.println(Value, DEC) ;
#endif  

  Value = Value;  // Silence Compiler Warnings...

//  if((CV == CV_ACCESSORY_DECODER_ADDRESS_MSB) || (CV == CV_ACCESSORY_DECODER_ADDRESS_LSB) ||
//     (CV == CV_ACCESSORY_DECODER_OUTPUT_PULSE_TIME) || (CV == CV_ACCESSORY_DECODER_CDU_RECHARGE_TIME) || (CV == CV_ACCESSORY_DECODER_ACTIVE_STATE))
//    initPinPulser();  // Some CV we care about changed so re-init the PinPulser with the new CV settings
}

void notifyCVResetFactoryDefault()
{
  // Make FactoryDefaultCVIndex non-zero and equal to num CV's to be reset 
  // to flag to the loop() function that a reset to Factory Defaults needs to be done
  FactoryDefaultCVIndex = sizeof(FactoryDefaultCVs)/sizeof(CVPair);
};

// This function is called by the NmraDcc library when a DCC ACK needs to be sent
// Calling this function should cause an increased 60ma current drain on the power supply for 6ms to ACK a CV Read 
#ifdef  ENABLE_DCC_ACK
void notifyCVAck(void)
{
#ifdef DEBUG_MSG
  Serial.println("notifyCVAck") ;
#endif
  // Configure the DCC CV Programing ACK pin for an output
  pinMode( ENABLE_DCC_ACK, OUTPUT );

  // Generate the DCC ACK 60mA pulse
  digitalWrite( ENABLE_DCC_ACK, HIGH );
  delay( 10 );  // The DCC Spec says 6ms but 10 makes sure... ;)
  digitalWrite( ENABLE_DCC_ACK, LOW );
}
#endif

#ifdef  NOTIFY_DCC_MSG
void notifyDccMsg( DCC_MSG * Msg)
{
  Serial.print("notifyDccMsg: ") ;
  for(uint8_t i = 0; i < Msg->Size; i++)
  {
    Serial.print(Msg->Data[i], HEX);
    Serial.write(' ');
  }
  Serial.println();
}
#endif
