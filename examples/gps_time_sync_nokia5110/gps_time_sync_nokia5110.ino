#include <Arduino.h>
//ATtiny85 - Not enough pins & compile size too large
//
//Arduino Nano
//              +----+=====+----+
//              |    | USB |    |
// LCD CLK <= 13| D13+-----+D12 |12 MISO
//              |3V3        D11~|11 => LCD Din
//              |Vref       D10~|10 SS
//            14| A0/D14     D9~|9 => WWVB ANTENNA
//            15| A1/D15     D8 |8
//            16| A2/D16     D7 |7 <= GPS Tx (Software Serial)
//            17| A3/D17     D6~|6 => GPS Rx (Software Serial)
//        SDA 18| A4/D18     D5~|5
//        SCL 19| A5/D19     D4 |4 => LCD RST
//            20| A6         D3~|3 => LCD CE/CS (Chip Enable)
//            21| A7         D2 |2 => LCD DC (Data/Command Select)
//              | 5V        GND |
//              | RST       RST |
//              | GND       TX1 |0
//              | Vin       RX1 |1
//              |  5V MOSI GND  |
//              |   [ ][ ][ ]   |
//              |   [*][ ][ ]   |
//              | MISO SCK RST  |
//              +---------------+
//
//Arduino Micro
//              +--------+=====+---------+
//              |        | USB |         |
//        SCK 13| D13    +-----+     D12 |12 MISO
//              |3V3                 D11~|11 => LCD Din
//              |Vref                D10~|10 SS
// LCD DC  <= 14| A0/D14              D9~|9 => WWVB ANTENNA
// LCD CE  <= 15| A1/D15              D8 |8
// LCD RST <= 16| A2/D16              D7 |7 <= GPS Tx (Software Serial)
//            17| A3/D17              D6~|6 => GPS Rx (Software Serial)
//        SDA 18| A4/D18              D5~|5
//        SCL 19| A5/D19              D4 |4
//            20| A6                  D3~|3 INT1
//            21| A7                  D2 |2 INT0
//              | 5V                 GND |
//              | RST                RST |
//              | GND                TX1 |0
//              | Vin                RX1 |1
//              |MISO MISO[*][ ]VCC    SS|
//              |SCK   SCK[ ][ ]MOSI MOSI|
//              |      RST[ ][ ]GND      |
//              +------------------------+
//
//Sparkfun Pro Micro
//                             +----+=====+----+
//                             |[J1]| USB |    |
//                            1| TXO+-----+RAW |
//                            0| RXI       GND |
//                             | GND       RST |
//                             | GND       VCC |
//                        SDA 2| D2         A3 |21
//                        SCL 3|~D3         A2 |20 => LCD RST
//                            4| D4         A1 |19 => LCD CE
//                            5|~D5         A0 |18 => LCD DC
//GPS Tx (Software Serial) <= 6|~D6        D15 |15 => LCD CLK
//GPS Rx (Software Serial) => 7| D7        D14 |14 MISO
//                            8| D8        D16 |16 => LCD Din
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

//#define _DEBUG 1 // Serial output
#define _DEBUG 0

bool sync_gpstime = true;

uint32_t t0;

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;

// The ISR sets the PWM pulse width to correspond with the WWVB bit
ISR(TIMER1_OVF_vect)
{
	wwvb_tx.interrupt_routine();
}

#include <SoftwareSerial.h>
#if defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
SoftwareSerial ttl(10, 6);// Rx, Tx pin
#else
SoftwareSerial ttl(7, 6);// Rx, Tx pin
#endif

//#define GPS_MODULE 0 // ublox : Flash 16,812 bytes, SRAM 1299 bytes
//#define GPS_MODULE 1 // mediatek (default) : Flash 16,138 bytes, SRAM 1053 bytes

#include <ATtinyGPS.h>
ATtinyGPS gps;

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

#if defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
Adafruit_PCD8544 nokia5110 = Adafruit_PCD8544(A0, A1, A2); // HardwareSPI
#else
Adafruit_PCD8544 nokia5110 = Adafruit_PCD8544(2, 3, 4); // HardwareSPI
#endif

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
	wwvb_tx.calibrate(33, 34); // 60.000017s
	// calibration
	//(0, 0) = 59.966523s (86, 86) = 60.052528s
	// 60.052528/(0.086005 / 86) = 52.5249 - so calibrate to (86,86) - (53,52) = (33,34)

	// set the timezone to local time
	gps.setTimezone(local_timezone[0], local_timezone[1]); // set this to your local time e.g.

	// set the timezone to your wwvb timezone (the negative is supposed to be here)
	wwvb_tx.setTimezone(-wwvb_timezone[0], -wwvb_timezone[1]);

	ttl.begin(9600);

	gps.setup(ttl);

	nokia5110.begin();
	nokia5110.setContrast(38);
	nokia5110.setTextSize(1);
	nokia5110.setTextColor(BLACK);

	updateDisplay();
#if (_DEBUG > 0)
	Serial.begin(9600);
	Serial.println("Start");
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

