#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#if HAVE_EPOLL
	#include "net/epoll_selector.h"
#elif HAVE_KQUEUE
	#include "net/kqueue_selector.h"
#elif HAVE_PORT
	#include "net/port_selector.h"
#elif HAVE_POLL
	#include "net/poll_selector.h"
#else
	#include "net/select_selector.h"
#endif

#include "net/tcp_connection.h"
#include "net/socket_wrapper.h"

class tcp_server : protected selector {
	friend struct tcp_connection;

	public:
		// Start.
		virtual void start();

		// Stop.
		virtual void stop();

		// On alarm.
		virtual void on_alarm();

		// Get port.
		unsigned short get_port() const;

	protected:
		static const unsigned MAX_IDLE_TIME; // [seconds]

		// Address to bind to.
		char _M_address[16];

		// Port to bind to.
		unsigned short _M_port;

		// Listener socket.
		int _M_listener;

		// TCP connections.
		tcp_connection** _M_connections;

		bool _M_client_writes_first;

		unsigned* _M_ready_list;
		unsigned _M_nready;

		unsigned _M_max_idle_time;

		bool _M_must_stop;

		bool _M_handle_alarm;

		// Constructor.
		tcp_server(bool client_writes_first);

		// Destructor.
		virtual ~tcp_server();

		// Create.
		virtual bool create(unsigned short port);
		virtual bool create(const char* address, unsigned short port);

		// Create connections.
		virtual bool create_connections() = 0;

		// Delete connections.
		virtual void delete_connections() = 0;

		// On event.
		virtual bool on_event(unsigned fd, int events);

		// On event error.
		virtual void on_event_error(unsigned fd);

		// On new connection.
		virtual int on_new_connection();

		// Allow connection?
		virtual bool allow_connection(const struct sockaddr& addr);

		// Process ready list.
		virtual void process_ready_list();

		// Handle alarm.
		virtual void handle_alarm();
};

inline void tcp_server::stop()
{
	_M_must_stop = true;
}

inline void tcp_server::on_alarm()
{
	_M_handle_alarm = true;
}

inline unsigned short tcp_server::get_port() const
{
	return _M_port;
}

inline bool tcp_server::create(unsigned short port)
{
	return create(socket_wrapper::ANY_ADDRESS, port);
}

inline void tcp_server::on_event_error(unsigned fd)
{
	_M_connections[fd]->free();
}

inline bool tcp_server::allow_connection(const struct sockaddr& addr)
{
	return true;
}

#endif // TCP_SERVER_H
