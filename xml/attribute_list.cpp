#include <stdlib.h>
#include "attribute_list.h"

const size_t attribute_list::ATTRIBUTE_ALLOC = 2;

attribute_list::attribute_list() : _M_buf(128)
{
	_M_attributes = NULL;
	_M_size = 0;
	_M_used = 0;
}

void attribute_list::free()
{
	_M_buf.free();

	if (_M_attributes) {
		::free(_M_attributes);
		_M_attributes = NULL;
	}

	_M_size = 0;
	_M_used = 0;
}

bool attribute_list::add(const char* name, size_t namelen, const char* value, size_t valuelen)
{
	size_t name_offset = _M_buf.count();

	if (!_M_buf.append_nul_terminated_string(name, namelen)) {
		return false;
	}

	size_t value_offset = _M_buf.count();

	if (!_M_buf.append_nul_terminated_string(value, valuelen)) {
		return false;
	}

	if (_M_used == _M_size) {
		size_t size = _M_size + ATTRIBUTE_ALLOC;
		struct attribute* attributes = (struct attribute*) realloc(_M_attributes, size * sizeof(struct attribute));
		if (!attributes) {
			return false;
		}

		_M_attributes = attributes;
		_M_size = size;
	}

	attribute* attribute = &(_M_attributes[_M_used]);

	attribute->name = name_offset;
	attribute->namelen = namelen;

	attribute->value = value_offset;
	attribute->valuelen = valuelen;

	_M_used++;

	return true;
}
