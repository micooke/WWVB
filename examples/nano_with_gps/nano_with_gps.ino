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

#define SetupGPS // Sends PMTK strings to setup the GPS

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
uint8_t LED_PIN = 1; // Use PB1 the Tx LED on the digistump
#elif defined(__AVR_ATmega16U4__) | defined(__AVR_ATmega32U4__)
uint8_t LED_PIN = SS;
#elif defined(__AVR_ATmega168__) | defined(__AVR_ATmega168P__) | defined(__AVR_ATmega328P__)
uint8_t LED_PIN = 13;
#endif
uint16_t LED_DELAY = 100;
bool LED_TOGGLE = false;
uint32_t t0;
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
SoftwareSerial ttl(7, 8);// Rx,Tx pin

#include <ATtinyGPS.h>
ATtinyGPS gps;

uint8_t count = 0;

// debug variables
uint32_t t1;
boolean isLow = false;

char c = '\0';

void setup()
{
   wwvb_tx.setup();
   
   // set the timezone before you set your time
   // if you are using CST (UTC - 6:00), set the timezone to -6,0 (below)
   wwvb_tx.setTimezone(-6,0);
   // wwvb_tx.set_1s_calibration(84, 85); // 64.76363
   //wwvb_tx.set_1s_calibration(0,0); // 65.837075
   //wwvb_tx.set_1s_calibration(-100,-100); // 65.78, 1.15
   //wwvb_tx.set_1s_calibration(-500,-500); // 1.08
   //wwvb_tx.set_1s_calibration(-1000,-1000); // 10.80, 1.056
   //wwvb_tx.set_1s_calibration(-5000,-5000); // 10.054484s
   //wwvb_tx.set_1s_calibration(-4000,-4000); // 10.220166s
   //wwvb_tx.set_1s_calibration(-5329,-5329); // 10.00525s
   //wwvb_tx.set_1s_calibration(-5400,-5400); // 10.000169s
   //wwvb_tx.set_1s_calibration(-5500,-5500); // 9.977221s
   //wwvb_tx.set_1s_calibration(-5410,-5410); // 9.983661s
   wwvb_tx.set_1s_calibration(-5400,-5400); // 9.9903362
   wwvb_tx.setPWM_LOW(0);
      
   #if (_DEBUG > 0)
   Serial.begin(9600);
   while(!Serial);
   t0 = millis();
   pinMode(LED_PIN, OUTPUT);
   #endif
   
#ifdef SetupGPS
	ttl.begin(9600);
   // Setup the GPS
	// send RMC & GGA only
	//                     GLL, RMC, VTG, GGA, GSA,
	ttl.println("$PMTK314,   0,   1,   0,   1,   0,"
		// GSV, GRS, GST,    ,    ,
		"    0,   0,   0,   0,   0,"
		//   ,   ,    , MALM,MEPH,
		"    0,   0,   0,   0,   0,"
		//MDGP,MDBG, ZDA,MCHN*checksum   
		"    0,   0,   0,   0*28");
	//ttl.println("$PMTK314,-1*04"); // reset NMEA sequences to system default

	//1Hz update rate
	ttl.println("$PMTK220,1000*1F");
	//ttl.println("$PMTK220,100*2F");//10Hz update rate
	//ttl.println("$PMTK220,200*2C");//5Hz update rate
	
   // 115200 BAUD (6.6% for bare minimum)
	//ttl.println("$PMTK251,9600*17");
   ttl.println("$PMTK251,14400*29");
   //ttl.println("$PMTK251,19200*22");
   //ttl.println("$PMTK251,38400*27");
   //ttl.println("$PMTK251,57600*2C");
   //ttl.println("$PMTK251,115200*1F");
	//ttl.println("$PMTK251,0*28\n"); // reset BAUD rate to system default
   //ttl.end();
   delay(500);
   ttl.begin(14400);
#endif
}

void loop()
{
   // if we are receiving gps data, parse it
   while (ttl.available())
   {
      c = ttl.read();
      gps.parse(c);
      
      #if (_DEBUG == 2)
      // Output raw NMEA string
      Serial.print(c);
      #endif
   }

   if (gps.new_data())
   {
    #if (_DEBUG == 2)
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
   #endif
   // set the wwvb time : If there is new data AND its a whole minute
     if (gps.ss == 0)
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
   }
   
   #if (_DEBUG > 0)
   // debug outputs at the end of each 1minute frame
   if(wwvb_tx.end_of_frame == true)
   {
      wwvb_tx.end_of_frame = false; // clear the EOF bit
      #if (_DEBUG == 2)
      Serial.println("##### From End Of Frame #####");
      wwvb_tx.debug_time();
      Serial.println();
      #endif
   }
   if(gps.satellites == 0)
   {
      if (millis() - t0 >= LED_DELAY)
      {
         t0 = millis();
         digitalWrite(LED_PIN, LED_TOGGLE);
         LED_TOGGLE != LED_TOGGLE;   
      }
   }
   #endif
}