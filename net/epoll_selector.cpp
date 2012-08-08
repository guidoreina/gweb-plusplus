#include <stdlib.h>
#include <unistd.h>
#include "epoll_selector.h"
#include "net/socket_wrapper.h"
#include "logger/logger.h"

const unsigned iselector::READ = EPOLLIN;
const unsigned iselector::WRITE = EPOLLOUT;

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

	if ((_M_fd = epoll_create(_M_size)) < 0) {
		logger::instance().perror("epoll_create");
		return false;
	}

	if ((_M_events = (struct epoll_event*) malloc(_M_size * sizeof(struct epoll_event))) == NULL) {
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

	struct epoll_event ev;

	if (modifiable) {
		ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
	} else {
		ev.events = events;
	}

	ev.data.u64 = 0;
	ev.data.fd = fd;

	if (epoll_ctl(_M_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		logger::instance().perror("epoll_ctl");
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

	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
	ev.data.u64 = 0;
	ev.data.fd = fd;

	if (epoll_ctl(_M_fd, EPOLL_CTL_DEL, fd, &ev) < 0) {
		logger::instance().perror("epoll_ctl");

		socket_wrapper::close(fd);
		return false;
	}

	socket_wrapper::close(fd);

	return true;
}

bool selector::wait_for_event()
{
	int ret = epoll_wait(_M_fd, _M_events, _M_size, -1);
	if (ret < 0) {
		return false;
	}

	process_events(ret);

	return true;
}

bool selector::wait_for_event(unsigned timeout)
{
	int ret = epoll_wait(_M_fd, _M_events, _M_size, timeout);
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
		unsigned fd = _M_events[i].data.fd;
		if (_M_descriptors[fd] != -1) {
			if ((_M_events[i].events & EPOLLIN) || (_M_events[i].events & EPOLLOUT)) {
				if (!on_event(fd, _M_events[i].events)) {
					// Remove from set.
					remove(fd);
				}
			} else {
				on_event_error(fd);

				// Remove from set.
				remove(fd);
			}
		}
	}
}
