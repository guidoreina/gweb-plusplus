#include <stdlib.h>
#include "url_encoder.h"

int url_encoder::encode(const char* url, size_t len, buffer& buf)
{
	const char* src = url;
	const char* end = url + len;

	// Compute bytes needed.
	size_t needed = 0;
	while (src < end) {
		unsigned char c = (unsigned char) *src++;

		if ((!is_unreserved(c)) && (c != '/')) {
			needed += 3;
		} else {
			needed++;
		}
	}

	if (!buf.allocate(needed)) {
		return -1;
	}

	char* dst = buf.data() + buf.count();

	// Encode.
	while (url < end) {
		unsigned char c = (unsigned char) *url++;

		if ((!is_unreserved(c)) && (c != '/')) {
			*dst++ = '%';

			unsigned char n = c / 16;
			if (n < 10) {
				*dst++ = '0' + n;
			} else {
				*dst++ = 'a' + (n - 10);
			}

			n = c % 16;
			if (n < 10) {
				*dst++ = '0' + n;
			} else {
				*dst++ = 'a' + (n - 10);
			}
		} else {
			*dst++ = c;
		}
	}

	buf.increment_count(needed);

	return needed;
}
