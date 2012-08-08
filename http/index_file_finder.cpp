#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "index_file_finder.h"

#ifdef NAME_MAX
#undef NAME_MAX
#endif

#define NAME_MAX 255

bool index_file_finder::add(const char* name, size_t len)
{
	// Too many index files?
	if (_M_used == MAX_INDEX_FILES) {
		return false;
	}

	if (len > NAME_MAX) {
		return false;
	}

	index_file* index = &(_M_index_files[_M_used]);

	index->offset = _M_buf.count();

	if (!_M_buf.append_nul_terminated_string(name, len)) {
		return false;
	}

	index->len = len;

	_M_used++;

	return true;
}

bool index_file_finder::search(char* dir, size_t& dirlen, struct stat* buf) const
{
	for (size_t i = 0; i < _M_used; i++) {
		const index_file* index = &(_M_index_files[i]);

		size_t len = dirlen + index->len;
		if (len <= PATH_MAX) {
			memcpy(dir + dirlen, _M_buf.data() + index->offset, index->len);
			dir[len] = 0;

			if (stat(dir, buf) == 0) {
				dirlen = len;
				return true;
			}
		}
	}

	return false;
}
