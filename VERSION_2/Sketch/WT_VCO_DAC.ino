/* Dual Wavetable Oscillator
*  Two independent voices made of two oscillators each.
*  Main hardware: Raspberry Pi Pico, PT8211 DAC
*  Software dependencies: Earle Philhower Pico core, Mozzi library, FourtySevenEffects MIDI library
*
*  WARNING: DON'T USE DUAL CORE!! I burned two Pi Pico by moving updateControl functions under loop1!
*
*  by barito 2024 (last update dec. 2024)
*/

// Configure Mozzi for Stereo output. This must be done before #include <Mozzi.h>
// MozziConfigValues.h provides named defines for available config options.
#include <MozziConfigValues.h>
#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_I2S_DAC //external I2C DAC
#define MOZZI_AUDIO_CHANNELS MOZZI_STEREO //one voice per-channel
#define MOZZI_CONTROL_RATE 128            //Default is 64
#define MOZZI_ANALOG_READ_RESOLUTION 12   //Pi Pico default is 12
#define MOZZI_AUDIO_BITS 16 //default. Available values are 8, 16 (default), 24 (LEFT ALIGN in 32 bits type!!) and 32 bits
#define MOZZI_I2S_PIN_BCK 2 //default 20. To DAC pin 1 (BCK)
//#define MOZZI_I2S_PIN_WS (MOZZI_I2S_PIN_BCK+1) // CANNOT BE CHANGED, HAS TO BE NEXT TO pBCLK, i.e. default is 21. To DAC pin 2 (WS)
#define MOZZI_I2S_PIN_DATA 4 //default 22. To DAC pin 3 (DIN)
#define MOZZI_I2S_FORMAT MOZZI_I2S_FORMAT_LSBJ //PT8211 mode as per PT8211 datasheet

#include <Mozzi.h>
#include <Oscil.h>         // oscillator template
#include <mozzi_midi.h>    // Mozzi utils for MIDI (i.e convertion of MIDI numbers into frequencies)
#include <MIDI.h>          // MIDI library (FortySevenEffects)
#include <SoftwareSerial.h>

#include "Stream_DATA_4096_8bit.h"  // stream oscil for audio sig
#include "Note_Freq_Chart.h" //ideal frequencies chart

#define VOICES 2
#define OSC_NUM 4
#define ROT_ENC 4
#define BTNS 4
#define LEDS 2

//Uncomment this to set correction factors for each channel. They will be displayed on the serial monitor (115200 baud)
//#define CALIBRATION

//PINS definitions
// 0 voice 1 out
// 1 voice 2 out
const int rotary_pin[2*ROT_ENC] = {12, 13, 21, 20, 10, 11, 9, 22}; //ROTARY ENCODERS, A, B pins
const int CV_pin[VOICES] = {27, 28};        //Control Voltages (analog inputs, A1 and A2)
const int trim_pin = 26;                    //buit-in trimmer (A0)
const int btn_pin[BTNS] = {15, 18, 14, 19}; //push buttons
const int LED_pin[LEDS] = {17, 16};         //LEDs
const int rxMIDIPin = 7;
const int txMIDIPin = 8;

//rotaries
int rotary_state[2*ROT_ENC];
int last_encoded[ROT_ENC];
int rotary_value[ROT_ENC];

//push buttons
int btn_state[BTNS];
unsigned long btn_deb_time[BTNS];
bool btn_func[BTNS];

//analog variables
float CV_data[VOICES];
float CV_V[VOICES];
float freq_Voct[OSC_NUM];
//float freq_fine[VOICES] = {0, 0}; 
float detn_fctr[VOICES] = {0, 0};
float freq_TOT[OSC_NUM];
float gain[OSC_NUM] = {1.0, 1.0, 1.0, 1.0};
//float base_freq[VOICES] = {BASE_FREQ, BASE_FREQ};
byte base_MIDI_num = 24;                    // C1 - MIDIXCV base MIDI num
long aOutVoice[VOICES];
//float trim_DATA;
//float LINEARITY; //linearity factor
float pitch_corr[VOICES];
float note_float[VOICES];
int note_trnc[VOICES];
int iOct[OSC_NUM]; //voice actual OCTAVE counter (defined as semitones!! 12*iOct = one full ocate)
int iFP[VOICES]; //voice fine pitch counter

