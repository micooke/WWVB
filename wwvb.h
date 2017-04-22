#ifndef wwvb_h
#define wwvb_h

// None - PWM modulated output on D9
// WWVB_MODULATION_OUT - modulation out of D9
// WWVB_PAM - Carrier output on D9, Modulation output on D10
/*
Designed for the ATtiny85 @ 16MHz / 5V
( but works on the ATmega328p and ATmega32u4 )

WWVB: 60Khz carrier, Amplitude modulated to Vp -17dB for signal low

Hopefully your wwvb receiver is insensitive to this -17dB value,
as this library uses pulse width modulation to set a 5% duty cycle
for the 'low' signal

WWVB format:
http://www.nist.gov/pml/div688/grp40/wwvb.cfm

A frame of data takes 60 seconds to send, and starts at the start of a minute

A frame of data contains:
* 6 subframes
* 10 bits of data per subframe / 60 bits of data
* 1 bit of data per second
	
Copyright (c) 2015 Mark Cooke, Martin Sniedze

Author/s: [Mark Cooke](https://www.github.com/micooke), [Martin Sniedze](https://www.github.com/mr-sneezy)

License: MIT license (see LICENSE file).
*/

//See minimum.ino for a brief example that sets the wwvb time to the compile time

#include <Arduino.h>

// If not already defined, assume that we require TimeString and DateString conversion
// (i.e. to set the clock to the compiled time)
#if !defined(REQUIRE_TIMEDATESTRING)
#define REQUIRE_TIMEDATESTRING 1
#endif

#if defined(WWVB_MODULATION_OUT)
#if (F_CPU != 16000000)
//#error "Currently only 16MHz ATmega328p and ATmega32u4 boards are supported for external AM"
#pragma message("ERROR : Currently only 16MHz ATmega328p and ATmega32u4 boards are supported for external AM")
#endif
#endif

#if (F_CPU < 16000000)
#define WWVB_LOW_ms 200
#define WWVB_HIGH_ms 500
#define WWVB_MARKER_ms 800
#define WWVB_EOB_ms 1000
#endif

/*
Default ATtiny85 to use OC1B as OC1A uses an SPI pin - you may want to use SPI and wwvb

Recommended debug setup
Use a RC (low-pass) filter to view the message (p1) as well as the modulated carrier (p0)

T = RC = 1/(2*pi*fc)
where;
T: Time constant
fc: cutoff frequency

for: R = 110 ohm, C = 1uF
fc = 1/(2*pi*RC) = 1.45kHz (the carrier is 60kHz, the modulation is about 1 to 5 Hz)

WWVB_OUT |--(p0)--[110R]--(p1)--[1uF]--|GND

+-----------------------+-----------+----------+--------------+
| Chip                  | #define   | WWVB_OUT |      N/A     |
+-----------------------+-----------+----------+--------------+
|                       |  WWVB_PAM | carrier  | **modulation |
+-----------------------+-----------+----------+--------------+
| ATtiny85              |  USE_OC1A |    D1    |       D2     |
|                       | *USE_OC1B |    D4    |       D3     |
+-----------------------+-----------+----------+--------------+
| ATmega32u4/ATmega328p | *USE_OC1A |    D9    |       D8     |
|                       |  USE_OC1B |   D10    |       D8     |
+-----------------------+-----------+----------+--------------+
* Default define
** default modulation pin ( can be changed using setup(modulation_pin) )
*/

#if !(defined(USE_OC1A) | defined(USE_OC1B))
#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
//#error "This library is currently broken for the ATtiny"
#pragma message("ERROR : This library is currently broken for the ATtiny")
#define USE_OC1B
#if !defined(_DEBUG)
#define _DEBUG 0
#endif
#else
#define USE_OC1A
#if !defined(_DEBUG)
#define _DEBUG 2
#endif
#endif
#endif

#include <TimeDateTools.h>

// wwvb class - note, you will need to specify the interrupt routine in your main sketch
class wwvb
{
private:
	
	// Define the Modulation and Carrier pins
	uint8_t modulation_pin;
	uint8_t carrier_pin;

	// temp variables
	volatile uint8_t t_ss, t_mm, t_hh, t_DD, t_MM, t_YY;

	// timezone variables
	int8_t timezone_HH, timezone_MM;

	// internal variables, set by set_time ONLY
	volatile uint8_t secs_, mins_, hour_; // 00-59, 00-23
	volatile uint8_t DD_, MM_, YY_; // 1-31, 1-12, 00-99
	volatile uint16_t doty_; // 1=1 Jan, 365 = 31 Dec (or 366 in a leap year)
	volatile bool is_leap_year_;
	volatile uint8_t daylight_savings_; // 00 - no, 10 - starts today, 11 - yes, 01 - ends today

