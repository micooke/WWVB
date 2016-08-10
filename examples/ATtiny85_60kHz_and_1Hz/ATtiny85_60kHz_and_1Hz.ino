//ATtiny85
//                        +-\/-+
//               RST PB5 1|*   |8 VCC
// 1Hz modulation <= PB3 2|    |7 PB2
// 60kHz carrier  <= PB4 3|    |6 PB1
//                   GND 4|    |5 PB0
//                        +----+

volatile uint8_t isr_count = 0;
#define DIVIDER_60kHz_TO_1Hz 60000
volatile uint16_t _60kHz_to_1Hz = DIVIDER_60kHz_TO_1Hz;
volatile bool mod_state = false;

#if defined(__AVR_ATtinyX5__)
#define PERIOD_REG OCR1C
#define MODULATION_PIN PB3
#define CARRIER_PIN PB4

#define USE_OC1B
#if defined(USE_OC1A)
#define PULSEWIDTH_REG OCR1A
#elif defined(USE_OC1B)
#define PULSEWIDTH_REG OCR1B
#endif
#else
#define PERIOD_REG ISR1
#endif

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
		PERIOD_REG = PWM_60kHz[0];
		++isr_count;
		break;
	case 2:
		PERIOD_REG = PWM_60kHz[1];
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
		if (mod_state == LOW)
		{
			pinMode(MODULATION_PIN, OUTPUT);
			digitalWrite(MODULATION_PIN, LOW);
		}
		else
		{
			pinMode(MODULATION_PIN, INPUT);
			digitalWrite(MODULATION_PIN, LOW);
		}
		mod_state = !mod_state;
		_60kHz_to_1Hz = DIVIDER_60kHz_TO_1Hz;
		break;
	default:
		--_60kHz_to_1Hz;
		break;
	}
}

void set8bitPWM(const uint16_t &FrequencyHz, const uint8_t Timer)
{
#if defined(__AVR_ATtinyX5__)
	uint32_t base_clock = 64000000;
#else
	uint32_t base_clock = F_CPU; 
#endif

	uint16_t prescalar[5] = { 1, 8, 64, 256, 1024 };
	uint16_t i = 0;
	uint32_t FrequencyCount = base_clock / FrequencyHz;
	
	// find the right prescalar
	while ((FrequencyCount > (base_clock / (prescalar[i] * 255))) & (i < 5))
	{
		++i;
	}

}

void setup()
{
	// set the modulation and carrier pins as outputs
	pinMode(CARRIER_PIN, OUTPUT);
	pinMode(MODULATION_PIN, OUTPUT);
	digitalWrite(MODULATION_PIN, LOW);
	
	PERIOD_REG = PWM_60kHz[0]; 	// Set PWM to 60kHz (8MHz / (132 + 1)) = 60150Hz
	PULSEWIDTH_REG = PERIOD_REG / 2;

#if defined(__AVR_ATtinyX5__)
	PLLCSR = _BV(PCKE) | _BV(PLLE);	// enable 64MHz clock
	
#if defined(USE_OC1A)
	TCCR1 = _BV(PWM1A)  			// Clear timer/counter after compare match to OCR1C
		| _BV(COM1A1); 				// Clear the OC1A output line after match
#elif defined(USE_OC1B)
	GTCCR = _BV(PWM1B) 				// Clear timer/counter after compare match to OCR1C
		| _BV(COM1B1);				// Clear the OC1B output line after match
#endif

	// Enable Timer1 overflow interrupt
	TIMSK |= _BV(TOIE1);
#else

#endif

	// enable interrupts
	sei();

	// Start transmission (Set the clock prescalar)
#if defined(__AVR_ATtinyX5__)
	TCCR1 |= _BV(CS12); // Set clock to PCK/8 (64MHz / 8 = 8MHz)
#else
	// Set clock prescalar to 1 (16MHz or 8MHz - as per the base clock)
	//TCCR1B |= _BV(CS10);
#endif
}

void loop() {}