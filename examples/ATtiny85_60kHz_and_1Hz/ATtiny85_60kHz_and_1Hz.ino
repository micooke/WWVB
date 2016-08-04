//ATtiny85
//                        +-\/-+
//               RST PB5 1|*   |8 VCC
// 1Hz modulation <= PB3 2|    |7 PB2
// 60kHz carrier  <= PB4 3|    |6 PB1
//                   GND 4|    |5 PB0
//                        +----+

volatile uint8_t isr_count = 0;
#define DIVIDER_60kHz_TO_1Hz 30000
volatile uint16_t _60kHz_to_1Hz = DIVIDER_60kHz_TO_1Hz;
volatile bool mod_state = false;
volatile uint8_t mod_pin = PB3;

// Set PWM to 60kHz (8MHz / (132 + 1)) = 60150Hz
// 132 resulted in 63492Hz
// 133 resulted in 62972Hz
// A OCR1C with an additional 5.72 was calculated to produce a 60kHz signal
// Well that failed..
// 137 resulted in 63492Hz
// 138 resulted in 62972Hz
// 139 resulted in 60369Hz
// 140 resulted in 59884Hz

const uint8_t PWM_60kHz[2] = { 132, 133 };
//const uint8_t PWM_60kHz[2] = { 139, 140 }; // Tuned

ISR(TIMER1_OVF_vect)
{
	// Freq = 8MHz / (OCR1C + 1)
	// isr_count == 0 : OCR1C = 132 -> 60150.38Hz (60kHz + 150.38Hz)
	// isr_count == 1 : OCR1C = 132 -> 60150.38Hz (60kHz + 150.38Hz)
	// isr_count == 2 : OCR1C = 133 -> 59701.49Hz (60kHz - 298.51Hz)
	//
	// Average frequency after 3 pulses = 60000.75Hz
	// 3 periods = 3/60000 = 50us
	//
	switch (isr_count)
	{
	// First (failed) attempt
	case 0:
	case 1:
		OCR1C = PWM_60kHz[0];
		++isr_count;
		break;
	case 2:
		OCR1C = PWM_60kHz[1];
		isr_count = 0;
		break;
	
	// Second/Third/Forth attempt
	/*
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		OCR1C = PWM_60kHz[1];
		++isr_count;
		break;
	case 8:
	case 9:
		OCR1C = PWM_60kHz[0];
		isr_count = 0;
		break;
	*/
	}

	// Divide down the clock to 10Hz
	switch (_60kHz_to_1Hz)
	{
	case 0:
		digitalWrite(mod_pin, mod_state);
		mod_state = !mod_state;
		_60kHz_to_1Hz = DIVIDER_60kHz_TO_1Hz;
		break;
	default:
		--_60kHz_to_1Hz;
		break;
	}
}

void setup()
{
	// set the modulation pin as output
	pinMode(mod_pin, OUTPUT);

	// Generate 60kHz carrier
	// OC0A | !OC1A : 0 PB0 MOSI PWM 8b
	// OC1A |  OC0B : 1 PB1 MISO PWM 8b
	//         OC1B : 4 PB4
	//        !OC1B : 3 PB3
	PLLCSR = _BV(PCKE) | _BV(PLLE); // enable 64MHz clock
	
	/// TIMER1
	// Note : I tried using the internal 16MHz or 8MHz clock, but it didnt work
	//        This is what is in the wwvb.h, hence wwvb.h doesnt work for attiny chips anymore :(
	//        I assume that the arduino framework for the attinys stuffs this, likely due to the delay routine
	//
	// Note2 : Using the system clock changes the PWM mode, which uses 255 as the TOP value
	//
	OCR1C = PWM_60kHz[0]; 	// Set PWM to 60kHz (8MHz / (132 + 1)) = 60150Hz
	TCCR1 = 0;

	pinMode(PB4, OUTPUT);	// setup OC1B PWM pin as output

	GTCCR = _BV(PWM1B) 		// Clear timer/counter after compare match to OCR1C
		| _BV(COM1B1);		// Clear the OC1B output line after match

	OCR1B = 66;				// ~50% duty cycle

	// Enable Timer1 overflow interrupt
	TIMSK |= _BV(TOIE1);

	// enable interrupts
	sei();

	// Start transmission (Set the clock prescalar)
	TCCR1 |= _BV(CS12); // Set clock to PCK/8 (64MHz / 8 = 8MHz)
}

void loop() {}