	// LOW :   Low for 0.2s / 1.0s (20% low duty cycle)
	// HIGH:   Low for 0.5s / 1.0s
	// MARKER: Low for 0.8s / 1.0s
	// hopefully your sketch can spare the extra 6 bytes for the convenience of being able to use WWVB_LOW etc.

	volatile uint16_t WWVB_LOW, WWVB_HIGH, WWVB_MARKER, WWVB_ENDOFBIT;
	#if (F_CPU < 16000000)
	uint32_t t0 = 0;
	uint32_t tELAPSED = 0;
	volatile uint16_t pulse_width[3] = { WWVB_LOW_ms, WWVB_HIGH_ms, WWVB_MARKER_ms };
	#else
	volatile uint16_t pulse_width[3] = { WWVB_LOW, WWVB_HIGH, WWVB_MARKER };
	#endif
	
	int16_t WWVB_EOB_CAL[2] = { 0,  0 };

	//                         0   1   2   3   4   5   6   7   8   9
	//                         M  40  20  10   0   8   4   2   1   M
	volatile bool MINS[10] = { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };
	//                         -   -  20  10   0   8   4   2   1   M
	volatile bool HOUR[10] = { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };
	//                         -   - 200 100   0  80  40  20  10   M
	volatile bool DOTY[10] = { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };
	//                         8   4   2   1   -   -   +   -   +   M
	volatile bool DUT1[10] = { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };
	//                         0.8 0.4 0.2 0.1 -  80  40  20  10   M
	volatile bool YEAR[10] = { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };
	//                         8   4   2   1   - LYI LSW   2   1   M
	volatile bool MISC[10] = { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };
	
	uint8_t PWM_LOW, PWM_HIGH;

	volatile uint16_t WWVB_LOWTIME;
	
	volatile bool _is_high = false;
	
	volatile int32_t isr_count = 0;
	volatile uint8_t subframe_index;
	
	volatile bool _is_active = false;
	volatile bool _is_odd_bit = true;
public:
	volatile uint8_t frame_index = 0;

	wwvb() : timezone_HH(0), timezone_MM(0), is_leap_year_(0), daylight_savings_(0)
	{
		pinMode(LED_BUILTIN, OUTPUT);
		// Set the carrier and modulation pins to output
		// Note : If WWVB_PAM is not defined, carrier refers to the PWM WWVB_OUT signal
#if defined(__AVR_ATtinyX5__)
#if defined(USE_OC1A)
#if defined(WWVB_PAM)
		modulation_pin = PB2;
#endif
		carrier_pin = PB1;
#elif defined(USE_OC1B)
#if defined(WWVB_PAM)
		modulation_pin = PB3;
#endif
		carrier_pin = PB4;
#endif
#else
#if defined(WWVB_PAM)
		modulation_pin = 8;
#endif
#if defined(USE_OC1A)
		carrier_pin = 9;
#elif defined(USE_OC1B)
		carrier_pin = 10;
#endif
#endif

#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
		calibrate(0, 0); // INVALID
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
		calibrate(-6, -6); // 16MHz
#elif defined(__AVR_ATmega168__) | defined(__AVR_ATmega168P__) | defined(__AVR_ATmega328P__)
		calibrate(86, 86); //16MHz
#endif
	}

	uint8_t ss() { return secs_; }
	uint8_t mm() { return mins_; }
	uint8_t hh() { return hour_; }
	uint8_t DD() { return DD_; }
	uint8_t MM() { return MM_; }
	uint8_t YY() { return YY_; }

	void calibrate(const int16_t &_c0, const int16_t &_c1)
	{
		WWVB_EOB_CAL[0] = _c0;
		WWVB_EOB_CAL[1] = _c1;
	}

	void set(const uint16_t &_low, const uint16_t &_high, const uint16_t &_marker, const uint16_t &_eob)
	{
		WWVB_LOW = _low;
		WWVB_HIGH = _high;
		WWVB_MARKER = _marker;
		WWVB_ENDOFBIT = _eob;

		pulse_width[0] = WWVB_LOW;
		pulse_width[1] = WWVB_HIGH;
		pulse_width[2] = WWVB_MARKER;
	}

