#ifndef TCP_CONNECTION_INL
#define TCP_CONNECTION_INL

#include "net/tcp_server.h"
#include "net/tcp_connection.h"

inline bool tcp_connection::modify(unsigned fd, int events)
{
	return _M_server->modify(fd, events);
}

#endif // TCP_CONNECTION_INL
