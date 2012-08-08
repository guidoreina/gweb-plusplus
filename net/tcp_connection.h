#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <limits.h>
#include "net/socket_wrapper.h"
#include "util/range_list.h"
#include "string/buffer.h"

class tcp_server;

struct tcp_connection {
	static const size_t READ_BUFFER_SIZE;

	static tcp_server* _M_server;

	static size_t _M_max_read;
	static size_t _M_max_write;

	struct sockaddr _M_addr;

	time_t _M_timestamp;

	buffer _M_in;
	buffer _M_out;

	off_t _M_inp;
	off_t _M_outp;

	unsigned _M_readable:1;
	unsigned _M_writable:1;

	unsigned _M_in_ready_list:1;

	// Constructor.
	tcp_connection();

	// Destructor.
	virtual ~tcp_connection();

	// Free.
	virtual void free();

	// Reset.
	virtual void reset();

	// Modify descriptor.
	bool modify(unsigned fd, int events);

	enum io_result {
		IO_ERROR,
		IO_NO_DATA_READ,
		IO_WOULD_BLOCK,
		IO_SUCCESS
	};

	// Read.
	io_result read(unsigned fd, size_t& total);
	io_result read(unsigned fd, buffer& in, size_t& total);

	// Write.
	bool write(unsigned fd, size_t& total);
	bool write(unsigned fd, const buffer& out, size_t& total);

	// Writev.
	bool writev(unsigned fd, const buffer** bufs, unsigned nbufs, size_t& total);
	bool writev(unsigned fd, const socket_wrapper::io_vector* io_vector, unsigned iovcnt, size_t& total);

	// Send file.
	bool sendfile(unsigned fd, unsigned in_fd, off_t filesize, size_t& total);
	bool sendfile(unsigned fd, unsigned in_fd, off_t filesize, const range_list* ranges, size_t nrange, size_t& total);

	// Loop.
	virtual bool loop(unsigned fd) = 0;
};

inline tcp_connection::~tcp_connection()
{
}

inline void tcp_connection::free()
{
	reset();

	_M_in.reset();
	_M_inp = 0;

	_M_readable = 0;
	_M_writable = 0;
}

inline void tcp_connection::reset()
{
	_M_out.reset();
	_M_outp = 0;

	_M_in_ready_list = 0;
}

inline tcp_connection::io_result tcp_connection::read(unsigned fd, size_t& total)
{
	return read(fd, _M_in, total);
}

inline bool tcp_connection::write(unsigned fd, size_t& total)
{
	return write(fd, _M_out, total);
}

inline bool tcp_connection::writev(unsigned fd, const buffer** bufs, unsigned nbufs, size_t& total)
{
	// Too many buffers?
	if (nbufs > IOV_MAX) {
		return false;
	}

	socket_wrapper::io_vector io_vector[IOV_MAX];

	for (unsigned i = 0; i < nbufs; i++) {
		io_vector[i].iov_base = (void*) bufs[i]->data();
		io_vector[i].iov_len = bufs[i]->count();
	}

	return writev(fd, io_vector, nbufs, total);
}

inline bool tcp_connection::sendfile(unsigned fd, unsigned in_fd, off_t filesize, size_t& total)
{
	return sendfile(fd, in_fd, filesize, NULL, 0, total);
}

#endif // TCP_CONNECTION_H
