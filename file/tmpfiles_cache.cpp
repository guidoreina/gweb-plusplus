#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "tmpfiles_cache.h"
#include "file/file_wrapper.h"
#include "logger/logger.h"

const size_t tmpfiles_cache::DEFAULT_MAX_SPARE_FILES = 32;
const size_t tmpfiles_cache::TMPFILE_NAME_LEN = 6;

tmpfiles_cache::tmpfiles_cache()
{
	*_M_path = 0;
	_M_dirlen = 0;

	_M_in_use.files = NULL;
	_M_in_use.size = 0;
	_M_in_use.used = 0;

	_M_spare_files.files = NULL;
	_M_spare_files.size = 0;
	_M_spare_files.used = 0;

	_M_count = 0;
}

tmpfiles_cache::~tmpfiles_cache()
{
	tmpfiles* files[2] = {&_M_in_use, &_M_spare_files};

	for (size_t i = 0; i < 2; i++) {
		if (files[i]->files) {
			for (size_t j = 0; j < files[i]->used; j++) {
				file_wrapper::close(files[i]->files[j].fd);

				sprintf(_M_path + _M_dirlen, "%0*u", TMPFILE_NAME_LEN, files[i]->files[j].count);
				unlink(_M_path);
			}

			free(files[i]->files);
		}
	}
}

bool tmpfiles_cache::create(const char* dir, size_t max_open_files, size_t max_spare_files)
{
	struct stat buf;
	if ((stat(dir, &buf) < 0) || (!S_ISDIR(buf.st_mode))) {
		return false;
	}

	size_t len = strlen(dir);
	if (dir[len - 1] == '/') {
		len--;
	}

	if (len + 1 + TMPFILE_NAME_LEN >= sizeof(_M_path)) {
		return false;
	}

	if ((_M_in_use.files = (struct tmpfile*) malloc(max_open_files * sizeof(struct tmpfile))) == NULL) {
		return false;
	}

	if ((_M_spare_files.files = (struct tmpfile*) malloc(max_spare_files * sizeof(struct tmpfile))) == NULL) {
		return false;
	}

	memcpy(_M_path, dir, len);
	_M_path[len++] = '/';

	_M_dirlen = len;

	_M_in_use.size = max_open_files;
	_M_spare_files.size = max_spare_files;

	return true;
}

int tmpfiles_cache::open()
{
	// If there is a temporary file available...
	if (_M_spare_files.used > 0) {
		unsigned count = _M_spare_files.files[--_M_spare_files.used].count;
		_M_in_use.files[_M_in_use.used].count = count;

		unsigned fd = _M_spare_files.files[_M_spare_files.used].fd;
		_M_in_use.files[_M_in_use.used++].fd = fd;

		file_wrapper::seek(fd, 0, SEEK_SET);

		logger::instance().log(logger::LOG_DEBUG, "Reusing file %0*u, fd %u.", TMPFILE_NAME_LEN, count, fd);

		return fd;
	}

	tmpfile* file = &_M_in_use.files[_M_in_use.used];

	file->count = _M_count++;
	sprintf(_M_path + _M_dirlen, "%0*u", TMPFILE_NAME_LEN, file->count);

	int fd;
	if ((fd = file_wrapper::open(_M_path, O_CREAT | O_TRUNC | O_RDWR, 0644)) < 0) {
		logger::instance().log(logger::LOG_ERROR, "Couldn't create temporary file %s.", _M_path);
		return -1;
	}

	file->fd = fd;

	_M_in_use.used++;

	logger::instance().log(logger::LOG_DEBUG, "Created temporary file %0*u, fd %u.", TMPFILE_NAME_LEN, file->count, fd);

	return fd;
}

bool tmpfiles_cache::close(unsigned fd)
{
	for (size_t i = 0; i < _M_in_use.used; i++) {
		tmpfile* file = &_M_in_use.files[i];

		if (fd == file->fd) {
			_M_in_use.used--;

			// Too many spare files?
			if (_M_spare_files.used == _M_spare_files.size) {
				logger::instance().log(logger::LOG_DEBUG, "Closing spare file %0*u, fd %u.", TMPFILE_NAME_LEN, file->count, fd);

				file_wrapper::close(fd);

				sprintf(_M_path + _M_dirlen, "%0*u", TMPFILE_NAME_LEN, file->count);
				unlink(_M_path);
			} else {
				logger::instance().log(logger::LOG_DEBUG, "Temporary file %0*u (fd %u) will be reused.", TMPFILE_NAME_LEN, file->count, fd);

				_M_spare_files.files[_M_spare_files.used].count = file->count;
				_M_spare_files.files[_M_spare_files.used++].fd = fd;
			}

			if (i < _M_in_use.used) {
				_M_in_use.files[i].count = _M_in_use.files[_M_in_use.used].count;
				_M_in_use.files[i].fd = _M_in_use.files[_M_in_use.used].fd;
			}

			return true;
		}
	}

	logger::instance().log(logger::LOG_WARNING, "[tmpfiles_cache::close] Temporary file (fd %u) not found.", fd);

	return false;
}
