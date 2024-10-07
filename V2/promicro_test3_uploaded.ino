#include "MIDIUSB.h"
#include "Keyboard.h"
#include "usbmidi.h" //not used
// #include "MIDIcontroller.h" //???



#define NUM_ROWS 6
#define NUM_COLS 9

//MIDI STATUS (1st byte)
#define NOTE_ON_CMD 144
#define NOTE_OFF_CMD 128
#define CONTROL_CHANGE_CMD 176
#define PITCH_BEND_CMD 224
//MIDI DATA (2nd byte)
#define MODULATION 1 //with Control change as 1st byte 
#define SUSTAIN_VAL 64 //with Control change as 1st byte 
#define CUTOFF_FREQ_VAL 71 //with Control change as 1st byte 
#define RESONANCE_VAL 74 //with Control change as 1st byte 
#define VOLUME_VAL 7 // with Control change as 1st byte MIDI CC for volume
#define PLAY_PAUSE_NOTE 91 // with Control change as 1st byte MIDI note for play/pause
//MIDI DATA(3rd byte, almost always from 0-127 or 0-1 (ON/OFF))
#define NOTE_VELOCITY 127
//SUSTAIN PEDAL IS: <=63 OFF,>=64 ON

//MIDI baud rate
#define SERIAL_RATE 31250

//KEYBOARD PINS
// Row input pins
#define row1Pin 4
#define row2Pin 5
#define row3Pin 6
#define row4Pin 7
#define row5Pin 8
#define row6Pin 9

// 74HC595 pins
#define DATA_PIN  14
#define LATCH_PIN 16
#define CLOCK_PIN 10

//BUTTONS IN SHIFT REGISTERS
#define BUTTON_INPUT_PIN   15  // Button input pin
#define NUM_BUTTONS         3  // Define the number of buttons

#define MIDI_SRT_Clock          0xF8    ///< System Real Time - Timing Clock
#define MIDI_SRT_Start          0xFA    ///< System Real Time - Start
#define MIDI_SRT_Continue       0xFB    ///< System Real Time - Continue
#define MIDI_SRT_Stop           0xFC    ///< System Real Time - Stop
#define MIDI_SRT_ActiveSensing  0xFE    ///< System Real Time - Active Sensing
#define MIDI_SRT_SystemReset    0xFF    ///< System Real Time - System Reset

// Button actions
enum ButtonAction { None, Up, Down };

// Structure to hold button state
typedef struct {
    uint8_t state;  // HIGH or LOW
    ButtonAction action;  // Last button action
} ButtonContext;

//General button vars
ButtonContext buttonContexts[NUM_BUTTONS];
unsigned long lastCheckTime = 0;

// Define a mapping for the buttons to specific shift register output pins
// ONLY IF PIN QA IS BUSY (Which is the case here)
uint8_t buttonShiftRegisterPositions[NUM_BUTTONS];

//SUSTAIN PEDAL
const int susPin = 2;
int susVal; 
int lastSusVal = 0;

//POTENTIOMETERS
const int potpin0 = A0; // MIDI volume
int potval0 = 0;
int lastpotval0;
const int potpin1 = A1; //Cutoff Freq
int potval1 = 0; 
int lastpotval1;
const int potpin2 = A2; //Resonance
int potval2 = 0; 
int lastpotval2;
const int potpin3 = A3; //Something (for now Reverb)
int potval3 = 0; 
int lastpotval3;

//JOYSICK
const int joyX = A6;
const int joyY = A7;
int previousX = 0;
int previousY = 0;

boolean keyPressed[NUM_ROWS][NUM_COLS];
uint8_t keyToMidiMap[NUM_ROWS][NUM_COLS];

// bitmasks for scanning columns in shift registers
int bits[] =
{ 
  B00000001,
  B00000010,
  B00000100,
  B00001000,
  B00010000,
  B00100000,
  B01000000,
  B10000000
};

//Soldered MATRIX (for these connections):
int notes[6][9] = {
  {42, 48, 60, 66, 72, 78, 84, 54, 36},
  {43, 49, 61, 67, 73, 79,  0, 55, 37},
  {44, 50, 62, 68, 74, 80,  0, 56, 38},
  {45, 51, 63, 69, 75, 81,  0, 57, 39},
  {46, 52, 64, 70, 76, 82,  0, 58, 40},
  {47, 53, 65, 71, 77, 83,  0, 59, 41}
};

