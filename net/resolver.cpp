#include <string.h>
#include <netdb.h>
#include "resolver.h"
#include "logger/logger.h"

bool resolver::resolve(const char* host, struct sockaddr* addr)
{
#if defined(__linux__) || defined(__sun__)
	struct hostent ret;
	char buf[2048];
#endif

	struct hostent* result;

#if defined(__linux__)
	int error;
	if (gethostbyname_r(host, &ret, buf, sizeof(buf), &result, &error) != 0) {
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
	if ((result = gethostbyname(host)) == NULL) {
#elif defined(__sun__)
	int error;
	if ((result = gethostbyname_r(host, &ret, buf, sizeof(buf), &error)) == NULL) {
#endif
		logger::instance().log(logger::LOG_WARNING, "Couldn't resolve [%s].", host);
		return false;
	}

	struct sockaddr_in* sin = (struct sockaddr_in*) addr;

	sin->sin_family = result->h_addrtype;
	memcpy(&sin->sin_addr, result->h_addr_list[0], result->h_length);
	memset(&sin->sin_zero, 0, sizeof(sin->sin_zero));

	return true;
}
