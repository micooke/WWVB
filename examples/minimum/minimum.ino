/*
ATtiny85

                       +-\/-+
                  RST 1|*   |8 VCC
                  PB3 2|    |7 PB2
 WWVB ANTENNA =>| PB4 3|    |6 PB1
                  GND 4|    |5 PB0
                       +----+
*/

#include <avr/interrupt.h>

#define REQUIRE_TIMEDATESTRING 1
#define USE_OC1A

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;
volatile boolean toggle = false;
volatile uint8_t count = 0;

unsigned char LED_PIN = 13;
boolean LED_TOGGLE = false;

uint32_t t0 = 0;

void fcn_call()
{
    if (toggle)
    {
      OCR1A = 65;
    }
    else
    {
      OCR1A = 6;
    }
    toggle = !toggle;
}

// The ISR sets the PWM pulse width to correspond with the WWVB bit
ISR(TIMER1_OVF_vect)
{
	cli(); // disable interrupts
  if (++count == 60)
  {
    count = 0;
  	fcn_call();
  }
	
	//wwvb_tx.interrupt_routine();
	sei(); // enable interrupts
}

void setup()
{
	wwvb_tx.setup();

	// Note: The default is to include DateString & TimeString tools (see wwvb.h)
	wwvb_tx.set_time(__DATE__, __TIME__);
	wwvb_tx.start(); // Thats it

	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN,HIGH);
}
void loop()
{
	digitalWrite(LED_PIN, LOW);
	delay(100);
	digitalWrite(LED_PIN, HIGH);
	delay(100);
}