//MIDI instance
using Transport = MIDI_NAMESPACE::SerialMIDI<SoftwareSerial>;
SoftwareSerial mySerial(rxMIDIPin, txMIDIPin);
Transport serialMIDI(mySerial);
MIDI_NAMESPACE::MidiInterface<Transport> MIDI((Transport&)serialMIDI);

//int8_t STREAM_DATA[STREAM_DIM]; // Full datat stream. Defined in Stream_DATA.h
int8_t WAVE1_DATA[WTABLE_DIM]; //Actual wave outputted. This is a fraction of the whole data stream.
int8_t WAVE2_DATA[WTABLE_DIM];

//Oscillators
// use: Oscil <table_size, update_rate> oscilName (wavetable)
Oscil <WTABLE_DIM, MOZZI_AUDIO_RATE> aOsc0; //Oscillators. Wavetables will be declared later
Oscil <WTABLE_DIM, MOZZI_AUDIO_RATE> aOsc1; //Oscillators. Wavetables will be declared later
Oscil <WTABLE_DIM, MOZZI_AUDIO_RATE> aOsc2; //Oscillators. Wavetables will be declared later
Oscil <WTABLE_DIM, MOZZI_AUDIO_RATE> aOsc3; //Oscillators. Wavetables will be declared later

int currVoice; //0 - first voice; 1 - second voice; 2 - both voices
int waveIndex[VOICES]; //this sets where to start getting wavetable data from the stream array

int e_hack_count[ROT_ENC];//HACK FOR A BUG (ROTARY DOUBLE INPUT)

void setup(){
  //rotaries
  for (int a = 0; a < 2*ROT_ENC; a++){
    pinMode(rotary_pin[a], INPUT_PULLUP);
    rotary_state[a] = digitalRead(rotary_pin[a]);
  }
  //push buttons
  for (int a = 0; a < BTNS; a++){
    pinMode(btn_pin[a], INPUT_PULLUP);
    btn_state[a] = digitalRead(btn_pin[a]);
  }
  //LEDs
  currVoice = 2; //both voices
  for (int a = 0; a < LEDS; a++){
    pinMode(LED_pin[a], OUTPUT);
  }
  Func_LEDs();

  //wavetables initialization
  waveIndex[0] = 0;
  waveIndex[1] = 0;
  WT_Update(0, waveIndex[0]); //voice, start index
  WT_Update(1, waveIndex[1]); //voice, start index

  pitch_corr[0] = 3.50; //pitch correction factor for voice #1 (empirical). This smooth out the difference betwen hardware input limits and note chart extension, but also ideal vs real 
  pitch_corr[1] = 3.50; //pitch correction factor for voice #2 (empirical).
  startMozzi();

  //MIDI functions (NOT IMPLEMENTED!)
  //MIDI.setHandleNoteOn(Handle_Note_On);
  //MIDI.setHandleNoteOff(Handle_Note_Off);
  //MIDI.setHandleControlChange(Handle_CC);
  //MIDI.setHandlePitchBend(Handle_PB);
  
  //MIDI.begin(MIDI_CHANNEL_OMNI); //start MIDI and listen to ALL MIDI channels

  #ifdef CALIBRATION
    Serial.begin(115200);
  #endif
}

