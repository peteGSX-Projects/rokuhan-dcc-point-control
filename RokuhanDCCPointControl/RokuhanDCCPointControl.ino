/*************************************************************
Arduino Rokuhan DCC Point Controller.

This uses motor controllers to control Rokuhan points and works with either:
- The L298 based FunduMoto "buzzer" type motor shield for two sets of points
- Two L293D motor control ICs for four sets of points

Define the MOTOR_CONTROLLER variable to select the above option:
- #define MOTOR_CONTROLLER FUNDUMOTO // Use this for two points with the FunduMoto shield
- #define MOTOR_CONTROLLER L293D     // Use this for four points with the L293D ICs

This is based on these example sketches supplied with the NMRA DCC library:
- NmraDccAccessoryDecoder_1
- NmraDccAccessoryDecoder_Pulsed_8

See the README for the full list of features and instructions.
*************************************************************/

// Include the NMRA DCC library
#include <NmraDcc.h>

// Create our objects
NmraDcc  Dcc;
DCC_MSG  Packet;

// Define the motor controller in use, uncomment the appropriate one only (not both)
#define MOTOR_CONTROLLER FUNDUMOTO            // Use this for two points with the FunduMoto shield
//#define MOTOR_CONTROLLER L293D                // Use this for four points with the L293D ICs

// Define our global variables
#define DCC_PIN     2                         // DCC input interupt pin
const int DccAckPin = A1;                     // DCC ACK output pin
uint16_t BaseTurnoutAddress;                  // First turnout address

// Define decoder version
#define DCC_DECODER_VERSION_NUM 1

// Define number of turnouts and turnout struct based on motor controller in use
#if MOTOR_CONTROLLER == FUNDUMOTO
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
#elif MOTOR_CONTROLLER == L293D
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
}

void initTurnouts() {
  // Function to initialise the turnout pins etc.
  for (uint8_t i = 0; i < (NUM_TURNOUTS); i++) {
    #if MOTOR_CONTROLLER == FUNDUMOTO
      Serial.println((String)"Initialising FunduMoto turnout " + i);
    #elif MOTOR_CONTROLLER == L293D
      Serial.println((String)"Initialising L293D turnout " + i);
    #endif
  }
  
}

void processTurnouts() {
  // Function to process the turnouts and switch them if necessary
  #if MOTOR_CONTROLLER == FUNDUMOTO
    Serial.println("Processing FunduMoto turnouts");
  #elif MOTOR_CONTROLLER == L293D
    Serial.println("Processing L293D turnouts");
  #endif
}

void setup()
{
  Serial.begin(115200);
  
  // Configure the DCC CV Programing ACK pin for an output
  pinMode( DccAckPin, OUTPUT );

  Serial.println("NMRA DCC Rokuhan Turnout Controller");
  Serial.println((String)"Configured to control " + NUM_TURNOUTS + " turnouts");
  
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

  BaseTurnoutAddress = (((Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_MSB) * 64) + Dcc.getCV(CV_ACCESSORY_DECODER_ADDRESS_LSB) - 1) * 4) + 1  ;
  // Initialise our turnouts
  initTurnouts();
  Serial.println((String)"Init Done, base turnout address is: " + BaseTurnoutAddress);
}

void loop()
{
  // You MUST call the NmraDcc.process() method frequently from the Arduino loop() function for correct library operation
  Dcc.process();
  // Process turnouts to ensure switching happens
  processTurnouts();
  if( FactoryDefaultCVIndex && Dcc.isSetCVReady())
  {
    FactoryDefaultCVIndex--; // Decrement first as initially it is the size of the array 
    Dcc.setCV( FactoryDefaultCVs[FactoryDefaultCVIndex].CV, FactoryDefaultCVs[FactoryDefaultCVIndex].Value);
  }
}
