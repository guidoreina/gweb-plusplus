#ifndef TMPFILES_CACHE_H
#define TMPFILES_CACHE_H

#include <limits.h>

class tmpfiles_cache {
	public:
		static const size_t DEFAULT_MAX_SPARE_FILES;

		// Constructor.
		tmpfiles_cache();

		// Destructor.
		virtual ~tmpfiles_cache();

		// Create.
		bool create(const char* dir, size_t max_open_files, size_t max_spare_files = DEFAULT_MAX_SPARE_FILES);

		// Open.
		int open();

		// Close.
		bool close(unsigned fd);

	protected:
		static const size_t TMPFILE_NAME_LEN;

		char _M_path[PATH_MAX + 1];
		size_t _M_dirlen;

		struct tmpfile {
			unsigned count;
			unsigned fd;
		};

		struct tmpfiles {
			tmpfile* files;
			size_t size;
			size_t used;
		};

		tmpfiles _M_in_use;
		tmpfiles _M_spare_files;

		size_t _M_count;
};

#endif // TMPFILES_CACHE_H
