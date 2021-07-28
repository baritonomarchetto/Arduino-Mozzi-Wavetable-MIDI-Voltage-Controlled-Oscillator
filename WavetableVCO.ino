/*
 * MIDI and Voltage Controlled Wavetable Oscillator
 * based on previous works from Tim Barrass, Francosis Best (FortySevenEffects) and Arduino Community
 * 
 * by Barito, july 2021
*/

#include <MIDI.h>
#include <MozziGuts.h>
#include <Oscil.h> // oscillator
#include <tables/sin2048_int8.h> // sine table
#include <tables/saw2048_int8.h> // saw table
#include <tables/triangle_hermes_2048_int8.h> // triangle table
#include <tables/square_no_alias_2048_int8.h> // square table
#include <tables/whitenoise8192_int8.h> // white noise table 

#include <mozzi_midi.h>

// Set up the MIDI channel to listen on
#define MIDI_CHANNEL 1

//Define the number of poentiometers and buttons

// Create and bind the MIDI interface to the hardware serial port (default)
MIDI_CREATE_DEFAULT_INSTANCE();

#define CONTROL_RATE 256 //decreasing this adds lag to note sequencies at high BPMs

//WAVES
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aOsc0;  //FIRST OSC (oscillators wavetable will be declared later)
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aOsc1;  //SECOND OSC
#define NUMWAVES 2

//Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> aMod0 (SIN2048_DATA); //modulator
//Oscil<SIN2048_NUM_CELLS, AUDIO_RATE> aMod1 (SIN2048_DATA); //modulator
//#define NUMMODS 1

boolean voltageControl;

int globalTune;
int detuneWave[NUMWAVES];
int gain[NUMWAVES];
int transpose[NUMWAVES];
int carrier_freq[NUMWAVES];
int tablecount[NUMWAVES];
int octcount[NUMWAVES];
//float modfreq[NUMMODS];
//int modInt[NUMMODS];
int pitchbend;

long wave;

int potcount;
int butcount;

float globalGainFactor;

//define buttons number
#define NUMBUT 4
//define the number of potentiometers on the front panel
#define NUMPOT 4

const int gateOutPin = A6;
//DIGITAL INPUT PINS (do not use analog pins for digital buttons: Mozzi don't like it)
const int bPin[] = {2,3,4,5};
boolean bState[NUMBUT];

int potVal[NUMPOT];
int analogCV;
int prevAnalogCV;
int noteNum;
int CVscaling;
int CVperNote100;

#define NUMTABLES 5
#define OCTAVES 2

void setWaveTbOsc0(int wavetable) {
  switch (wavetable) {
  case 1:
    aOsc0.setTable(TRIANGLE_HERMES_2048_DATA);
    break;
  case 2:
    aOsc0.setTable(SAW2048_DATA);
    break;
  case 3:
    aOsc0.setTable(SQUARE_NO_ALIAS_2048_DATA);
    break;
  case 4:
    aOsc0.setTable(WHITENOISE8192_DATA);
    break;
  default: // case 0
    aOsc0.setTable(SIN2048_DATA);
    break;
  }
}

void setWaveTbOsc1(int wavetable) {
  switch (wavetable) {
  case 1:
    aOsc1.setTable(TRIANGLE_HERMES_2048_DATA);
    break;
  case 2:
    aOsc1.setTable(SAW2048_DATA);
    break;
  case 3:
    aOsc1.setTable(SQUARE_NO_ALIAS_2048_DATA);
    break;
  case 4:
    aOsc1.setTable(WHITENOISE8192_DATA);
    break;
  default: // case 0
    aOsc1.setTable(SIN2048_DATA);
    break;
  }
}

void SetFreqs(){
carrier_freq[0] = mtof(noteNum+octcount[0]*12 + pitchbend) + globalTune;
carrier_freq[1] = mtof(noteNum+octcount[1]*12+ pitchbend) + detuneWave[1] + globalTune;
aOsc0.setFreq(carrier_freq[0]);
aOsc1.setFreq(carrier_freq[1]);
}

/*void SetMods(){
aMod0.setFreq(modfreq[0]);
//aMod1.setFreq(modfreq[1]);
}*/

void HandleNoteOn(byte channel, byte note, byte velocity) {
noteNum = note;
if(voltageControl == true){
  voltageControl = false; //disable Voltage control if MIDI is incoming
}
digitalWrite(gateOutPin, HIGH);
SetFreqs();
}

void HandleNoteOff(byte channel, byte note, byte velocity) {
if(note == noteNum){
  digitalWrite(gateOutPin, LOW);
}
}

void HandlePitchBend(byte channel, int bend){
pitchbend = bend>>10;
}

void setup(){
pinMode(gateOutPin, OUTPUT); 
for (int a=0; a<NUMBUT; a++){
  pinMode(bPin[a], INPUT_PULLUP);
  bState[a] = digitalRead(bPin[a]);
}
//MIDI initialization
MIDI.setHandleNoteOn(HandleNoteOn);
MIDI.setHandleNoteOff(HandleNoteOff);
MIDI.setHandlePitchBend(HandlePitchBend);
MIDI.begin(MIDI_CHANNEL);
//counters initialization
potcount = 0;
butcount = 0;
octcount[0] = -1;
octcount[1] = -1;
//we start listening to CV, MIDI will eventually disable it
voltageControl = true;
//waves initialization
for (int a = 0; a < NUMWAVES; a++){
detuneWave[a] = 0;
gain[a] = 128;
}
globalTune = 0;
globalGainFactor = 0.1;
SetFreqs();
//wavetables initialization
setWaveTbOsc0(2); // 0 - sine; 1 - triangle; 2 - saw; 3 - square; 4 - white noise
setWaveTbOsc1(2);
/*//modulators initialization
for (int b = 0; b < NUMWAVES; b++){
modfreq[b] = 0.0;
modInt[b] = 0;
}
SetMods();*/
startMozzi(CONTROL_RATE);
}  // SETUP END

