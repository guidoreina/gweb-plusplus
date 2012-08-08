#ifndef INDEX_FILE_FINDER_H
#define INDEX_FILE_FINDER_H

#include <sys/stat.h>
#include "string/buffer.h"

#define MAX_INDEX_FILES 32

class index_file_finder {
	public:
		// Constructor.
		index_file_finder();

		// Destructor.
		virtual ~index_file_finder();

		// Reset.
		void reset();

		// Add index file.
		bool add(const char* name, size_t len);

		// Search index file.
		bool search(char* dir, size_t& dirlen, struct stat* buf) const;

	protected:
		buffer _M_buf;

		struct index_file {
			size_t offset;
			size_t len;
		};

		index_file _M_index_files[MAX_INDEX_FILES];
		size_t _M_used;
};

inline index_file_finder::index_file_finder()
{
	_M_used = 0;
}

inline index_file_finder::~index_file_finder()
{
}

inline void index_file_finder::reset()
{
	_M_buf.reset();
	_M_used = 0;
}

#endif // INDEX_FILE_FINDER_H
