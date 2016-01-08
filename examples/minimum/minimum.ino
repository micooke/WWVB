/*
ATtiny85

                       +-\/-+
                  RST 1|*   |8 VCC
                  PB3 2|    |7 PB2
 WWVB ANTENNA =>| PB4 3|    |6 PB1
                  GND 4|    |5 PB0
                       +----+

Recommended debug setup
Use a RC (lowpass) filter to view the message (p1) as well as the modulated carrier (p0)

T = RC = 1/(2*pi*fc)
where;
T: Time constant
fc: cutoff frequency

for: R = 110 ohm, C = 1uF
fc = 1/(2*pi*RC) = 1.45kHz (the carrier is 60kHz, the modulation is about 1 to 5 Hz) 

PB4 |--(p0)--[110R]--(p1)--[1uF]--|GND
*/

#include <avr/interrupt.h>

#define REQUIRE_TIMEDATESTRING 1

#define USE_OC1A // For the nano. Comment out if using with the ATtiny85

/*
 _DEBUG == 0: Set wwvb time to the compile time, use the wwvb interrupt, dont blink the led
 _DEBUG == 1: Set wwvb time to the compile time, use the wwvb interrupt, blink the led
 _DEBUG == 2: Set wwvb time to the compile time, use a dummy interrupt, blink the led
 */
 
#define _DEBUG 1

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;

struct counter
{
  uint8_t loop : 3;
  uint8_t interrupt;
};

volatile counter count;

#if (_DEBUG > 0)
unsigned char LED_PIN = 13;
boolean LED_TOGGLE = false;
#endif

#if (_DEBUG == 2)
volatile boolean toggle = false;

void dummy_interrupt()
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

#endif

// The ISR sets the PWM pulse width to correspond with the WWVB bit
ISR(TIMER1_OVF_vect)
{
	cli(); // disable interrupts
	
	#if (_DEBUG == 2)
  if ((++count.interrupt) == 60)
	{
		count.interrupt = 0;
		dummy_interrupt();
	}
  #else
	wwvb_tx.interrupt_routine();
	#endif
	
	sei(); // enable interrupts
}

void setup()
{
	wwvb_tx.setup();

	// Note: The default is to include DateString & TimeString tools (see wwvb.h)
	wwvb_tx.set_time(__DATE__, __TIME__);
	wwvb_tx.start(); // Thats it

  #if (_DEBUG > 0)
  Serial.begin(9600);
  while(!Serial);
  Serial.print("wwvb set to: ");
  Serial.print(__TIME__);
  Serial.print(" on ");
  Serial.println(__DATE__);
	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, HIGH);
  #endif
}
void loop()
{
  #if (_DEBUG > 0)
	digitalWrite(LED_PIN, LOW);
	delay(100);
	digitalWrite(LED_PIN, HIGH);
	delay(100);
  // count.loop is 3 bits, hence overflows at 7->0
  if(++(count.loop) == 0)
  {
    wwvb_tx.print_time();
    Serial.println();
  }

  if (wwvb_tx.is_incremented)
  {
    wwvb_tx.is_incremented = false;
    Serial.println(F("ADD_TIME ################################################"));
    Serial.print(F("Mins: "));
    Serial.println(wwvb_tx.mins_);
  }
  #endif
}
