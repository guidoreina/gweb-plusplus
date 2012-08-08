#ifndef RANGE_PARSER_H
#define RANGE_PARSER_H

#include "util/range_list.h"

class range_parser {
	public:
		static bool parse(const char* string, size_t len, off_t filesize, range_list& ranges);
};

#endif // RANGE_PARSER_H
