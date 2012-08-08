#include <stdlib.h>
#include "date_parser.h"

time_t date_parser::parse(const char* string, size_t len, struct tm* timestamp)
{
	// RFC 2616
	// http://www.faqs.org/rfcs/rfc2616.html
	// 3.3.1 Full Date
	// HTTP applications have historically allowed three different formats
	// for the representation of date/time stamps:

	// Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
	// Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
	// Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format

	enum {
		RFC822,
		RFC850,
		ANSIC
	} date_format;

	const unsigned char* ptr = (const unsigned char*) string;
	const unsigned char* end = ptr + len;

	// Skip day of the week.
	while ((ptr < end) && (IS_ALPHA(*ptr))) {
		ptr++;
	}

	if (ptr == end) {
		return (time_t) -1;
	}

	size_t n = ptr - (const unsigned char*) string;

	if (*ptr == ' ') {
		if (n != 3) {
			return (time_t) -1;
		}

		ptr++;

		if ((size_t) (end - ptr) < sizeof("Nov 6 08:49:37 1994") - 1) {
			return (time_t) -1;
		}

		return parse_ansic(ptr, end, timestamp);
	}

	if ((*ptr != ',') || (n < 3) || (n > 9)) {
		return (time_t) -1;
	}

	if (n == 3) {
		date_format = RFC822;
	} else {
		date_format = RFC850;
	}

	ptr++;

	if ((size_t) (end - ptr) < sizeof("06-Nov-94 08:49:37 GMT") - 1) {
		return (time_t) -1;
	}

	// Skip white space (if any).
	if (*ptr == ' ') {
		ptr++;
	}

	// Parse day of the month.
	if ((!IS_DIGIT(*ptr)) || (!IS_DIGIT(*(ptr + 1)))) {
		return (time_t) -1;
	}

	unsigned mday = ((*ptr - '0') * 10) + (*(ptr + 1) - '0');
	if ((mday < 1) || (mday > 31)) {
		return (time_t) -1;
	}

	ptr += 2;

	if (date_format == RFC822) {
		if (*ptr != ' ') {
			return (time_t) -1;
		}
	} else {
		if (*ptr != '-') {
			return (time_t) -1;
		}
	}

	ptr++;

	// Parse month.
	unsigned mon;
	if (!parse_month(ptr, mon)) {
		return (time_t) -1;
	}

	ptr += 3;

	// Parse year.
	unsigned year;
	if (date_format == RFC822) {
		if (*ptr != ' ') {
			return (time_t) -1;
		}

		ptr++;

		if (!parse_year(ptr, year)) {
			return (time_t) -1;
		}

		ptr += 4;
	} else {
		if (*ptr != '-') {
			return (time_t) -1;
		}

		ptr++;

		if ((!IS_DIGIT(*ptr)) || (!IS_DIGIT(*(ptr + 1)))) {
			return (time_t) -1;
		}

		year = ((*ptr - '0') * 10) + (*(ptr + 1) - '0');
		if (year < 70) {
			year += 2000;
		} else {
			year += 1900;
		}

		ptr += 2;
	}

	if (*ptr != ' ') {
		return (time_t) -1;
	}

	ptr++;

	if (ptr + sizeof("08:49:37 GMT") - 1 != end) {
		return (time_t) -1;
	}

	// Parse time.
	unsigned hour;
	unsigned min;
	unsigned sec;
	if (!parse_time(ptr, hour, min, sec)) {
		return (time_t) -1;
	}

	ptr += 9;

	if (((*ptr != 'G') && (*ptr != 'g')) || ((*(ptr + 1) != 'M') && (*(ptr + 1) != 'm')) || ((*(ptr + 2) != 'T') && (*(ptr + 2) != 't'))) {
		return (time_t) -1;
	}

	timestamp->tm_year = year - 1900;
	timestamp->tm_mon = mon;
	timestamp->tm_mday = mday;
	timestamp->tm_hour = hour;
	timestamp->tm_min = min;
	timestamp->tm_sec = sec;
	timestamp->tm_isdst = 0;

#ifdef HAVE_TIMEGM
	return timegm(timestamp);
#else
	return mktime(timestamp) - timezone;
#endif
}

time_t date_parser::parse_ansic(const unsigned char* begin, const unsigned char* end, struct tm* timestamp)
{
	const unsigned char* ptr = begin;

	// Parse month.
	unsigned mon;
	if (!parse_month(ptr, mon)) {
		return (time_t) -1;
	}

	ptr += 3;

	if (*ptr != ' ') {
		return (time_t) -1;
	}

	ptr++;

	// Parse day of the month.
	if (!IS_DIGIT(*ptr)) {
		return (time_t) -1;
	}

	unsigned mday = *ptr - '0';

	ptr++;

	if (IS_DIGIT(*ptr)) {
		mday = (mday * 10) + (*ptr - '0');
		ptr++;
	}

	if ((mday < 1) || (mday > 31)) {
		return (time_t) -1;
	}

	if (*ptr != ' ') {
		return (time_t) -1;
	}

	ptr++;

	if (ptr + sizeof("08:49:37 1994") - 1 != end) {
		return (time_t) -1;
	}

	// Parse time.
	unsigned hour;
	unsigned min;
	unsigned sec;
	if (!parse_time(ptr, hour, min, sec)) {
		return (time_t) -1;
	}

	ptr += 9;

	// Parse year.
	unsigned year;
	if (!parse_year(ptr, year)) {
		return (time_t) -1;
	}

	timestamp->tm_year = year - 1900;
	timestamp->tm_mon = mon;
	timestamp->tm_mday = mday;
	timestamp->tm_hour = hour;
	timestamp->tm_min = min;
	timestamp->tm_sec = sec;
	timestamp->tm_isdst = 0;

#ifdef HAVE_TIMEGM
	return timegm(timestamp);
#else
	return mktime(timestamp) - timezone;
#endif
}

