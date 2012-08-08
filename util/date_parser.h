#ifndef DATE_PARSER_H
#define DATE_PARSER_H

#include <time.h>
#include "macros/macros.h"

class date_parser {
	public:
		static time_t parse(const char* string, size_t len, struct tm* timestamp);

	private:
		static time_t parse_ansic(const unsigned char* begin, const unsigned char* end, struct tm* timestamp);

		static bool parse_year(const unsigned char* ptr, unsigned& year);
		static bool parse_month(const unsigned char* ptr, unsigned& mon);
		static bool parse_time(const unsigned char* ptr, unsigned& hour, unsigned& min, unsigned& sec);
};

inline bool date_parser::parse_year(const unsigned char* ptr, unsigned& year)
{
	if ((!IS_DIGIT(*ptr)) || (!IS_DIGIT(*(ptr + 1))) || (!IS_DIGIT(*(ptr + 2))) || (!IS_DIGIT(*(ptr + 3)))) {
		return false;
	}

	year = ((*ptr - '0') * 1000) + ((*(ptr + 1) - '0') * 100) + ((*(ptr + 2) - '0') * 10) + (*(ptr + 3) - '0');
	return (year >= 1970);
}

#endif // DATE_PARSER_H
