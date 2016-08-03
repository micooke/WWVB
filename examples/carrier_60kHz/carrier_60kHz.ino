//ATtiny85
//                          +-\/-+
//                 RST PB5 1|*   |8 VCC
//                     PB3 2|    |7 PB2
// 60kHz (USE_OC1B) <= PB4 3|    |6 PB1 => (USE_OC1A) 60kHz
//                     GND 4|    |5 PB0
//                          +----+

#define USE_OC1B

volatile uint8_t isr_count = 0;

// Set PWM to 60kHz (8MHz / (132 + 1)) = 60150Hz
// 132 resulted in 63492Hz
// 133 resulted in 62972Hz
// A OCR1C with an additional 5.72 was calculated to produce a 60kHz signal
// Well that failed..
// 137 resulted in 63492Hz
// 138 resulted in 62972Hz
// 139 resulted in 60369Hz
// 140 resulted in 59884Hz

const uint8_t PWM_60kHz[2] = { 139, 140 };

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
	/*
	// First (failed) attempt
	case 0:
	case 1:
		OCR1C = 132;
		++isr_count;
		break;
	case 2:
		OCR1C = 133;
		isr_count = 0;
		break;
	*/
	// Second/Third/Forth attempt
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
	}
}

void setup()
{
	// Generate 60kHz carrier
	// OC0A | !OC1A : 0 PB0 MOSI PWM 8b
	// OC1A |  OC0B : 1 PB1 MISO PWM 8b
	//         OC1B : 4 PB4
	//        !OC1B : 3 PB3
	PLLCSR = _BV(PCKE) | _BV(PLLE); // enable 64MHz clock
	
	// Note : I tried using the internal 16MHz or 8MHz clock, but it didnt work
	//        This is what is in the wwvb.h, hence wwvb.h doesnt work for attiny chips anymore :(
	//        I assume that the arduino framework for the attinys stuffs this, likely due to the delay routine
	//
	TCCR1 = _BV(CS12); 		// Internal 64MHz PCK, prescalar = 8 (64MHz/8 = 8MHz)

	OCR1C = PWM_60kHz[0]; 	// Set PWM to 60kHz (8MHz / (132 + 1)) = 60150Hz

#if defined(USE_OC1A)
	pinMode(PB1, OUTPUT);	// setup OC1A PWM pin as output

	TCCR1 |= _BV(PWM1A)  	// Clear timer/counter after compare match to OCR1C
		| _BV(COM1A1); 		// Clear the OC1A output line after match

	OCR1A = 66;				// ~50% duty cycle
#elif defined(USE_OC1B)
	pinMode(PB4, OUTPUT);	// setup OC1B PWM pin as output

	GTCCR = _BV(PWM1B) 		// Clear timer/counter after compare match to OCR1C
		| _BV(COM1B1);		// Clear the OC1B output line after match

	OCR1B = 66;				// ~50% duty cycle
#endif

	// Enable Timer1 overflow interrupt
	TIMSK = _BV(TOIE1);	// enable overflow interrupt on Timer1

	// enable interrupts
	sei();

	// Start transmission (Set the clock prescalar)
	TCCR1 |= _BV(CS12); // Set clock prescalar to 8 (64MHz / 8 = 8MHz)
}

void loop() {}