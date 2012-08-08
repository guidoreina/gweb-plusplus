#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include "fdmap.h"

fdmap::fdmap()
{
	_M_descriptors = NULL;
	_M_size = 0;
	_M_used = 0;

	_M_index = NULL;
}

fdmap::~fdmap()
{
	if (_M_index) {
		for (size_t i = 0; i < _M_used; i++) {
			close(_M_index[i]);
		}

		free(_M_index);
	}

	if (_M_descriptors) {
		free(_M_descriptors);
	}
}

bool fdmap::create()
{
	struct rlimit rlim;

	// Get the maximum number of file descriptors.
	if (getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
		return false;
	}

	_M_descriptors = (int*) malloc(rlim.rlim_cur * sizeof(int));
	if (!_M_descriptors) {
		return false;
	}

	_M_index = (unsigned*) malloc(rlim.rlim_cur * sizeof(unsigned));
	if (!_M_index) {
		return false;
	}

	_M_size = rlim.rlim_cur;

	for (size_t i = 0; i < _M_size; i++) {
		_M_descriptors[i] = -1;
	}

	return true;
}

bool fdmap::add(unsigned fd)
{
	if (_M_descriptors[fd] != -1) {
		// Already inserted.
		return false;
	}

	_M_descriptors[fd] = _M_used;
	_M_index[_M_used++] = fd;

	return true;
}

bool fdmap::remove(unsigned fd)
{
	int index = _M_descriptors[fd];
	if (index == -1) {
		// Not inserted.
		return false;
	}

	_M_descriptors[fd] = -1;

	_M_used--;

	if ((size_t) index < _M_used) {
		_M_index[index] = _M_index[_M_used];
		_M_descriptors[_M_index[index]] = index;
	}

	return true;
}