//BUG: FOR SOME REASON, ANY ENCODER READ IS MADE TWICE.
void UpdateEnc(){
  for(int r = 0; r < ROT_ENC; r++){
    rotary_state[2*r] = digitalRead(rotary_pin[2*r]);
    if(rotary_state[2*r] != last_encoded[r]){//ON STATE CHANGE ((DOUBLE TRIG BUG IS AT THIS LEVEL!! MAYBE HARDWARE ISSUE?!))
      last_encoded[r] = rotary_state[2*r];
      e_hack_count[r]++;
      if (e_hack_count[r] >= 2){//HACK!!
        e_hack_count[r] = 0;
        if (digitalRead(rotary_pin[2*r+1]) != rotary_state[2*r]){ //CW
          #ifdef CALIBRATION
            switch(r){
              case 0://ENCODER #1
                Func_Calib(0, 0.10f); //channel, increment
              break;
              case 1://ENCODER #2
                Func_Calib(1, 0.10f); //channel, increment
              break;
              case 2://ENCODER #3
                //DO NOTHING
              break;
              case 3://ENCODER #4
                Func_WT_Shift(WTABLE_DIM);
                Serial.print("wavetable number: ");
                Serial.println(waveIndex[0]/WTABLE_DIM+1);
              break;
            }
          #else
            switch(r){
              case 0://ENCODER #1
                if(btn_func[r] == 0){
                  Func_Octave(12, 12); //+1 full octave on both oscillators
                }
                else{
                  Func_Octave(0, 12); //+1 full octave on oscillator B
                  //Func_FPitch(2); //fine pitch (disabled but coded!)
                }
              break;
              case 1://ENCODER #2
                Func_Detune(1);
              break;
              case 2://ENCODER #3
                Func_RelGain(0.05f, 0.05f);
              break;
              case 3://ENCODER #4
                if(btn_func[r] == 0){
                  Func_WT_Shift(WTABLE_DIM);
                }
                else{
                  Func_WT_Shift(WTABLE_DIM/4);// mixed wavetables
                }
              break;
            }
          #endif
        }
        else{ //CCW
          #ifdef CALIBRATION
            switch(r){
              case 0://ENCODER #1
                Func_Calib(0, -0.10f); //channel, increment
              break;
              case 1://ENCODER #2
                Func_Calib(1, -0.10f); //channel, increment
              break;
              case 2://ENCODER #3
                //DO NOTHING
              break;
              case 3://ENCODER #4
                Func_WT_Shift(-WTABLE_DIM);
                Serial.print("wavetable number: ");
                Serial.println(waveIndex[0]/WTABLE_DIM+1);
              break;
            }
          #else
            switch(r){
              case 0://first encoder
                if(btn_func[r] == 0){
                  Func_Octave(-12, -12); //-1 full octave on both oscillators
                }
                else{
                  Func_Octave(0, -12); //-1 full octave on oscillator B
                  //Func_FPitch(-1); //fine pitch (disabled but coded!!)
                }
              break;
              case 1://ENCODER #2
                Func_Detune(-1);
              break;
              case 2://ENCODER #3
                Func_RelGain(-0.05f, -0.05f);//OSC_A, OSC_B
              break;
              case 3://ENCODER #4
                if(btn_func[r] == 0){
                  Func_WT_Shift(-WTABLE_DIM);
                }
                else{
                  Func_WT_Shift(-WTABLE_DIM/4);// mixed wavetables
                }
              break;
            }
          #endif
        }
      }
    }
  }
}

void Btn_Read(){
  for (int a = 0; a < BTNS; a++){
    if(digitalRead(btn_pin[a])!=btn_state[a] && mozziMicros()-btn_deb_time[a] > 100){ //ON STATE CHANGE
      btn_deb_time[a] = mozziMicros();
      btn_state[a] = !btn_state[a];
      if(btn_state[a] == LOW){//button pressed
        btn_func[a] = !btn_func[a]; //toggle button fuction. We do for any Z-button, even when they have no functions associated.
        switch(a){
          case 0: //ENCODER #1 SWITCH
            //Serial.println("button 1 pressed");
          break;
          case 1: //ENCODER #2 SWITCH
            Null_Detune();
          break;
          case 2: //ENCODER #3 SWITCH
            Func_Voice_Sel();
          break;
          case 3: //ENCODER #4 SWITCH
           if(btn_func[a] == 0){//conventional wavetable cycling...
              Func_WT_Reset();
           }
          break;
        }
      }
    }
  }
}

void Func_Calib(int vc, float inc){
  pitch_corr[vc] = pitch_corr[vc] + inc;
  Serial.print("Pitch correction factor for channel ");
  Serial.print(vc);
  Serial.print(" ");
  Serial.println(pitch_corr[vc]);
}

