#include <Arduino.h>

/*
Arduino Nano
       +----+=====+----+
       |    | USB |    |
SCK B5 |D13 +-----+D12 | B4 MISO
       |3V3        D11~| B3 MOSI
       |Vref       D10~| B2 SS
    C0 |A0          D9~| B1 |=> WWVB ANTENNA
    C1 |A1          D8 | B0
    C2 |A2          D7 | D7 |<= GPS Tx (Software Serial)
    C3 |A3          D6~| D6
SDA C4 |A4          D5~| D5
SCL C5 |A5          D4 | D4
       |A6          D3~| D3 INT1
       |A7          D2 | D2 INT0
       |5V         GND |
    C6 |RST        RST | C6
       |GND        TX1 | D0
       |Vin        RX1 | D1 |<= GPS Tx (Hardware Serial)
       |  5V MOSI GND  |
       |   [] [] []    |
       |   [] [] []    |
       | MISO SCK RST  |
       +---------------+
*/


#define REQUIRE_TIMEDATESTRING 0
// Set to zero if you dont set the date/time off of the compiled date/time
// e.g. wwvb_tx.set_time(__DATE__, __TIME__);
// Note: #define REQUIRE_TIMEDATESTRING 0 saves 1172 bytes of flash and 46 bytes of static RAM!

#define USE_OC1A // Use OC1A / D9

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;

// The ISR sets the PWM pulse width to correspond with the WWVB bit
ISR(TIMER1_OVF_vect)

{
	cli(); // disable interrupts
	wwvb_tx.interrupt_routine();
	sei(); // enable interrupts
}

#include <SoftwareSerial.h>
SoftwareSerial ttl(7, 8); // Tx pin (8) not used in this example

#include <ATtinyGPS.h>
ATtinyGPS gps;

uint8_t count = 0;

// debug variables
uint32_t t1;
boolean isLow = false;

void setup()
{
	wwvb_tx.setup();

	ttl.begin(9600);

}

void loop()
{
	// if we are receiving gps data, parse it
	if (ttl.available())
	{
		gps.parse(ttl.read());
	}

	// If there is new data AND its a whole minute
	// set the wwvb time
	if (gps.new_data() & (gps.ss == 0))
	{
		// Yeah im ignoring the last parameter to set whether we are in daylight savings time
		wwvb_tx.set_time(gps.mm, gps.hh, gps.DD, gps.MM, gps.YY);
		if (!wwvb_tx.is_active())
		{
			wwvb_tx.start();
		}
	}
}