void setup() {
  for(int rowCtr = 0; rowCtr < NUM_ROWS; ++rowCtr)
  {
    for(int colCtr = 0; colCtr < NUM_COLS; ++colCtr)
    {
      keyPressed[rowCtr][colCtr] = false;
      keyToMidiMap[rowCtr][colCtr] = notes[rowCtr][colCtr]; //+12;
    }
  }

  //Shift button pins according to connections
  for (int i = 0; i<NUM_BUTTONS; i++){
    buttonShiftRegisterPositions[i] = 1 << (i+1);
    // Button 1 is connected to QB (bit 1), etc.
  }
  // Initialize button states to false
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    buttonContexts[i].state = LOW;  // not pressed
    buttonContexts[i].action = None;
  }

  // setup pins output/input mode
  pinMode(DATA_PIN,  OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  pinMode(row1Pin, INPUT);
  pinMode(row2Pin, INPUT);
  pinMode(row3Pin, INPUT);
  pinMode(row4Pin, INPUT);
  pinMode(row5Pin, INPUT);
  pinMode(row6Pin, INPUT);

  pinMode(BUTTON_INPUT_PIN, INPUT); //external 10k ohm resistor to ground
  pinMode(susPin, INPUT_PULLUP); //No external resistor so using arduino's internal pull-up resistor

  Serial.begin(SERIAL_RATE);
}


void loop() {
  scanKeyboard();
  // handleButtons(); //sending MIDI SYSTEM EXCLUSIVE messaages
  processButtonCC(); //sending regular CC messages (to map with ableton scripting)
  processSustainPedal();
  // processJoystick();       //Uncomment when soldered

  processPotentiometer(potpin0,              91,  &lastpotval0); //CC91 = Reverb
  processPotentiometer(potpin1, CUTOFF_FREQ_VAL,  &lastpotval1); //74
  processPotentiometer(potpin2,   RESONANCE_VAL,  &lastpotval2); //71
  processPotentiometer(potpin3,      VOLUME_VAL,  &lastpotval3); //7
  // // processPotentiometer(potpin4, 10,              &lastpotval4); //CC10 = pan
  
  delay(2);
}

// Keyboard scanning remains as per your reference
void scanKeyboard() {
  for (int colCtr = 0; colCtr < NUM_COLS; ++colCtr) {
    // Scan next column
    scanColumn(colCtr);

    // Get row values at this column
    int rowValue[NUM_ROWS];
    rowValue[0] = digitalRead(row1Pin);
    rowValue[1] = digitalRead(row2Pin);
    rowValue[2] = digitalRead(row3Pin);
    rowValue[3] = digitalRead(row4Pin);
    rowValue[4] = digitalRead(row5Pin);
    rowValue[5] = digitalRead(row6Pin);

    // Process keys pressed
    for (int rowCtr = 0; rowCtr < NUM_ROWS; ++rowCtr) {
      if (rowValue[rowCtr] != 0 && !keyPressed[rowCtr][colCtr]) {
        keyPressed[rowCtr][colCtr] = true;
        noteOnMIDI(0, keyToMidiMap[rowCtr][colCtr], NOTE_VELOCITY);
      }
    }

    // Process keys released
    for (int rowCtr = 0; rowCtr < NUM_ROWS; ++rowCtr) {
      if (rowValue[rowCtr] == 0 && keyPressed[rowCtr][colCtr]) {
        keyPressed[rowCtr][colCtr] = false;
        noteOffMIDI(0, keyToMidiMap[rowCtr][colCtr], NOTE_VELOCITY);
      }
    }
  }
}
// Function to scan the columns
void scanColumn(int colNum) {
  digitalWrite(LATCH_PIN, LOW);
  if (colNum <= 7) {
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00000000); // First shift register inactive
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, bits[colNum]); // Second shift register active
  } else {
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, bits[colNum - 8]); // First shift register active
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00000000); // Second shift register inactive
  }
  digitalWrite(LATCH_PIN, HIGH);
}

