#include <Arduino.h>
/*
Compile size examples
+------------+---------+---------+------+
|  microchip | _DEBUG  |  Flash  | SRAM |
+------------+---------+---------+------+
| ATmega328p |    1    | 11,568b | 562b |
|   ATtiny85 |    0    |  6,414b | 287b |
|   ATtiny85 |    1    |   DOESNT FIT   |
+------------+---------+---------+------+
*/
//ATtiny85 - it fits! (just dont enable debug...)
//                     +-\/-+
//            RST PB5 1|*   |8 VCC
//                PB3 2|    |7 PB2 <= GPS Tx (Software Serial)
//WWVB ANTENNA <= PB4 3|    |6 PB1 => GPS Rx (Software Serial)
//                GND 4|    |5 PB0
//                     +----+
//
//Arduino Nano
//       +----+=====+----+
//       |    | USB |    |
// SCK 13| D13+-----+D12 |12 MISO
//       |3V3        D11~|11 MOSI
//       |Vref       D10~|10 SS
//     14| A0/D14     D9~|9 => WWVB ANTENNA
//     15| A1/D15     D8 |8
//     16| A2/D16     D7 |7 <= GPS Tx (Software Serial)
//     17| A3/D17     D6~|6 => GPS Rx (Software Serial)
// SDA 18| A4/D18     D5~|5
// SCL 19| A5/D19     D4 |4
//     20| A6         D3~|3 INT1
//     21| A7         D2 |2 INT0
//       | 5V        GND |
//       | RST       RST |
//       | GND       TX1 |0
//       | Vin       RX1 |1
//       |  5V MOSI GND  |
//       |   [ ][ ][ ]   |
//       |   [*][ ][ ]   |
//       | MISO SCK RST  |
//       +---------------+
//
//Arduino Micro
//       +--------+=====+---------+
//       |        | USB |         |
// SCK 13| D13    +-----+     D12 |12 MISO
//       |3V3                 D11~|11 MOSI
//       |Vref                D10~|10 SS
//     14| A0/D14              D9~|9 => WWVB ANTENNA
//     15| A1/D15              D8 |8
//     16| A2/D16              D7 |7 <= GPS Tx (Software Serial)
//     17| A3/D17              D6~|6 => GPS Rx (Software Serial)
// SDA 18| A4/D18              D5~|5
// SCL 19| A5/D19              D4 |4
//     20| A6                  D3~|3 INT1
//     21| A7                  D2 |2 INT0
//       | 5V                 GND |
//       | RST                RST |
//       | GND                TX1 |0
//       | Vin                RX1 |1
//       |MISO MISO[*][ ]VCC    SS|
//       |SCK   SCK[ ][ ]MOSI MOSI|
//       |      RST[ ][ ]GND      |
//       +------------------------+
//
//Sparkfun Pro Micro
//                             +----+=====+----+
//                             |[J1]| USB |    |
//                            1| TXO+-----+RAW |
//                            0| RXI       GND |
//                             | GND       RST |
//                             | GND       VCC |
//                        SDA 2| D2         A3 |21
//                        SCL 3|~D3         A2 |20
//                            4| D4         A1 |19
//                            5|~D5         A0 |18
//GPS Tx (Software Serial) <= 6|~D6        D15 |15 SCK
//GPS Rx (Software Serial) => 7| D7        D14 |14 MISO
//                            8| D8        D16 |16 MOSI
//           WWVB ANTENNA  <= 9|~D9        D10~|10
//                             +---------------+

/*
Recommended debug setup
Use a RC (low-pass) filter to view the message (p1) as well as the modulated carrier (p0)

T = RC = 1/(2*pi*fc)
where;
T: Time constant
fc: cutoff frequency

for: R = 110 ohm, C = 1uF
fc = 1/(2*pi*RC) = 1.45kHz (the carrier is 60kHz, the modulation is about 1 to 5 Hz)

WWVB_OUT |--(p0)--[110R]--(p1)--[1uF]--|GND

-----------+-----------+-----------------
Chip       | #define   | WWVB_OUT
-----------+-----------+-----------------
ATtiny85   |  USE_OC1A | D1 / PB1 (pin 6)
ATtiny85   | *USE_OC1B | D4 / PB4 (pin 3)
-----------+-----------+-----------------
ATmega32u4 | *USE_OC1A | D9
ATmega32u4 |  USE_OC1B | D10
-----------+-----------+-----------------
ATmega328p | *USE_OC1A | D9
ATmega328p |  USE_OC1B | D10
-----------+-----------+-----------------

* Default setup
*/

