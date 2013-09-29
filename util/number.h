#ifndef NUMBER_H
#define NUMBER_H

#include <sys/types.h>
#include <limits.h>

class number {
	public:
		static size_t length(off_t number);

		enum parse_result_t {PARSE_ERROR, PARSE_UNDERFLOW, PARSE_OVERFLOW, PARSE_SUCCEEDED};

		static parse_result_t parse_unsigned(const char* string, size_t len, unsigned& n, unsigned min = 0, unsigned max = UINT_MAX);
		static parse_result_t parse_size_t(const char* string, size_t len, size_t& n, size_t min = 0, size_t max = ULONG_MAX);
		static parse_result_t parse_off_t(const char* string, size_t len, off_t& n, off_t min = LLONG_MIN, off_t max = LLONG_MAX);
};

#endif // NUMBER_H