//// MIDI STATUS FUNCTIONS ////
void noteOnMIDI(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, NOTE_ON_CMD | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
  MidiUSB.flush();// Nueva Linea Leonardo
}
void noteOffMIDI(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, NOTE_OFF_CMD | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
  MidiUSB.flush();// Nueva Linea Leonardo
}
void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, CONTROL_CHANGE_CMD | channel, control, value};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();
}
void pitchBend(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0E, PITCH_BEND_CMD | channel, control, value};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();
}

//// CONTROL CHANGE FUNCTIONS ////
void processSustainPedal() {
  susVal = digitalRead(susPin);
  if (susVal != lastSusVal) {
    controlChange(0, SUSTAIN_VAL, susVal == LOW ? 127 : 0);
    lastSusVal = susVal;
  }
}
void processPotentiometer(int pin, int control, int* value) {
  int potval = analogRead(pin);
  potval = map(potval, 0, 1023, 127, 0);
  int potvalDiff = potval - *value;
  if (abs(potvalDiff) > 4) { // execute only if new and old values differ enough
    controlChange(0, control, potval);
    *value = potval;
  }
  //delay(2);
}
void processJoystick() {
  // Read the joystick axes
  int currentX = analogRead(joyX);
  int currentY = analogRead(joyY);

  // X - pitch bend is STATUS
  int pitchBendValue = map(currentX, 0, 1023, 0, 16383);
  // Snap to center when within a small range around center value 8192
  if (abs(currentX - 512) <= 10) { // Joystick close to center position
    pitchBendValue = 8192;         // Force pitch bend to center value
  }
  // Check for significant changes (dead zone)
  if (abs(pitchBendValue - previousX) > 20) {
    pitchBend(0, pitchBendValue & 0x7F, (pitchBendValue >> 7) & 0x7F);
    previousX = pitchBendValue; // Update the previous value
  }

  // Y - modulation is CONTROL CHANGE
  int modulationValue;
  if (currentY < 512) {
    // Below center, map 0 to 512 to 0 to 127
    modulationValue = map(currentY, 0, 512, 127, 0);
  }
  else {
    // Above center, map 512 to 1023 to 0 to 127
    modulationValue = map(currentY, 512, 1023, 0, 127);
  }
  // Snap to center when within a small range around center value 512
  if (abs(currentY - 512) <= 10) { // Joystick close to center position
    modulationValue = 0;          // Force modulation to center value
  }
  // Apply dead zone to prevent minor signals
  if (abs(modulationValue - previousY) > 5) {
    controlChange(0, MODULATION, modulationValue);
    previousY = modulationValue; // Update the previous value
  }
}
void processButtonCC() {
  // Check for button state change every 50 milliseconds
  if (millis() - lastCheckTime >= 50) {
    for (uint8_t currentButton = 0; currentButton < NUM_BUTTONS; currentButton++) {
      uint8_t currentShiftValue = buttonShiftRegisterPositions[currentButton];

      // Isolate shift register usage for buttons
      digitalWrite(LATCH_PIN, LOW);
      shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, currentShiftValue);
      shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00000000); // Ensure only the button column is active
      digitalWrite(LATCH_PIN, HIGH);

      // Read the state of the button
      uint8_t buttonState = digitalRead(BUTTON_INPUT_PIN);
      if (buttonState != buttonContexts[currentButton].state) {
        buttonContexts[currentButton].action = (buttonState == HIGH) ? Down : Up;
        buttonContexts[currentButton].state = buttonState;
      }

      // Print button actions
      if (buttonContexts[currentButton].action != None) {
        if (buttonContexts[currentButton].action == Down) {
          switch (currentButton) {
            case 1:  // Button 1: Play/Pause
              controlChange(0, 16, 127); // Send CC16 = General purpose controller 1 ON
              break;
            case 0:  // Button 2: Stop
              controlChange(0, 17, 127); // Send CC17 = General purpose controller 2 ON
              break;
            case 2:  // Button 3: Record/Stop Recording
              controlChange(0, 18, 127); // Send CC16 = General purpose controller 3 ON
              break;

            default:
              break;
          }

        }
        else {
          switch (currentButton) {
            case 1:  // Button 1
              controlChange(0, 16, 0); // OFF
              break;
            case 0:  // Button 2
              controlChange(0, 17, 0); // OFF
              break;
            case 2:  // Button 3
              controlChange(0, 18, 0); // OFF
              break;

            default:
              break;
          }
        }
        buttonContexts[currentButton].action = None;  // Reset action after processing
      }
    }
    lastCheckTime = millis();  // Update last check time
  }
}

