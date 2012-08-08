#ifndef FILELIST_H
#define FILELIST_H

#include <sys/types.h>

class filelist {
	public:
		enum sort_criteria {
			SORT_BY_NAME,
			SORT_BY_NAMELEN,
			SORT_BY_SIZE,
			SORT_BY_DATE
		};

		enum sort_order {
			ASCENDING = 1,
			DESCENDING = -1
		};

		struct file {
			char name[256];
			unsigned short namelen;
			unsigned short utf8len;
			off_t size;
			time_t mtime;
		};

		// Constructor.
		filelist();

		// Destructor.
		virtual ~filelist();

		// Free list.
		void free();

		// Reset list.
		void reset();

		// Set sort criteria.
		bool set_sort_criteria(sort_criteria sort_criteria);

		// Set sort order.
		bool set_sort_order(sort_order sort_order);

		// Insert file.
		bool insert(const char* name, unsigned short namelen, unsigned short utf8len, off_t size, time_t mtime);

		// Get file.
		const file* get_file(unsigned idx) const;

	protected:
		static const size_t FILE_ALLOC;

		file* _M_files;
		unsigned* _M_index;

		size_t _M_size;
		size_t _M_used;

		sort_criteria _M_sort_criteria;
		sort_order _M_sort_order;

		size_t search(const char* name, unsigned short namelen, off_t size, time_t mtime) const;

		bool allocate();
};

inline filelist::~filelist()
{
	free();
}

inline void filelist::reset()
{
	_M_used = 0;
}

inline bool filelist::set_sort_criteria(sort_criteria sort_criteria)
{
	if (_M_used > 0) {
		return false;
	}

	_M_sort_criteria = sort_criteria;

	return true;
}

inline bool filelist::set_sort_order(sort_order sort_order)
{
	if (_M_used > 0) {
		return false;
	}

	_M_sort_order = sort_order;

	return true;
}

inline const filelist::file* filelist::get_file(unsigned idx) const
{
	if (idx >= _M_used) {
		return NULL;
	}

	return &(_M_files[_M_index[idx]]);
}

#endif // FILELIST_H
