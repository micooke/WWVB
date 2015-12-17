#ifndef ATtinyGPS_h
#define ATtinyGPS_h

#include <TimeDateTools.h>

// Constants
//#define knots2kph 1852
//#define knots2mps 1852/(60*60) // 0.51444444444;

// ParseGPS
// Examples: 
// $GPRMC,dddddt.ddm,A,eeae.eeee,l,eeeae.eeee,o,djdk,ddd.dc,dddddy,,,A*??
// $GPRMC,194509.000,A,4042.6142,N,07400.4168,W,2.03,221.11,160412,,,A*77
//
// $GPZDA,dddddt.ddm,dD,dM,dddY,,*??
// $GPZDA,035751.000,08,12,2015,,*5E

char msg[7] = "$$$$$$"; 
const char fmt[7] = "$GPRMC";
const char rmc[] = "$GPRMC,dddddt.ddm,A,eeae.eeee,l,eeeae.eeee,o,djdk,ddd.dc,dddddy,,,A*??";
#ifdef _GPZDA_
const char zda[] = "$GPZDA,dddddt.ddm,dD,dM,dddY,,*??";
#endif

class ATtinyGPS
{
private:
	float kph_ = 0.0f;
	float kph_avg_ = 0.0f;
	int state_ = 0;
	unsigned int temp_ = 0;
	long ltmp_ = 0;
	boolean is_complete_ = false;
	boolean new_data_ = false;
	uint8_t nmea_index_ = 0;
	
public:
	// GPS variables
	unsigned int Time, Date, Knots, Course;
	uint8_t hh, mm, ss, ms;
	uint8_t DD, MM, YY;
	uint16_t YYYY;
	int8_t timezone_MM, timezone_HH, GPS_to_UTC_offset;
	long Lat, Long;
	boolean IsValid = false;
	ATtinyGPS() : timezone_HH(9), timezone_MM(30), GPS_to_UTC_offset(-17), IsValid(false){};
	void parse(char c)
	{
		if (c == '$') { state_ = 0; temp_ = 0; is_complete_ = false; msg[5] = '?'; new_data_ = false; }
		
		char mode;
		
		if (state_ < 6)
		{
			msg[state_] = c;
			state_++;
			return;
		}
		else if (state_ == 6)
		{
			if (strcmp(msg, "$GPRMC"))
			{
				IsValid = true;
				nmea_index_ = 1;
			}
#ifdef _GPZDA_
			else if (strcmp(msg, "$GPZDA"))
			{
				IsValid = true;
				nmea_index_ = 17;
			}
#endif
		}
		else
		{
			switch (nmea_index_)
			{
			case 1:
				mode = rmc[state_];
				break;
#ifdef _GPZDA_
			case 17:
				mode = zda[state_];
				break;
#endif
			}
		}

		// Check that a valid message format is being passed
		if ( !IsValid | ((mode == ',') & (c != ',')) )
		{
			IsValid = false;
			state_++;
			return;
		}

		// The nmea string is complete (or near enough) if the format is '?'
		if (mode == '?')
		{
			if (is_complete_ == false)
			{
				msg[5] = '!'; // Set the last character to ! to signify completion (? indicates in progress)

				if (nmea_index_ == 1) // GPRMC - the start of a new data frame
				{
					new_data_ = true;
				}
				is_complete_ = true;
				process();
			}
		}

		// Process $GPRMC messages
		if (nmea_index_ == 1)
		{
			char mode = rmc[state_++];
			char d = c - '0';
			// d=decimal digit
			if (mode == 'd') { temp_ = temp_ * 10 + d; }
			// t=Time - hhmm
			else if (mode == 't') { Time = temp_ * 10 + d; temp_ = 0; }
			// m=Millisecs
			else if (mode == 'm') { ms = temp_ * 10 + d; temp_ = 0; ltmp_ = 0; }
			// l=Latitude - in minutes*10000
			else if (mode == 'l') { if (c == 'N') Lat = ltmp_; else Lat = -ltmp_; temp_ = 0; ltmp_ = 0; }
			// o=Longitude - in minutes*10000
			else if (mode == 'o') { if (c == 'E') Long = ltmp_; else Long = -ltmp_; temp_ = 0; ltmp_ = 0; }
			// j/k=Speed - in knots*100
			else if (mode == 'j') { if (c != '.') { temp_ = temp_ * 10 + d; state_--; } }
			else if (mode == 'k') { Knots = temp_ * 10 + d; temp_ = 0; }
			// c=Course (Track) - in degrees*100
			else if (mode == 'c') { Course = temp_ * 10 + d; temp_ = 0; }
			// y=Date - ddmm
			else if (mode == 'y') { Date = temp_ * 10 + d; }
			// A=*A-active, V-void, *A-autonomous, *D-differential, E-Estimated, N-not valid, S-Simulator
			else state_ = 0;
		}
#ifdef _GPZDA_
		// Process $GPZDA messages
		else if (nmea_index_ == 17)
		{
			char mode = zda[state_++];
			char d = c - '0';
			// d=decimal digit
			if (mode == 'd') { temp_ = temp_ * 10 + d; }
			// t=Time - hhmm
			else if (mode == 't') { Time = temp_ * 10 + d; temp_ = 0; }
			// m=Millisecs
			else if (mode == 'm') { ms = temp_ * 10 + d; temp_ = 0; }
			// D=Day
			else if (mode == 'D') { DD = temp_ * 10 + d; temp_ = 0; }
			// M=Month
			else if (mode == 'M') { mm = temp_ * 10 + d; temp_ = 0; }
			// Y=Year
			else if (mode == 'Y') { YYYY = temp_ * 10 + d; temp_ = 0; }
			else state_ = 0;
		}
#endif
		// increment the state
		state_++;
	}

	boolean new_data()
	{
		if (new_data_)
		{
			new_data_ = false;
			return true;
		}
		return false;
	}
private:
	
	void process()
	{
		switch (nmea_index_)
		{
			case 0: break;  // GLL
			case 1: // RMC
				processRMC(); break;
			case 17: break;// ZDA
				processZDA(); break;
			case 18: break; // MCHN
				break;
			/*
			case 2: break;  // VTG
			case 3: break;  // GGA
			case 4: break;  // GSA
			case 5: break;  // GSV
			case 6: break;  // GRS
			case 7: break;  // GST
			case 8: break;  // ?
			case 9: break;  // ?
			case 10: break; // ?
			case 11: break; // ?
			case 12: break; // ?
			case 13: break; // MALM
			case 14: break; // MEPH
			case 15: break; // MDPG
			case 16: break; // MDBG
			*/
		}

		// Add the GPS to UTC offset
		// (GPS doesnt compensate for leap seconds, as of Dec 2015 its 17 seconds ahead of UTC)
		int8_t gps_ss = ss + GPS_to_UTC_offset;

		// Add the local timezone to the GPS time
		addTimezone(ss, mm, hh, MM, DD, YY, timezone_HH, timezone_MM, gps_ss);
	}
	void processRMC()
	{
		hh = static_cast<uint8_t>(Time / 100);
		mm = Time % 100;
		DD = static_cast<uint8_t>(Date / 10000);
		mm = static_cast<uint8_t>(Date / 100) % 100;
		YY = Date % 100;
		YYYY = 2000 + YY; // may have to change this in 85 years...
	}
	void processZDA()
	{
#ifdef _GPZDA_
		hh = static_cast<uint8_t>(Time / 100);
		mm = Time % 100;
		YY = YYYY % 100;
#endif
	}
};

#endif