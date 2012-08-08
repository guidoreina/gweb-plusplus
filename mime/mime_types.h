#ifndef MIME_TYPES_H
#define MIME_TYPES_H

#include "string/buffer.h"

class mime_types {
	public:
		static const char* MIME_TYPES_DEFAULT_FILE;

		static const char* DEFAULT_MIME_TYPE;
		static const unsigned short DEFAULT_MIME_TYPE_LEN;

		// Constructor.
		mime_types();

		// Destructor.
		virtual ~mime_types();

		// Free object.
		void free();

		// Reset object.
		void reset();

		// Load.
		bool load(const char* filename = MIME_TYPES_DEFAULT_FILE);

		// Get MIME type.
		const char* get_mime_type(const char* extension, unsigned short extensionlen, unsigned short& typelen) const;

	protected:
		static const size_t MIME_TYPES_ALLOC;

		buffer _M_buf;

		struct mime_type {
			size_t extension;
			unsigned short extensionlen;

			size_t type;
			unsigned short typelen;
		};

		mime_type* _M_mime_types;
		size_t _M_size;
		size_t _M_used;

		bool search(const char* extension, unsigned short extensionlen, size_t& position) const;

		bool add(size_t type, unsigned short typelen, const char* extension, unsigned short extensionlen);
};

inline mime_types::~mime_types()
{
	free();
}

inline void mime_types::reset()
{
	_M_buf.reset();
	_M_used = 0;
}

inline const char* mime_types::get_mime_type(const char* extension, unsigned short extensionlen, unsigned short& typelen) const
{
	size_t position;
	if (!search(extension, extensionlen, position)) {
		typelen = DEFAULT_MIME_TYPE_LEN;
		return DEFAULT_MIME_TYPE;
	}

	typelen = _M_mime_types[position].typelen;

	return _M_buf.data() + _M_mime_types[position].type;
}

#endif // MIME_TYPES_H
