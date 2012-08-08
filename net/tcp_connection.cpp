#include <stdlib.h>
#include <errno.h>
#include "tcp_connection.h"
#include "net/tcp_server.h"
#include "net/filesender.h"
#include "logger/logger.h"
#include "util/now.h"
#include "macros/macros.h"

const size_t tcp_connection::READ_BUFFER_SIZE = 1024;
tcp_server* tcp_connection::_M_server = NULL;
size_t tcp_connection::_M_max_read = 0;
size_t tcp_connection::_M_max_write = 0;

tcp_connection::tcp_connection() : _M_in(READ_BUFFER_SIZE), _M_out(READ_BUFFER_SIZE)
{
	_M_inp = 0;
	_M_outp = 0;

	_M_readable = 0;
	_M_writable = 0;

	_M_in_ready_list = 0;
}

tcp_connection::io_result tcp_connection::read(unsigned fd, buffer& in, size_t& total)
{
	// Get the number of bytes to receive.
	size_t count = READ_BUFFER_SIZE;
	if (_M_max_read > 0) {
		count = MIN(count, _M_max_read - total);

		// If we have received too much already...
		if (count == 0) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::read] (fd %d) Received too much data.", fd);

			_M_in_ready_list = 1;
			return IO_NO_DATA_READ;
		}
	}

	// Allocate memory.
	if (!in.allocate(count)) {
		return IO_ERROR;
	}

	// Receive.
	ssize_t ret = socket_wrapper::read(fd, in.data() + in.count(), count);
	if (ret < 0) {
		if (errno == EAGAIN) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::read] (fd %d) EAGAIN.", fd);

			_M_readable = 0;
			return IO_NO_DATA_READ;
		} else if (errno == EINTR) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::read] (fd %d) EINTR.", fd);

			_M_in_ready_list = 1;
			return IO_NO_DATA_READ;
		} else {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::read] (fd %d) errno = %d.", fd, errno);
			return IO_ERROR;
		}
	} else if (ret == 0) {
		// The peer has performed an orderly shutdown.
		logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::read] (fd %d) Connection closed by peer.", fd);
		return IO_ERROR;
	} else {
		logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::read] (fd %d) Received %d bytes.", fd, ret);

		_M_timestamp = now::_M_time;

		in.increment_count(ret);
		total += ret;

		if ((size_t) ret < count) {
			_M_readable = 0;
			return IO_WOULD_BLOCK;
		}

		return IO_SUCCESS;
	}
}

bool tcp_connection::write(unsigned fd, const buffer& out, size_t& total)
{
	// Get the number of bytes to send.
	size_t count = out.count() - _M_outp;
	if (_M_max_write > 0) {
		count = MIN(count, _M_max_write - total);

		// If we have sent too much already...
		if (count == 0) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::write] (fd %d) Sent too much data.", fd);

			_M_in_ready_list = 1;
			return true;
		}
	}

	// Send.
	ssize_t ret = socket_wrapper::write(fd, out.data() + _M_outp, count);
	if (ret < 0) {
		if (errno == EAGAIN) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::write] (fd %d) EAGAIN.", fd);

			_M_writable = 0;
		} else if (errno == EINTR) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::write] (fd %d) EINTR.", fd);

			_M_in_ready_list = 1;
		} else {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::write] (fd %d) errno = %d.", fd, errno);
			return false;
		}
	} else if (ret > 0) {
		logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::write] (fd %d) Sent %d bytes.", fd, ret);

		_M_timestamp = now::_M_time;

		_M_outp += ret;
		total += ret;

		if ((size_t) ret < count) {
			_M_writable = 0;
		}
	}

	return true;
}

bool tcp_connection::writev(unsigned fd, const socket_wrapper::io_vector* io_vector, unsigned iovcnt, size_t& total)
{
	// Too many buffers?
	if (iovcnt > IOV_MAX) {
		return false;
	}

	size_t left;
	if (_M_max_write > 0) {
		left = _M_max_write - total;

		// If we have sent too much already...
		if (left == 0) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::writev] (fd %d) Sent too much data.", fd);

			_M_in_ready_list = 1;
			return true;
		}
	} else {
		left = UINT_MAX;
	}

	off_t outp = _M_outp;

	unsigned i;
	for (i = 0; (i < iovcnt) && (outp >= io_vector[i].iov_len); i++) {
		outp -= io_vector[i].iov_len;
	}

	socket_wrapper::io_vector iov[IOV_MAX];
	unsigned cnt = 0;

	size_t count = 0;

	for (; (i < iovcnt) && (left > 0); i++) {
		iov[cnt].iov_base = (char*) io_vector[i].iov_base + outp;
		iov[cnt].iov_len = MIN(io_vector[i].iov_len - outp, left);

		left -= iov[cnt].iov_len;
		count += iov[cnt].iov_len;

		outp = 0;

		cnt++;
	}

	// Send.
	ssize_t ret = socket_wrapper::writev(fd, iov, cnt);
	if (ret < 0) {
		if (errno == EAGAIN) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::writev] (fd %d) EAGAIN.", fd);

			_M_writable = 0;
		} else if (errno == EINTR) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::writev] (fd %d) EINTR.", fd);

			_M_in_ready_list = 1;
		} else {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::writev] (fd %d) errno = %d.", fd, errno);
			return false;
		}
	} else if (ret > 0) {
		logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::writev] (fd %d) Sent %d bytes.", fd, ret);

		_M_timestamp = now::_M_time;

		_M_outp += ret;
		total += ret;

		if ((size_t) ret < count) {
			_M_writable = 0;
		}
	}

	return true;
}

bool tcp_connection::sendfile(unsigned fd, unsigned in_fd, off_t filesize, const range_list* ranges, size_t nrange, size_t& total)
{
	// Get the number of bytes to send.
	off_t count;

	if ((!ranges) || (ranges->count() == 0)) {
		count = filesize - _M_outp;
	} else {
		const range_list::range* range = ranges->get(nrange);

		count = range->to - _M_outp + 1;
	}

	if (_M_max_write > 0) {
		count = MIN(count, _M_max_write - total);

		// If we have sent too much already...
		if (count == 0) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::sendfile] (fd %d) Sent too much data.", fd);

			_M_in_ready_list = 1;
			return true;
		}
	}

	off_t outp = _M_outp;

	// Send.
	ssize_t ret = filesender::sendfile(fd, in_fd, &_M_outp, count, filesize);
	if (ret < 0) {
		if (errno == EAGAIN) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::sendfile] (fd %d) EAGAIN.", fd);

			off_t sent = _M_outp - outp;

			if (sent > 0) {
				_M_timestamp = now::_M_time;

				total += sent;
			}

			_M_writable = 0;
		} else if (errno == EINTR) {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::sendfile] (fd %d) EINTR.", fd);

			off_t sent = _M_outp - outp;

			if (sent > 0) {
				_M_timestamp = now::_M_time;

				total += sent;
			}

			_M_in_ready_list = 1;
		} else {
			logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::sendfile] (fd %d) errno = %d.", fd, errno);
			return false;
		}
	} else if (ret > 0) {
		logger::instance().log(logger::LOG_DEBUG, "[tcp_connection::sendfile] (fd %d) Sent %d bytes.", fd, ret);

		_M_timestamp = now::_M_time;

		total += ret;

		if ((size_t) ret < count) {
			_M_writable = 0;
		}
	}

	return true;
}
