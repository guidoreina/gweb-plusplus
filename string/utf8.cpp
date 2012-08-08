#include <stdlib.h>
#include "utf8.h"

bool utf8::length(const char* string, size_t& count, size_t& utf8len)
{
	const unsigned char* end = (const unsigned char*) string;
	int state = 0;
	unsigned char left = 0;
	unsigned char c;

	utf8len = 0;

	while ((c = *end) != 0) {
		if (state == 0) {
			if ((c & 0xf8) == 0xf0) {
				left = 3;
				state = 1;
			} else if ((c & 0xf0) == 0xe0) {
				left = 2;
				state = 1;
			} else if ((c & 0xe0) == 0xc0) {
				left = 1;
				state = 1;
			} else if ((c & 0x80) != 0) {
				return false;
			}

			utf8len++;
		} else {
			if ((c & 0xc0) != 0x80) {
				return false;
			}

			if (--left == 0) {
				state = 0;
			}
		}

		end++;
	}

	if (left > 0) {
		return false;
	}

	count = end - (const unsigned char*) string;

	return true;
}

bool utf8::length(const char* string, const char* end, size_t& utf8len)
{
	int state = 0;
	unsigned char left = 0;

	utf8len = 0;

	while (string < end) {
		unsigned char c = (unsigned char) *string++;

		if (state == 0) {
			if ((c & 0xf8) == 0xf0) {
				left = 3;
				state = 1;
			} else if ((c & 0xf0) == 0xe0) {
				left = 2;
				state = 1;
			} else if ((c & 0xe0) == 0xc0) {
				left = 1;
				state = 1;
			} else if ((c & 0x80) != 0) {
				return false;
			}

			utf8len++;
		} else {
			if ((c & 0xc0) != 0x80) {
				return false;
			}

			if (--left == 0) {
				state = 0;
			}
		}
	}

	return (left == 0);
}
