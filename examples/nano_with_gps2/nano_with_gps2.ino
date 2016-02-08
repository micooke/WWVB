#include <Arduino.h>

//          Arduino Nano
//        +----+=====+----+
//        |    | USB |    |
// SCK B5 |D13 +-----+D12 | B4 MISO
//        |3V3        D11~| B3 MOSI
//        |Vref       D10~| B2 SS
//     C0 |A0          D9~| B1 |=> WWVB ANTENNA
//     C1 |A1          D8 | B0 |=> GPS Rx (Software Serial)
//     C2 |A2          D7 | D7 |<= GPS Tx (Software Serial)
//     C3 |A3          D6~| D6
// SDA C4 |A4          D5~| D5
// SCL C5 |A5          D4 | D4
//        |A6          D3~| D3 INT1
//        |A7          D2 | D2 INT0
//        |5V         GND |
//     C6 |RST        RST | C6
//        |GND        TX1 | D0
//        |Vin        RX1 | D1
//        |  5V MOSI GND  |
//        |   [] [] []    |
//        |   [] [] []    |
//        | MISO SCK RST  |
//        +---------------+

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

#define SetupGPS // Sends PMTK strings to setup the GPS

bool sync_gpstime = false;

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
uint8_t LED_PIN = 13;
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
   wwvb_tx.interrupt_routine();
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
   
   // Set the wwvb calibration values
   //wwvb_tx.set_1s_calibration(0, 0); // 59.952668
   wwvb_tx.setPWM_LOW(0);
   //wwvb_tx.set(12030,30075,48120,60150); // What they should be
   //wwvb_tx.set(12030,30075,47600,54461); // Set the values for WWVB pulse widths
   // Note these values are stored in a uint16_t with a max of 65536.
   // If greater than this value is required, the 1s calibration value 
   // is added to this so set that appropriately
   
   // set the timezone before you set your time
   wwvb_tx.setTimezone(0,0); // Default state - this line is not needed
   // if you are using CST (UTC - 6:00), set the timezone to -6,0 (below)
   //gps.setTimezone(10,30); // +9:30 Adelaide time + 1:00 for DST
      
   #if (_DEBUG > 0)
   Serial.begin(9600);
   while(!Serial);
   t0 = millis();
   pinMode(LED_PIN, OUTPUT);
   #endif
   
   //ttl.begin(14400);
   // ublox
   // send RMC & GGA only
   //ttl.println("");
   //1Hz update rate
   //ttl.println(""");
   #ifdef SetupGPS
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
   
   //ttl.println("$PMTK251,9600*17");
   //ttl.println("$PMTK251,14400*29"); // Fastest we can go with the current parsing method with only minimal over-writing of serial buffer data (TinyGPS++ may be better?)
   //ttl.println("$PMTK251,19200*22");
   //ttl.println("$PMTK251,38400*27");
   //ttl.println("$PMTK251,57600*2C");
   //ttl.println("$PMTK251,115200*1F");
   //ttl.println("$PMTK251,0*28\n"); // reset BAUD rate to system default
   delay(100);
   ttl.begin(9600);
   #endif
   
   #if (_DEBUG > 0)
   Serial.print("Waiting on first GPS sync ");
   t0 = millis();
   #endif
   
   // first up : sync the wwvb_tx to the gps
   // wait until we get gps data
   while ( !((gps.IsValid) & (gps.ss == 0)) )
   {
      while (ttl.available())
      {
         gps.parse(ttl.read());
      }
      #if (_DEBUG > 0)
      if (millis() - t0 > 1000)
      {
         Serial.print(".");
         t0 = millis();
      }
      #endif
   }
   
   // Yeah im ignoring the last parameter to set whether we are in daylight savings time
   wwvb_tx.set_time(gps.mm, gps.hh, gps.DD, gps.MM, gps.YY);
   wwvb_tx.start();
   
   #if (_DEBUG > 0)
   Serial.println(" GPS synced!");
   
   Serial.println("##### GPS SYNCED #####");
   Serial.print("IsValid    : "); Serial.println(gps.IsValid);
   Serial.print("Quality    : "); Serial.println(gps.quality);
   Serial.print("Satellites : "); Serial.println(gps.satellites);
   Serial.print("Date/Time  : "); print_datetime(gps.mm,gps.hh,gps.DD,gps.MM,gps.YY);
   
   Serial.println("WWVB transmit started");
   #endif
}

void loop()
{
   // if we are receiving gps data, parse it
   if (ttl.available())
   {
      if (sync_gpstime)
      {
          // Parse gps data until we have new data
         while(gps.new_data() == false)
         {
            while (ttl.available())
            {
               gps.parse(ttl.read());
            }
         }
         
         // If the gps data is valid, sync the wwvb time
         if ( gps.IsValid )
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
            sync_gpstime = false;
         }      
      }
   }
   
   // If there is new data set the wwvb time
   // Note
   // * wwvb time is synced and wwvb transmission is started when minutes = 0,10,20,30,40 or 50
   // * wwvb transmission is stopped when minutes = 9,19,29,39,49 or 59
   //
   // In other words, wwvb will transmit for 9 minutes, then turn off and sync for a minute
   //
   if (gps.new_data() & (gps.IsValid) & (gps.ss == 0) )
   {
      switch (gps.mm % 10)
      {
         case 0:
         #if (_DEBUG > 0)
         Serial.println("WWVB started");
         #endif
            
         wwvb_tx.start();
         break;
         case 9:
         #if (_DEBUG > 0)
         Serial.println("WWVB stopped");
         #endif
         
         wwvb_tx.stop();
         sync_gpstime = true;
         break;
      }
   }
   
   #if (_DEBUG > 0)

   if(wwvb_tx.is_active())
   {
      if (millis() - t0 >= LED_DELAY)
      {
         t0 = millis();
         digitalWrite(LED_PIN, LED_TOGGLE);
         LED_TOGGLE = !LED_TOGGLE;
      }
   }
   else
   {
      if(gps.satellites == 0)
      {
         digitalWrite(LED_PIN, LOW);
      }
      else
      {
         digitalWrite(LED_PIN, HIGH);
      }
   }
   #endif
}