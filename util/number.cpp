#include <stdlib.h>
#include "number.h"
#include "macros/macros.h"

size_t number::length(off_t number)
{
	size_t len;

	if (number < 0) {
		len = 2;
		number *= -1;
	} else {
		len = 1;
	}

	for (off_t n = 9; n < number; len++) {
		off_t tmp = (n * 10) + 9;

		// Overflow?
		if (tmp < n) {
			break;
		}

		n = tmp;
	}

	return len;
}

number::parse_result_t number::parse_unsigned(const char* string, size_t len, unsigned& n, unsigned min, unsigned max)
{
	if (len == 0) {
		return PARSE_ERROR;
	}

	const char* end = string + len;

	n = 0;

	while (string < end) {
		unsigned char c = (unsigned char) *string++;
		if (!IS_DIGIT(c)) {
			return PARSE_ERROR;
		}

		unsigned tmp = (n * 10) + (c - '0');

		// Overflow?
		if (tmp < n) {
			return PARSE_ERROR;
		}

		n = tmp;
	}

	if (n < min) {
		return PARSE_UNDERFLOW;
	} else if (n > max) {
		return PARSE_OVERFLOW;
	}

	return PARSE_SUCCEEDED;
}

number::parse_result_t number::parse_size_t(const char* string, size_t len, size_t& n, size_t min, size_t max)
{
	if (len == 0) {
		return PARSE_ERROR;
	}

	const char* end = string + len;

	n = 0;

	while (string < end) {
		unsigned char c = (unsigned char) *string++;
		if (!IS_DIGIT(c)) {
			return PARSE_ERROR;
		}

		size_t tmp = (n * 10) + (c - '0');

		// Overflow?
		if (tmp < n) {
			return PARSE_ERROR;
		}

		n = tmp;
	}

	if (n < min) {
		return PARSE_UNDERFLOW;
	} else if (n > max) {
		return PARSE_OVERFLOW;
	}

	return PARSE_SUCCEEDED;
}

number::parse_result_t number::parse_off_t(const char* string, size_t len, off_t& n, off_t min, off_t max)
{
	if (len == 0) {
		return PARSE_ERROR;
	}

	const char* end = string + len;

	off_t sign;

	if (*string == '-') {
		if (len == 1) {
			return PARSE_ERROR;
		}

		sign = -1;

		string++;
	} else if (*string == '+') {
		if (len == 1) {
			return PARSE_ERROR;
		}

		sign = 1;

		string++;
	} else {
		sign = 1;
	}

	n = 0;

	while (string < end) {
		unsigned char c = (unsigned char) *string++;
		if (!IS_DIGIT(c)) {
			return PARSE_ERROR;
		}

		off_t tmp = (n * 10) + (c - '0');

		// Overflow?
		if (tmp < n) {
			return PARSE_ERROR;
		}

		n = tmp;
	}

	n *= sign;

	if (n < min) {
		return PARSE_UNDERFLOW;
	} else if (n > max) {
		return PARSE_OVERFLOW;
	}

	return PARSE_SUCCEEDED;
}
