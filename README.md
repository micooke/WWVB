# Arduino WWVB Transmitter
An Arduino based WWVB transmitter ATtiny85, nano, micro etc., synced off of GPS time (optional).
Designed for the ATtiny85 @ 16MHz / 5V, but should work with 3.3V / 5V leonardo or uno

* Author/s: [Mark Cooke](https://www.github.com/micooke), [Martin Sniedze](https://www.github.com/mr-sneezy)

## Requirements
* [TimeDateTools.h](https://github.com/micooke/ATtinyGPS/TimeDateTools.h)
* [ATtinyGPS.h](https://github.com/micooke/ATtinyGPS/ATtinyGPS.h) : for setting time based off a serial GPS


## About
http://www.nist.gov/pml/div688/grp40/wwvb.cfm

WWVB: 60kHz carrier, Amplitude modulated to Vp -17dB for signal low

Im hoping that the wwvb receiver is insensitive to this -17dB value as im using pulse width modulation 
to set a 5% duty cycle for the low signal.

*See examples folder (start with minimum)*

## TODO:
* Remove the software SPI stuff in the ATtiny85 example as software SPI for a TFT just wont fit
* Slim the library back down - ive been targetting a nano lately and the library has gained some weight!
* Look at the GPS parseing - 
* Fix the GPS parseing example and the ATtiny85 example (wwvb.ino)
* Put this on hackaday and win $1 million in the lotto... or just get some sleep