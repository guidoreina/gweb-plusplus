#ifndef SOCK_H
#define SOCK_H

#include <sys/socket.h>
#include "net/socket_wrapper.h"

class sock {
	public:
		static const unsigned DEFAULT_CONNECT_TIMEOUT;
		static const unsigned DEFAULT_READ_TIMEOUT;
		static const unsigned DEFAULT_WRITE_TIMEOUT;

		// Constructors.
		sock();
		sock(unsigned sd);

		// Destructor.
		virtual ~sock();

		// Get descriptor.
		int get_descriptor() const;

		// Set descriptor.
		void set_descriptor(unsigned sd);

		// Close socket.
		bool close();

		// Shutdown.
		bool shutdown(int how);

		// Wait readable.
		bool wait_readable(unsigned timeout);

		// Wait writable.
		bool wait_writable(unsigned timeout);

		// Connect.
		bool connect(const char* host, unsigned short port, unsigned timeout = DEFAULT_CONNECT_TIMEOUT);
		bool connect(const struct sockaddr* addr, unsigned timeout = DEFAULT_CONNECT_TIMEOUT);

		// Create listener socket.
		bool listen(unsigned short port);
		bool listen(const char* address, unsigned short port);

		// Read.
		ssize_t read(void* buf, size_t len, unsigned timeout = DEFAULT_READ_TIMEOUT);

		// Read into multiple buffers.
		ssize_t readv(const socket_wrapper::io_vector* iov, int iovcnt, unsigned timeout = DEFAULT_READ_TIMEOUT);

		// Write.
		bool write(const void* buf, size_t len, unsigned timeout = DEFAULT_WRITE_TIMEOUT);

		// Write from multiple buffers.
		bool writev(const socket_wrapper::io_vector* iov, int iovcnt, unsigned timeout = DEFAULT_WRITE_TIMEOUT);

		// Accept.
		bool accept(sock& sock);
		bool accept(sock& sock, struct sockaddr* addr, socklen_t* addrlen);

		// Disable the Nagle algorithm.
		bool disable_nagle_algorithm();

		// Cork.
		bool cork();

		// Uncork.
		bool uncork();

	protected:
		int _M_sd;

		// Create.
		bool create();
};

inline sock::sock()
{
	_M_sd = -1;
}

inline sock::sock(unsigned sd)
{
	_M_sd = sd;
}

inline sock::~sock()
{
	if (_M_sd != -1) {
		close();
	}
}

inline int sock::get_descriptor() const
{
	return _M_sd;
}

inline void sock::set_descriptor(unsigned sd)
{
	_M_sd = sd;
}

inline bool sock::close()
{
	if (!socket_wrapper::close(_M_sd)) {
		return false;
	}

	_M_sd = -1;

	return true;
}

inline bool sock::shutdown(int how)
{
	return socket_wrapper::shutdown(_M_sd, how);
}

inline bool sock::listen(unsigned short port)
{
	return ((_M_sd = socket_wrapper::create_listener(port)) != -1);
}

inline bool sock::listen(const char* address, unsigned short port)
{
	return ((_M_sd = socket_wrapper::create_listener(address, port)) != -1);
}

inline ssize_t sock::read(void* buf, size_t len, unsigned timeout)
{
	if (!wait_readable(timeout)) {
		return -1;
	}

	return socket_wrapper::read(_M_sd, buf, len);
}

inline ssize_t sock::readv(const socket_wrapper::io_vector* iov, int iovcnt, unsigned timeout)
{
	if (!wait_readable(timeout)) {
		return -1;
	}

	return socket_wrapper::readv(_M_sd, iov, iovcnt);
}

inline bool sock::accept(sock& sock)
{
	int sd;
	if ((sd = socket_wrapper::accept(_M_sd)) < 0) {
		return false;
	}

	sock.set_descriptor(sd);

	return true;
}

inline bool sock::accept(sock& sock, struct sockaddr* addr, socklen_t* addrlen)
{
	int sd;
	if ((sd = socket_wrapper::accept(_M_sd, addr, addrlen)) < 0) {
		return false;
	}

	sock.set_descriptor(sd);

	return true;
}

inline bool sock::disable_nagle_algorithm()
{
	return socket_wrapper::disable_nagle_algorithm(_M_sd);
}

inline bool sock::cork()
{
	return socket_wrapper::cork(_M_sd);
}

inline bool sock::uncork()
{
	return socket_wrapper::uncork(_M_sd);
}

inline bool sock::create()
{
	return ((_M_sd = socket_wrapper::create()) != -1);
}

#endif // SOCK_H
