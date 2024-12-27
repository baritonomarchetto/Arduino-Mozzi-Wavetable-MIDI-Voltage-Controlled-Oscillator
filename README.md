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

# Features
Two independent voices with two, detunable, oscillators each

Fully programmable

Stream-wave wavetables (more on this later)

V/octave control voltage standard (input range 0 - 12V)

MIDI-ready hardware

Small footprint (only 10 HP for two, dual-oscillator voices!)

Open source firmware

# Hardware Design
The wavetable oscillator module is made of three PCBs:

Front panel (aluminum)
Front board
Main board
The module has been layed down in a stacked-PCBs design. I generally prefer to adopt flag-mount design, but I like how these stacked-boards modules turns out, even if they call for more design work on my side.

The front panel, or face plate, is the white board you see when looking to the assembled module's front. It's made of alluminum alloy, not FR4.

The front board hosts female jacks, rotary encoders and LEDS. It puts them in electrical contact with the main board. It also hosts the control voltage input circuit and the rotary encoders slave circuitry.

The main board hosts active and passive circuits and components. MIDI IN and OUT circuits, the microcontroller board, the power input filters and connections are all hosted on the main board.

# Circuits
**Microcontroller board**

The use of a Raspberry Pi Pico microcontroller board brings a series of advantages over an arduino nano.

First of all, we have a default audio output with 11 bits of resolution instead of 7. This means that we can have a well defined wave even whitout the need to adopt dual PWM.

Second, the increased CPU speed makes it possible to output more than one waves with ease, e.g. one on the right audio channel, the other on the left one.

Third, "more RAM" means "more wavetables in memory".

Fourth, the higher ADC resolution extends analog readings to the 0 - 4095 range. This augmented resolution makes it possible to, poorly speaking, "concentrate" a higher amount of notes withing it's 0-3V range of analog input, without overlapping them.

Pi Pico also has some disadvantages with respect to an evergreen Arduino nano.

One is the limited number of analog inputs. I have obviated to this by keeping analog inputs only for control voltages and adopting rotary encoders for direct user interations.

Another limit is the more complex coding language (circuit phyton is not for me, yet. Shame on me.). Luckily, as far as we keep things "simple", we can use Arduino IDE as alternative.

**Rotary Encoders**

Rotaries are user main devices for communication with the microcontroller board of the module. There are four of them, each with it's own filter circuit to clean the signal and make the microcontroller work easier. Filter design has been taken from Bourns rotary encoders datasheet. Please notice that I have not used such exact encoders, but generic (and vastly available) encodes from a Chinese supplier (see "Supplies" Step for details).

Rotaries have a push-button on the z axis, a common (and very welcome when dealing with small spaces)feature this module takes advantage from.

**Output Stage**

Outputs of each voice are low-pass filtered to limit digital artifacts at higher frequencies (noises and whines).

The filter adopted is not the "classical" RC filter, but a Sallen-Key low-pass filter. This is a simple-but-effective, second-order active filter I had already adopted for a project of mine in the past.

The filter is amplified with a X2 gain in order to reach the 6.6Vpp ballpark. I know, I know, it's not a line level voltage, but my DIY synth is built around 3340 VCOs modules with hot outputs (approx 8Vpp). The hardware can be modified at assemblying phase for 1X gain.

An AC coupling capacitor and a current limiting series resistor complete che output stage for each voice.

**Control Voltage**

Each voice has its own indipendent control voltage for pitch in the V/octave format (0-10V range).

The RP2040 microcontroller GPIOs work at 3.3V level and are not 5V tolerant. The solution I have adopted here is to "compress" incoming signals in the 0/+3.3V range by an op-amp in differential voltage confiuration (see >>this<< reference if you want to go deeper).

CV inputs are Schottky diode protected against possible damages due to the application of negative voltages.

I designed the circuit with the help of Falstad's CircuitJS. >>HERE<< is a link to the circuit simulation, if you want to toy with it ;)

**MIDI IN and OUT**

MIDI IN and MIDI OUT circuits are also present. These are well tested circuits I have already used in many of my previous projects (see >>this one<< in example).

The MIDI IN circuit is an opto-isolated circuit respecting the MIDI association specifications. It makes use of a single channel Darlinghton optocoupler (6N138), and some other slave components. Very basic, but functional, circuit.

**Please notice that MIDI is not implemented in the current sketch. I wanted the circuit to be there for future upgrades, but the actual project makes no use of it.**

# The Code
The code generates two indipendent, free-running voices with two wavetable oscillators each.

This very special (and complex) part of the code is made simple by the use of Mozzy library, a very nice sound synthesis library for Arduino.

Mozzy is a vast library and I am still skratching the top of it's ice. If you are willing to improve this sketch, my suggestion is to give a good reading at Mozzi documentation since there are very interesting features a sound generation project like this can take advantage from.

The most "exciting" (at least for me) part of the code I wrote regards how wavetables are handled. Commonly projects load one or more wavetables from those available and reproduce them. In my sketch wavetables are instead stored as a single looong wavetable which is actually a series of smaller wavetables in the same array. I called this a "streamwave", but I could have called it a "transwave" (this is for the older of you, I suppose :) ).

