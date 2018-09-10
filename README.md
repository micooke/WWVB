**Project has been superseeded**

Sep 2018 - Ive noticed a few people have starred this project recently, so i wanted to mention that this project is just for legacy and that my other repo is more current / will see development when i get back to it https://github.com/micooke/wwvb_jjy
Having said this, you may find that this repo is simpler to understand or that it 'just works'. If so -> awesome!

# Arduino WWVB Transmitter
An Arduino based WWVB transmitter ATtiny85, nano, micro etc., synced off of GPS time (optional).
Designed for the 16MHz or 8MHz leonardo or uno

**Broken for the ATtiny85**
I tried unifying the code for the ATtiny85, ATmega32u4 and ATmega328 series Arduino chips/boards but this implemented a breaking change for the ATtiny85.
I assume the reason for this break is an incompatability between the Arduino framework (likely the delay routine) for these chips.

In other words, the library needs to be rewritten for the least useful use case (the ATtiny85). This has a very low priority at the moment, as there is no current need

* Author/s: [Mark Cooke](https://www.github.com/micooke), [Martin Sniedze](https://www.github.com/mr-sneezy)

## Requirements
* [TimeDateTools.h](https://github.com/micooke/ATtinyGPS/TimeDateTools.h)
* [ATtinyGPS.h](https://github.com/micooke/ATtinyGPS/ATtinyGPS.h) : for setting time based off a serial GPS

## Confirmed working clocks / watches 
* Equity by La Crosse SkyScan 31269 LCD Atomic Alarm Clock
* La Crosse Technology WS-8418U-IT Atomic Digital Wall Clock with Moon Phase
* BALDR B0114ST - Blue Backlight LCD Atomic Alarm Clock Digital Thermometer Calendar Temperature Table Watch Snooze Timer WWVB Desktop Clock

## Confirmed NOT WORKING clocks / watches
* Casio Lineage LCW-M170TD-7AJF watch

## About
http://www.nist.gov/pml/div688/grp40/wwvb.cfm

WWVB: 60kHz carrier, Amplitude modulated to Vp -17dB for signal low

Im hoping that the wwvb receiver is insensitive to this -17dB value as im using pulse width modulation 
to set a 5% duty cycle for the low signal.

## Hookup

![wwvb wiring options](wwvb_bb.png?raw=true)

*See examples folder (start with minimum)*
