/*
Minimum.ino
Set the wwvb time to the compile time.
The wwvb interrupt routine auto-increments the minute at the end of each frame 

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
*/

#include <Arduino.h>
#include <avr/interrupt.h>

#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
#define _DEBUG 0
#else
#define _DEBUG 2
#endif

/*
_DEBUG == 0: Set wwvb time to the compile time, use the wwvb interrupt, dont blink the led
_DEBUG == 1: Set wwvb time to the compile time, use the wwvb interrupt, blink the led
_DEBUG == 2: Set wwvb time to the compile time, use the wwvb interrupt, blink the led, serial output
*/
#if (_DEBUG > 0)
#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
unsigned char LED_PIN = 1; // Use PB1 the Tx LED on the digistump
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
unsigned char LED_PIN = SS;
#elif defined(__AVR_ATmega168__) | defined(__AVR_ATmega168P__) | defined(__AVR_ATmega328P__)
unsigned char LED_PIN = 13;
#endif
boolean LED_TOGGLE = false;
#endif

#define REQUIRE_TIMEDATESTRING 1

#include <TimeDateTools.h> // include before wwvb.h AND/OR ATtinyGPS.h
#include <wwvb.h> // include before ATtinyGPS.h
wwvb wwvb_tx;

// The ISR sets the PWM pulse width to correspond with the WWVB bit
ISR(TIMER1_OVF_vect)
{
   cli(); // disable interrupts
   
   wwvb_tx.interrupt_routine();
   
   sei(); // enable interrupts
}

void setup()
{
   wwvb_tx.setup();

   #if (REQUIRE_TIMEDATESTRING == 1)
   wwvb_tx.set_time(__DATE__, __TIME__);
   #endif
   
   #if (_DEBUG == 2)
   Serial.begin(9600);
   while(!Serial);
   
   Serial.print("wwvb set to: ");
   Serial.print(__TIME__);
   Serial.print(" on ");
   Serial.println(__DATE__);
   
   wwvb_tx.debug_time();
   Serial.println();
   #endif
   
   #if (_DEBUG > 0)
   pinMode(LED_PIN, OUTPUT);
   digitalWrite(LED_PIN, HIGH);
   #endif

   wwvb_tx.start(); // Thats it
}
void loop()
{
   #if (_DEBUG > 0)
   // debug outputs at the end of each 1minute frame
   if(wwvb_tx.end_of_frame == true)
   {
      wwvb_tx.end_of_frame = false; // clear the EOF bit
      #if (_DEBUG == 2)
	  wwvb_tx.debug_time();
      Serial.println();
      #endif
      
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
	}
   #endif
}