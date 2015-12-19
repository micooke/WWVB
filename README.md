# Arduino
Arduino libraries and projects

All libraries (unless ive forked it from another repo) should be under a MIT license.
Copyright owners are the authors listed

##wwvb
* Author/s: Mark Cooke, Martin Sniedze @mr-sneezy

Requirements: `TimeDateTools.h`, ATtiny_GPS (for setting based off GPS)

WWVB: 60Khz carrier, Amplitude modulated to Vp -17dB for signal low

Im hoping that the wwvb receiver is insensitive to this -17dB value,
as im using pulse width modulation in slow time (changing compare vector OCR1A)
to 5% duty cycle

Designed for the ATtiny85 @ 16MHz / 5V, but should work with 3.3V / 5V leonardo or uno
*See examples folder (start with minimum)*

TODO: i need to remove the software SPI stuff in the example as software SPI for the TFT just wont fit

##ATtiny_GPS
* Author/s: Mark Cooke

A very basic NMEA string parser for GPS strings GPRMC and GPZDA.
It is very static at the moment (fixed decimal places for LLA, Time, Date etc), and needs a re-write (and rename)
It was designed for the ATtiny85 - i couldnt find any other parsers that actually fit on it.

##TimeDateTools
* Author/s: Mark Cooke

Converts from (char arrays) TimeString and DateString to their integers components
Also does a bunch of other things like:
1. calculates leapyears
2. converts day, month to day of the year 
3. converts invalid time/date additions ie. hour 25, day 1 -> hour 1, day 2 etc

TODO: I need to move the static int arrays at the top to PROGMEM
