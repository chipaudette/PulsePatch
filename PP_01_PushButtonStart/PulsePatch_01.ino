/*
  Pulse Patch
  This code targets a Simblee
  I2C Interface with MAX30102 Sp02 Sensor Module
*/

#include <Wire.h>
#include "MAX30102_Definitions.h"

//This line added by Chip 2016-09-28 to enable plotting by Arduino Serial Plotter
const int PRINT_ONLY_FOR_PLOTTER = 0;  //Set this to zero to return normal verbose print() statements

unsigned int LED_timer;
int LED_delayTime = 300;
boolean boardLEDstate = HIGH;
int lastSwitchState;
volatile boolean MAX_interrupt = false;
short interruptSetting;
short interruptFlags;
float Celcius;
float Fahrenheit;
char sampleCounter = 0;
int REDvalue;
int IRvalue;
char mode = SPO2_MODE;  // SPO2_MODE or HR_MODE
char readPointer;
char writePointer;
char ovfCounter;
int rAmp = 10;
int irAmp = 10;
boolean is_running = 0;
boolean received_first_data = false;
boolean SWITCH_interrupt = false;
boolean use_LEDs = true;

//  TESTING
unsigned int thisTestTime;
unsigned int thatTestTime;

char sampleRate;
boolean useFilter = false;
int gain = 10;
float HPfilterInputRED[NUM_SAMPLES];
float HPfilterOutputRED[NUM_SAMPLES];
float LPfilterInputRED[NUM_SAMPLES];
float LPfilterOutputRED[NUM_SAMPLES];
float HPfilterInputIR[NUM_SAMPLES];
float HPfilterOutputIR[NUM_SAMPLES];
float LPfilterInputIR[NUM_SAMPLES];
float LPfilterOutputIR[NUM_SAMPLES];

void setup() {

  Wire.beginOnPins(SCL_PIN, SDA_PIN);
  Serial.begin(230400);
  setLEDasOutput(use_LEDs);
  digitalWrite(RED_LED, boardLEDstate);
  digitalWrite(GRN_LED, !boardLEDstate);
  pinMode(TACT_SWITCH, INPUT);
  //lastSwitchState = digitalRead(TACT_SWITCH);
  //attachPinInterrupt(TACT_SWITCH,SWITCH_ISR,HIGH);

  pinMode(MAX_INT, INPUT_PULLUP);
  attachPinInterrupt(MAX_INT, MAX_ISR, LOW);

  if (!PRINT_ONLY_FOR_PLOTTER) Serial.println("\nPulsePatch 01\n");
  LED_timer = millis();
  MAX_init(SR_100); // initialize MAX30102, specify sampleRate
  if (useFilter) {
    initFilter();
  }
  if (!PRINT_ONLY_FOR_PLOTTER) {
    printAllRegisters();
    Serial.println();
    printHelpToSerial();
    Serial.println();
  } else {
    //when configured for the Arduino Serial Plotter, start the system running right away
    enableMAX30102(true);
    thatTestTime = micros();
  }

  setLEDamplitude(rAmp, irAmp);
}


void loop() {

  //asm(" WFI"); //sleep while waiting for an interrupt (for ARM chips...Simblee is MO)

  if (MAX_interrupt) {
    serviceInterrupts(); // go see what woke us up, and do the work
    //received_first_data = true; //for servicing of push button.
    if (sampleCounter == 0x00) { // rolls over to 0 at 200
      MAX30102_writeRegister(TEMP_CONFIG, 0x01); // take temperature
    }

    blinkBoardLEDs();
    //} else {
    //  Simblee_ULPDelay(5);
  }



  readSwitch();
  //  if (SWITCH_interrupt) {
  //    SWITCH_interrupt = false;
  //   if (is_running && received_first_data) {
  //      Serial.print("Stop running (from button)...");
  //      is_running=enableMAX30102(false);
  //    } else if (!is_running) {
  //      Serial.print("Start running (from button)...");
  //      is_running=enableMAX30102(true);
  //      thatTestTime = micros();
  //    }
  //  }

  eventSerial();
}