//BUTTONS:
// Button handling with isolated shift register usage
void handleButtons() {
  // Check for button state change every 50 milliseconds
  if (millis() - lastCheckTime >= 50) {
    for (uint8_t currentButton = 0; currentButton < NUM_BUTTONS; currentButton++) {
      // Activate the corresponding bit for the current button
      uint8_t currentShiftValue = buttonShiftRegisterPositions[currentButton];

      // Isolate shift register usage for buttons
      digitalWrite(LATCH_PIN, LOW);
      shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, currentShiftValue);
      shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, B00000000); // Ensure only the button column is active
      digitalWrite(LATCH_PIN, HIGH);

      // Read the state of the button
      uint8_t buttonState = digitalRead(BUTTON_INPUT_PIN);
      if (buttonState != buttonContexts[currentButton].state) {
        buttonContexts[currentButton].action = (buttonState == HIGH) ? Down : Up;
        buttonContexts[currentButton].state = buttonState;
      }

      // Print button actions
      if (buttonContexts[currentButton].action != None) {
        // Serial.print("Button ");
        // Serial.print(currentButton + 1);  // Print button number (1-based index)
        if (buttonContexts[currentButton].action == Down) {
          // Serial.println(" pressed!");

          switch (currentButton) {
            case 0:  // Button 1: Play/Pause
              Serial.println("Continue");
              // shortcut_PlayPause();
              // midi_PlayPause();
              midi_machine_Start();
              // midi_machine_ultimate();
              break;

            case 1:  // Button 2: Record/Stop
              Serial.println("Stop");
              // shortcut_Stop();
              // midi_Stop();

              midi_machine_Pause();
              // midi_machine_Stop();
              break;

            default:
              break;
          }

        }
        else {
          // Serial.println(" released!");
          switch (currentButton) {
            case 0:  // Button 1: Play/Pause
              // // controlChange(0, 16, 0); // Send CC16 = General purpose controller 1 with value 0
              break;

            case 1:  // Button 2: Record/Stop
              // // controlChange(0, 17, 0); // Send CC17 = General purpose controller 2 with value 0
              break;

            default:
              break;
          }
        }
        buttonContexts[currentButton].action = None;  // Reset action after processing
      }
    }
    lastCheckTime = millis();  // Update last check time
  }
}
void sendRealTimeMessage(byte message) {
  midiEventPacket_t midiEvent = {0x0F, message, 0, 0};  // 0x0F: Cable 0, real-time message
  MidiUSB.sendMIDI(midiEvent);
  MidiUSB.flush();  // Ensure the message is sent immediately
}

// SOLUTION 1: KEYBOARD SHORTCUTS
void shortcut_Stop() {
  //stop playback
  Keyboard.begin();
  Keyboard.write(32); //space in ASCII decimal format
  Keyboard.end();
}
void shortcut_PlayPause() {
  //Play or pause at position
  Keyboard.begin();
  Keyboard.press(KEY_LEFT_SHIFT);
  Keyboard.write(32);
  Keyboard.releaseAll();
  Keyboard.end();
}


// SOLUTION 2: Actual MIDI Messages
bool isPlaying = false;
void midi_PlayPause(){
  if (isPlaying) {
    midi_Stop();
  }
  else {
    sendRealTimeMessage(0xFB); // Continue msg
  }
  isPlaying = !isPlaying;
  midi_clock();
}
void midi_Stop(){
  sendRealTimeMessage(0xFC);  // Stop message
  // midi_clock();
}
//MIDI CLOCK:
void midi_clock(){
  midiEventPacket_t clock = {0x0F, 0xF8, 0, 0};  // MIDI Clock
  MidiUSB.sendMIDI(clock);
  MidiUSB.flush();
}