bool sync_gpstime = true;

#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
#define _DEBUG 0
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
#define _DEBUG 1
#else
#define _DEBUG 1
#endif

/*
_DEBUG == 0: Set wwvb time to the compile time, use the wwvb interrupt, dont blink the led
_DEBUG == 1: Set wwvb time to the compile time, use the wwvb interrupt, blink the led, serial output
_DEBUG == 2: Set wwvb time to the compile time, use the wwvb interrupt, blink the led, VERBOSE serial output
*/

#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
unsigned char LED_PIN = 1; // Use the Tx LED on the digistump
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
unsigned char LED_PIN = SS;
#elif defined(__AVR_ATmega168__) | defined(__AVR_ATmega168P__) | defined(__AVR_ATmega328P__)
unsigned char LED_PIN = 13;
#elif defined(__STM32F1__)
unsigned char LED_PIN = PB13;
#else
unsigned char LED_PIN = 13;
#endif

uint16_t LED_FAST = 100;
uint16_t SLOW_DELAY = 1000;
bool LED_TOGGLE = false;
uint32_t t0;
uint8_t last_satellites = 0;

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;

// The ISR sets the PWM pulse width to correspond with the WWVB bit
ISR(TIMER1_OVF_vect)
{
	wwvb_tx.interrupt_routine();
}

#if _DEBUG == 0
#define SU_MODE 2
#else
#define SU_MODE 0
#endif

#include <SoftwareUart.h>
#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
#define Serial ttl
SoftwareUart<> ttl(2, 1);// Rx, Tx pin
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
SoftwareUart<> ttl(10, 6);// Rx, Tx pin
#else
SoftwareUart<> ttl(7, 6);// Rx, Tx pin
#endif

/*
#include <SoftwareSerial.h>
#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
SoftwareSerial ttl(2, 1);// Rx, Tx pin
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
SoftwareSerial ttl(10, 6);// Rx, Tx pin
#else
SoftwareSerial ttl(7, 6);// Rx, Tx pin
#endif
*/

//#define GPS_MODULE 0 // ublox : Flash 16,812 bytes, SRAM 1299 bytes
//#define GPS_MODULE 1 // mediatek (default) : Flash 16,138 bytes, SRAM 1053 bytes

#include <ATtinyGPS.h>
ATtinyGPS gps;

// Setup your timezones here
const int8_t local_timezone[2] = { 10, 30 }; // This is your local timezone : ACDT (UTC +10:30)
const int8_t wwvb_timezone[2] = { -6, 0 }; // This is the timezone of your wwvb clock : CST (UTC -6:00)

void setup()
{
	wwvb_tx.setup();

	// Set the wwvb calibration values
	// ATmega328p : _DEBUG = 0 or 1 : frametime for calibrate( 86, 86) = 60.000254s
	// ATmega32u4 : _DEBUG = 0 or 1 : frametime for calibrate(-6,-6) = 59.999825s
	//wwvb_tx.setPWM_LOW(0); // sets the pulsewidth for the wwvb 'low' signal : 0 - 133  = 0 - 100% for 16MHz

	// set the timezone to local time
	gps.setTimezone(local_timezone[0], local_timezone[1]); // set this to your local time e.g.

	// set the timezone to your wwvb timezone (the negative is supposed to be here)
	wwvb_tx.setTimezone(-wwvb_timezone[0], -wwvb_timezone[1]);

#if (_DEBUG > 0)
	Serial.begin(9600);
#if defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
	while (!Serial); // If using a leonardo/micro, wait for the Serial connection
#endif
	t0 = millis();
#endif

	pinMode(LED_PIN, OUTPUT);

	ttl.begin(9600);

	//gps.setup(ttl);

#if (_DEBUG > 0)
	Serial.println(F("Waiting on first GPS sync (sync only occurs at 0s)"));
	t0 = millis();
#endif
}

