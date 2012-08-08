#ifndef STRING_LIST_H
#define STRING_LIST_H

#include <string.h>
#include "string/buffer.h"

class string_list {
	public:
		// Constructor.
		string_list();

		// Destructor.
		virtual ~string_list();

		// Free object.
		void free();

		// Reset object.
		void reset();

		// Get count.
		size_t count() const;

		// Add string.
		bool add(const char* string);
		bool add(const char* string, unsigned short len);

		// Exists.
		bool exists(const char* string) const;
		bool exists(const char* string, unsigned short len) const;

	protected:
		static const size_t STRING_ALLOC;
		static const size_t DATA_ALLOC;

		buffer _M_buf;

		struct string {
			size_t offset;
			unsigned short len;
		};

		string* _M_strings;
		size_t _M_size;
		size_t _M_used;

		bool search(const char* string, unsigned short len, size_t& pos) const;
};

inline string_list::string_list() : _M_buf(DATA_ALLOC)
{
	_M_strings = NULL;
	_M_size = 0;
	_M_used = 0;
}

inline string_list::~string_list()
{
	free();
}

inline void string_list::reset()
{
	_M_buf.reset();
	_M_used = 0;
}

inline size_t string_list::count() const
{
	return _M_used;
}

inline bool string_list::add(const char* string)
{
	return add(string, strlen(string));
}

inline bool string_list::exists(const char* string) const
{
	size_t pos;
	return search(string, strlen(string), pos);
}

inline bool string_list::exists(const char* string, unsigned short len) const
{
	size_t pos;
	return search(string, len, pos);
}

#endif // STRING_LIST_H