//MIDI MACHINE CONTROL:
// This is start:
// F0 7F 7F 06 02 F7
// This is stop
// F0 7F 7F 06 01 F7

// void midi_machine_Start() {
//   byte PlayCommandArray [6] = {0xF0, 0x7F, 0x7F, 0x06, 0x02, 0xF7}; 
//   // usbMIDI.sendSysEx(6, PlayCommandArray, true); //Send an array of 6 bytes representing the play command
  
//   midiEventPacket_t sysExMessage = {0x0F, 0xF0, 0, 0};  // Initialize the SysEx message
//   MidiUSB.sendMIDI(sysExMessage); 
//   // Send the command bytes (excluding start and end)
//   for (uint8_t i = 1; i < 5; i++) {
//     sysExMessage = {0x0F, PlayCommandArray[i], 0, 0};  // Use 0x0F for cable 0
//     MidiUSB.sendMIDI(sysExMessage); 
//   }
//   sysExMessage = {0x0F, 0xF7, 0, 0};  // SysEx End byte
//   MidiUSB.sendMIDI(sysExMessage);

//   MidiUSB.flush(); // Ensure all messages are sent
// }
// void midi_machine_Stop() {
//   byte StopCommandArray[6] = {0xF0, 0x7F, 0x7F, 0x06, 0x01, 0xF7}; 
//   midiEventPacket_t sysExMessage = {0x0F, 0xF0, 0, 0};  // Initialize the SysEx message

//   // Send SysEx Start byte
//   MidiUSB.sendMIDI(sysExMessage); 

//   // Send the command bytes (excluding start and end)
//   for (uint8_t i = 1; i < 5; i++) {
//     sysExMessage = {0x0F, StopCommandArray[i], 0, 0};  // Use 0x0F for cable 0
//     MidiUSB.sendMIDI(sysExMessage); 
//   }

//   // Send SysEx End byte
//   sysExMessage = {0x0F, 0xF7, 0, 0};  // SysEx End byte
//   MidiUSB.sendMIDI(sysExMessage);

//   MidiUSB.flush(); // Ensure all messages are sent
// }
void midi_machine_Start() {
  byte PlayCommandArray[6] = {0xF0, 0x7F, 0x7F, 0x06, 0x02, 0xF7};
  sendSysEx(PlayCommandArray, sizeof(PlayCommandArray));
}
void midi_machine_Stop() {
  byte StopCommandArray[6] = {0xF0, 0x7F, 0x7F, 0x06, 0x01, 0xF7};
  sendSysEx(StopCommandArray, sizeof(StopCommandArray));
}
void sendSysEx(byte* data, size_t length) {
  // Send the SysEx message byte by byte
  for (size_t i = 0; i < length; i++) {
    midiEventPacket_t midiEvent = {0x0F, data[i], 0, 0}; // Cable 0, data byte, other bytes as 0
    MidiUSB.sendMIDI(midiEvent);
  }
  MidiUSB.flush();  // Ensure the message is sent immediately
}




void midi_machine_Pause() {
  byte PauseCommandArray[6] = {0xF0, 0x7F, 0x7F, 0x06, 0x09, 0xF7}; 
  midiEventPacket_t sysExMessage = {0x0F, 0xF0, 0, 0};  // Initialize the SysEx message

  // Send SysEx Start byte
  MidiUSB.sendMIDI(sysExMessage); 

  // Send the command bytes (excluding start and end)
  for (uint8_t i = 1; i < 5; i++) {
    sysExMessage = {0x0F, PauseCommandArray[i], 0, 0};  // Use 0x0F for cable 0
    MidiUSB.sendMIDI(sysExMessage); 
  }

  // Send SysEx End byte
  sysExMessage = {0x0F, 0xF7, 0, 0};  // SysEx End byte
  MidiUSB.sendMIDI(sysExMessage);

  MidiUSB.flush(); // Ensure all messages are sent
}
// void midi_machine_ultimate(){
//   byte PlayCommandArray [6] = {0xF0, 0x7F, 0x7F, 0x06, 0x02, 0xF7}; 
//   usbMIDI.sendSysEx(6, PlayCommandArray, true); //Send an array of 6 bytes representing the play command
// }