void updateControl(){
MIDI.read();
VoltagesHandling(potcount);
ButtonsHandling(butcount);
if(voltageControl == true){
  VoltagePitchControl();
}
potcount+=1;
if(potcount >= NUMPOT){
  potcount = 0;
}
butcount+=1;
if(butcount >= NUMBUT){
  butcount = 0;
}
}

void VoltagePitchControl() {
//V/oct
analogCV = mozziAnalogRead(A6);
if(analogCV != prevAnalogCV){
  prevAnalogCV = analogCV;
  CVscaling = 256 - (mozziAnalogRead(A7)>>1); //range 256 to -256; A7 - built-in trimmer
  CVperNote100 = 1705 + CVscaling; //100*1023/60 = 1705 (avoids truncations)
  noteNum = 36 + analogCV*100/CVperNote100;
  SetFreqs();
}
}

void VoltagesHandling(int potcount){
// Read the potentiometers - do one on each updateControl scan.
switch (potcount){
case 0:
  //SECOND OSCILLATOR DETUNING
  potVal[potcount] = mozziAnalogRead(A0);
  analogCV = mozziAnalogRead(A4);
  if( analogCV> 1020){ //nothing connected
    detuneWave[1] = (potVal[potcount] >> 6) - 7;  // Range -7 to 7
  }
  else{
    detuneWave[1] = ((potVal[potcount] +  analogCV) >> 7) - 7;  // Range -7 to 7
  }
  SetFreqs();
break;
case 1:
  //RECIPROCAL GAIN
  potVal[potcount] = mozziAnalogRead(A1);
  gain[0] = 255 - (potVal[potcount] >> 2);  // Range 255 to 0
  gain[1] = potVal[potcount] >> 2;  // Range 0 to 255
break;
case 2:
  /*//MODULATION INTENSITY
  potVal[potcount] = mozziAnalogRead(A2);
  modInt[1] = potVal[potcount] >> 3; // Range 0 - 256
  */
  
  //GAIN FACTOR
  potVal[potcount] = mozziAnalogRead(A2);
  globalGainFactor = (float)potVal[potcount]/10240; //0.10 - 0.00
break;
case 3:
  /*//MODULATION FREQUENCY
  potVal[potcount] = mozziAnalogRead(A3);  
  modfreq[0] = potVal[potcount]/1023.0; //max value = 1.0
  //modfreq[1] = 0.5 * modfreq[0];
  SetMods();*/

  //MAIN TUNING
  potVal[potcount] = mozziAnalogRead(A3); 
  globalTune = (potVal[potcount] >> 5) - 16;  // Range -16 to 16
  SetFreqs();
break;

} //switch close
} //voltage handling close

void ButtonsHandling(int butcount){
// monitor buttons for state changes - do one on each updateControl scan.
switch (butcount){
case 0:
  //WAVETABLE OSC0 SELECT
  if(digitalRead(bPin[butcount]) != bState[butcount]){
   bState[butcount] = !bState[butcount];
   if(bState[butcount] == LOW){
    if(tablecount[0] > NUMTABLES){
      tablecount[0] = 0;
    }
    tablecount[0]+=1;
    setWaveTbOsc0(tablecount[0]); // 0 - sine; 1 - triangle; 2 - saw; 3 - square; 4 - white noise
   }
  }
break;
case 1:
  //WAVETABLE OSC1 SELECT
  if(digitalRead(bPin[butcount]) != bState[butcount]){
   bState[butcount] = !bState[butcount];
   if(bState[butcount] == LOW){
    if(tablecount[1] > NUMTABLES){
      tablecount[1] = 0;
    }
    tablecount[1]+=1;
    setWaveTbOsc1(tablecount[1]); // 0 - sine; 1 - triangle; 2 - saw; 3 - square, 4 - white noise
   }
  }
break;
case 2:
  //OSC0 OCTAVE TRANSPOSE
  if(digitalRead(bPin[butcount]) != bState[butcount]){
   bState[butcount] = !bState[butcount];
   if(bState[butcount] == LOW){
    if(octcount[0] >= OCTAVES){
      octcount[0] = -2;
    }
    octcount[0]+=1;
    SetFreqs();
   }
  }
break;
case 3:
  //OSC1 OCTAVE TRANSPOSE
  if(digitalRead(bPin[butcount]) != bState[butcount]){
   bState[butcount] = !bState[butcount];
   if(bState[butcount] == LOW){
    if(octcount[1] >= OCTAVES){
      octcount[1] = -2;
    }
    octcount[1]+=1;
    SetFreqs();
   }
  }
break;
}
}

int updateAudio(){
//Q15n16 modulator0 = (Q15n16) modInt[0] * aMod0.next();
//Q15n16 modulator1 = (Q15n16) modInt[1] * aMod1.next();
//wave = aOsc0.phMod(modulator0)* gain[0] + aOsc1.phMod(modulator1) * gain[1];
//wave = (aOsc0.next()* gain[0] + aOsc1.phMod(modulator0) * gain[1])*globalGainFactor;
wave = (aOsc0.next()*gain[0] + aOsc1.next()*gain[1])*globalGainFactor;
return MonoOutput::from16Bit(wave);
}

void loop(){
audioHook();
}
