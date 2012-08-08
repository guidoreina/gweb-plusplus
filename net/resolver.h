#ifndef RESOLVER_H
#define RESOLVER_H

#include <sys/socket.h>
#include <netinet/in.h>

class resolver {
	public:
		static bool resolve(const char* host, struct sockaddr* addr);
};

#endif // RESOLVER_H
