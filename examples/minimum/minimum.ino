/*
Minimum.ino
Set the wwvb time to the compile time.
The wwvb interrupt routine auto-increments the minute at the end of each frame

Arduino Nano
       +----+=====+----+
       |    | USB |    |
SCK B5 |D13 +-----+D12 | B4 MISO
       |3V3        D11~| B3 MOSI
       |Vref       D10~| B2 SS
    C0 |A0          D9~| B1 |=> WWVB ANTENNA
    C1 |A1          D8 | B0 
    C2 |A2          D7 | D7 
    C3 |A3          D6~| D6
SDA C4 |A4          D5~| D5
SCL C5 |A5          D4 | D4
       |A6          D3~| D3 INT1
       |A7          D2 | D2 INT0
       |5V         GND |
    C6 |RST        RST | C6
       |GND        TX1 | D0
       |Vin        RX1 | D1
       |  5V MOSI GND  |
       |   [] [] []    |
       |   [] [] []    |
       | MISO SCK RST  |
       +---------------+
	   
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
#define _DEBUG 1
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

   // set the timezone before you set your time
   // if you are using CST (UTC - 6:00), set the timezone to 6,0 (below)
   wwvb_tx.setTimezone(6,0);
   wwvb_tx.setPWM_LOW(0);
   
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
   }
   else
   {
      /*
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
      */
   }
   #endif
}