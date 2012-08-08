#ifndef RANGE_LIST_H
#define RANGE_LIST_H

#include <sys/types.h>

class range_list {
	public:
		struct range {
			off_t from;
			off_t to;
		};

		// Constructor.
		range_list();

		// Destructor.
		virtual ~range_list();

		// Free object.
		void free();

		// Reset object.
		void reset();

		// Get count.
		size_t count() const;

		// Get.
		bool get(unsigned idx, off_t& from, off_t& to) const;
		const range* get(unsigned idx) const;

		// Add range.
		bool add(off_t from, off_t to);

	protected:
		static const size_t RANGE_ALLOC;

		range* _M_ranges;
		size_t _M_size;
		size_t _M_used;
};

inline range_list::range_list()
{
	_M_ranges = NULL;
	_M_size = 0;
	_M_used = 0;
}

inline range_list::~range_list()
{
	free();
}

inline void range_list::reset()
{
	_M_used = 0;
}

inline size_t range_list::count() const
{
	return _M_used;
}

inline bool range_list::get(unsigned idx, off_t& from, off_t& to) const
{
	if (idx >= _M_used) {
		return false;
	}

	from = _M_ranges[idx].from;
	to = _M_ranges[idx].to;

	return true;
}

inline const range_list::range* range_list::get(unsigned idx) const
{
	if (idx >= _M_used) {
		return NULL;
	}

	return &(_M_ranges[idx]);
}

#endif // RANGE_LIST_H
