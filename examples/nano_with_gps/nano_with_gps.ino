#include <Arduino.h>

/*
Arduino Nano
       +----+=====+----+
       |    | USB |    |
SCK B5 |D13 +-----+D12 | B4 MISO
       |3V3        D11~| B3 MOSI
       |Vref       D10~| B2 SS
    C0 |A0          D9~| B1 |=> WWVB ANTENNA
    C1 |A1          D8 | B0 |=> GPS Rx (Software Serial)
    C2 |A2          D7 | D7 |<= GPS Tx (Software Serial)
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


#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__) | defined(__AVR_ATtiny85__)
#define _DEBUG 0
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
#define _DEBUG 0
#else
#define _DEBUG 1
#endif

/*
_DEBUG == 0: Set wwvb time to the compile time, use the wwvb interrupt, dont blink the led
_DEBUG == 1: Set wwvb time to the compile time, use the wwvb interrupt, blink the led, serial output
_DEBUG == 2: Set wwvb time to the compile time, use the wwvb interrupt, blink the led, VERBOSE serial output
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

#include <SoftwareSerial.h>
SoftwareSerial ttl(7, 8); // Tx pin (8) not used in this example

#include <ATtinyGPS.h>
ATtinyGPS gps;

uint8_t count = 0;

// debug variables
uint32_t t1;
boolean isLow = false;

void setup()
{
   wwvb_tx.setup();
   
   // set the timezone before you set your time
   // if you are using CST (UTC - 6:00), set the timezone to -6,0 (below)
   wwvb_tx.setTimezone(-6,0);
   wwvb_tx.setPWM_LOW(0);
   
   ttl.begin(9600);
   
   #if (_DEBUG > 0)
   Serial.begin(9600);
   while(!Serial);

   pinMode(LED_PIN, OUTPUT);
   #endif
}

void loop()
{
   // if we are receiving gps data, parse it
   while (ttl.available())
   {
      char c = ttl.read();
      gps.parse(c);
      
      #if (_DEBUG == 2)
      // Output raw NMEA string
      Serial.print(c);
      #endif
   }

   #if (_DEBUG == 2)
   if (gps.new_data())
   {
      switch(gps.ss)
      {
         case 0:
         case 10:
         case 20:
         case 30:
         case 40:
         case 50:
            Serial.print("##### ");
            Serial.print(gps.ss);
            Serial.println("s GPS status #####");
            Serial.print("IsValid    : "); Serial.println(gps.IsValid);
            Serial.print("Quality    : "); Serial.println(gps.quality);
            Serial.print("Satellites : "); Serial.println(gps.satellites);
            Serial.print("Date/Time  : "); print_datetime(gps.mm,gps.hh,gps.DD,gps.MM,gps.YY);
            Serial.print("DD/MM/YY: "); Serial.print(gps.DD); Serial.print("/"); Serial.print(gps.MM); Serial.print("/"); Serial.println(gps.YY); 
      }
   }   
   #endif
   // set the wwvb time : If there is new data AND its a whole minute
   if (gps.new_data() & (gps.ss == 0))
   {
      if (gps.IsValid)
      {
         #if (_DEBUG > 0)
         Serial.println("##### GPS SYNCED #####");
         Serial.print("IsValid    : "); Serial.println(gps.IsValid);
         Serial.print("Quality    : "); Serial.println(gps.quality);
         Serial.print("Satellites : "); Serial.println(gps.satellites);
         Serial.print("Date/Time  : "); print_datetime(gps.mm,gps.hh,gps.DD,gps.MM,gps.YY);
         #endif
         
         // Yeah im ignoring the last parameter to set whether we are in daylight savings time
         wwvb_tx.set_time(gps.mm, gps.hh, gps.DD, gps.MM, gps.YY);
         if (!wwvb_tx.is_active())
         {
            wwvb_tx.start();
         }
         #if (_DEBUG > 0)
         Serial.println("##### From GPS #####");
         wwvb_tx.debug_time();
         Serial.println();
         #endif
      }
   }
   
   #if (_DEBUG > 0)
   // debug outputs at the end of each 1minute frame
   if(wwvb_tx.end_of_frame == true)
   {
      wwvb_tx.end_of_frame = false; // clear the EOF bit
      #if (_DEBUG > 0)
      Serial.println("##### From End Of Frame #####");
      wwvb_tx.debug_time();
      Serial.println();
      #endif
   }
   else
   {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
   }
   #endif
}
