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
#define _DEBUG 0
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
  
#ifdef LED_BUILTIN
  #undef LED_BUILTIN
#endif
#if defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
#define LED_BUILTIN 2
#else
#define LED_BUILTIN LED_PIN
#endif

uint32_t tZero = 0;

#define REQUIRE_TIMEDATESTRING 1

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;

// The ISR sets the PWM pulse width to correspond with the WWVB bit


#if (F_CPU < 16000000)
ISR(TIMER1_OVF_vect) {}
#else
ISR(TIMER1_OVF_vect) { wwvb_tx.interrupt_routine(); }
#endif

// Setup your timezones here
//const int8_t local_timezone[2] = { 10, 30 }; // This is your local timezone : ACDT (UTC +10:30)
const int8_t wwvb_timezone[2] = { 0, 0 }; // This is the timezone of your wwvb clock : CST (UTC -6:00)

void setup()
{
	wwvb_tx.setup();

	// Note: set the timezone before you set your time

	// set the timezone to your wwvb timezone (the negative is supposed to be here)
	wwvb_tx.setTimezone(-wwvb_timezone[0], -wwvb_timezone[1]);

	wwvb_tx.setPWM_LOW(0); // sets the pulsewidth for the wwvb 'low' signal : 0 - 133  = 0 - 100% for 16MHz
/*
#if (REQUIRE_TIMEDATESTRING == 1)
	wwvb_tx.set_time(__DATE__, __TIME__);
#endif
*/
  wwvb_tx.set_time(8,43,7,2,05);

#if (_DEBUG > 0)
	Serial.begin(9600);
	#if defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
	while (!Serial); // If using a leonardo/micro, wait for the Serial connection
	#endif
	/*
	Serial.print("wwvb set to: ");
	Serial.print(__TIME__);
	Serial.print(" on ");
	Serial.println(__DATE__);
	*/
	wwvb_tx.debug_time();
	Serial.println();
#endif

  tZero = millis();
  #if defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
  while (millis() - tZero < 3741);
  #else
  while (millis() - tZero < 1000);
  #endif
  
	pinMode(LED_PIN, OUTPUT);
  
	wwvb_tx.start(); // Thats it
#if (_DEBUG > 0)
	Serial.println("wwvb_tx started");
#endif
}
void loop()
{
	#if (F_CPU < 16000000)
	wwvb_tx.interrupt_routine();
	#endif
}
