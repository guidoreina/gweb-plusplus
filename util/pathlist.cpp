#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "pathlist.h"
#include "logger/logger.h"
#include "macros/macros.h"

const size_t pathlist::PATH_ALLOC = 8;
const size_t pathlist::DATA_ALLOC = 2 * 1024;

void pathlist::free()
{
	_M_buf.free();

	if (_M_paths) {
		::free(_M_paths);
		_M_paths = NULL;
	}

	_M_size = 0;
	_M_used = 0;
}

bool pathlist::add(const char* path, unsigned short len)
{
	if ((*path != '/') || (len > PATH_MAX)) {
		logger::instance().log(logger::LOG_WARNING, "Invalid path (%.*s).", len, path);
		return false;
	}

	if (len > 1) {
		// Remove trailing '/' (if present).
		if (path[len - 1] == '/') {
			len--;
		}
	}

	size_t pos;
	if (search(path, len, pos)) {
		// Already inserted.
		logger::instance().log(logger::LOG_DEBUG, "Path (%.*s) already added.", len, path);
		return true;
	}

	size_t offset = _M_buf.count();

	if (!_M_buf.append(path, len)) {
		return false;
	}

	if (_M_used == _M_size) {
		size_t size = _M_size + PATH_ALLOC;
		struct path* paths = (struct path*) realloc(_M_paths, size * sizeof(struct path));
		if (!paths) {
			return false;
		}

		_M_paths = paths;
		_M_size = size;
	}

	if (pos < _M_used) {
		memmove(&_M_paths[pos + 1], &_M_paths[pos], (_M_used - pos) * sizeof(struct path));
	}

	_M_paths[pos].offset = offset;
	_M_paths[pos].len = len;

	_M_used++;

	logger::instance().log(logger::LOG_DEBUG, "Path (%.*s) has been added.", len, path);

	return true;
}

bool pathlist::exists(const char* path, unsigned short len) const
{
	if (len > 1) {
		// Remove trailing '/' (if present).
		if (path[len - 1] == '/') {
			len--;
		}
	}

	size_t l = 1;

	do {
		size_t pos;
		if (search(path, l, pos)) {
			logger::instance().log(logger::LOG_DEBUG, "Path (%.*s) exists.", len, path);
			return true;
		}

		if (l == len) {
			logger::instance().log(logger::LOG_DEBUG, "Path (%.*s) doesn't exist.", len, path);
			return false;
		}

		for (++l; l < len; l++) {
			if (path[l] == '/') {
				break;
			}
		}
	} while (true);
}

bool pathlist::search(const char* path, unsigned short len, size_t& pos) const
{
	const char* data = _M_buf.data();

	int i = 0;
	int j = _M_used - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		int ret = memcmp(path, data + _M_paths[pivot].offset, MIN(len, _M_paths[pivot].len));

		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (len < _M_paths[pivot].len) {
				j = pivot - 1;
			} else if (len == _M_paths[pivot].len) {
				pos = (size_t) pivot;
				return true;
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	pos = (size_t) i;

	return false;
}
