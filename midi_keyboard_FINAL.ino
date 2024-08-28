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
int NOTE_VELOCITY = 127;
//SUSTAIN PEDAL IS: <=63 OFF,>=64 ON

//MIDI baud rate
#define SERIAL_RATE 31250

//// Pin Definitions ////
//Free Digital Pins: D5 and some of a Multiplexor (CHECK THIS)
//Free Analog Pins: A5

// midi channels
const int CHANNEL1 = 0;
const int CHANNEL2 = 1;

// Row input pins
const int row1Pin = 12;
const int row2Pin = 11;
const int row3Pin = 10;
const int row4Pin = 9;
const int row5Pin = 8;
const int row6Pin = 7;

// 74HC595 pins
const int dataPin = 4;
const int latchPin = 3;
const int clockPin = 2;

//SUSTAIN PEDAL
const int susPin = 13;
int susVal = 0; 
int lastSusVal;

// Play/Pause button (NOT SOLDERED YET)
const int playPausePin = 6;
int playPauseVal = 0;
int lastPlayPauseVal;

//POTENTIOMETERS
const int volpotpin = A0; // MIDI volume
int volpotval = 0;
int lastVolpotval;
const int potpin1 = A1; //Cutoff Freq
int potval1 = 0; 
int lastpotval1;
const int potpin2 = A2; //Resonance
int potval2 = 0; 
int lastpotval2;
// const int potpin3 = A3; //
// int potval3 = 0; 
// int lastpotval3;
const int potpin4 = A4; // 
int potval4 = 0; 
int lastpotval4;

//JOYSICK
const int joyX = A6;
const int joyY = A7;
int previousX = 0;
int previousY = 0;

boolean keyPressed[NUM_ROWS][NUM_COLS];
uint8_t keyToMidiMap[NUM_ROWS][NUM_COLS];

// bitmasks for scanning columns
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

// int notes[6][9] = {
//     {48, 60, 66,	72,	78,	84,	54,	42,	36},
//     {49, 61,	67,	73,	79,	96,	55,	43,	37},
//     {50, 62,	68,	74,	80,	96,	56,	44,	38},
//     {51, 63,	69,	75,	81,	96,	57,	45,	39},
//     {52, 64,	70,	76,	82,	96,	58,	46,	40},
//     {53, 65,	71,	77,	83,	96,	59,	47,	41}
// };

int notes[6][9] = {
    {48, 42, 54, 84, 78, 72, 66, 60, 36},
    {49, 43, 55,  0, 79, 73, 67, 61, 37},
    {50, 44, 56,  0, 80, 74, 68, 62, 38},
    {51, 45, 57,  0, 81, 75, 69, 63, 39},
    {52, 46, 58,  0, 82, 76, 70, 64, 40},
    {53, 47, 59,  0, 83, 77, 71, 65, 41}
};

void setup(){
  //Current conections dont allow the following confiuration:
  //note = 36 //for C2 to be the starting note of matrix
  // int note = 48; //(C3) is row1 col 1

  // for(int colCtr = 0; colCtr < NUM_COLS; ++colCtr)
  // {
  //   for(int rowCtr = 0; rowCtr < NUM_ROWS; ++rowCtr)
  //   {
  //     keyPressed[rowCtr][colCtr] = false;
  //     keyToMidiMap[rowCtr][colCtr] = note;
  //     note++;
  //   }
  // }

  for(int rowCtr = 0; rowCtr < NUM_ROWS; ++rowCtr)
  {
    for(int colCtr = 0; colCtr < NUM_COLS; ++colCtr)
    {
      keyPressed[rowCtr][colCtr] = false;
      keyToMidiMap[rowCtr][colCtr] = notes[rowCtr][colCtr]; //+12;
    }
  }

  // setup pins output/input mode
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);

  pinMode(row1Pin, INPUT);
  pinMode(row2Pin, INPUT);
  pinMode(row3Pin, INPUT);
  pinMode(row4Pin, INPUT);
  pinMode(row5Pin, INPUT);
  pinMode(row6Pin, INPUT);

  pinMode(susPin, INPUT_PULLUP);
  pinMode(playPausePin, INPUT_PULLUP);

  Serial.begin(SERIAL_RATE);
}

void loop() {
  scanKeyboard();
  processSustainPedal();
  processJoystick(); //Uncomment when joystick soldered
  processPotentiometer(volpotpin, CONTROL_CHANGE_CMD, VOLUME_VAL, &lastVolpotval); //7
  processPotentiometer(potpin1, CONTROL_CHANGE_CMD, CUTOFF_FREQ_VAL, &lastpotval1); //74
  processPotentiometer(potpin2, CONTROL_CHANGE_CMD, RESONANCE_VAL, &lastpotval2); //71
  // processPotentiometer(potpin3, CONTROL_CHANGE_CMD, VOLUME_VAL, &lastpotval3); //7
  processPotentiometer(potpin4, CONTROL_CHANGE_CMD, 10, &lastpotval4); //CC10 = pan 
  // checkPlayPauseButton();
  // checkRecordButton();
  delay(2);
}

