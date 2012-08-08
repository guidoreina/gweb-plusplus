#ifndef HTTP_ERROR_H
#define HTTP_ERROR_H

#include "http/http_headers.h"
#include "string/buffer.h"

struct http_connection;

class http_error {
	public:
		static const unsigned short OK;
		static const unsigned short MOVED_PERMANENTLY;
		static const unsigned short NOT_MODIFIED;
		static const unsigned short BAD_REQUEST;
		static const unsigned short FORBIDDEN;
		static const unsigned short NOT_FOUND;
		static const unsigned short LENGTH_REQUIRED;
		static const unsigned short REQUEST_ENTITY_TOO_LARGE;
		static const unsigned short REQUEST_URI_TOO_LONG;
		static const unsigned short REQUESTED_RANGE_NOT_SATISFIABLE;
		static const unsigned short INTERNAL_SERVER_ERROR;
		static const unsigned short NOT_IMPLEMENTED;
		static const unsigned short BAD_GATEWAY;
		static const unsigned short SERVICE_UNAVAILABLE;
		static const unsigned short GATEWAY_TIMEOUT;

		// Get reason phrase.
		static const char* get_reason_phrase(unsigned short status_code);

		// Set a pointer to the headers.
		static void set_headers(http_headers* headers);

		// Set port.
		static void set_port(unsigned short port);

		// Build page.
		static bool build_page(http_connection* conn);

	private:
		struct error {
			unsigned short status_code;
			const char* reason_phrase;
			buffer body;
		};

		static error _M_errors[];

		static http_headers* _M_headers;

		static unsigned short _M_port;

		static error* search(unsigned short status_code);
};

inline const char* http_error::get_reason_phrase(unsigned short status_code)
{
	error* err;
	if ((err = search(status_code)) == NULL) {
		return NULL;
	}

	return err->reason_phrase;
}

inline void http_error::set_headers(http_headers* headers)
{
	_M_headers = headers;
}

inline void http_error::set_port(unsigned short port)
{
	_M_port = port;
}

#endif // HTTP_ERROR_H
