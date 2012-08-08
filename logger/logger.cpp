#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "logger.h"
#include "util/now.h"

const off_t logger::MAX_SIZE = 1024L * 1024L * 1024L;

logger::logger() : _M_buf(512)
{
	*_M_path = 0;
	_M_pathlen = 0;

	_M_fd = -1;

	_M_last_file = 0;

	_M_enabled = true;

	_M_level = LOG_ERROR;

	_M_max_size = MAX_SIZE;
	_M_size = 0;
}

logger::~logger()
{
	if (_M_fd != -1) {
		close(_M_fd);
	}
}

bool logger::create(level level, const char* dir, const char* filename, off_t max_size)
{
	size_t dirlen = strlen(dir);
	if (dirlen >= sizeof(_M_path)) {
		return false;
	}

	struct stat buf;
	if ((stat(dir, &buf) < 0) || (!S_ISDIR(buf.st_mode))) {
		return false;
	}

	size_t filenamelen = strlen(filename);
	if (filenamelen >= sizeof(_M_path)) {
		return false;
	}

	if (dir[dirlen - 1] == '/') {
		dirlen--;
	}

	if (dirlen + 1 + filenamelen >= sizeof(_M_path)) {
		return false;
	}

	memcpy(_M_path, dir, dirlen);
	_M_path[dirlen] = '/';
	memcpy(_M_path + dirlen + 1, filename, filenamelen);

	_M_pathlen = dirlen + 1 + filenamelen;

	// Search last file.
	do {
		snprintf(_M_path + _M_pathlen, sizeof(_M_path) - _M_pathlen, ".%u", _M_last_file + 1);
	} while ((stat(_M_path, &buf) == 0) && (++_M_last_file));

	_M_path[_M_pathlen] = 0;

	// Open file.
	if ((_M_fd = open(_M_path, O_CREAT | O_WRONLY, 0644)) < 0) {
		return false;
	}

	_M_level = level;

	_M_max_size = max_size;
	_M_size = lseek(_M_fd, 0, SEEK_END);

	return true;
}

bool logger::add(level level, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);

	bool ret = add(level, format, ap);

	va_end(ap);

	return ret;
}

bool logger::add(level level, const char* format, va_list ap)
{
	_M_buf.reset();

	if (!_M_buf.format("[%04d/%02d/%02d %02d:%02d:%02d] ", 1900 + now::_M_tm.tm_year, 1 + now::_M_tm.tm_mon, now::_M_tm.tm_mday, now::_M_tm.tm_hour, now::_M_tm.tm_min, now::_M_tm.tm_sec)) {
		return false;
	}

	switch (level) {
		case LOG_ERROR:
			if (!_M_buf.append("[ERROR] ", 8)) {
				return false;
			}

			break;
		case LOG_WARNING:
			if (!_M_buf.append("[WARNING] ", 10)) {
				return false;
			}

			break;
		case LOG_INFO:
			if (!_M_buf.append("[INFO] ", 7)) {
				return false;
			}

			break;
		case LOG_DEBUG:
			if (!_M_buf.append("[DEBUG] ", 8)) {
				return false;
			}

			break;
	}

	if ((!_M_buf.vformat(format, ap)) || (!_M_buf.append('\n'))) {
		return false;
	}

	if (_M_size + _M_buf.count() > _M_max_size) {
		if (!rotate()) {
			return false;
		}
	}

	size_t count = 0;

	do {
		ssize_t ret;
		if ((ret = write(_M_fd, _M_buf.data() + count, _M_buf.count() - count)) < 0) {
			close(_M_fd);
			_M_fd = -1;

			return false;
		} else if (ret > 0) {
			count += ret;
		}
	} while (count < _M_buf.count());

	_M_size += _M_buf.count();

	return true;
}

bool logger::rotate()
{
	close(_M_fd);

	struct stat buf;

	char newpath[PATH_MAX + 1];
	memcpy(newpath, _M_path, _M_pathlen);

	do {
		snprintf(newpath + _M_pathlen, sizeof(newpath) - _M_pathlen, ".%u", ++_M_last_file);
	} while (stat(newpath, &buf) == 0);

	if (rename(_M_path, newpath) < 0) {
		_M_fd = -1;
		return false;
	}

	if ((_M_fd = open(_M_path, O_CREAT | O_WRONLY, 0644)) < 0) {
		return false;
	}

	_M_size = 0;

	return true;
}
