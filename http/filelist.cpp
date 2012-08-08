#include <stdlib.h>
#include <string.h>
#include "filelist.h"

const size_t filelist::FILE_ALLOC = 64;

filelist::filelist()
{
	_M_files = NULL;
	_M_index = NULL;

	_M_size = 0;
	_M_used = 0;

	_M_sort_criteria = SORT_BY_NAME;
	_M_sort_order = ASCENDING;
}

void filelist::free()
{
	if (_M_files) {
		::free(_M_files);
		_M_files = NULL;
	}

	if (_M_index) {
		::free(_M_index);
		_M_index = NULL;
	}

	_M_size = 0;
	_M_used = 0;

	_M_sort_criteria = SORT_BY_NAME;
	_M_sort_order = ASCENDING;
}

bool filelist::insert(const char* name, unsigned short namelen, unsigned short utf8len, off_t size, time_t mtime)
{
	if (!allocate()) {
		return false;
	}

	file* file = &(_M_files[_M_used]);

	memcpy(file->name, name, namelen);
	file->name[namelen] = 0;

	file->namelen = namelen;
	file->utf8len = utf8len;

	file->size = size;
	file->mtime = mtime;

	size_t position = search(name, namelen, size, mtime);

	if (position < _M_used) {
		memmove(&(_M_index[position + 1]), &(_M_index[position]), (_M_used - position) * sizeof(unsigned));
	}

	_M_index[position] = _M_used++;

	return true;
}

size_t filelist::search(const char* name, unsigned short namelen, off_t size, time_t mtime) const
{
	int i = 0;
	int j = _M_used - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		const file* file = &(_M_files[_M_index[pivot]]);

		int ret;

		switch (_M_sort_criteria) {
			case SORT_BY_NAME:
				ret = strcmp(name, file->name);
				break;
			case SORT_BY_NAMELEN:
				if (namelen < file->namelen) {
					ret = -1;
				} else if (namelen == file->namelen) {
					ret = 0;
				} else {
					ret = 1;
				}

				break;
			case SORT_BY_SIZE:
				if (size < file->size) {
					ret = -1;
				} else if (size == file->size) {
					ret = 0;
				} else {
					ret = 1;
				}

				break;
			default:
				// SORT_BY_DATE
				if (mtime < file->mtime) {
					ret = -1;
				} else if (mtime == file->mtime) {
					ret = 0;
				} else {
					ret = 1;
				}
		}

		ret *= (int) _M_sort_order;

		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			return (size_t) pivot + 1;
		} else {
			i = pivot + 1;
		}
	}

	return (size_t) i;
}

bool filelist::allocate()
{
	if (_M_used == _M_size) {
		size_t size = _M_size + FILE_ALLOC;
		unsigned* index = (unsigned*) malloc(size * sizeof(unsigned));
		if (!index) {
			return false;
		}

		file* files = (struct file*) realloc(_M_files, size * sizeof(struct file));
		if (!files) {
			::free(index);
			return false;
		}

		if (_M_index) {
			memcpy(index, _M_index, _M_used * sizeof(unsigned));
			::free(_M_index);
		}

		_M_files = files;
		_M_index = index;

		_M_size = size;
	}

	return true;
}
