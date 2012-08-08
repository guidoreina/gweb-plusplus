#include <stdlib.h>
#include <sys/select.h>
#include <errno.h>
#include <limits.h>
#include "sock.h"
#include "logger/logger.h"

const unsigned sock::DEFAULT_CONNECT_TIMEOUT = 10 * 1000; // [ms]
const unsigned sock::DEFAULT_READ_TIMEOUT = 10 * 1000; // [ms]
const unsigned sock::DEFAULT_WRITE_TIMEOUT = 10 * 1000; // [ms]

bool sock::wait_readable(unsigned timeout)
{
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(_M_sd, &rfds);

	struct timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	return (select(_M_sd + 1, &rfds, NULL, NULL, &tv) == 1);
}

bool sock::wait_writable(unsigned timeout)
{
	fd_set wfds;
	FD_ZERO(&wfds);
	FD_SET(_M_sd, &wfds);

	struct timeval tv;
	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	return (select(_M_sd + 1, NULL, &wfds, NULL, &tv) == 1);
}

bool sock::connect(const char* host, unsigned short port, unsigned timeout)
{
	if ((_M_sd = socket_wrapper::connect(host, port)) < 0) {
		return false;
	}

	// Wait socket is writable.
	if (!wait_writable(timeout)) {
		return false;
	}

	int error;
	if (!socket_wrapper::get_socket_error(_M_sd, error)) {
		return false;
	}

	if (error) {
		logger::instance().log(logger::LOG_WARNING, "Socket (%u) error: %d.", _M_sd, error);
		return false;
	}

	return true;
}

bool sock::connect(const struct sockaddr* addr, unsigned timeout)
{
	if (!create()) {
		return false;
	}

	if ((::connect(_M_sd, addr, sizeof(struct sockaddr)) < 0) && (errno != EINPROGRESS)) {
		logger::instance().perror("connect");
		return false;
	}

	// Wait socket is writable.
	if (!wait_writable(timeout)) {
		return false;
	}

	int error;
	if (!socket_wrapper::get_socket_error(_M_sd, error)) {
		return false;
	}

	if (error) {
		logger::instance().log(logger::LOG_WARNING, "Socket (%u) error: %d.", _M_sd, error);
		return false;
	}

	return true;
}

bool sock::write(const void* buf, size_t len, unsigned timeout)
{
	const char* ptr = (const char*) buf;
	size_t written = 0;

	do {
		if (!wait_writable(timeout)) {
			logger::instance().log(logger::LOG_WARNING, "Socket (%u) timeout.", _M_sd);
			return false;
		}

		ssize_t ret = socket_wrapper::write(_M_sd, ptr + written, len - written);
		if (ret < 0) {
			if ((errno != EINTR) && (errno != EAGAIN)) {
				return false;
			}
		} else if (ret > 0) {
			written += ret;
		}
	} while (written < len);

	return true;
}

bool sock::writev(const socket_wrapper::io_vector* iov, int iovcnt, unsigned timeout)
{
	if ((iovcnt < 0) || (iovcnt > IOV_MAX)) {
		return false;
	}

	socket_wrapper::io_vector vec[IOV_MAX];
	size_t count = 0;

	for (int i = 0; i < iovcnt; i++) {
		vec[i].iov_base = iov[i].iov_base;
		vec[i].iov_len = iov[i].iov_len;

		count += vec[i].iov_len;
	}

	socket_wrapper::io_vector* v = vec;
	size_t written = 0;

	do {
		if (!wait_writable(timeout)) {
			logger::instance().log(logger::LOG_WARNING, "Socket (%u) timeout.", _M_sd);
			return false;
		}

		ssize_t ret = socket_wrapper::writev(_M_sd, v, iovcnt);
		if (ret < 0) {
			if ((errno != EINTR) && (errno != EAGAIN)) {
				logger::instance().perror(logger::LOG_ERROR, "writev");
				return false;
			}
		} else if (ret > 0) {
			written += ret;

			if (written == count) {
				return true;
			}

			while ((size_t) ret >= v->iov_len) {
				ret -= v->iov_len;
				v++;
			}

			if (ret > 0) {
				v->iov_base = (char*) v->iov_base + ret;
				v->iov_len -= ret;
			}
		}
	} while (true);
}
