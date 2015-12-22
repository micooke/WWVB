/*
ATtiny85

                       +-\/-+
                  RST 1|*   |8 VCC
                  PB3 2|    |7 PB2
 WWVB ANTENNA =>| PB4 3|    |6 PB1
                  GND 4|    |5 PB0
                       +----+
*/
#define REQUIRE_TIMEDATESTRING 1
#define USE_OC1A

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;

// The ISR sets the PWM pulse width to correspond with the WWVB bit
#if defined(USE_OC1A)
ISR(TIMER1_COMPA_vect)
#elif defined(USE_OC1B)
ISR(TIMER1_COMPB_vect)
#endif
{
	cli(); // disable interrupts
	wwvb_tx.interrupt_routine();
	sei(); // enable interrupts
}

void setup()
{
	wwvb_tx.setup();

	// Note: The default is to include DateString & TimeString tools (see wwvb.h)
	wwvb_tx.set_time(__DATE__, __TIME__);
	wwvb_tx.start(); // Thats it
}
void loop()
{
}