	void raw()
	{
#if (_DEBUG > 0)
		for (uint8_t fI = 0; fI < 60; ++fI)
		{
			uint8_t sI = fI % 10;
			uint8_t subframe = fI / 10;

			switch (fI)
			{
				// markers
			case 0:  // start frame
				Serial.println(F("INDX: [0 1 2 3 4 5 6 7 8 9]"));
				Serial.print(F("MINS: [M ")); break;
			case 9:  // end MINS
				Serial.print(F("M]\n"));
				Serial.print(F("HOUR: [")); break;
			case 19: // end HOUR
				Serial.print(F("M]\n"));
				Serial.print(F("DOTY: [")); break;
			case 29: // end DOTY
				Serial.print(F("M]\n"));
				Serial.print(F("DUT1: [")); break;
			case 39: // end DUT1
				Serial.print(F("M]\n"));
				Serial.print(F("YEAR: [")); break;
			case 49: // end YEAR
				Serial.print(F("M]\n"));
				Serial.print(F("MISC: [")); break;
			case 59: // end MISC, end frame
				Serial.println(F("M]\n")); break;
			default:
				switch (subframe)
				{
				case 0:
					Serial.print(MINS[sI]); break;
				case 1:
					Serial.print(HOUR[sI]); break;
				case 2:
					Serial.print(DOTY[sI]); break;
				case 3:
					Serial.print(DUT1[sI]); break;
				case 4:
					Serial.print(YEAR[sI]); break;
				default:
					Serial.print(MISC[sI]); break;
				}
				Serial.print(' '); break;
			}
		}
#endif
	}

	void setup(const int8_t _modulation_pin = -1)
	{
		/*
		Setup the count values that correspond to 0.2s,0.5s,0.8s for wwvb encoding [LOW,HIGH,MARKER]
		i.e.  .0 .1 .2 .3 .4 .5 .6 .7 .8 .9 1.0 (1s = END OF BIT)
		//    |     |________|________|______|
		LOW   :____/          _______________\
		HIGH  :______________/         ______\
		MARKER:_______________________/      \

		Note: Pulse shape is indicative of the amplitude of the 60kHz carrier signal

		Some math to work out how much we will be off by
		(Matlab code)
		% Using an 8MHz clock
		Ts = 1/(8e6/(2*66));
		tics = round([100,200,500,800,1000]*1e-3/ Ts);
		fprintf('[%d,%d,%d,%d,%d] -> [%0.6f,%0.6f,%0.6f,%0.6f,%0.6f]\n',tics, tics*Ts  - [.1,.2,.5,.8,1.0]);
		% [6061,12121,30303,48485,60606] -> [0.000006,-0.000004,-0.000001,0.000002,-0.000001]

		% Using a 16MHz clock
		% Note: This is the Ts for AtMega328p/AtMega32u4 which ends up being the same Ts as for ATtiny85
		% in Fast PWM whether its 8MHz or 16MHz as we divide down the internal PLL source
		Ts = 1/(16e6/(2*133));
		tics = round([100,200,500,800,1000]*1e-3/ Ts);
		fprintf('[%d,%d,%d,%d,%d] -> [%0.6f,%0.6f,%0.6f,%0.6f,%0.6f]\n',tics, tics*Ts  - [.1,.2,.5,.8,1.0]);
		% [6015,12030,30075,48120,60150] -> [-0.000001,-0.000001,-0.000003,-0.000005,-0.000006]

		% Note: uint16_t maximum value is 65535, we need a max of 60606 (8MHz  ATmega328p, ATmega32u4) or
		% 60150 (ATtiny85 or 16MHz ATmega328p, ATmega32u4)so we can store the counter in 16 bits
		*/
		pinMode(carrier_pin, OUTPUT);
#if defined(WWVB_PAM)
		if (_modulation_pin != -1)
		{
			modulation_pin = _modulation_pin;
		}
		pinMode(modulation_pin, OUTPUT);
		digitalWrite(modulation_pin, LOW);
#endif

#if defined(WWVB_MODULATION_OUT)
		set(12500, 31250, 50000, 62500);
#else
#if (F_CPU == 16000000)
		set(12030, 30075, 48120, 60150);
#else // Its a 8MHz clock on a non-AT
		//set(12121, 30303, 48485, 60606);
		set(WWVB_LOW_ms, WWVB_HIGH_ms, WWVB_MARKER_ms, WWVB_EOB_ms);
#endif
#endif
		WWVB_LOWTIME = WWVB_LOW; //DEFAULT STATE

		// Generate 60kHz carrier
/*
#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)

//OC0A | !OC1A : 0 PB0 MOSI PWM 8b
//OC1A |  OC0B : 1 PB1 MISO PWM 8b
//        OC1B : 4 PB4
//       !OC1B : 3 PB3

		PLLCSR = 0; // clear all flags, use synchronous clock CK

#if (F_CPU == 16000000)
		TCCR1 = _BV(CS12) 		// Internal (non-PLL) CK selected
			| _BV(CS11);		// prescalar = 2 (16MHz/2 = 8MHz)
#elif (F_CPU == 8000000)
		TCCR1 = _BV(CS12) 		// Internal (non-PLL) CK selected
			| _BV(CS10); 		// prescalar = 1 (8MHz/1 = 8MHz)
#endif

		PWM_HIGH = 66; 			// ~50% duty cycle
		PWM_LOW = 6; 			// ~5% duty cycle
		OCR1C = 132; 			// Set PWM to 60kHz (8MHz / (132 + 1)) = 60150Hz

#if defined(USE_OC1A)
		TCCR1 |= _BV(PWM1A)  	// Clear timer/counter after compare match to OCR1C
			| _BV(COM1A1); 	// Clear the OC1A output line after match

		TIMSK |= _BV(OCIE1A);	// enable compare match interrupt on Timer1

		OCR1A = PWM_LOW; 		// Set pulse width to 5% duty cycle
#elif defined(USE_OC1B)
		GTCCR = _BV(PWM1B) 		// Clear timer/counter after compare match to OCR1C
			| _BV(COM1B1);	// Clear the OC1B output line after match

		TIMSK |= _BV(OCIE1B);	// enable compare match interrupt on Timer1

		OCR1B = PWM_LOW; 		// Set pulse width to 5% duty cycle
#endif
#else // This is a catchall for any other chip, but ive only tested it against the chips listed below
*/
//ATmega32u4
//OC0A |  OC1C : 11 PB7 PWM 8/16b
//        OC0B :  3 PD0 SCL PWM 8b | 18 PWM 10b?
//OC1A | !OC4B :  9 PB5 PWM 16b
//OC1B |  OC4B : 10 PB6 PWM 16b
//OC3A |  OCB4 :  5 PC6 PWM HS
//        OC4A : 13 PC7 PWM 10b
//       !OC4D : 12 PD6 PWM 16b
//
//ATmega328p
//OC0A :  6 PD6
//OC0B :  5 PD5
//OC1A :  9 PB1
//OC1B : 10 PB2 !SS
//OC2A : 11 PB3 MOSI
//OC2B :  3 PD3

#if defined(WWVB_MODULATION_OUT)
		// Mode 14: Fast PWM
		TCCR1B = _BV(WGM13) + _BV(WGM12);
		TCCR1A = _BV(WGM11);

		// Inverted output ie. D9 -> !OCR1A
		TCCR1A |= _BV(COM1A1) + _BV(COM1A0);

		ICR1 = WWVB_ENDOFBIT; // Set PWM to 1Hz (16MHz / (64*250)) = 1Hz

		OCR1A = WWVB_ENDOFBIT; // Default low
#else

		// Use Phase & Frequency correct PWM for other chips (leonardo, uno et. al.)
		TCCR1B = _BV(WGM13); // Mode 8: Phase & Frequency correct PWM

#if (F_CPU == 16000000)
		ICR1 = 133; // Set PWM to 60kHz (16MHz / (2*133)) = 60150Hz
		// Yes, its 133 and not 132 because the compare is double sided : 0->133->132->1
		PWM_HIGH = 66; // ~50% duty cycle
		PWM_LOW = 6; // ~5% duty cycle
#elif (F_CPU == 8000000)
		ICR1 = 67; // Set PWM to 60.6kHz (8MHz / (2*66)) = 60606Hz
		PWM_HIGH = 33; // ~50% duty cycle
		PWM_LOW = 3; // ~5% duty cycle
#endif

#if defined(USE_OC1A)
		TCCR1A = _BV(COM1A1); // Clear OC1A on compare match to OCR1A

		OCR1A = PWM_HIGH;
#elif defined(USE_OC1B)
		TCCR1A = _BV(COM1B1); // Clear OC1B on compare match to OCR1B

		OCR1B = PWM_HIGH;
#endif
#endif
		// enable interrupt on Timer1 overflow
		TIMSK1 |= _BV(TOIE1);

		//#endif
		// clear the indexing
		frame_index = 0;
		subframe_index = 0;
		isr_count = 0;
		set_dut1(); // This isnt set again - dut1 is unused (but still sent)

		set_lowTime();

		sei(); // enable interrupts
	}

