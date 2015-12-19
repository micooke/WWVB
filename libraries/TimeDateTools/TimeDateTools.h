#ifndef TimeDateTools_h
#define TimeDateTools_h

#include <Arduino.h>

//											      Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
const uint8_t days_in_a_month[12] = { 31, 28, 31, 30, 31, 30, 31, 31,30, 31, 30,  31 }; // actual (non-leap year)
const uint16_t cumulative_days_in_a_month[12] = { 0 , 31, 59, 90,120,151,181,212,243,273,304,334 }; // cumulative

inline uint8_t ascii_to_int(const char &c0, const char &c1)
{
	return (c1 - '0') * 10 + (c0 - '0');
}

#if !defined(REQUIRE_TIMEDATESTRING)
#define REQUIRE_TIMEDATESTRING 1
#endif

#if (REQUIRE_TIMEDATESTRING == 1)
void TimeString_to_HHMMSS(const char timeString[], uint8_t &hh, uint8_t &mm, uint8_t &ss)
{
	//Example: "23:59:01"

	hh = ascii_to_int(timeString[0], timeString[1]); // [2] = ':'
	mm = ascii_to_int(timeString[3], timeString[4]); // [5] = ':'
	ss = ascii_to_int(timeString[6], timeString[7]);
}

void DateString_to_DDMMYY(const char dateString[], uint8_t &DD, uint8_t &MM, uint8_t &YY)
{
	//Example: "Feb 12 1996"

	// convert the Month string to Month index [0->12]
	// Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
	switch (dateString[0]) {
	case 'J': MM = dateString[1] == 'a' ? 1 : MM = dateString[2] == 'n' ? 6 : 7; break;
	case 'F': MM = 2; break;
	case 'A': MM = dateString[2] == 'r' ? 4 : 8; break;
	case 'M': MM = dateString[2] == 'r' ? 3 : 5; break;
	case 'S': MM = 9; break;
	case 'O': MM = 10; break;
	case 'N': MM = 11; break;
	case 'D': MM = 12; break;
	}

	DD = ascii_to_int(dateString[4], dateString[5]); // [6] = ' '
	YY = ascii_to_int(dateString[9], dateString[10]);
}
#endif

boolean is_leap_year(const uint16_t &_year)
{
	const boolean is_DivBy4 = (_year % 4) == 0;
	const boolean is_DivBy100 = (_year % 100) == 0;
	const boolean is_DivBy400 = (_year % 400) == 0;

	return (is_DivBy4 & (!is_DivBy100 | is_DivBy400));
}

uint16_t to_day_of_the_year(const uint8_t &_DD, const uint8_t &_MM, const boolean &_is_leap_year)
{
	// if its a leap year and the month is March -> December, add +1
	return cumulative_days_in_a_month[_MM - 1] + _DD + (_is_leap_year & _MM > 1);
}

// example: input = day (unbounded), TD0 = day (bounded), TD1 = month (bounded), prevLimit = days in last month, thisLimit = days in this month
// timeDateCompensate(-1, day, month, 28, 31)
//
// 1. set TD0 such that it stays within [0:thisLimit]
// 2. set TD1 such that it stays within [0:prevLimit]
void timeDateCompensate(const int8_t &input, uint8_t &TD0, int8_t &TD1, const uint8_t &prevLimit, const uint8_t &thisLimit)
{
	if (input > (thisLimit - 1))
	{
		TD0 = input % thisLimit;
		TD1++;
	}
	else if (input < 0)
	{
		TD0 = prevLimit - TD0;
		TD1--;
	}
}

void timeDateCompensate(const int8_t &input, uint8_t &TD0, int8_t &TD1, const uint8_t &thisLimit)
{
	timeDateCompensate(input, TD0, TD1, thisLimit, thisLimit);
}

// Note: I normally use the gps to utc seconds value (-17seconds as of Dec 2015) for _timezone_secs
void addTimezone(uint8_t &secs, uint8_t &mins, uint8_t &hour, uint8_t &DD, uint8_t &MM, uint8_t &YY, const int8_t &_timezone_hours, const int8_t &_timezone_mins, const int8_t _timezone_secs = 0)
{
	int8_t secs_ = secs + _timezone_secs;
	int8_t mins_ = mins + _timezone_mins;
	int8_t hour_ = hour + _timezone_hours;
	int8_t day_ = DD;
	int8_t month_ = MM;
	int8_t year_ = YY;

	const uint8_t prevMonth = (month_ == 1) ? 12 : month_ - 1;
	int8_t decade_ = 20; // we dont actually do anything with this

	const boolean is_leap_year_ = is_leap_year(decade_ * 100 + year_);

	// compensate for the timezone
	timeDateCompensate(secs_, secs, mins_, 60); // secs
	timeDateCompensate(mins_, mins, hour_, 60); // mins
	timeDateCompensate(hour_, hour, day_, 60); // hour

	uint8_t prevMonthDays = days_in_a_month[prevMonth - 1] + ((prevMonth == 2) & is_leap_year_); // Add 1 day if in Feb on a leapyear
	uint8_t thisMonthDays = days_in_a_month[month_ - 1] + ((month_ == 2) & is_leap_year_); // Add 1 day if in Feb on a leapyear
	timeDateCompensate(day_, DD, month_, prevMonthDays, thisMonthDays); // day

	timeDateCompensate(month_, MM, year_, 12); // month
	timeDateCompensate(year_, YY, decade_, 100); // year
}

#endif