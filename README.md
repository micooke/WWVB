# Arduino WWVB Transmitter
An Arduino based WWVB transmitter ATtiny85, nano, micro etc., synced off of GPS time (optional).
Designed for the 16MHz or 8MHz leonardo or uno

# Broken for the ATtiny85
I tried unifying the code for the ATtiny85, ATmega32u4 and ATmega328 series Arduino chips/boards but this implemented a breaking change for the ATtiny85.
I assume the reason for this break is an incompatability between the Arduino framework (likely the delay routine) for these chips.

In other words, the library needs to be rewritten for the least useful use case (the ATtiny85). This has a very low priority at the moment, as there is no current need

* Author/s: [Mark Cooke](https://www.github.com/micooke), [Martin Sniedze](https://www.github.com/mr-sneezy)

## Requirements
* [TimeDateTools.h](https://github.com/micooke/ATtinyGPS/TimeDateTools.h)
* [ATtinyGPS.h](https://github.com/micooke/ATtinyGPS/ATtinyGPS.h) : for setting time based off a serial GPS


## About
http://www.nist.gov/pml/div688/grp40/wwvb.cfm

WWVB: 60kHz carrier, Amplitude modulated to Vp -17dB for signal low

Im hoping that the wwvb receiver is insensitive to this -17dB value as im using pulse width modulation 
to set a 5% duty cycle for the low signal.

## Hookup

![wwvb wiring options](wwvb_bb.png?raw=true)

*See examples folder (start with minimum)*