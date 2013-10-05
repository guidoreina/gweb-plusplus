#include <stdio.h>
#include "buffer.h"

const size_t buffer::DEFAULT_BUFFER_INCREMENT = 64;

bool buffer::allocate(size_t size)
{
	size += _M_used;
	if (size <= _M_size) {
		return true;
	}

	size_t quotient = size / _M_buffer_increment;
	size_t remainder = size % _M_buffer_increment;
	if (remainder != 0) {
		quotient++;
	}

	size = quotient * _M_buffer_increment;
	char* data = (char*) realloc(_M_data, size);
	if (!data) {
		return false;
	}

	_M_data = data;
	_M_size = size;

	return true;
}

bool buffer::vformat(const char* format, va_list ap)
{
	if (!allocate(_M_buffer_increment)) {
		return false;
	}

	int size = _M_size - _M_used;

	do {
		va_list aq;
		va_copy(aq, ap);

		int n = vsnprintf(_M_data + _M_used, size, format, aq);

		va_end(aq);

		if (n > -1) {
			if (n < size) {
				_M_used += n;
				break;
			}

			size = n + 1;
		} else {
			size *= 2;
		}

		if (!allocate(size)) {
			return false;
		}
	} while (true);

	return true;
}
