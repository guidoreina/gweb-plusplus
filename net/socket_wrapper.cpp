#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include "socket_wrapper.h"
#include "net/resolver.h"
#include "logger/logger.h"

const char* socket_wrapper::ANY_ADDRESS = "0.0.0.0";
const unsigned socket_wrapper::BACKLOG = 128;

int socket_wrapper::create()
{
	int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sd < 0) {
		logger::instance().perror("socket");
		return -1;
	}

	if (!set_non_blocking(sd)) {
		close(sd);
		return -1;
	}

	return sd;
}

int socket_wrapper::create_listener(const char* address, unsigned short port)
{
	int sd = create();
	if (sd < 0) {
		return -1;
	}

	// Reuse address.
	int optval = 1;
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) < 0) {
		logger::instance().perror("setsockopt");

		close(sd);
		return -1;
	}

	struct sockaddr_in addr;
	if (inet_pton(AF_INET, address, &addr.sin_addr) <= 0) {
		logger::instance().perror("inet_pton");

		close(sd);
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

	if (bind(sd, (const struct sockaddr*) &addr, sizeof(struct sockaddr)) < 0) {
		logger::instance().perror("bind");

		close(sd);
		return -1;
	}

	if (listen(sd, BACKLOG) < 0) {
		logger::instance().perror("listen");

		close(sd);
		return -1;
	}

	return sd;
}

bool socket_wrapper::set_non_blocking(int sd)
{
	int flags = fcntl(sd, F_GETFL);
	flags |= O_NONBLOCK;

	if (fcntl(sd, F_SETFL, flags) < 0) {
		logger::instance().perror("fcntl");
		return false;
	}

	return true;
}

bool socket_wrapper::close(int sd)
{
	if (::close(sd) < 0) {
		logger::instance().perror("close");
		return false;
	}

	return true;
}

bool socket_wrapper::shutdown(int sd, int how)
{
	if (::shutdown(sd, how) < 0) {
		logger::instance().perror("shutdown");
		return false;
	}

	return true;
}

int socket_wrapper::connect(const char* host, unsigned short port)
{
	struct sockaddr addr;
	if (!resolver::resolve(host, &addr)) {
		return -1;
	}

	((struct sockaddr_in*) &addr)->sin_port = htons(port);

	return connect(&addr);
}

int socket_wrapper::connect(const struct sockaddr* addr)
{
	int sd = create();
	if (sd < 0) {
		return -1;
	}

	if ((::connect(sd, addr, sizeof(struct sockaddr)) < 0) && (errno != EINPROGRESS)) {
		logger::instance().perror("connect");

		close(sd);
		return -1;
	}

	return sd;
}

bool socket_wrapper::get_socket_error(int sd, int& error)
{
	socklen_t errorlen = sizeof(error);
	if (getsockopt(sd, SOL_SOCKET, SO_ERROR, &error, &errorlen) < 0) {
		logger::instance().perror("getsockopt");
		return false;
	}

	return true;
}

ssize_t socket_wrapper::read(int sd, void* buf, size_t len)
{
	ssize_t ret;

	if ((ret = recv(sd, buf, len, 0)) < 0) {
		logger::instance().perror(logger::LOG_INFO, "recv");
		return -1;
	}

	return ret;
}

ssize_t socket_wrapper::readv(int sd, const io_vector* iov, int iovcnt)
{
	ssize_t ret;

	if ((ret = ::readv(sd, (const struct iovec*) iov, iovcnt)) < 0) {
		logger::instance().perror(logger::LOG_INFO, "readv");
		return -1;
	}

	return ret;
}

ssize_t socket_wrapper::write(int sd, const void* buf, size_t len)
{
	ssize_t ret;

	if ((ret = send(sd, buf, len, 0)) < 0) {
		logger::instance().perror(logger::LOG_INFO, "send");
		return -1;
	}

	return ret;
}

ssize_t socket_wrapper::writev(int sd, const io_vector* iov, int iovcnt)
{
	ssize_t ret;

	if ((ret = ::writev(sd, (const struct iovec*) iov, iovcnt)) < 0) {
		logger::instance().perror(logger::LOG_INFO, "writev");
		return -1;
	}

	return ret;
}

int socket_wrapper::accept(int sd)
{
	int client = ::accept(sd, NULL, NULL);
	if (client < 0) {
		logger::instance().perror("accept");
		return -1;
	}

	if (!set_non_blocking(client)) {
		close(client);
		return -1;
	}

	return client;
}

int socket_wrapper::accept(int sd, struct sockaddr* addr, socklen_t* addrlen)
{
	int client = ::accept(sd, addr, addrlen);
	if (client < 0) {
		logger::instance().perror("accept");
		return -1;
	}

	if (!set_non_blocking(client)) {
		close(client);
		return -1;
	}

	return client;
}

bool socket_wrapper::disable_nagle_algorithm(int sd)
{
	int optval = 1;
	if (setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int)) < 0) {
		logger::instance().perror("setsockopt");
		return false;
	}

	return true;
}

bool socket_wrapper::cork(int sd)
{
#if HAVE_TCP_CORK
	int optval = 1;
	if (setsockopt(sd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(int)) < 0) {
		logger::instance().perror("setsockopt");
		return false;
	}
#elif HAVE_TCP_NOPUSH
	int optval = 1;
	if (setsockopt(sd, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(int)) < 0) {
		logger::instance().perror("setsockopt");
		return false;
	}
#endif

	return true;
}

bool socket_wrapper::uncork(int sd)
{
#if HAVE_TCP_CORK
	int optval = 0;
	if (setsockopt(sd, IPPROTO_TCP, TCP_CORK, &optval, sizeof(int)) < 0) {
		logger::instance().perror("setsockopt");
		return false;
	}
#elif HAVE_TCP_NOPUSH
	int optval = 0;
	if (setsockopt(sd, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(int)) < 0) {
		logger::instance().perror("setsockopt");
		return false;
	}
#endif

	return true;
}
