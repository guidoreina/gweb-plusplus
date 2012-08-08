#include <stdlib.h>
#include "html_encoder.h"

int html_encoder::encode(const char* string, size_t len, buffer& buf)
{
	const char* src = string;
	const char* end = string + len;

	// Compute bytes needed.
	size_t needed = 0;
	while (src < end) {
		unsigned char c = (unsigned char) *src++;
		if ((c == '<') || (c == '>')) {
			needed += 4;
		} else if ((c == '&') || (c == '\'')) {
			needed += 5;
		} else if (c == '"') {
			needed += 6;
		} else {
			needed++;
		}
	}

	if (!buf.allocate(needed)) {
		return -1;
	}

	char* dst = buf.data() + buf.count();

	// Encode.
	while (string < end) {
		unsigned char c = (unsigned char) *string++;
		if (c == '<') {
			*dst++ = '&';
			*dst++ = 'l';
			*dst++ = 't';
			*dst++ = ';';
		} else if (c == '>') {
			*dst++ = '&';
			*dst++ = 'g';
			*dst++ = 't';
			*dst++ = ';';
		} else if (c == '&') {
			*dst++ = '&';
			*dst++ = 'a';
			*dst++ = 'm';
			*dst++ = 'p';
			*dst++ = ';';
		} else if (c == '"') {
			*dst++ = '&';
			*dst++ = 'q';
			*dst++ = 'u';
			*dst++ = 'o';
			*dst++ = 't';
			*dst++ = ';';
		} else if (c == '\'') {
			*dst++ = '&';
			*dst++ = '#';
			*dst++ = '3';
			*dst++ = '9';
			*dst++ = ';';
		} else {
			*dst++ = c;
		}
	}

	buf.increment_count(needed);

	return needed;
}