void disableSoftwareSerialRead()
{
	// Note : SoftwareSerial enables all pin change interrupt registers
	// disable the pin change interrupt
#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
// GIMSK  = [   -   |  INT0 |  PCIE |   -   |   -   |   -   |   -   |   -   ]
//GIMSK = 0x00;
// PCMSK0 = [   -   |   -   | PCINT5| PCINT4| PCINT3| PCINT2| PCINT1| PCINT0]
//          [   -   |   -   |    RST|    PB4|    PB3|    PB2|    PB1|    PB0]
	PCMSK = 0x00;
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
// PCICR  = [   -   |   -   |   -   |   -   |   -   |   -   |   -   |  PCIE0]
//PCICR = 0x00;
// PCMSK0 = [ PCINT7| PCINT6| PCINT5| PCINT4| PCINT3| PCINT2| PCINT1| PCINT0]
//          [    D12|    D11|    D10|     D9|   MISO|   MOSI|    SCK|     SS]
	PCMSK0 = 0x00;
#else
// PCICR  = [   -   |   -   |   -   |   -   |   -   |  PCIE2|  PCIE1|  PCIE0]
//PCICR = 0x00;
// PCMSK2 = [PCINT23|PCINT22|PCINT21|PCINT20|PCINT19|PCINT18|PCINT17|PCINT16]
//          [     D7|     D6|     D5|     D4|     D3|     D2|     D1|     D0]
	PCMSK2 = 0x00;
	// PCMSK1 = [   -   |PCINT14|PCINT13|PCINT12|PCINT11|PCINT10|PCINT09|PCINT08]
	//          [   -   |    RST|     A5|     A4|     A3|     A2|     A1|     A0]
	PCMSK1 = 0x00;
	// PCMSK0 = [ PCINT7| PCINT6| PCINT5| PCINT4| PCINT3| PCINT2| PCINT1| PCINT0]
	//          [  XTAL2|  XTAL1|    D13|    D11|    D12|    D10|     D9|     D8]
	PCMSK0 = 0x00;
#endif
}

void enableSoftwareSerialRead()
{
	// Note : SoftwareSerial enables all pin change interrupt registers
	// disable the pin change interrupt
#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
// GIMSK  = [   -   |  INT0 |  PCIE |   -   |   -   |   -   |   -   |   -   ]
// PCMSK0 = [   -   |   -   | PCINT5| PCINT4| PCINT3| PCINT2| PCINT1| PCINT0]
//          [   -   |   -   |    RST|    PB4|    PB3|    PB2|    PB1|    PB0]
	PCMSK |= _BV(PCINT2) | _BV(PCINT1);
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
// PCICR  = [   -   |   -   |   -   |   -   |   -   |   -   |   -   |  PCIE0]
// PCMSK0 = [ PCINT7| PCINT6| PCINT5| PCINT4| PCINT3| PCINT2| PCINT1| PCINT0]
//          [    D12|    D11|    D10|     D9|   MISO|   MOSI|    SCK|     SS]
	PCMSK0 |= _BV(PCINT5); // enable pin interrupt on D10
#else
// PCICR  = [   -   |   -   |   -   |   -   |   -   |  PCIE2|  PCIE1|  PCIE0]
// PCMSK2 = [PCINT23|PCINT22|PCINT21|PCINT20|PCINT19|PCINT18|PCINT17|PCINT16]
//          [     D7|     D6|     D5|     D4|     D3|     D2|     D1|     D0]
	PCMSK2 |= _BV(PCINT23); // enable pin interrupt on D7
	// PCMSK1 = [   -   |PCINT14|PCINT13|PCINT12|PCINT11|PCINT10|PCINT09|PCINT08]
	//          [   -   |    RST|     A5|     A4|     A3|     A2|     A1|     A0]
	// PCMSK0 = [ PCINT7| PCINT6| PCINT5| PCINT4| PCINT3| PCINT2| PCINT1| PCINT0]
	//          [  XTAL2|  XTAL1|    D13|    D11|    D12|    D10|     D9|     D8]
#endif
}

uint8_t mins = 0;

