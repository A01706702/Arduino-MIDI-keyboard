#define NUM_ROWS 6
#define NUM_COLS 9

#define NOTE_ON_CMD 0x90 //144
#define NOTE_OFF_CMD 0x80 //128
#define NOTE_VELOCITY 127
#define CONTROL_CHANGE_CMD 0xB0 //176
#define SUSTAIN_VAL 64
#define POTENTIOMETER_VAL 74

//MIDI baud rate
#define SERIAL_RATE 31250

// Pin Definitions //

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

//SUSTAIN PEDAL pins
const int susPin = 13;
//SUSTAIN PEDAL vars
int susVal = 0; 
int lastSusVal;

//POT FILTER pins
const int potPin = A0;
//SUSTAIN PEDAL vars
int potVal = 0; 
int lastpotVal;

//POT FILTER pins
const int potPin1 = A1;
//SUSTAIN PEDAL vars
int potVal1 = 0; 
int lastpotVal1;

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

void setup()
{
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

  Serial.begin(SERIAL_RATE);
}

void loop()
{
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
      }
    }

    // process keys released
    for(int rowCtr=0; rowCtr<NUM_ROWS; ++rowCtr)
    {
      if(rowValue[rowCtr] == 0 && keyPressed[rowCtr][colCtr])
      {
        keyPressed[rowCtr][colCtr] = false;
        noteOff(rowCtr,colCtr);
      }
    }
  }

  // SUSTAIN PEDAL
  susVal = digitalRead(susPin); //reads sus pedal input    
  //This code makes sure the pedal only sends a signal when there is a change, rather than constantly
  if(susVal != lastSusVal)
  {
    MIDImessage(CONTROL_CHANGE_CMD, SUSTAIN_VAL, 127*susVal);
    lastSusVal = susVal;
  }
  delay(2);

  // POTENIOMETER CUTOFF FREQ FILTER
  potVal = analogRead(potPin); //reads sus pedal input
  potVal = map(potVal, 0, 1023, 127, 0);
  //This code makes sure the pedal only sends a signal when there is a change, rather than constantly
  // if(potVal != lastpotVal)
  // {
  //   MIDImessage(CONTROL_CHANGE_CMD, POTENTIOMETER_VAL,  map(potVal, 0, 1023, 0, 127));
  //   lastpotVal = potVal;
  // }
  int potValdiff = potVal - lastpotVal;
  if (abs(potValdiff) > 4) // execute only if new and old values differ enough
  {
    MIDImessage(176, 74, potVal); // map sensor range to MIDI range
    lastpotVal = potVal; // reset old value with new reading
  }
  delay(2);

  ///// pot1
  potVal1 = analogRead(potPin1);
  potVal1 = map(potVal1, 0, 1023, 127, 0);
  int potValdiff1 = potVal1 - lastpotVal1;
  if (abs(potValdiff1) > 4) // execute only if new and old values differ enough
  {
    MIDImessage(176, 71, potVal1); // map sensor range to MIDI range
    lastpotVal1 = potVal1; // reset old value with new reading
  }
  delay(2);

}

void scanColumn(int colNum)
{
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

void noteOn(int row, int col)
{
  Serial.write(NOTE_ON_CMD);
  Serial.write(keyToMidiMap[row][col]);
  Serial.write(NOTE_VELOCITY);
}

void noteOff(int row, int col)
{
  Serial.write(NOTE_OFF_CMD);
  Serial.write(keyToMidiMap[row][col]);
  Serial.write(NOTE_VELOCITY);
}

//MIDI message function
void MIDImessage(int command, int MIDInote, int MIDIvelocity)
{
  Serial.write(command); 
  Serial.write(MIDInote);//sends the CC (in our case the CC is 64, which is sustain)
  Serial.write(MIDIvelocity);//sends a number between 0 and 127 (pedal on/off)
}