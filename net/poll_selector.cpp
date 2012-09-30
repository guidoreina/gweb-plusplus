#include <stdlib.h>
#include <unistd.h>
#include "poll_selector.h"
#include "net/socket_wrapper.h"
#include "logger/logger.h"

const unsigned iselector::READ = POLLIN;
const unsigned iselector::WRITE = POLLOUT;

selector::selector()
{
	_M_events = NULL;
}

selector::~selector()
{
	if (_M_events) {
		free(_M_events);
	}
}

bool selector::create()
{
	if (!fdmap::create()) {
		return false;
	}

	if ((_M_events = (struct pollfd*) malloc(_M_size * sizeof(struct pollfd))) == NULL) {
		return false;
	}

	return true;
}

bool selector::add(unsigned fd, int events, bool modifiable)
{
	if (!fdmap::add(fd)) {
		// The file descriptor has been already inserted.
		logger::instance().log(logger::LOG_WARNING, "The file descriptor %d has been already inserted.", fd);
		return true;
	}

	_M_events[_M_used - 1].fd = fd;
	_M_events[_M_used - 1].events = events;
	_M_events[_M_used - 1].revents = 0;

	return true;
}

bool selector::remove(unsigned fd)
{
	// Save index.
	int index = _M_descriptors[fd];

	if (!fdmap::remove(fd)) {
		// The file descriptor has not been inserted.
		logger::instance().log(logger::LOG_WARNING, "The file descriptor %d has not been inserted.", fd);
		return true;
	}

	if ((size_t) index < _M_used) {
		_M_events[index] = _M_events[_M_used];
	}

	socket_wrapper::close(fd);

	return true;
}

bool selector::modify(unsigned fd, int events)
{
	int index = _M_descriptors[fd];

	if (index < 0) {
		// The file descriptor has not been inserted.
		logger::instance().log(logger::LOG_WARNING, "The file descriptor %d has not been inserted.", fd);
		return false;
	}

	_M_events[index].events = events;
	_M_events[index].revents = 0;

	return true;
}

bool selector::wait_for_event()
{
	int ret = poll(_M_events, _M_used, -1);
	if (ret < 0) {
		return false;
	}

	process_events(ret);

	return true;
}

bool selector::wait_for_event(unsigned timeout)
{
	int ret = poll(_M_events, _M_used, timeout);
	if (ret <= 0) {
		return false;
	}

	process_events(ret);

	return true;
}

void selector::process_events(unsigned nevents)
{
	logger::instance().log(logger::LOG_DEBUG, "[selector::process_events] Processing %d events.", nevents);

	size_t i = 0;
	while ((i < _M_used) && (nevents > 0)) {
		if (_M_events[i].revents != 0) {
			nevents--;

			unsigned fd = _M_events[i].fd;
			if (_M_descriptors[fd] != -1) {
				if ((_M_events[i].revents & POLLIN) || (_M_events[i].revents & POLLOUT)) {
					if (!on_event(fd, _M_events[i].revents)) {
						// Remove from set.
						remove(fd);
						continue;
					}
				} else {
					on_event_error(fd);

					// Remove from set.
					remove(fd);

					continue;
				}
			}
		}

		i++;
	}
}
