#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "backend_list.h"
#include "net/socket_wrapper.h"
#include "net/resolver.h"

const time_t backend_list::DEFAULT_RETRY_INTERVAL = 30;
const size_t backend_list::BACKEND_ALLOC = 4;

backend_list::backend_list() : _M_buf(256)
{
	_M_max_open_files = 0;

	_M_backends = NULL;
	_M_size = 0;
	_M_used = 0;

	_M_current = 0;

	_M_connections = NULL;

	_M_retry_interval = DEFAULT_RETRY_INTERVAL;
}

void backend_list::free()
{
	_M_max_open_files = 0;

	if (_M_backends) {
		::free(_M_backends);
		_M_backends = NULL;
	}

	_M_size = 0;
	_M_used = 0;

	_M_current = 0;

	if (_M_connections) {
		::free(_M_connections);
		_M_connections = NULL;
	}

	_M_retry_interval = DEFAULT_RETRY_INTERVAL;
}

bool backend_list::create(size_t max_open_files)
{
	_M_max_open_files = max_open_files;

	if ((_M_connections = (struct backend**) malloc(_M_max_open_files * sizeof(struct backend*))) == NULL) {
		return false;
	}

	return true;
}

bool backend_list::add(const char* host, unsigned short hostlen, unsigned short port)
{
	size_t offset = _M_buf.count();

	if (!_M_buf.append_nul_terminated_string(host, hostlen)) {
		return false;
	}

	struct sockaddr addr;
	if (!resolver::resolve(_M_buf.data() + offset, &addr)) {
		return false;
	}

	((struct sockaddr_in*) &addr)->sin_port = htons(port);

	if (_M_used == _M_size) {
		size_t size = _M_size + BACKEND_ALLOC;
		struct backend* backends = (struct backend*) realloc(_M_backends, size * sizeof(struct backend));
		if (!backends) {
			return false;
		}

		_M_backends = backends;
		_M_size = size;
	}

	backend* backend = &_M_backends[_M_used];

	backend->offset = offset;
	backend->hostlen = hostlen;

	backend->port = port;

	memcpy(&backend->addr, &addr, sizeof(struct sockaddr));

	backend->available = true;

	_M_used++;

	return true;
}

int backend_list::connect(const char*& host, unsigned short& hostlen, unsigned short& port)
{
	unsigned first = _M_current;

	do {
		backend* backend = &_M_backends[_M_current];

		_M_current = (_M_current + 1) % _M_used; // Round-robin.

		if ((backend->available) || (backend->downtime + _M_retry_interval <= now::_M_time)) {
			int sd;
			if ((sd = socket_wrapper::connect(&backend->addr)) != -1) {
				_M_connections[sd] = backend;

				host = _M_buf.data() + backend->offset;
				hostlen = backend->hostlen;

				port = backend->port;

				return sd;
			}

			backend->available = false;
			backend->downtime = now::_M_time;
		}
	} while (_M_current != first);

	return -1;
}
