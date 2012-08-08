#include <stdlib.h>
#include "range_list.h"

const size_t range_list::RANGE_ALLOC = 2;

void range_list::free()
{
	if (_M_ranges) {
		::free(_M_ranges);
		_M_ranges = NULL;
	}

	_M_size = 0;
	_M_used = 0;
}

bool range_list::add(off_t from, off_t to)
{
	if (to < from) {
		return false;
	}

	if (_M_used == _M_size) {
		size_t size = _M_size + RANGE_ALLOC;
		struct range* ranges = (struct range*) realloc(_M_ranges, size * sizeof(struct range));
		if (!ranges) {
			return false;
		}

		_M_ranges = ranges;
		_M_size = size;
	}

	_M_ranges[_M_used].from = from;
	_M_ranges[_M_used].to = to;

	_M_used++;

	return true;
}