	void interrupt_routine()
	{
		#if (F_CPU < 16000000)
		tELAPSED = millis() - t0;
		#endif

	#if defined(WWVB_MODULATION_OUT)
		//This routine is checked at 1Hz
		subframe_index = ++frame_index % 10;
		set_lowTime();

		OCR1A = WWVB_LOWTIME;

		// flip _is_odd_bit
		_is_odd_bit = !_is_odd_bit;

		// increment to the next second
		add_time(0, 0, 1);

		// increment the frame indices
		if (frame_index == 60)
		{
			// increment to the next minute
			//t_ss = 0;
			//add_time(0, 1, 0);

			// reset the frame_index
			frame_index = 0;
		}
#else
		//This routine is checked at each counter overflow - i.e. at 60kHz

		//if the 60kHz PWM pulse has been low for the correct time
		//1. set the PWM pulse high
		//if instead the PWM pulse has finished sending the data bit (1 second has elapsed)
		//2.1. set the PWM pulse low
		//2.2. increment to the next bit in the subframe
		//2.3. set the next WWVB_LOWTIME
		#if (F_CPU < 16000000)
		if (tELAPSED >= WWVB_EOB_ms)
		{
			t0 = millis();
		#else
		if (isr_count >= (WWVB_ENDOFBIT + WWVB_EOB_CAL[_is_odd_bit]))
		{
			// reset the isr_count (poor-mans timer, but its synced to the ~60kHz clock)
			isr_count = 0;
		#endif
			digitalWrite(LED_BUILTIN, LOW);
						
		#if defined(WWVB_PAM)
			pinMode(modulation_pin, OUTPUT);
			digitalWrite(modulation_pin, LOW);
		#else
			#if defined(USE_OC1A)
			OCR1A = PWM_LOW;
			#elif defined(USE_OC1B)
			OCR1B = PWM_LOW;
			#endif
		#endif
			_is_high = false;

			subframe_index = ++frame_index % 10;
			set_lowTime();

			// flip _is_odd_bit
			_is_odd_bit = !_is_odd_bit;

			// increment to the next second
			add_time(0, 0, 1);

			// increment the frame indices
			if (frame_index == 60)
			{
				// increment to the next minute
				//t_ss = 0;
				//add_time(0, 1, 0);

				// reset the frame_index
				frame_index = 0;
			}
		}
		#if (F_CPU < 16000000)
		else if ((_is_high == false) & (tELAPSED >= WWVB_LOWTIME))
		#else
		else if ((_is_high == false) & (++isr_count >= WWVB_LOWTIME))
		#endif
		{
			digitalWrite(LED_BUILTIN, HIGH);
			
		#if defined(WWVB_PAM)
			pinMode(modulation_pin, INPUT);
			digitalWrite(modulation_pin, LOW);
		#else
			#if defined(USE_OC1A)
			OCR1A = PWM_HIGH;
			#elif defined(USE_OC1B)
			OCR1B = PWM_HIGH;
			#endif
		#endif
			_is_high = true;
		}
		
	#endif
	}

	void start()
	{
		frame_index = 0;
		subframe_index = 0;
		set_lowTime();

		resume();
	}
	void set_low()
	{
#if defined(WWVB_MODULATION_OUT)
		OCR1A = WWVB_ENDOFBIT;
#else
#if defined(USE_OC1A)
		OCR1A = PWM_LOW;   // Set PWM to 5% duty cycle (signal LOW)
#elif defined(USE_OC1B)
		OCR1B = PWM_LOW;   // Set PWM to 5% duty cycle (signal LOW)
#endif
#endif
	}
	void set_high()
	{
#if defined(WWVB_MODULATION_OUT)
		OCR1A = 0;
#else
#if defined(USE_OC1A)
		OCR1A = PWM_HIGH;   // Set PWM to 50% duty cycle (signal LOW)
#elif defined(USE_OC1B)
		OCR1B = PWM_HIGH;   // Set PWM to 50% duty cycle (signal LOW)
#endif
#endif
	}
	void stop()
	{
		_is_active = false;

		// Set clock prescalar to 000 (STOP Timer/Clock)
		TCCR1B &= ~_BV(CS12) & ~_BV(CS11) & ~_BV(CS10);
	}
	void resume()
	{
		_is_active = true;

#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
		TCCR1 |= _BV(CS12); // Set clock prescalar to 8 (64MHz / 8 = 8MHz)
#else
#if defined(WWVB_MODULATION_OUT)
		// Set clock prescalar to 256
		TCCR1B |= _BV(CS12);
#else
		// Set clock prescalar to 1 (16MHz or 8MHz - as per the base clock)
		TCCR1B |= _BV(CS10);
#endif
#endif
	}

	bool is_active()
	{
		return _is_active;
	}

	void debug_time()
	{
#if (_DEBUG > 0)
		Serial.println(F("internal state"));
		print_datetime(hour_, mins_, DD_, MM_, YY_);

		Serial.print("raw bits\n");
		raw();

		Serial.print("decoded state\n");
		uint8_t mins = get_mins();
		uint8_t hour = get_hour();
		uint16_t doty = get_doty();
		uint8_t year = get_year();

		float dut1 = get_dut1();
		bool is_ly, is_ls;
		uint8_t ds;
		get_misc(is_ly, is_ls, ds);

		uint8_t day, month;
		from_day_of_the_year<uint8_t>(doty, day, month, is_ly);

		print_datetime(hour, mins, day, month, year);

		Serial.print(F("dut1 = "));
		Serial.println(dut1);
		Serial.print(F("leap year = "));
		if (is_ly) { Serial.println(F("true")); }
		else { Serial.println(F("false")); }
		Serial.print(F("leap second = "));
		if (is_ls) { Serial.println(F("true")); }
		else { Serial.println(F("false")); }
		Serial.print(F("daylight savings time "));
		switch (ds)
		{
		case 0:
			Serial.println(F("is not in effect")); break;
		case 1:
			Serial.println(F("ends today")); break;
		case 2:
			Serial.println(F("begins today")); break;
		default:
			Serial.println(F("is in effect")); break;
		}
#endif
	}

	void get_time(volatile uint8_t &_hour, volatile uint8_t &_mins,
		volatile uint8_t &_DD, volatile uint8_t &_MM, volatile uint8_t &_YY)
	{
		_mins = mins_;
		_hour = hour_;
		_DD = DD_;
		_MM = MM_;
		_YY = YY_;
	}

	void setPWM_LOW(const uint8_t &_value)
	{
		// dont allow it to be set higher than the max 8 bit value
		PWM_LOW = min(_value, 255);
	}

	void setPWM_HIGH(const uint8_t &_value)
	{
		// dont allow it to be set higher than the max 8 bit value
		PWM_HIGH = min(_value, 255);
	}

	void setTimezone(const int8_t &_hour, const int8_t &_mins)
	{
		timezone_HH = _hour;
		timezone_MM = _mins;
	}
	void getTimezone(int8_t &_hour, int8_t &_mins)
	{
		_hour = timezone_HH;
		_mins = timezone_MM;
	}
	void set_time(const uint8_t &_hour, const uint8_t &_mins,
		const uint8_t &_DD, const uint8_t &_MM, const uint8_t &_YY,
		const uint8_t _daylight_savings = 0)
	{
		// get the current time
		t_mm = _mins;
		t_hh = _hour;
		t_DD = _DD;
		t_MM = _MM;
		t_YY = _YY;

		addTimezone<volatile uint8_t>(t_hh, t_mm, t_ss, t_DD, t_MM, t_YY, timezone_HH, timezone_MM, 0);

		// set the correct frame bits
		set_time(_daylight_savings);
	}
	void add_time(const uint8_t &_hour, const uint8_t &_mins, const uint8_t _secs = 0)
	{
		// get the current time
		get_time(t_hh, t_mm, t_DD, t_MM, t_YY);

		//increment by 1 minute (last 3 digits specify increment in : hour, min, sec)
		addTimezone<volatile uint8_t>(t_hh, t_mm, t_ss, t_DD, t_MM, t_YY, _hour, _mins, _secs);

		// set the correct frame bits
		set_time();
	}
	void print()
	{
		print_datetime(hour_, mins_, DD_, MM_, YY_);
	}
#if( REQUIRE_TIMEDATESTRING == 1)
	void set_time(char dateString[], char timeString[], const uint8_t _daylight_savings = 0)
	{

		uint8_t _hour, _mins, _secs, _DD, _MM, _YY;

		DateString_to_DDMMYY(dateString, _DD, _MM, _YY);
		TimeString_to_HHMMSS(timeString, _hour, _mins, _secs);

		set_time(_hour, _mins, _DD, _MM, _YY, _daylight_savings);
	}
#endif
private:
	/// set
	// this function performs no range-checking of variables
	void set_time(const uint8_t _daylight_savings = 0)
	{
		bool _is_leap_year = is_leap_year(2000 + t_YY);
		uint16_t _doty = to_day_of_the_year<volatile uint8_t>(t_DD, t_MM, _is_leap_year);

		// Note: these set commands are conditional on a difference
		// between the set value and the internal (saved) state
		set_secs(t_ss);
		set_mins(t_mm);
		set_hour(t_hh);

		set_day(t_DD);
		set_month(t_MM);

		set_doty(_doty);
		set_dut1(); // only need to do this once at setup as we set dut1 to all zeros
		set_year(t_YY);

		set_misc(_is_leap_year, _daylight_savings);
	}
	void set_secs(uint8_t _secs)
	{
		if (secs_ != _secs)
		{
			secs_ = _secs;
		}
	}
	void set_mins(uint8_t _mins)
	{
		if (mins_ != _mins)
		{
			mins_ = _mins;

			// set MINS
			//                  0   1   2   3   4   5   6   7   8   9
			//                  M  40  20  10   0   8   4   2   1   M
			if (_mins >= 40) { _mins -= 40; MINS[1] = 1; }
			else { MINS[1] = 0; }
			if (_mins >= 20) { _mins -= 20; MINS[2] = 1; }
			else { MINS[2] = 0; }
			if (_mins >= 10) { _mins -= 10; MINS[3] = 1; }
			else { MINS[3] = 0; }
			if (_mins >= 8) { _mins -= 8; MINS[5] = 1; }
			else { MINS[5] = 0; }
			if (_mins >= 4) { _mins -= 4; MINS[6] = 1; }
			else { MINS[6] = 0; }
			if (_mins >= 2) { _mins -= 2; MINS[7] = 1; }
			else { MINS[7] = 0; }
			MINS[8] = (_mins & 0x01);
		}
	}
	void set_hour(uint8_t _hour)
	{
		if (hour_ != _hour)
		{
			hour_ = _hour;

			// set HOUR
			//                  0   1   2   3   4   5   6   7   8   9
			//                  -   -  20  10   0   8   4   2   1   M
			if (_hour >= 20) { _hour -= 20; HOUR[2] = 1; }
			else { HOUR[2] = 0; }
			if (_hour >= 10) { _hour -= 10; HOUR[3] = 1; }
			else { HOUR[3] = 0; }
			if (_hour >= 8) { _hour -= 8; HOUR[5] = 1; }
			else { HOUR[5] = 0; }
			if (_hour >= 4) { _hour -= 4; HOUR[6] = 1; }
			else { HOUR[6] = 0; }
			if (_hour >= 2) { _hour -= 2; HOUR[7] = 1; }
			else { HOUR[7] = 0; }
			HOUR[8] = (_hour & 0x01);
		}
	}
	void set_doty(uint16_t _doty)
	{
		if (doty_ != _doty)
		{
			doty_ = _doty;

			// DOTY             0   1   2   3   4   5   6   7   8   9
			//                  -   - 200 100   0  80  40  20  10   M
			if (_doty >= 200) { _doty -= 200; DOTY[2] = 1; }
			else { DOTY[2] = 0; }
			if (_doty >= 100) { _doty -= 100; DOTY[3] = 1; }
			else { DOTY[3] = 0; }
			if (_doty >= 80) { _doty -= 80; DOTY[5] = 1; }
			else { DOTY[5] = 0; }
			if (_doty >= 40) { _doty -= 40; DOTY[6] = 1; }
			else { DOTY[6] = 0; }
			if (_doty >= 20) { _doty -= 20; DOTY[7] = 1; }
			else { DOTY[7] = 0; }
			if (_doty >= 10) { _doty -= 10; DOTY[8] = 1; }
			else { DOTY[8] = 0; }

			if (_doty >= 8) { _doty -= 8; DUT1[0] = 1; }
			else { DUT1[0] = 0; }
			if (_doty >= 4) { _doty -= 4; DUT1[1] = 1; }
			else { DUT1[1] = 0; }
			if (_doty >= 2) { _doty -= 2; DUT1[2] = 1; }
			else { DUT1[2] = 0; }
			DUT1[3] = (_doty & 0x01);
		}
	}
	void set_dut1()
	{
		// DUT1             0   1   2   3   4   5   6   7   8   9
		//                  8   4   2   1   -   -  (+) (-) (+)  M
		DUT1[6] = 1; // set sign to +ve
		DUT1[8] = 1; // set sign to +ve


		// clear the first 4 values
		YEAR[0] = 0;
		YEAR[1] = 0;
		YEAR[2] = 0;
		YEAR[3] = 0;
		// Note: +ve, -ve makes not difference because the DUT1 value
		// (resides in YEAR) is set to zero on the next line
	}
	void set_day(uint8_t _DD)
	{
		if (DD_ != _DD)
		{
			DD_ = _DD;
		}
	}
	void set_month(uint8_t _MM)
	{
		if (MM_ != _MM)
		{
			MM_ = _MM;
		}
	}
	void set_year(uint8_t _year)
	{
		if (YY_ != _year)
		{
			YY_ = _year;

			// YEAR             0   1   2   3   4   5   6   7   8   9
			//                0.8 0.4 0.2 0.1   -  80  40  20  10   M
			// MISC             0   1   2   3   4   5   6   7   8   9
			//                  8   4   2   1   - LYI LSW   2   1   M
			if (_year >= 80) { _year -= 80; YEAR[5] = 1; }
			else { YEAR[5] = 0; }
			if (_year >= 40) { _year -= 40; YEAR[6] = 1; }
			else { YEAR[6] = 0; }
			if (_year >= 20) { _year -= 20; YEAR[7] = 1; }
			else { YEAR[7] = 0; }
			if (_year >= 10) { _year -= 10; YEAR[8] = 1; }
			else { YEAR[8] = 0; }

			if (_year >= 8) { _year -= 8; MISC[0] = 1; }
			else { MISC[0] = 0; }
			if (_year >= 4) { _year -= 4; MISC[1] = 1; }
			else { MISC[1] = 0; }
			if (_year >= 2) { _year -= 2; MISC[2] = 1; }
			else { MISC[2] = 0; }
			MISC[3] = (_year & 0x01);
		}
	}
	void set_misc(const bool &_is_leap_year, const uint8_t &_daylight_savings)
	{
		// set leap year, leap second and daylight saving time info
		//                  0   1   2   3   4   5   6   7   8   9
		//                  8   4   2   1   - LYI LSW   2   1   M
		if (is_leap_year_ != _is_leap_year)
		{
			is_leap_year_ = _is_leap_year;

			MISC[5] = _is_leap_year;
		}
		MISC[6] = 0; // Ignore leap second
		if (daylight_savings_ != _daylight_savings)
		{
			daylight_savings_ = _daylight_savings;

			MISC[7] = (_daylight_savings >> 1) & 0x1;
			MISC[8] = _daylight_savings & 0x1;
		}

	}
	/// get
	uint8_t get_mins()
	{
		// MINS
		// 0   1   2   3   4   5   6   7   8   9
		// M  40  20  10   0   8   4   2   1   M
		uint8_t _temp = MINS[1] * 40 + MINS[2] * 20 + MINS[3] * 10 + MINS[4] * 0;
		_temp += MINS[5] * 8 + MINS[6] * 4 + MINS[7] * 2 + MINS[8] * 1;
		return _temp;
	}
	uint8_t get_hour()
	{
		// HOUR
		// 0   1   2   3   4   5   6   7   8   9
		// -   -  20  10   0   8   4   2   1   M
		uint8_t _temp = HOUR[2] * 20 + HOUR[3] * 10 + HOUR[4] * 0;
		_temp += HOUR[5] * 8 + HOUR[6] * 4 + HOUR[7] * 2 + HOUR[8] * 1;
		return _temp;
	}
	uint16_t get_doty()
	{
		// DOTY
		// 0   1   2   3   4   5   6   7   8   9
		// -   - 200 100   0  80  40  20  10   M
		// DUT1
		// 0   1   2   3   4   5   6   7   8   9
		// 8   4   2   1   -   -   +   -   +   M

		uint16_t _temp = DOTY[2] * 200 + DOTY[3] * 100 + DOTY[4] * 0;
		_temp += DOTY[5] * 80 + DOTY[6] * 40 + DOTY[7] * 20 + DOTY[8] * 10;
		_temp += DUT1[0] * 8 + DUT1[1] * 4 + DUT1[2] * 2 + DUT1[3] * 1;

		return _temp;
	}
	float get_dut1()
	{
		// DUT1
		// 0   1   2   3   4   5   6   7   8   9
		// 8   4   2   1   -   -   +   -   +   M
		// YEAR
		//  0   1   2   3   4   5   6   7   8   9
		// 0.8 0.4 0.2 0.1  -  80  40  20  10   M
		float _temp = YEAR[0] * 0.8f + YEAR[1] * 0.4f + YEAR[2] * 0.2f + YEAR[3] * 0.1f;

		if (DUT1[7])
		{
			_temp = -_temp;
		}
		else if (!(DUT1[6] & DUT1[8]))
		{
			_temp = -999; // ERROR
		}

		return _temp;
	}

	uint8_t get_year()
	{
		// YEAR
		//  0   1   2   3   4   5   6   7   8   9
		// 0.8 0.4 0.2 0.1  -  80  40  20  10   M
		// MISC
		// 0   1   2   3   4   5   6   7   8   9
		// 8   4   2   1   - LYI LSW   2   1   M

		uint8_t _temp = YEAR[5] * 80 + YEAR[6] * 40 + YEAR[7] * 20 + YEAR[8] * 10;
		_temp += MISC[0] * 8 + MISC[1] * 4 + MISC[2] * 2 + MISC[3] * 1;
		return _temp;
	}
	void get_misc(bool &_is_leap_year, bool &_is_leap_second, uint8_t &_daylight_savings)
	{
		// MISC
		// 0   1   2   3   4   5   6   7   8   9
		// 8   4   2   1   - LYI LSW   2   1   M
		_is_leap_year = MISC[5];
		_is_leap_second = MISC[6];
		_daylight_savings = ((MISC[7] << 1) & 0x2) | (MISC[8] & 0x01);
	}
	void set_lowTime()
	{
		// Check for a frame marker
		switch (frame_index)
		{
			// markers
		case 0:  // start frame
		case 9:  // end MINS
		case 19: // end HOUR
		case 29: // end DOTY
		case 39: // end DUT1
		case 49: // end YEAR
		case 59: // end MISC, end frame
			WWVB_LOWTIME = WWVB_MARKER;
			return;
		}

		// No frame marker, so set the subframe bit data
		subframe_index = frame_index % 10;
		if (frame_index < 10) // MINS
		{
			WWVB_LOWTIME = pulse_width[MINS[subframe_index]];
		}
		else if (frame_index < 20) // HOUR
		{
			WWVB_LOWTIME = pulse_width[HOUR[subframe_index]];
		}
		else if (frame_index < 30) // DOTY
		{
			WWVB_LOWTIME = pulse_width[DOTY[subframe_index]];
		}
		else if (frame_index < 40) // DUT1
		{
			WWVB_LOWTIME = pulse_width[DUT1[subframe_index]];
		}
		else if (frame_index < 50) // YEAR
		{
			WWVB_LOWTIME = pulse_width[YEAR[subframe_index]];
		}
		else if (frame_index < 60) // MISC
		{
			WWVB_LOWTIME = pulse_width[MISC[subframe_index]];
		}
	}
};

#endif
