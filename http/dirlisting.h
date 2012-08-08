#ifndef DIRLISTING_H
#define DIRLISTING_H

#include <sys/types.h>
#include <limits.h>
#include "http/filelist.h"
#include "string/buffer.h"

class dirlisting {
	public:
		static const off_t MAX_FOOTER_SIZE;

		// Constructor.
		dirlisting();

		// Destructor.
		virtual ~dirlisting();

		// Free.
		void free();

		// Set root directory.
		bool set_root(const char* root, size_t rootlen);

		// Set sort criteria.
		bool set_sort_criteria(filelist::sort_criteria sort_criteria);

		// Set sort order.
		bool set_sort_order(filelist::sort_order sort_order);

		// Show files with exact size?
		void set_exact_size(bool exact_size);

		// Load footer.
		bool load_footer(const char* filename);

		// Build directory listing.
		bool build(const char* dir, size_t dirlen, buffer& buf);

	protected:
		static const unsigned short WIDTH_OF_NAME_COLUMN;

		filelist _M_directories;
		filelist _M_files;

		size_t _M_rootlen;

		char _M_path[PATH_MAX + 1];
		size_t _M_pathlen;

		bool _M_exact_size;

		buffer _M_footer;

		bool build_file_lists();
};

inline dirlisting::~dirlisting()
{
}

inline void dirlisting::free()
{
	_M_directories.free();
	_M_files.free();
	_M_footer.free();
}

inline bool dirlisting::set_sort_criteria(filelist::sort_criteria sort_criteria)
{
	return _M_directories.set_sort_criteria(sort_criteria) && _M_files.set_sort_criteria(sort_criteria);
}

inline bool dirlisting::set_sort_order(filelist::sort_order sort_order)
{
	return _M_directories.set_sort_order(sort_order) && _M_files.set_sort_order(sort_order);
}

inline void dirlisting::set_exact_size(bool exact_size)
{
	_M_exact_size = exact_size;
}

#endif // DIRLISTING_H
