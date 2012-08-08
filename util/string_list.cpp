#include <stdlib.h>
#include "string_list.h"
#include "macros/macros.h"

const size_t string_list::STRING_ALLOC = 64;
const size_t string_list::DATA_ALLOC = 2 * 1024;

void string_list::free()
{
	_M_buf.free();

	if (_M_strings) {
		::free(_M_strings);
		_M_strings = NULL;
	}

	_M_size = 0;
	_M_used = 0;
}

bool string_list::add(const char* string, unsigned short len)
{
	size_t pos;
	if (search(string, len, pos)) {
		// Already inserted.
		return true;
	}

	size_t offset = _M_buf.count();

	if (!_M_buf.append(string, len)) {
		return false;
	}

	if (_M_used == _M_size) {
		size_t size = _M_size + STRING_ALLOC;
		struct string* strings = (struct string*) realloc(_M_strings, size * sizeof(struct string));
		if (!strings) {
			return false;
		}

		_M_strings = strings;
		_M_size = size;
	}

	if (pos < _M_used) {
		memmove(&_M_strings[pos + 1], &_M_strings[pos], (_M_used - pos) * sizeof(struct string));
	}

	_M_strings[pos].offset = offset;
	_M_strings[pos].len = len;

	_M_used++;

	return true;
}

bool string_list::search(const char* string, unsigned short len, size_t& pos) const
{
	const char* data = _M_buf.data();

	int i = 0;
	int j = _M_used - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		int ret = memcmp(string, data + _M_strings[pivot].offset, MIN(len, _M_strings[pivot].len));

		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (len < _M_strings[pivot].len) {
				j = pivot - 1;
			} else if (len == _M_strings[pivot].len) {
				pos = (size_t) pivot;
				return true;
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	pos = (size_t) i;

	return false;
}