void CV_Read() {
  //float trim_DATA = mozziAnalogRead(trim_pin);//0-4095 in Pi Pico
  //pitch_corr[1] = trim_DATA/1000;
  for (int a = 0; a < VOICES; a++){
    CV_data[a] = mozziAnalogRead(CV_pin[a]);//0-4095 in Pi Pico
    //CV_V[a] = (float) CV_data[a]/4095 * 10.11;//0-10V is the IDEAL hardware range
    //freq_Voct[a] = (float) pow(2, CV_V[a]);
    note_float[a] = (float) CV_data[a]*(NOTES_DIM - pitch_corr[a])/4095;
    //note_trnc[a] = round(data_raw[a]);
    note_trnc[a] = trunc(note_float[a]); //works better than "round"
    //freq_Voct[a] = NOTE_FREQ[note_trnc[a]+12*iOct[a]];
  }
  //VOICE 1
  freq_Voct[0] = NOTE_FREQ[note_trnc[0]+iOct[0]]; //OSC 0
  freq_Voct[1] = NOTE_FREQ[note_trnc[0]+iOct[1]];// OSC 1
  //VOICE 2
  freq_Voct[2] = NOTE_FREQ[note_trnc[1]+iOct[2]]; //OSC 0
  freq_Voct[3] = NOTE_FREQ[note_trnc[1]+iOct[3]];// OSC 1
  //Serial.println(pitch_corr[1]);
  //Serial.println(" ");
}

void Func_Voice_Sel(){
  currVoice++;
  if(currVoice > VOICES){
    currVoice = 0;
  }
  Func_LEDs();
}

void Func_WT_Reset(){
  if(currVoice<2){//single voice
      waveIndex[currVoice] = 0;
  }
  else{//both voices the same value
    waveIndex[0] = 0;
    waveIndex[1] = waveIndex[0]; //both the same
  }
  WT_Update(0, waveIndex[0]);
  WT_Update(1, waveIndex[1]);
}

void Freq_Update(){
  //voice #1
    //OSC0
  freq_TOT[0] = freq_Voct[0] + freq_Voct[0]/1000*iFP[0]; //CV frequency + fine pitch
  aOsc0.setFreq(freq_TOT[0]);
    //OSC1
  freq_TOT[1] = freq_Voct[1] + freq_Voct[1]/1000*iFP[0]; //CV frequency + fine pitch
  aOsc1.setFreq(freq_TOT[1] + freq_Voct[1]/1000*detn_fctr[0]);//CV frequency + fine pitch + relative detune
  //voice #2
    //OSC2
  freq_TOT[2] = freq_Voct[2] + freq_Voct[2]/1000*iFP[1]; //CV frequency + fine pitch
  aOsc2.setFreq(freq_TOT[2]);
    //OSC3
  freq_TOT[3] = freq_Voct[3] + freq_Voct[3]/1000*iFP[1]; //CV frequency + fine pitch
  aOsc3.setFreq(freq_TOT[3] + freq_Voct[3]/1000*detn_fctr[1]);//CV frequency + fine pitch + relative detune
}

void Func_LEDs(){
  switch(currVoice){
    case 0:
      digitalWrite(LED_pin[0], HIGH);
      digitalWrite(LED_pin[1], LOW);
    break;
    case 1:
      digitalWrite(LED_pin[0], LOW);
      digitalWrite(LED_pin[1], HIGH);
    break;
    case 2:
      digitalWrite(LED_pin[0], HIGH);
      digitalWrite(LED_pin[1], HIGH);
    break;
  }
}

void Null_Detune(){
  if(currVoice<2){//single voice
      detn_fctr[currVoice] = 0;
  }
  else{//both voices the same value
    detn_fctr[0] = 0;
    detn_fctr[1] = detn_fctr[0]; //both the same
  }
}

void Func_Detune (float dt){
  if(currVoice<2){//single voice
    detn_fctr[currVoice] = detn_fctr[currVoice] + dt;
  }
  else{//both voices the same value
    detn_fctr[0] = detn_fctr[0] + dt;
    detn_fctr[1] = detn_fctr[0]; //both the same
  }
}

void Func_RelGain (float gn_a, float gn_b){
  if(currVoice<2){//single voice
    gain[currVoice*2] = gain[currVoice*2] + gn_a; //OSC0 or OSC2
    gain[currVoice*2+1] = gain[currVoice*2+1] + gn_b; //OSC1 or OSC3
  }
  else{//both voices the same value
    gain[0] = gain[0] + gn_a;
    gain[2] = gain[0];
    gain[1] = gain[1] + gn_b;
    gain[3] = gain[1];
  }
  //overflow
  for (int a = 0; a < OSC_NUM; a++){
    if (gain[a] <= 0){
      gain[a] = 0;
    }
    else if(gain[a] > 1){
      gain[a] = 1;
    }
  }
  //this is updated automatically in updateAudio()
}

