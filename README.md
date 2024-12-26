# About this repository
This repo contains two wavetable oscillators projects, an older one stored in "VERSION_1_(OLD)" folder and a new one stored in "VERSION_2" folder.

The older one was a single-voice, dual oscillators, PCB-only project that still stands because of the fact that has a full-MIDI implementation. It was a starting point for a monophonic synthesizer project in it's first days that has become a 4-voice paraphonic since then. 
It has not been updated for years and I am not planning to support it in any way in the future. For more info, here is an article I wrote about this very first oscillator project (Instructables): 

https://www.instructables.com/Arduino-Voltage-Controlled-Wavetable-Oscillator/

The new project (VERSION_2) features a lot of improvements with respect to the first one (dual voice, faster microprocessor, full module... more details in the following), but misses the MIDI implementation. Hardware-wise MIDI is there (MIDI-ready) but the code I wrote is only compatible with V/oct pitch control.
Here a link to the new version Instructables

https://www.instructables.com/Pi-Pico-Dual-Voice-Voltage-Controlled-Wavetable-Mo/

# Dual Wavetable Voltage Controlled Oscillator
A dual wavetable voltage controlled oscillator eurorack module based on Mozzi library.
