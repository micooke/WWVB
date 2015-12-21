# Arduino WWVB Transmitter
An Arduino based WWVB transmitter ATtiny85, nano, micro etc., synced off of GPS time (optional).
Designed for the ATtiny85 @ 16MHz / 5V, but should work with 3.3V / 5V leonardo or uno

* Author/s: [Mark Cooke](https://www.github.com/micooke), [Martin Sniedze](https://www.github.com/mr-sneezy)

## Requirements
* [TimeDateTools.h](https://github.com/micooke/ATtinyGPS/TimeDateTools.h)
* [ATtinyGPS.h](https://github.com/micooke/ATtinyGPS/ATtinyGPS.h) : for setting time based off a serial GPS


## About
http://www.nist.gov/pml/div688/grp40/wwvb.cfm

WWVB: 60Khz carrier, Amplitude modulated to Vp -17dB for signal low

Im hoping that the wwvb receiver is insensitive to this -17dB value, as im using pulse width modulation 
to set a 5% duty cycle for the low signal.

*See examples folder (start with minimum)*

TODO: i need to remove the software SPI stuff in the example as software SPI for the TFT just wont fit
