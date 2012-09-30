#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include "port_selector.h"
#include "net/socket_wrapper.h"
#include "logger/logger.h"

const unsigned iselector::READ = POLLIN;
const unsigned iselector::WRITE = POLLOUT;

selector::selector()
{
	_M_fd = -1;
	_M_port_events = NULL;
	_M_events = NULL;
}

selector::~selector()
{
	if (_M_fd != -1) {
		close(_M_fd);
	}

	if (_M_port_events) {
		free(_M_port_events);
	}

	if (_M_events) {
		free(_M_events);
	}
}

bool selector::create()
{
	if (!fdmap::create()) {
		return false;
	}

	if ((_M_fd = port_create()) < 0) {
		logger::instance().perror("port_create");
		return false;
	}

	if ((_M_port_events = (port_event_t*) malloc(_M_size * sizeof(port_event_t))) == NULL) {
		return false;
	}

	if ((_M_events = (int*) malloc(_M_size * sizeof(int))) == NULL) {
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

	if (port_associate(_M_fd, PORT_SOURCE_FD, (uintptr_t) fd, events, NULL) < 0) {
		logger::instance().perror("port_associate");

		fdmap::remove(fd);
		return false;
	}

	_M_events[fd] = events;

	return true;
}

bool selector::remove(unsigned fd)
{
	if (!fdmap::remove(fd)) {
		// The file descriptor has not been inserted.
		logger::instance().log(logger::LOG_WARNING, "The file descriptor %d has not been inserted.", fd);
		return true;
	}

	if (port_dissociate(_M_fd, PORT_SOURCE_FD, (uintptr_t) fd) < 0) {
		logger::instance().perror("port_dissociate");

		socket_wrapper::close(fd);
		return false;
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

	_M_events[fd] = events;

	return true;
}

bool selector::wait_for_event()
{
	uint_t nget = 1;
	if (port_getn(_M_fd, _M_port_events, _M_size, &nget, NULL) < 0) {
		return false;
	}

	process_events(nget);

	return true;
}

bool selector::wait_for_event(unsigned timeout)
{
	uint_t nget = 1;

	struct timespec ts;
	ts.tv_sec = timeout / 1000;
	ts.tv_nsec = (timeout % 1000) * 1000000;

	if ((port_getn(_M_fd, _M_port_events, _M_size, &nget, &ts) < 0) || (nget == 0)) {
		return false;
	}

	process_events(nget);

	return true;
}

void selector::process_events(unsigned nevents)
{
	logger::instance().log(logger::LOG_DEBUG, "[selector::process_events] Processing %d events.", nevents);

	for (unsigned i = 0; i < nevents; i++) {
		unsigned fd = (unsigned) _M_port_events[i].portev_object;
		if (_M_descriptors[fd] != -1) {
			if ((_M_port_events[i].portev_events & POLLIN) || (_M_port_events[i].portev_events & POLLOUT)) {
				if (!on_event(fd, _M_port_events[i].portev_events)) {
					// Remove from set (the file descriptor is already disassociated).
					fdmap::remove(fd);

					socket_wrapper::close(fd);
				} else {
					// Reassociate the file descriptor.
					if (port_associate(_M_fd, PORT_SOURCE_FD, (uintptr_t) fd, (int) _M_events[fd], NULL) < 0) {
						logger::instance().perror("port_associate");

						on_event_error(fd);

						fdmap::remove(fd);

						socket_wrapper::close(fd);
					}
				}
			} else {
				on_event_error(fd);

				// Remove from set (the file descriptor is already disassociated).
				fdmap::remove(fd);

				socket_wrapper::close(fd);
			}
		}
	}
}
