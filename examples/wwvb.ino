#include <Arduino.h>

/*
ATtiny85 connections

				     +-\/-+
				RST 1|*   |8 VCC
GPS Tx/ SS Rx | PB3 2|    |7 PB2 | SCK
 WWVB ANTENNA | PB4 3|    |6 PB1 | MISO
				GND 4|    |5 PB0 | MOSI
					 +----+

Note: If using a 3.3V GPS, change the ATtiny85 clock frequency to 8MHz
*/

#define USE_SPI_TFT
//#define USE_GPS
//#define SetupGPS
//#define SoftwareSPI

#define REQUIRE_TIMEDATESTRING 0
// Set to zero if you dont set the date/time off of the compiled date/time
// e.g. wwvb_tx.set_time(__DATE__, __TIME__);
// Note: #define REQUIRE_TIMEDATESTRING 0 saves 1172 bytes of flash and 46 bytes of static RAM!

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;

// The ISR sets the PWM pulse width to correspond with the WWVB bit
#if defined(USE_OC1A)
ISR(TIMER1_COMPA_vect)
#elif defined(USE_OC1B) // ATtiny
ISR(TIMER1_COMPB_vect)
#endif
{
	cli(); // disable interrupts
	wwvb_tx.interrupt_routine();
	sei(); // enable interrupts
}

#ifdef USE_GPS
#include <TinyPinChange.h>
#include <SoftSerial.h>
#include <ATtinyGPS.h>
#endif

#ifdef SoftwareSPI
// Note: DigitalPins.h modified to add ATtiny85 pins: https://github.com/greiman/DigitalIO/tree/master/DigitalIO
#include <SoftSPI.h> 

//SoftSPI<MISO,MOSI,SCK,MODE>
SoftSPI<PB4, PB0, PB2, 0> spi;
#endif

#ifdef USE_SPI_TFT
#include <Adafruit_GFX.h>
#endif

#ifdef USE_GPS
// Rx,Tx = PB3,PB5
SoftSerial ss(3, 5);
ATtinyGPS gps;
#endif

uint8_t count = 0;

// debug variables
uint32_t t1;
boolean isLow = false;

void setup()
{
	wwvb_tx.setup();

#if (REQUIRE_TIMEDATESTRING == 1)
	// set the time to the compile time
	wwvb_tx.set_time(__DATE__, __TIME__);
#endif

#ifdef USE_GPS
	ss.begin(9600);
#endif
#ifdef SetupGPS
	// Setup the GPS
	// send RMC & ZDA only
	//                 0= GLL, RMC, VTG, GGA, GSA
	ss.println("$PMTK314,   0,   1,   0,   0,   0,"
		//                 5= GSV, GRS, GST,    ,   
		"   0,   0,   0,   0,   0,"
		//                10=    ,   ,    ,MALM,MEPH
		"   0,   0,   0,   0,   0,"
		//                15=MDGP,MDBG, ZDA,MCHN, checksum   
		"   0,   0,   1,   0, 0*34");
	//					"   0,   0,   1,   0*28");

	//1Hz update rate
	ss.println("$PMTK220,1000*1F");

	//ss.println("$PMTK314,-1*04"); // reset NMEA sequences to system default
	//ss.println("$PMTK251,0*28"); // reset BAUD rate to system default
#endif

	
#ifdef SoftwareSPI
	ss.println("Software SPI test");
	spi.send(0x55);
	ss.println(spi.receive(), HEX);
	ss.println(spi.transfer(0XAA), HEX);
#endif
	t1 = millis();

	// please make this the last line in setup as it starts the transmission
	// which starts the timing
	wwvb_tx.start();
}

void loop()
{
	// Note: wwvb PWM signal is managed by the ISR at the top of this file
#ifdef USE_GPS
	// if we are receiving gps data, parse it
	if (ss.available())
	{
		gps.parse(ss.read());
	}

	// If there is new data AND its a whole minute
	// set the wwvb time
	if (gps.new_data() & (gps.ss == 0) )
	{
		count++;

		// Update every other minute
		if (count % 2 == 0)
		{
			// Yeah im ignoring the last parameter to set daylight savings time
			wwvb_tx.set_time(gps.mm, gps.hh, gps.DD, gps.MM, gps.YY);
			if (!wwvb_tx.is_active())
			{
				wwvb_tx.start();
			}
		}
		else
		{
			//wwvb_tx.pause(); // set the PWM to low
			wwvb_tx.stop(); // turn off the PWM
		}
	}
#else
	if (millis() - t1 >= 5000)
	{
		if (isLow)
		{
			wwvb_tx.start();//set_high();
			isLow = false;
		}
	}
	if (millis() - t1 >= 10000)
	{
		t1 = millis();
		wwvb_tx.stop();//set_low();
		isLow = true;
	}
#endif
}
