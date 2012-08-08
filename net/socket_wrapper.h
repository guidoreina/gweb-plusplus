#ifndef SOCKET_WRAPPER_H
#define SOCKET_WRAPPER_H

#include <sys/socket.h>

class socket_wrapper {
	public:
		static const char* ANY_ADDRESS;
		static const unsigned BACKLOG;

		struct io_vector {
			void* iov_base;
			size_t iov_len;
		};

		// Create socket.
		static int create();

		// Create listener socket.
		static int create_listener(unsigned short port);
		static int create_listener(const char* address, unsigned short port);

		// Make socket non-blocking.
		static bool set_non_blocking(int sd);

		// Close socket.
		static bool close(int sd);

		// Shutdown.
		static bool shutdown(int sd, int how);

		// Connect.
		static int connect(const char* host, unsigned short port);
		static int connect(const struct sockaddr* addr);

		// Get socket error.
		static bool get_socket_error(int sd, int& error);

		// Read.
		static ssize_t read(int sd, void* buf, size_t len);

		// Read into multiple buffers.
		static ssize_t readv(int sd, const io_vector* iov, int iovcnt);

		// Write.
		static ssize_t write(int sd, const void* buf, size_t len);

		// Write from multiple buffers.
		static ssize_t writev(int sd, const io_vector* iov, int iovcnt);

		// Accept.
		static int accept(int sd);
		static int accept(int sd, struct sockaddr* addr, socklen_t* addrlen);

		// Disable the Nagle algorithm.
		static bool disable_nagle_algorithm(int sd);

		// Cork.
		static bool cork(int sd);

		// Uncork.
		static bool uncork(int sd);
};

inline int socket_wrapper::create_listener(unsigned short port)
{
	return create_listener(ANY_ADDRESS, port);
}

#endif // SOCKET_WRAPPER_H
