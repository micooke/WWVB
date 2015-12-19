#include <Arduino.h>

/*
Nano connections
Using OC1B (default)
Nano Pin 10 | D7 -> WWVB Antenna

Hardware serial (default)
Nano Pin 2 | D0/RX -> GPS TX

Software serial
Nano Pin 11 | D8 -> GPS TX

TODO: Include ascii art for the nano from:
http://busyducks.com/ascii-art-arduinos
*/


#define REQUIRE_TIMEDATESTRING 0
// Set to zero if you dont set the date/time off of the compiled date/time
// e.g. wwvb_tx.set_time(__DATE__, __TIME__);
// Note: #define REQUIRE_TIMEDATESTRING 0 saves 1172 bytes of flash and 46 bytes of static RAM!

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;

// The ISR sets the PWM pulse width to correspond with the WWVB bit
ISR(TIMER1_COMPB_vect)
{
	cli(); // disable interrupts
	wwvb_tx.interrupt_routine();
	sei(); // enable interrupts
}


//#define USE_SOFTWARE_SERIAL
#ifdef USE_SOFTWARE_SERIAL
#include <SoftwareSerial.h>
SoftwareSerial ttl(11,12); // Tx pin (12) not used in this example
#else
#define ttl Serial
#endif

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

	t1 = millis();
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
	if (gps.new_data() & (gps.ss == 0) )
	{
			// Yeah im ignoring the last parameter to set whether we are in daylight savings time
			wwvb_tx.set_time(gps.mm, gps.hh, gps.DD, gps.MM, gps.YY);
			if (!wwvb_tx.is_active())
			{
				wwvb_tx.start();
			}
	}
}
