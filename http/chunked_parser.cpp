#include <stdlib.h>
#include "chunked_parser.h"

chunked_parser::parse_result chunked_parser::parse_chunk(const char* buf, size_t len, size_t& size)
{
	const char* ptr = buf;
	const char* end = ptr + len;

	while (ptr < end) {
		unsigned char c = (unsigned char) *ptr;
		if (_M_state == 0) { // Chunk size.
			if ((c >= '0') && (c <= '9')) {
				_M_chunk_size = c - '0';

				_M_state = 1; // Reading chunk size.
			} else if ((c >= 'A') && (c <= 'F')) {
				_M_chunk_size = c - 'A' + 10;

				_M_state = 1; // Reading chunk size.
			} else if ((c >= 'a') && (c <= 'f')) {
				_M_chunk_size = c - 'a' + 10;

				_M_state = 1; // Reading chunk size.
			} else {
				return INVALID_CHUNKED_RESPONSE;
			}
		} else if (_M_state == 1) { // Reading chunk size.
			if ((c >= '0') && (c <= '9')) {
				_M_chunk_size = (_M_chunk_size * 16) + (c - '0');
			} else if ((c >= 'A') && (c <= 'F')) {
				_M_chunk_size = (_M_chunk_size * 16) + (c - 'A' + 10);
			} else if ((c >= 'a') && (c <= 'f')) {
				_M_chunk_size = (_M_chunk_size * 16) + (c - 'a' + 10);
			} else if ((c == ' ') || (c == '\t') || (c == ';')) {
				_M_state = 2; // After chunk size.
			} else if (c == '\r') {
				_M_state = 3; // '\r' after chunk size.
			} else {
				return INVALID_CHUNKED_RESPONSE;
			}
		} else if (_M_state == 2) { // After chunk size.
			if (c == '\r') {
				_M_state = 3; // '\r' after chunk size.
			} else if ((c < ' ') && (c != '\t')) {
				return INVALID_CHUNKED_RESPONSE;
			}
		} else if (_M_state == 3) { // '\r' after chunk size.
			if (c == '\n') {
				if (_M_chunk_size > 0) {
					_M_state = 4; // Chunk data.
				} else {
					_M_state = 7; // Trailer.
				}
			} else {
				return INVALID_CHUNKED_RESPONSE;
			}
		} else if (_M_state == 4) { // Chunk data.
			size_t pending = len - (ptr - buf);
			if (_M_chunk_size <= pending) {
				if (!add_chunked_data(ptr, _M_chunk_size)) {
					return CALLBACK_FAILED;
				}

				ptr += _M_chunk_size;

				_M_state = 5; // After chunk data.

				continue;
			} else {
				if (!add_chunked_data(ptr, pending)) {
					return CALLBACK_FAILED;
				}

				_M_chunk_size -= pending;

				break;
			}
		} else if (_M_state == 5) { // After chunk data.
			if (c != '\r') {
				return INVALID_CHUNKED_RESPONSE;
			}

			_M_state = 6; // '\r' after chunk data.
		} else if (_M_state == 6) { // '\r' after chunk data.
			if (c != '\n') {
				return INVALID_CHUNKED_RESPONSE;
			}

			_M_state = 0; // Chunk size.
		} else if (_M_state == 7) { // Trailer.
			if (c == '\r') {
				_M_state = 10; // '\r' after trailer.
			} else if ((c >= ' ') || (c == '\t')) {
				_M_state = 8; // Entity header.
			} else {
				return INVALID_CHUNKED_RESPONSE;
			}
		} else if (_M_state == 8) { // Entity header.
			if (c == '\r') {
				_M_state = 9; // '\r' after entity header.
			} else if ((c < ' ') && (c != '\t')) {
				return INVALID_CHUNKED_RESPONSE;
			}
		} else if (_M_state == 9) { // '\r' after entity header.
			if (c != '\n') {
				return INVALID_CHUNKED_RESPONSE;
			}

			_M_state = 7; // Trailer.
		} else if (_M_state == 10) { // '\r' after trailer.
			if (c != '\n') {
				return INVALID_CHUNKED_RESPONSE;
			}

			size = ++ptr - buf;

			return END_OF_RESPONSE;
		}

		ptr++;
	}

	return NOT_END_OF_RESPONSE;
}