bool date_parser::parse_month(const unsigned char* ptr, unsigned& mon)
{
	switch (*ptr) {
		case 'J':
		case 'j':
			if ((*(ptr + 1) == 'a') || (*(ptr + 1) == 'A')) {
				if ((*(ptr + 2) == 'n') || (*(ptr + 2) == 'N')) {
					mon = 0;
				} else {
					return false;
				}
			} else if ((*(ptr + 1) == 'u') || (*(ptr + 1) == 'U')) {
				if ((*(ptr + 2) == 'n') || (*(ptr + 2) == 'N')) {
					mon = 5;
				} else if ((*(ptr + 2) == 'l') || (*(ptr + 2) == 'L')) {
					mon = 6;
				} else {
					return false;
				}
			} else {
				return false;
			}

			break;
		case 'F':
		case 'f':
			if (((*(ptr + 1) == 'e') || (*(ptr + 1) == 'E')) && ((*(ptr + 2) == 'b') || (*(ptr + 2) == 'B'))) {
				mon = 1;
			} else {
				return false;
			}

			break;
		case 'M':
		case 'm':
			if ((*(ptr + 1) == 'a') || (*(ptr + 1) == 'A')) {
				if ((*(ptr + 2) == 'r') || (*(ptr + 2) == 'R')) {
					mon = 2;
				} else if ((*(ptr + 2) == 'y') || (*(ptr + 2) == 'Y')) {
					mon = 4;
				} else {
					return false;
				}
			} else {
				return false;
			}

			break;
		case 'A':
		case 'a':
			if ((*(ptr + 1) == 'p') || (*(ptr + 1) == 'P')) {
				if ((*(ptr + 2) == 'r') || (*(ptr + 2) == 'R')) {
					mon = 3;
				} else {
					return false;
				}
			} else if ((*(ptr + 1) == 'u') || (*(ptr + 1) == 'U')) {
				if ((*(ptr + 2) == 'g') || (*(ptr + 2) == 'G')) {
					mon = 7;
				} else {
					return false;
				}
			} else {
				return false;
			}

			break;
		case 'S':
		case 's':
			if (((*(ptr + 1) == 'e') || (*(ptr + 1) == 'E')) && ((*(ptr + 2) == 'p') || (*(ptr + 2) == 'P'))) {
				mon = 8;
			} else {
				return false;
			}

			break;
		case 'O':
		case 'o':
			if (((*(ptr + 1) == 'c') || (*(ptr + 1) == 'C')) && ((*(ptr + 2) == 't') || (*(ptr + 2) == 'T'))) {
				mon = 9;
			} else {
				return false;
			}

			break;
		case 'N':
		case 'n':
			if (((*(ptr + 1) == 'o') || (*(ptr + 1) == 'O')) && ((*(ptr + 2) == 'v') || (*(ptr + 2) == 'V'))) {
				mon = 10;
			} else {
				return false;
			}

			break;
		case 'D':
		case 'd':
			if (((*(ptr + 1) == 'e') || (*(ptr + 1) == 'E')) && ((*(ptr + 2) == 'c') || (*(ptr + 2) == 'C'))) {
				mon = 11;
			} else {
				return false;
			}

			break;
		default:
			return false;
	}

	return true;
}

bool date_parser::parse_time(const unsigned char* ptr, unsigned& hour, unsigned& min, unsigned& sec)
{
	// Parse hour.
	if ((!IS_DIGIT(*ptr)) || (!IS_DIGIT(*(ptr + 1)))) {
		return false;
	}

	hour = ((*ptr - '0') * 10) + (*(ptr + 1) - '0');
	if (hour > 23) {
		return false;
	}

	ptr += 2;

	if (*ptr != ':') {
		return false;
	}

	ptr++;

	// Parse minutes.
	if ((!IS_DIGIT(*ptr)) || (!IS_DIGIT(*(ptr + 1)))) {
		return false;
	}

	min = ((*ptr - '0') * 10) + (*(ptr + 1) - '0');
	if (min > 59) {
		return false;
	}

	ptr += 2;

	if (*ptr != ':') {
		return false;
	}

	ptr++;

	// Parse seconds.
	if ((!IS_DIGIT(*ptr)) || (!IS_DIGIT(*(ptr + 1)))) {
		return false;
	}

	sec = ((*ptr - '0') * 10) + (*(ptr + 1) - '0');
	if (sec > 59) {
		return false;
	}

	ptr += 2;

	return (*ptr == ' ');
}