void loop()
{
	// if we are receiving gps data, parse it
	if (sync_gpstime)
	{
		enableSoftwareSerialRead(); // enable SoftwareSerial pin change interrupts
		ttl.listen(); // reset buffer status
		gps.new_data(); // clear gps.new_data

		// wait until we get gps data
		char c;
		while (!(((gps.IsValid) | ((gps.YY < 80) & (gps.YY > 15))) & ((gps.ss == 0) & gps.new_data())))
		{
			while (ttl.available())
			{
				c = ttl.read();
				gps.parse(c);
#if (_DEBUG == 2)
				Serial.print(c);
#endif
			}
#ifndef TIMESYNC_ONLY
#if (_DEBUG > 0)
			if (millis() - t0 > 1000)
			{
				if (gps.satellites > last_satellites)
				{
					Serial.print(gps.satellites);
					last_satellites = gps.satellites;
				}
				else if (gps.ss % 10 == 0)
				{
					Serial.print(gps.ss);
				}
				else
				{
					Serial.print(".");
				}
				t0 = millis();
			}
#endif
#endif
		}

		// disable the pin change interrupts that SoftwareSerial uses to read data
		// as it interferes with the wwvb timing
		ttl.stopListening();
		disableSoftwareSerialRead(); // disable SoftwareSerial pin change interrupts

		// Yeah im ignoring the last parameter to set whether we are in daylight savings time
		wwvb_tx.set_time(gps.hh, gps.mm, gps.DD, gps.MM, gps.YY);
		wwvb_tx.start();

#if (_DEBUG > 0)
		// get the time from gps
		uint8_t hh = gps.hh;
		uint8_t mm = gps.mm;
		uint8_t ss = gps.ss;
		uint8_t DD = gps.DD;
		uint8_t MM = gps.MM;
		uint8_t YY = gps.YY;

		// Display as local time (ACDT = UTC+10:30)
		addTimezone<uint8_t>(hh, mm, ss, DD, MM, YY, 10, 30, 0);

		Serial.println("");
		Serial.println(F("##### GPS SYNCED #####"));
		Serial.print(F("IsValid    : ")); Serial.println(gps.IsValid);
#ifndef TIMESYNC_ONLY
		Serial.print(F("Quality    : ")); Serial.println(gps.quality);
		Serial.print(F("Satellites : ")); Serial.println(gps.satellites);
#endif
		Serial.print(F("Time/Date  : ")); print_datetime(hh, mm, DD, MM, YY);
		Serial.println(F("WWVB transmit started"));
#endif
		sync_gpstime = false;
	}

	if (!sync_gpstime)
	{
		// Note
		// * wwvb time is synced and wwvb transmission is started when minutes = 0,10,20,30,40 or 50
		// * wwvb transmission is stopped when minutes = 9,19,29,39,49 or 59
		//
		// In other words, wwvb will transmit for 9 minutes, then turn off and sync for a minute
		//
		if ((wwvb_tx.mm() % 10 == 9) & wwvb_tx.is_active())
		{
			wwvb_tx.stop();
			sync_gpstime = true;
#if (_DEBUG > 0)
			Serial.println(F("WWVB transmit stopped\nResyncing time with GPS"));
#endif
		}
	}

#if (_DEBUG > 0)
	if (mins != wwvb_tx.mm())
	{
		// get the time from gps
		uint8_t hh = wwvb_tx.hh();
		uint8_t mm = wwvb_tx.mm();
		uint8_t ss = wwvb_tx.ss();
		uint8_t DD = wwvb_tx.DD();
		uint8_t MM = wwvb_tx.MM();
		uint8_t YY = wwvb_tx.YY();

		// Convert wwvb time transmitted time to local time
		addTimezone<uint8_t>(hh, mm, ss, DD, MM, YY, wwvb_timezone[0], wwvb_timezone[1], 0);

		Serial.print(F("Time/Date  : ")); print_datetime(hh, mm, DD, MM, YY);
		mins = wwvb_tx.mm();
	}
#endif

	// Debug LED
	if (wwvb_tx.is_active())
	{
		if (millis() - t0 >= LED_FAST)
		{
			t0 = millis();
			digitalWrite(LED_PIN, LED_TOGGLE);
			LED_TOGGLE = !LED_TOGGLE;
		}
	}
	else
	{
#ifndef TIMESYNC_ONLY
		if (gps.satellites == 0)
#else
		if (gps.IsValid == false)
#endif
		{
			if (millis() - t0 >= SLOW_DELAY)
			{
				t0 = millis();
				digitalWrite(LED_PIN, LED_TOGGLE);
				LED_TOGGLE = !LED_TOGGLE;
			}
		}
		else
		{
			digitalWrite(LED_PIN, HIGH);
		}
	}
}