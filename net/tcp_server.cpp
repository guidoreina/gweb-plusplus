#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "tcp_server.h"
#include "logger/logger.h"
#include "util/now.h"

const unsigned tcp_server::MAX_IDLE_TIME = 30;

tcp_server::tcp_server(bool client_writes_first)
{
	tcp_connection::_M_server = this;

	*_M_address = 0;
	_M_port = 0;

	_M_listener = -1;

	_M_connections = NULL;

	_M_client_writes_first = client_writes_first;

	_M_ready_list = NULL;
	_M_nready = 0;

	_M_max_idle_time = MAX_IDLE_TIME;

	_M_must_stop = true;

	_M_handle_alarm = false;
}

tcp_server::~tcp_server()
{
	if (_M_connections) {
		free(_M_connections);
	}

	if (_M_ready_list) {
		free(_M_ready_list);
	}
}

bool tcp_server::create(const char* address, unsigned short port)
{
	size_t addrlen = strlen(address);
	if (addrlen >= sizeof(_M_address)) {
		return false;
	}

	if (!selector::create()) {
		return false;
	}

	if ((_M_listener = socket_wrapper::create_listener(address, port)) < 0) {
		return false;
	}

	if (!add(_M_listener, selector::READ, false)) {
		socket_wrapper::close(_M_listener);
		_M_listener = -1;

		return false;
	}

	if ((_M_connections = (tcp_connection**) malloc(_M_size * sizeof(tcp_connection*))) == NULL) {
		return false;
	}

	if (!create_connections()) {
		return false;
	}

	if ((_M_ready_list = (unsigned*) malloc(2 * _M_size * sizeof(unsigned))) == NULL) {
		return false;
	}

	memcpy(_M_address, address, addrlen);
	_M_address[addrlen] = 0;

	_M_port = port;

	return true;
}

void tcp_server::start()
{
	_M_must_stop = false;

	do {
		if (_M_handle_alarm) {
			handle_alarm();
			_M_handle_alarm = false;
		}

		process_ready_list();

		wait_for_event();
	} while (!_M_must_stop);

	logger::instance().log(logger::LOG_INFO, "Server stopped.");
}

bool tcp_server::on_event(unsigned fd, int events)
{
	// New connection?
	if ((int) fd == _M_listener) {
		on_new_connection();
	} else {
		tcp_connection* conn = _M_connections[fd];

		if ((events & READ) != 0) {
			conn->_M_readable = 1;
		}

		if ((events & WRITE) != 0) {
			conn->_M_writable = 1;
		}

		logger::instance().log(logger::LOG_DEBUG, "%s event for fd %d.", ((conn->_M_readable) && (conn->_M_writable)) ? "READ & WRITE" : conn->_M_readable ? "READ" : "WRITE", fd);

		if (!conn->loop(fd)) {
			conn->free();
			return false;
		}

		if (conn->_M_in_ready_list) {
			// Add to ready list.
			_M_ready_list[_M_nready++] = fd;

			logger::instance().log(logger::LOG_DEBUG, "Added fd %d to ready list.", fd);
		}
	}

	return true;
}

int tcp_server::on_new_connection()
{
	int fd;
	struct sockaddr addr;
	socklen_t addrlen = sizeof(struct sockaddr);
	if ((fd = socket_wrapper::accept(_M_listener, &addr, &addrlen)) < 0) {
		return -1;
	}

#if DEBUG
	const unsigned char* ip = (const unsigned char*) &(((struct sockaddr_in*) &addr)->sin_addr);
	logger::instance().log(logger::LOG_INFO, "New connection from %d.%d.%d.%d (fd %d).", ip[0], ip[1], ip[2], ip[3], fd);
#endif

	if (!allow_connection(addr)) {
#if DEBUG
		logger::instance().log(logger::LOG_INFO, "Connection from %d.%d.%d.%d (fd %d) not allowed.", ip[0], ip[1], ip[2], ip[3], fd);
#endif

		socket_wrapper::close(fd);
		return -1;
	}

	if (!socket_wrapper::disable_nagle_algorithm(fd)) {
		socket_wrapper::close(fd);
		return -1;
	}

	if (!add(fd, _M_client_writes_first ? READ : WRITE, true)) {
		socket_wrapper::close(fd);
		return -1;
	}

	_M_connections[fd]->_M_addr = addr;
	_M_connections[fd]->_M_timestamp = now::_M_time;

	return fd;
}

void tcp_server::process_ready_list()
{
	logger::instance().log(logger::LOG_DEBUG, "Processing %d connection(s) from the ready list.", _M_nready);

	unsigned nready = _M_nready;
	for (unsigned i = 0; i < nready; i++) {
		unsigned fd = _M_ready_list[i];
		tcp_connection* conn = _M_connections[fd];

		conn->_M_in_ready_list = 0;

		if (!conn->loop(fd)) {
			remove(fd);
			conn->free();
		}
	}

	for (unsigned i = nready; i < _M_nready; i++) {
		_M_ready_list[i - nready] = _M_ready_list[i];
	}

	_M_nready -= nready;
}

void tcp_server::handle_alarm()
{
	logger::instance().log(logger::LOG_DEBUG, "Handling alarm...");

	// Drop connections without activity.
	size_t i = 1;
	while (i < _M_used) {
		unsigned fd = _M_index[i];
		tcp_connection* conn = _M_connections[fd];
		if (conn->_M_timestamp + (time_t) _M_max_idle_time < now::_M_time) {
			logger::instance().log(logger::LOG_INFO, "Connection fd %d timed out.", fd);

			remove(fd);
			conn->free();
		} else {
			i++;
		}
	}
}
