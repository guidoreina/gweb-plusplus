#ifndef BACKEND_LIST_H
#define BACKEND_LIST_H

#include <time.h>
#include <sys/socket.h>
#include "string/buffer.h"
#include "util/now.h"

class backend_list {
	public:
		static const time_t DEFAULT_RETRY_INTERVAL;

		// Constructor.
		backend_list();

		// Destructor.
		virtual ~backend_list();

		// Free object.
		void free();

		// Reset object.
		void reset();

		// Set retry interval.
		void set_retry_interval(time_t retry_interval);

		// Create.
		bool create(size_t max_open_files);

		// Add backend.
		bool add(const char* host, unsigned short hostlen, unsigned short port);

		// Connect.
		int connect(const char*& host, unsigned short& hostlen, unsigned short& port);

		// Connection failed.
		void connection_failed(unsigned fd);

	protected:
		static const size_t BACKEND_ALLOC;

		buffer _M_buf;

		size_t _M_max_open_files;

		struct backend {
			size_t offset;
			unsigned short hostlen;

			unsigned short port;

			struct sockaddr addr;

			bool available;
			time_t downtime;
		};

		backend* _M_backends;
		size_t _M_size;
		size_t _M_used;

		size_t _M_current;

		struct backend** _M_connections;

		time_t _M_retry_interval;
};

inline backend_list::~backend_list()
{
	free();
}

inline void backend_list::reset()
{
	_M_used = 0;
	_M_current = 0;
}

inline void backend_list::set_retry_interval(time_t retry_interval)
{
	_M_retry_interval = retry_interval;
}

inline void backend_list::connection_failed(unsigned fd)
{
	_M_connections[fd]->available = false;
	_M_connections[fd]->downtime = now::_M_time;
}

#endif // BACKEND_LIST_H
