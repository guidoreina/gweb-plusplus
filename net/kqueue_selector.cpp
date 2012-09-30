#include <stdlib.h>
#include <unistd.h>
#include "kqueue_selector.h"
#include "net/socket_wrapper.h"
#include "logger/logger.h"

const unsigned iselector::READ = 1;
const unsigned iselector::WRITE = 2;

selector::selector()
{
	_M_fd = -1;
	_M_events = NULL;
}

selector::~selector()
{
	if (_M_fd != -1) {
		close(_M_fd);
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

	if ((_M_fd = kqueue()) < 0) {
		logger::instance().perror("kqueue");
		return false;
	}

	if ((_M_events = (struct kevent*) malloc(_M_size * sizeof(struct kevent))) == NULL) {
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

	struct kevent ev[2];
	unsigned nevents = 0;

	if (modifiable) {
		events = READ | WRITE;
	}

	if (events & READ) {
		EV_SET(&ev[0], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);
		nevents++;
	}

	if (events & WRITE) {
		EV_SET(&ev[nevents], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, NULL);
		nevents++;
	}

	struct timespec timeout = {0, 0};

	if (kevent(_M_fd, ev, nevents, NULL, 0, &timeout) < 0) {
		logger::instance().perror("kevent");

		fdmap::remove(fd);
		return false;
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

	// Events which are attached to file descriptors are automatically deleted
	// on the last close of the descriptor.

	socket_wrapper::close(fd);

	return true;
}

bool selector::wait_for_event()
{
	int ret = kevent(_M_fd, NULL, 0, _M_events, _M_size, NULL);
	if (ret < 0) {
		return false;
	}

	process_events(ret);

	return true;
}

bool selector::wait_for_event(unsigned timeout)
{
	struct timespec ts;
	ts.tv_sec = timeout / 1000;
	ts.tv_nsec = (timeout % 1000) * 1000000;

	int ret = kevent(_M_fd, NULL, 0, _M_events, _M_size, &ts);
	if (ret <= 0) {
		return false;
	}

	process_events(ret);

	return true;
}

void selector::process_events(unsigned nevents)
{
	logger::instance().log(logger::LOG_DEBUG, "[selector::process_events] Processing %d events.", nevents);

	for (unsigned i = 0; i < nevents; i++) {
		unsigned fd = _M_events[i].ident;
		if (_M_descriptors[fd] != -1) {
			if ((_M_events[i].flags & EV_ERROR) || (_M_events[i].flags & EV_EOF)) {
				on_event_error(fd);

				// Remove from set.
				remove(fd);
			} else {
				int event;
				if (_M_events[i].filter == EVFILT_READ) {
					event = READ;
				} else if (_M_events[i].filter == EVFILT_WRITE) {
					event = WRITE;
				} else {
					continue;
				}

				if (!on_event(fd, event)) {
					// Remove from set.
					remove(fd);
				}
			}
		}
	}
}
