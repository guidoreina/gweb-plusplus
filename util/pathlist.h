#ifndef PATHLIST_H
#define PATHLIST_H

#include "string/buffer.h"

class pathlist {
	public:
		// Constructor.
		pathlist();

		// Destructor.
		virtual ~pathlist();

		// Free object.
		void free();

		// Reset object.
		void reset();

		// Get count.
		size_t count() const;

		// Add path.
		bool add(const char* path, unsigned short len);

		// Exists.
		bool exists(const char* path, unsigned short len) const;

	protected:
		static const size_t PATH_ALLOC;
		static const size_t DATA_ALLOC;

		buffer _M_buf;

		struct path {
			size_t offset;
			unsigned short len;
		};

		path* _M_paths;
		size_t _M_size;
		size_t _M_used;

		bool search(const char* path, unsigned short len, size_t& pos) const;
};

inline pathlist::pathlist() : _M_buf(DATA_ALLOC)
{
	_M_paths = NULL;
	_M_size = 0;
	_M_used = 0;
}

inline pathlist::~pathlist()
{
	free();
}

inline void pathlist::reset()
{
	_M_buf.reset();
	_M_used = 0;
}

inline size_t pathlist::count() const
{
	return _M_used;
}

#endif // PATHLIST_H
