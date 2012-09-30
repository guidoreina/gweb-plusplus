#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "select_selector.h"
#include "net/socket_wrapper.h"
#include "logger/logger.h"

const unsigned iselector::READ = 1;
const unsigned iselector::WRITE = 4;

const int selector::EMPTY_SET = -2;
const int selector::UNDEFINED = -1;

selector::selector()
{
	FD_ZERO(&_M_rfds);
	FD_ZERO(&_M_wfds);

	_M_highest_fd = EMPTY_SET;
}

selector::~selector()
{
}

bool selector::add(unsigned fd, int events, bool modifiable)
{
	if (!fdmap::add(fd)) {
		// The file descriptor has been already inserted.
		logger::instance().log(logger::LOG_WARNING, "The file descriptor %d has been already inserted.", fd);
		return true;
	}

	if (events & READ) {
		FD_SET(fd, &_M_rfds);
	}

	if (events & WRITE) {
		FD_SET(fd, &_M_wfds);
	}

	if ((_M_highest_fd == EMPTY_SET) || ((_M_highest_fd != UNDEFINED) && ((int) fd > _M_highest_fd))) {
		_M_highest_fd = fd;
	}

	return true;
}

bool selector::remove(unsigned fd)
{
	if (!fdmap::remove(fd)) {
		// The file descriptor has not been inserted.
		logger::instance().log(logger::LOG_WARNING, "The file descriptor %d has not been inserted.", fd);
		return true;
	}

	FD_CLR(fd, &_M_rfds);
	FD_CLR(fd, &_M_wfds);

	// If the file descriptor was the highest-numbered file descriptor...
	if ((int) fd == _M_highest_fd) {
		_M_highest_fd = UNDEFINED;
	}

	socket_wrapper::close(fd);

	return true;
}

bool selector::modify(unsigned fd, int events)
{
	if (_M_descriptors[fd] < 0) {
		// The file descriptor has not been inserted.
		logger::instance().log(logger::LOG_WARNING, "The file descriptor %d has not been inserted.", fd);
		return false;
	}

	if (events & READ) {
		FD_SET(fd, &_M_rfds);
		FD_CLR(fd, &_M_wfds);
	} else {
		FD_CLR(fd, &_M_rfds);
		FD_SET(fd, &_M_wfds);
	}

	return true;
}

bool selector::wait_for_event()
{
	fd_set rfds = _M_rfds;
	fd_set wfds = _M_wfds;

	int ret = select(compute_highest_fd() + 1, &rfds, &wfds, NULL, NULL);
	if (ret < 0) {
		return false;
	}

	process_events(&rfds, &wfds, ret);

	return true;
}

bool selector::wait_for_event(unsigned timeout)
{
	fd_set rfds = _M_rfds;
	fd_set wfds = _M_wfds;

	struct timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	int ret = select(compute_highest_fd() + 1, &rfds, &wfds, NULL, &tv);
	if (ret <= 0) {
		return false;
	}

	process_events(&rfds, &wfds, ret);

	return true;
}

void selector::process_events(const fd_set* rfds, const fd_set* wfds, unsigned nevents)
{
	logger::instance().log(logger::LOG_DEBUG, "[selector::process_events] Processing %d events.", nevents);

	size_t i = 0;
	while ((i < _M_used) && (nevents > 0)) {
		unsigned fd = _M_index[i];
		int events = 0;

		if (FD_ISSET(fd, rfds)) {
			events |= READ;
			nevents--;
		}

		if (FD_ISSET(fd, wfds)) {
			events |= WRITE;
			nevents--;
		}

		if (events) {
			if (_M_descriptors[fd] != -1) {
				if (!on_event(fd, events)) {
					// Remove from set.
					remove(fd);
					continue;
				}
			}
		}

		i++;
	}
}