//// NOTES FUNCTIONS ////
void scanKeyboard() {
  for (int colCtr = 0; colCtr < NUM_COLS; ++colCtr)
  {
    //scan next column
    scanColumn(colCtr);

    //get row values at this column
    int rowValue[NUM_ROWS];
    rowValue[0] = digitalRead(row1Pin);
    rowValue[1] = digitalRead(row2Pin);
    rowValue[2] = digitalRead(row3Pin);
    rowValue[3] = digitalRead(row4Pin);
    rowValue[4] = digitalRead(row5Pin);
    rowValue[5] = digitalRead(row6Pin);

    // process keys pressed
    for(int rowCtr=0; rowCtr<NUM_ROWS; ++rowCtr)
    {
      if(rowValue[rowCtr] != 0 && !keyPressed[rowCtr][colCtr])
      {
        keyPressed[rowCtr][colCtr] = true;
        noteOn(rowCtr,colCtr);
        // sendMidiMessage(NOTE_ON_CMD, CHANNEL1, keyToMidiMap[rowCtr][colCtr], NOTE_VELOCITY); //new
      }
    }

    // process keys released
    for(int rowCtr=0; rowCtr<NUM_ROWS; ++rowCtr)
    {
      if(rowValue[rowCtr] == 0 && keyPressed[rowCtr][colCtr])
      {
        keyPressed[rowCtr][colCtr] = false;
        noteOff(rowCtr,colCtr);
        // sendMidiMessage(NOTE_ON_CMD, CHANNEL1, keyToMidiMap[rowCtr][colCtr], NOTE_VELOCITY); //new
      }
    }
  }
}
void scanColumn(int colNum){
  digitalWrite(latchPin, LOW);
  if(0 <= colNum && colNum <= 7)
  {
    shiftOut(dataPin, clockPin, MSBFIRST, B00000000); //right sr
    shiftOut(dataPin, clockPin, MSBFIRST, bits[colNum]); //left sr
  }
  else
  {
    shiftOut(dataPin, clockPin, MSBFIRST, bits[colNum-8]); //right sr
    shiftOut(dataPin, clockPin, MSBFIRST, B00000000); //left sr
  }
  digitalWrite(latchPin, HIGH);
}
void noteOn(int row, int col){
  Serial.write(NOTE_ON_CMD);
  Serial.write(keyToMidiMap[row][col]);
  Serial.write(NOTE_VELOCITY);
}
void noteOff(int row, int col){
  Serial.write(NOTE_OFF_CMD);
  Serial.write(keyToMidiMap[row][col]);
  Serial.write(NOTE_VELOCITY);
}

//// CC FUNCTIONS ////
void processSustainPedal() {
  susVal = digitalRead(susPin);
  if (susVal != lastSusVal) {
    MIDImessage(CONTROL_CHANGE_CMD, SUSTAIN_VAL, 127 * susVal);
    lastSusVal = susVal;
  }
  // delay(2);
}
void processPotentiometer(int pin, int status_byte, int control_byte, int* lastVal) {
  int potval = analogRead(pin);
  potval = map(potval, 0, 1023, 127, 0);
  int potvalDiff = potval - *lastVal;
  if (abs(potvalDiff) > 4) { // execute only if new and old values differ enough
    MIDImessage(status_byte, control_byte, potval);
    *lastVal = potval;
  }
  //delay(2);
}
void processJoystick() {
  // Read the joystick axes
  int currentX = analogRead(joyX);
  int currentY = analogRead(joyY);

  // X
  int pitchBendValue = map(currentX, 0, 1023, 0, 16383);
  // Snap to center when within a small range around center value 8192
  if (abs(currentX - 512) <= 10) { // Joystick close to center position
    pitchBendValue = 8192;         // Force pitch bend to center value
  }
  // Check for significant changes (dead zone)
  if (abs(pitchBendValue - previousX) > 20) {
    MIDImessage(224, pitchBendValue & 0x7F, (pitchBendValue >> 7) & 0x7F);
    previousX = pitchBendValue; // Update the previous value
  }

  // Y
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
    MIDImessage(176, 1, modulationValue);
    previousY = modulationValue; // Update the previous value
  }
}
void checkPlayPauseButton() {
  playPauseVal = digitalRead(playPausePin);
  if(playPauseVal != lastPlayPauseVal) {
    MIDImessage(NOTE_ON_CMD, PLAY_PAUSE_NOTE, 127 * playPauseVal);
    lastPlayPauseVal = playPauseVal;
  }
}

//// MIDI send message functions ////
void MIDImessage(int status, int data1, int data2){
  Serial.write(status); //Byte 1 is the status of MIDI (128-255)
  Serial.write(data1); //Byte 2 sends the CC (in our case the CC is 64, which is sustain or 176 or 224 etc)
  Serial.write(data2); //Byte 3 sends a number between 0 and 127 (pedal on/off or pots values)
}
// //new
// void sendMidiMessage(int cmd, int channel, int lsb, int msb) {
//   Serial.write(cmd + channel); // send command plus the channel number
//   Serial.write(lsb); // least significant bit 
//   Serial.write(msb); // most significant bit
// }