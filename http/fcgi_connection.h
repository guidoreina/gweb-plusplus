#ifndef FCGI_CONNECTION_H
#define FCGI_CONNECTION_H

#include <sys/socket.h>
#include "net/tcp_connection.h"
#include "http/http_connection.h"
#include "http/fastcgi.h"
#include "file/file_wrapper.h"

struct fcgi_connection : public tcp_connection,
                         public fastcgi {
	// FastCGI states.
	static const unsigned char CONNECTING_STATE;
	static const unsigned char SENDING_REQUEST_STATE;
	static const unsigned char SENDING_STDIN_STATE;
	static const unsigned char READING_HEADERS_STATE;
	static const unsigned char READING_BODY_STATE;
	static const unsigned char PREPARING_ERROR_PAGE_STATE;
	static const unsigned char RESPONSE_COMPLETED_STATE;

	int _M_fd;

	int _M_tmpfile;

	http_connection* _M_client;

	unsigned _M_state:4;

	// Constructor.
	fcgi_connection();

	// Destructor.
	virtual ~fcgi_connection();

	// Free.
	virtual void free();

	// Reset.
	virtual void reset();

	// Loop.
	virtual bool loop(unsigned fd);

	// Read response.
	bool read_response(unsigned fd, size_t& total);

	// Prepare HTTP response.
	bool prepare_http_response(unsigned fd);

	virtual bool get_values_result(const char* name, unsigned short namelen, const char* value, unsigned short valuelen);

	virtual bool stdout_stream(unsigned short requestId, const void* buf, unsigned short len);
	virtual bool stderr_stream(unsigned short requestId, const void* buf, unsigned short len);

	virtual bool end_request(unsigned short requestId, unsigned appStatus, unsigned char protocolStatus);

	// State to string.
	static const char* state_to_string(unsigned state);
};

inline fcgi_connection::~fcgi_connection()
{
}

inline void fcgi_connection::free()
{
	tcp_connection::free();
}

inline bool fcgi_connection::get_values_result(const char* name, unsigned short namelen, const char* value, unsigned short valuelen)
{
	return true;
}

inline bool fcgi_connection::stderr_stream(unsigned short requestId, const void* buf, unsigned short len)
{
	return true;
}

inline bool fcgi_connection::end_request(unsigned short requestId, unsigned appStatus, unsigned char protocolStatus)
{
	_M_state = RESPONSE_COMPLETED_STATE;
	return true;
}

#endif // FCGI_CONNECTION_H