// Set the default time to GPS epoch : UTC 00:00 on 06/Jan/1980
uint8_t ss = 0, mm = 0, hh = 0, DD = 6, MM = 1, YY = 80;

void updateDisplay()
{
	nokia5110.clearDisplay();
	nokia5110.setCursor(0, 0);
	if (sync_gpstime)
	{
		// increment the internal time while wwvb is stopped (as it is syncing with gps)
		addTimezone<uint8_t>(hh, mm, ss, DD, MM, YY, 0, 0, 1);
	}
	else
	{
		// get the time from the wwvb state
		hh = wwvb_tx.hh();
		mm = wwvb_tx.mm();
		ss = wwvb_tx.ss();
		DD = wwvb_tx.DD();
		MM = wwvb_tx.MM();
		YY = wwvb_tx.YY();

		// Convert wwvb time transmitted time to local time
		addTimezone<uint8_t>(hh, mm, ss, DD, MM, YY, wwvb_timezone[0], wwvb_timezone[1], 0);
	}
	// line 1
	//nokia5110.print("   HH:MM:SS   ");
	nokia5110.print("   ");
	if (hh < 10) { nokia5110.print(0); } // zero pad
	nokia5110.print(hh); nokia5110.print(":");
	if (mm < 10) { nokia5110.print(0); } // zero pad
	nokia5110.print(mm); nokia5110.print(":");
	if (ss < 10) { nokia5110.print(0); } // zero pad
	nokia5110.println(ss);
	// line 2
	//nokia5110.print("  DD/MM/YYYY  ");
	nokia5110.print("  ");
	if (DD < 10) { nokia5110.print(0); } // zero pad
	nokia5110.print(DD); nokia5110.print("/");
	if (MM < 10) { nokia5110.print(0); } // zero pad
	nokia5110.print(MM); nokia5110.print("/");
	if (YY < 80) { nokia5110.print(20); } // year pad ;)
	else { nokia5110.print(19); }
	if (YY < 10) { nokia5110.print(0); } // zero pad
	nokia5110.println(YY);
	// line 3
	nokia5110.println("");
	// line 4
	nokia5110.print("wwvb: ");
	if (wwvb_tx.is_active())
	{
		nokia5110.print("     on");
	}
	else if (sync_gpstime)
	{
		nokia5110.print("syncing");
	}
	else
	{
		nokia5110.print("    off");
	}
	// line 5
	nokia5110.print("satellites:");
	if (gps.satellites < 10) { nokia5110.print(" "); }
	nokia5110.print(gps.satellites);
	// line 6
	nokia5110.print("fix:");
	if (gps.quality == 0)
	{
		nokia5110.print("  invalid");
	}
	else
	{
		switch (gps.quality)
		{
		case 1:
			nokia5110.print("      GPS"); break;
		case 2:
			nokia5110.print("     DGPS"); break;
		case 3:
			nokia5110.print("      PPS"); break;
		case 4:
			nokia5110.print("      RTK"); break;
		case 5:
			nokia5110.print("float RTK"); break;
		case 6:
			nokia5110.print("DEAD RECN"); break;
		case 7:
			nokia5110.print("   MANUAL"); break;
		case 8:
			nokia5110.print("SIMULATED"); break;
		}
	}
	nokia5110.display();
}

void loop()
{
	// if we are receiving gps data, parse it
	if (sync_gpstime)
	{
#if (_DEBUG > 0)
		Serial.println("Sync gps");
#endif
		t0 = millis(); // sync the internal arduino millis() to the wwvb sync time
		enableSoftwareSerialRead(); // enable SoftwareSerial pin change interrupts
		ttl.listen(); // reset buffer status
		gps.new_data(); // clear gps.new_data

		// wait until we get gps data
		while (!(((gps.IsValid) | ((gps.YY < 80) & (gps.YY > 15))) & ((gps.ss == 0) & gps.new_data())))
		{
			while (ttl.available())
			{
				gps.parse(ttl.read());
			}
			// update display every 1s
			if (millis() - t0 >= 1000)
			{
				t0 = millis();
				updateDisplay();
			}
		}

		// disable the pin change interrupts that SoftwareSerial uses to read data
		// as it interferes with the wwvb timing
		ttl.stopListening();
		disableSoftwareSerialRead(); // disable SoftwareSerial pin change interrupts

		// Yeah im ignoring the last parameter to set whether we are in daylight savings time
		wwvb_tx.set_time(gps.hh, gps.mm, gps.DD, gps.MM, gps.YY);
		wwvb_tx.start();
#if (_DEBUG > 0)
		Serial.println("Start wwvb");
#endif
		sync_gpstime = false;
		t0 = millis(); // sync the internal arduino millis() to the wwvb sync time
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
#if (_DEBUG > 0)
			Serial.println("Stop wwvb");
#endif
			wwvb_tx.stop();
			sync_gpstime = true;
		}
	}

	// update display every 1s
	if (millis() - t0 >= 1000)
	{
		t0 = millis();
		updateDisplay();
	}
}
