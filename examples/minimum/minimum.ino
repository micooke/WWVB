/*
ATtiny85

                       +-\/-+
                  RST 1|*   |8 VCC
                  PB3 2|    |7 PB2
 WWVB ANTENNA =>| PB4 3|    |6 PB1
                  GND 4|    |5 PB0
                       +----+
OC1A : PB1
OC1B : PB4

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
#include <Arduino.h>
#include <avr/interrupt.h>

#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
#define USE_OC1B
#define _DEBUG 0
#else
#define USE_OC1A
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

struct counter
{
  uint8_t loop : 3;
  uint8_t interrupt;
};

volatile counter count;

// The ISR sets the PWM pulse width to correspond with the WWVB bit
#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
#if defined(USE_OC1A)
ISR(TIMER1_COMPA_vect)
#else
ISR(TIMER1_COMPB_vect)
#endif
#else
ISR(TIMER1_OVF_vect)
#endif
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
	digitalWrite(LED_PIN, LOW);
	delay(100);
	digitalWrite(LED_PIN, HIGH);
	delay(100);
  #endif
  
  #if (_DEBUG == 2)
  // count.loop is 3 bits, hence overflows at 7->0
  if(++(count.loop) == 0)
  {
    wwvb_tx.debug_time();
    Serial.println();
  }
  #endif
}