void Func_Octave(int oct_oscA, int oct_oscB){
  if(currVoice<2){//single voice
    iOct[2*currVoice] = iOct[2*currVoice] + oct_oscA;     //current voice, oscilator A
    iOct[2*currVoice+1] = iOct[2*currVoice+1] + oct_oscB; //current voice, oscilator B
  }
  else{//both voices the same value
    iOct[0] = iOct[0] + oct_oscA; //voice 1, osc A
    iOct[1] = iOct[1] + oct_oscB; //voice 1, osc B
    iOct[2] = iOct[0];            //voice 2, osc A
    iOct[3] = iOct[1];            //voice 2, osc B
  }
  //overflow
  for (int a = 0; a < OSC_NUM; a++){
    if(iOct[a] > 24){ //MAX OCTAVES UP (2 OCTAVES)
      iOct[a] = 24;
    }
    else if (iOct[a] < -24){ //MIN OCTAVES DOWN (- 2 OCTAVES)
      iOct[a] = -24;
    }
  }
}

void Func_FPitch(int pitch_c){
  if(currVoice<2){//single voice
    iFP[currVoice] = iFP[currVoice] + pitch_c;
  }
  else{//both voices the same value
    iFP[0] = iFP[0] + pitch_c;
    iFP[1] = iFP[0];
  }
}

void Func_WT_Shift(int shft){
  if(currVoice < 2){//single voice
    waveIndex[currVoice] = waveIndex[currVoice] + shft;
  }
  else{//both voices the same value
    waveIndex[0] = waveIndex[0] + shft;
    waveIndex[1] = waveIndex[0];
  }
  //overflow
  for (int a = 0; a < VOICES; a++){
    if(waveIndex[a] > STREAM_DIM - WTABLE_DIM){
      waveIndex[a] = 0;
    }
    else if (waveIndex[a] < 0){
      waveIndex[a] = STREAM_DIM - WTABLE_DIM;
    }
  }
  //update
  WT_Update(0, waveIndex[0]);
  WT_Update(1, waveIndex[1]);
}

void WT_Update(int v, int i) {
  switch (v) {
    case 0: // case 0
      for (int a = 0; a < WTABLE_DIM; a++){
        WAVE1_DATA[a] = STREAM_DATA[i + a];
      }      
      aOsc0.setTable(WAVE1_DATA);
      aOsc1.setTable(WAVE1_DATA);
    break;
    case 1:
      for (int a = 0; a < WTABLE_DIM; a++){
        WAVE2_DATA[a] = STREAM_DATA[i + a];
      }      
      aOsc2.setTable(WAVE2_DATA);
      aOsc3.setTable(WAVE2_DATA);
    break;
  }
}

void updateControl(){
  // put changing controls in here.
  CV_Read();
  Btn_Read();
  UpdateEnc();
  Freq_Update();
}

AudioOutput updateAudio(){
  aOutVoice[0] = aOsc0.next() * gain[0] + aOsc1.next() * gain[1];
  aOutVoice[1] = aOsc2.next() * gain[2] + aOsc3.next() * gain[3];
  return StereoOutput::fromNBit(9, aOutVoice[0], aOutVoice[1]); //N is the number of bit of aVoice[] (bit-dept, or amplitude). Being a sum of bytes, 9 bits is the range. Multiplying by an envelope (in example) you end with 8+1+8=17 bits. It is not the platform (Pi Pico) or DAC resolution!!!
}

/*void loop1(){
  this engages Core#2. 
  DON'T USE! I BURNED TWO PICOS WORKING ON THIS SKETCH, 
  NOT SURE WHY (Mozzi incompatibility? For sure a software issue)
}*/

void loop(){
  audioHook(); // required here
}

/*void Handle_Note_On(byte mchannel, byte pitch, byte velocity){
  //MIDI note-on routine here
}*/

/*void Handle_Note_Off(byte mchannel, byte pitch, byte velocity){ 
  //MIDI note-oFF routine here
}*/

/*void Handle_CC(byte mchannel, byte number, byte value){
  //MIDI CC routine here
}*/

/*void Handle_PB(byte mchannel, int bend){
  //MIDI pitchbend routine here
}*/
