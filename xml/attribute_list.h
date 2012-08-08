#ifndef ATTRIBUTE_LIST_H
#define ATTRIBUTE_LIST_H

#include "string/buffer.h"

class attribute_list {
	public:
		// Constructor.
		attribute_list();

		// Destructor.
		virtual ~attribute_list();

		// Free.
		void free();

		// Reset.
		void reset();

		// Get count.
		size_t count() const;

		// Add attribute.
		bool add(const char* name, size_t namelen, const char* value, size_t valuelen);

		// Get attribute.
		bool get(size_t idx, const char*& name, size_t& namelen, const char*& value, size_t& valuelen) const;
		bool get(size_t idx, const char*& name, const char*& value) const;

	protected:
		static const size_t ATTRIBUTE_ALLOC;

		buffer _M_buf;

		struct attribute {
			size_t name;
			size_t namelen;

			size_t value;
			size_t valuelen;
		};

		attribute* _M_attributes;
		size_t _M_size;
		size_t _M_used;
};

inline attribute_list::~attribute_list()
{
	free();
}

inline void attribute_list::reset()
{
	_M_buf.reset();
	_M_used = 0;
}

inline size_t attribute_list::count() const
{
	return _M_used;
}

inline bool attribute_list::get(size_t idx, const char*& name, size_t& namelen, const char*& value, size_t& valuelen) const
{
	if (idx >= _M_used) {
		return false;
	}

	attribute* attribute = &(_M_attributes[idx]);

	name = _M_buf.data() + attribute->name;
	namelen = attribute->namelen;

	value = _M_buf.data() + attribute->value;
	valuelen = attribute->valuelen;

	return true;
}

inline bool attribute_list::get(size_t idx, const char*& name, const char*& value) const
{
	if (idx >= _M_used) {
		return false;
	}

	attribute* attribute = &(_M_attributes[idx]);

	name = _M_buf.data() + attribute->name;
	value = _M_buf.data() + attribute->value;

	return true;
}

#endif // ATTRIBUTE_LIST_H