void eventSerial() {
  while (Serial.available()) {
    char inByte = Serial.read();
    uint16_t intSource;
    switch (inByte) {
      case 'h':
        printHelpToSerial();
        break;
      case 'b':
        Serial.println("start running");
        is_running = enableMAX30102(true);
        thatTestTime = micros();
        break;
      case 's':
        Serial.println("stop running");
        is_running = enableMAX30102(false);
        break;
      case 't':
        MAX30102_writeRegister(TEMP_CONFIG, 0x01);
        break;
      case 'i':
        intSource = MAX30102_readShort(STATUS_1);
        Serial.print("intSource: 0x"); Serial.println(intSource, HEX);
        break;
      case 'v':
        getDeviceInfo();
        break;
      case '?':
        printAllRegisters();
        break;

      case 'f':
        useFilter = false;
        break;
      case 'F':
        useFilter = true;
        break;
      case '1':
        rAmp++; if (rAmp > 50) {
          rAmp = 50;
        }
        setLEDamplitude(rAmp, irAmp);
        serialAmps();
        break;
      case '2':
        rAmp--; if (rAmp < 1) {
          rAmp = 0;
        }
        setLEDamplitude(rAmp, irAmp);
        serialAmps();
        break;
      case '3':
        irAmp++; if (irAmp > 50) {
          irAmp = 50;
        }
        setLEDamplitude(rAmp, irAmp);
        serialAmps();
        break;
      case '4':
        irAmp--; if (irAmp < 1) {
          irAmp = 0;
        }
        setLEDamplitude(rAmp, irAmp);
        serialAmps();
      default:
        break;
    }
  }
}


void setLEDasOutput(boolean use_LEDs) {
  if (use_LEDs) {
    pinMode(RED_LED, OUTPUT);
    pinMode(GRN_LED, OUTPUT);
  } else {
    pinMode(RED_LED, INPUT);
    pinMode(GRN_LED, INPUT);
  }
}

void blinkBoardLEDs() {
  if (millis() - LED_timer > LED_delayTime) {
    LED_timer = millis();
    boardLEDstate = !boardLEDstate;
    digitalWrite(RED_LED, boardLEDstate);
    digitalWrite(GRN_LED, !boardLEDstate);
  }
}

long switch_timer_millis = 0;
const long switch_delayTime = 50;
int prevState = LOW;
void readSwitch() {
  if (millis() - switch_timer_millis > switch_delayTime) {
    switch_timer_millis = millis();

    int switchState = digitalRead(TACT_SWITCH);
    if ((switchState == LOW) && (prevState == HIGH)) { //action on release only
      //Serial.print("Swtich: "); Serial.println(switchState);
      if (is_running) {
        Serial.print("Stop running (from button)...");
        is_running = enableMAX30102(false);
        use_LEDs = !use_LEDs;
        setLEDasOutput(use_LEDs);
      } else {
        Serial.print("Start running (from button)...");
        is_running = enableMAX30102(true);
        thatTestTime = micros();
      }
    }
    prevState = switchState;
  }
}


//Print out all of the commands so that the user can see what to do
//Added: Chip 2016-09-28
void printHelpToSerial() {
  Serial.println(F("Commands:"));
  Serial.println(F("   'h'  Print this help information on available commands"));
  Serial.println(F("   'b'  Start the thing running at the sample rate selected"));
  Serial.println(F("   's'  Stop the thing running"));
  Serial.println(F("   't'  Initiate a temperature conversion. This should work if 'b' is pressed or not"));
  Serial.println(F("   'i'  Query the interrupt flags register. Not really useful"));
  Serial.println(F("   'v'  Verify the device by querying the RevID and PartID registers (hex 6 and hex 15 respectively)"));
  Serial.println(F("   '1'  Increase red LED intensity"));
  Serial.println(F("   '2'  Decrease red LED intensity"));
  Serial.println(F("   '3'  Increase IR LED intensity"));
  Serial.println(F("   '4'  Decrease IR LED intensity"));
  Serial.println(F("   '?'  Print all registers"));
  Serial.println(F("   'F'  Turn on filters"));
  Serial.println(F("   'f'  Turn off filters"));
}

int MAX_ISR(uint32_t dummyPin) { // gotta have a dummyPin...
  MAX_interrupt = true;
  return 0; // gotta return something, somehow...
}

int SWITCH_ISR(uint32_t dummyPin) {
  SWITCH_interrupt = true;
  return 0;
}

void serialAmps() {
  Serial.print("PA\t");
  Serial.print(rAmp); printTab(); Serial.println(irAmp);
}
