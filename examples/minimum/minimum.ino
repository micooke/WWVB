//ATtiny85
//                     +-\/-+
//            RST PB5 1|*   |8 VCC
//                PB3 2|    |7 PB2
//WWVB ANTENNA <= PB4 3|    |6 PB1
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
//     16| A2/D16     D7 |7
//     17| A3/D17     D6~|6
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
//     16| A2/D16              D7 |7
//     17| A3/D17              D6~|6
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
//                  +----+=====+----+
//                  |[J1]| USB |    |
//                 1| TXO+-----+RAW |
//                 0| RXI       GND |
//                  | GND       RST |
//                  | GND       VCC |
//             SDA 2| D2         A3 |21
//             SCL 3|~D3         A2 |20
//                 4| D4         A1 |19
//                 5|~D5         A0 |18
//                 6|~D6        D15 |15 SCK
//                 7| D7        D14 |14 MISO
//                 8| D8        D16 |16 MOSI
//WWVB ANTENNA  <= 9|~D9        D10~|10
//                  +---------------+

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


#include <Arduino.h>
#include <avr/interrupt.h>

#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
#define _DEBUG 0
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
#define _DEBUG 1
#else
#define _DEBUG 1
#endif

/*
_DEBUG == 0: Set wwvb time to the compile time, use the wwvb interrupt, blink the led
_DEBUG == 1: Set wwvb time to the compile time, use the wwvb interrupt, blink the led, serial output
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

uint8_t mins = 0;
uint16_t LED_FAST = 100;
uint16_t SLOW_DELAY = 1000;
bool LED_TOGGLE = false;
uint32_t t0;

#define REQUIRE_TIMEDATESTRING 1

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;

// The ISR sets the PWM pulse width to correspond with the WWVB bit
ISR(TIMER1_OVF_vect)
{
	wwvb_tx.interrupt_routine();
}

// Setup your timezones here
//const int8_t local_timezone[2] = { 10, 30 }; // This is your local timezone : ACDT (UTC +10:30)
const int8_t wwvb_timezone[2] = { -6, 0 }; // This is the timezone of your wwvb clock : CST (UTC -6:00)

void setup()
{
	wwvb_tx.setup();

	// Note: set the timezone before you set your time

	// set the timezone to your wwvb timezone (the negative is supposed to be here)
	wwvb_tx.setTimezone(-wwvb_timezone[0], -wwvb_timezone[1]);

	//wwvb_tx.setPWM_LOW(0); // sets the pulsewidth for the wwvb 'low' signal : 0 - 133  = 0 - 100% for 16MHz

#if (REQUIRE_TIMEDATESTRING == 1)
	wwvb_tx.set_time(__DATE__, __TIME__);
#endif

#if (_DEBUG > 0)
	Serial.begin(9600);
#if defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
	while (!Serial); // If using a leonardo/micro, wait for the Serial connection
#endif

	Serial.print("wwvb set to: ");
	Serial.print(__TIME__);
	Serial.print(" on ");
	Serial.println(__DATE__);

	wwvb_tx.debug_time();
	Serial.println();
#endif

	pinMode(LED_PIN, OUTPUT);

	wwvb_tx.start(); // Thats it
}
void loop()
{
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
		digitalWrite(LED_PIN, HIGH);
	}
}