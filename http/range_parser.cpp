#include <stdlib.h>
#include "range_parser.h"
#include "macros/macros.h"

bool range_parser::parse(const char* string, size_t len, off_t filesize, range_list& ranges)
{
	const char* end = string + len;

	int state = 0; // Initial state.
	off_t from = 0;
	off_t to = 0;
	off_t count = 0;

	while (string < end) {
		unsigned char c = (unsigned char) *string++;

		switch (state) {
			case 0: // Initial state.
				if (IS_DIGIT(c)) {
					from = c - '0';
					state = 1; // From.
				} else if (c == '-') {
					state = 7; // Start of suffix length.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 1: // From.
				if (IS_DIGIT(c)) {
					off_t n = (from * 10) + (c - '0');

					if ((n < from) || (n >= filesize)) {
						return false;
					}

					from = n;
				} else if (c == '-') {
					state = 3; // After '-'.
				} else if (IS_WHITE_SPACE(c)) {
					state = 2; // Whitespace after from.
				} else {
					return false;
				}

				break;
			case 2: // Whitespace after from.
				if (c == '-') {
					state = 3; // After '-'.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 3: // After '-'.
				if (IS_DIGIT(c)) {
					to = c - '0';
					state = 4; // To.
				} else if (c == ',') {
					if (!ranges.add(from, filesize - 1)) {
						return false;
					}

					state = 6; // A new range starts.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 4: // To.
				if (IS_DIGIT(c)) {
					off_t n = (to * 10) + (c - '0');

					// Overflow?
					if (n < to) {
						return false;
					}

					to = n;
				} else if (c == ',') {
					to = MIN(to, filesize - 1);

					if (!ranges.add(from, to)) {
						return false;
					}

					state = 6; // A new range starts.
				} else if (IS_WHITE_SPACE(c)) {
					state = 5; // Whitespace after to.
				} else {
					return false;
				}

				break;
			case 5: // Whitespace after to.
				if (c == ',') {
					to = MIN(to, filesize - 1);

					if (!ranges.add(from, to)) {
						return false;
					}

					state = 6; // A new range starts.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 6: // A new range starts.
				if (IS_DIGIT(c)) {
					from = c - '0';
					state = 1; // From.
				} else if (c == '-') {
					state = 7; // Start of suffix length.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 7: // Start of suffix length.
				if (IS_DIGIT(c)) {
					count = c - '0';
					state = 8; // Suffix length.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
			case 8: // Suffix length.
				if (IS_DIGIT(c)) {
					off_t n = (count * 10) + (c - '0');

					// Overflow?
					if (n < count) {
						return false;
					}

					count = n;
				} else if (c == ',') {
					if (count == 0) {
						return false;
					}

					count = MIN(count, filesize);

					if (!ranges.add(filesize - count, filesize - 1)) {
						return false;
					}

					state = 6; // A new range starts.
				} else if (IS_WHITE_SPACE(c)) {
					state = 9; // Whitespace after suffix length.
				} else {
					return false;
				}

				break;
			case 9: // Whitespace after suffix length.
				if (c == ',') {
					if (count == 0) {
						return false;
					}

					count = MIN(count, filesize);

					if (!ranges.add(filesize - count, filesize - 1)) {
						return false;
					}

					state = 6; // A new range starts.
				} else if (!IS_WHITE_SPACE(c)) {
					return false;
				}

				break;
		}
	}

	switch (state) {
		case 0:
			return true;
		case 3:
			return ranges.add(from, filesize - 1);
		case 4:
		case 5:
			to = MIN(to, filesize - 1);
			return ranges.add(from, to);
		case 8:
		case 9:
			if (count == 0) {
				return false;
			}

			count = MIN(count, filesize);

			return ranges.add(filesize - count, filesize - 1);
		default:
			return false;
	}
}