Loading a streamwave makes it possible not only to freely cycle between wavetables constituting the stream, but also reproduce a mix of adiacent wavetables. Being the wave array fixed lenght, by warying the starting point of the streamwave you end up with mixed (and unpredictably sounding) wavetables. Cool :)

Wave samples (or wavetables) are, in general, characterized by two main factors: sample resolution and bit-depth. In very simple (and inaccurate) words, sample resolution is the number of points (cells) for a complete cycle of a waveform (AKA "frame") and is here represented by the wavetable array lenght. Bit-depth is the "amplitude" of the single wave point. Typical values for Mozzi wavetables are 512 ~ 2048 cells and 8 bit (one byte) of depth (range -127 to 127).

After various testings, I have here adopted a sample resolution of 4096. I kept the resolution at 8-bit given that Mozzi library shift the wavetable amplitude to that value anyway, for compatibility reasons.

Fourteen wavetables are available in the moment I am writing, and defined in file Stream_DATA_4096_8bit. Included wavetables come from some single cycle wav file I found on the internet. I am not sure about how they were recorded, but had evocative names so I could not resist :)

More wavetables to come since there's still a lot of free space available! To help you increase the number by youself in the meantime, I wrote a brief "How To": take a look at "Wavetables From Audio File" Step!

Pitches are set as a function of incoming control voltages in the V/oct format at the dedicated inputs (input range 0 to 10V). This part of the code saw a complete rewrite after the first try, because direct convertion of incoming signals gave me a +/-20 cents of detuning: unacceptable! I obtained way (WAY) better results with a tabulated approach: frequencies withing one cent from the ideal!

Rotary encoders give user control over voice octave, fine pitch, absolute gain, wavetable ("straight" and "mixed") and oscillators relative detune.

Each rotary has a Z-button. These are used for voice selection and to toggle between rotary functions. More on this in the "How To Use" Step.

Software dependencies:

Earle Phil Hower's Arduino Pi Pico core

Mozzi Team's Mozzi library V2

(Forty Seven Effects MIDI library)

All these libraries can be directly downloaded from arduino's IDE.

# Module Improvements After PWM Prototype

When I started working on my very first wavetable oscillator module, my only need was a "MIDI controlled something" oscillating at audible rates. No matter how it sounded like (and it ended being good enought for me at the time), the project made it's job.

Now that my ears are a little bit more trained and demanding, in total honesty I don't like how pure PWM audio sound. It's not Mozzi's fault (that's actually an enabling software for this project, so I must give it due credits) but likely PWM technology vs my ears.

I made variuos tests, from adopting wavetables with higher frame dimension and increased bit-depth, to the use of custom wavetables: nothing was satisfactory enough (note to self: on PWM, resolutions higher than 2048 makes no difference to my ears).

Anyway, I don't give up easily. I really wanted to enjoy my new module and started digging for a different approach.

Mozzi is a currently developed library, with a set of impressive features already there. Using it's PWM capabilities is only the top of it's surface and the fastest way to give a kick to a musical project, but other approaches are possible. The one I ended using to improve audio quality is the adoption of external DAC. There are plenty of DACs out there, but the one mostly documented with Mozzi is PT8211. This is a 16 bits, I2S, dual channel DAC perfectly working with Mozzi.

It took little time to have a sketch ready (many thanks to Tom Combriat for his help) and then install it on my current hardware for a fly test. The module sounded definitely better after that! Way less interference and noise, especially on mixed wavetables, less interference between the two channels (even if not null on low sample resolution waves), and a sensible difference in quality by using higher resolution tables.

No brainer: I implemented it permanently on Gerber! In particular, I drawn a new main board, the face plate and front board being left unchanged.

Unfortunately I have not the new hardware manufactured, so pictures in this Instructables show the PWM version. The new version with DAC is the one shared on this repository. It's untested (again, I don't have it on my hands yet, only tested with a DAC soldered on my prototype through flying wires) so use it at your own risk.

# Acknowledgments
Many thanks to the nice girls and guys at [JLCPCB](https://jlcpcb.com/?from=IAT) for sponsoring the manufacturing of PCBs for this module. Without their contribution this project would have never seen the light.

JLCPCB is a high-tech manufacturer specialized in the production of high-reliable and cost-effective PCBs. They offer a flexible PCB assembly service with a huge library of more than 350.000 components in stock.

3D printing is part of their portfolio of services so one could create a full finished product, all in one place!

By registering at JLCPCB site via [THIS LINK](https://jlcpcb.com/?from=IAT) (affiliated link) you will receive a series of coupons for your orders. Registering costs nothing, so it could be the right opportunity to give their service a due try ;)

My projects are free and for everybody. You are anyway welcome if you want to donate some change to help me cover components costs and push the development of new projects (I have a new one on a third wavetable oscillator module that could be interesting for some of you... :) )

[>>HERE<<](https://paypal.me/GuidolinMarco?country.x=IT&locale.x=it_IT) is my paypal donation page, just in case ;)


