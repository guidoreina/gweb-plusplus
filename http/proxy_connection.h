#ifndef PROXY_CONNECTION_H
#define PROXY_CONNECTION_H

#include <sys/types.h>
#include <sys/socket.h>
#include "net/tcp_connection.h"
#include "http/http_connection.h"
#include "http/chunked_parser.h"
#include "file/file_wrapper.h"

struct proxy_connection : public tcp_connection,
                          public chunked_parser {
	static const unsigned short STATUS_LINE_MAX_LEN;

	// FastCGI states.
	static const unsigned char CONNECTING_STATE;
	static const unsigned char SENDING_HEADERS_STATE;
	static const unsigned char SENDING_BODY_STATE;
	static const unsigned char READING_STATUS_LINE_STATE;
	static const unsigned char READING_HEADERS_STATE;
	static const unsigned char PROCESSING_RESPONSE_STATE;
	static const unsigned char READING_BODY_STATE;
	static const unsigned char READING_CHUNKED_BODY_STATE;
	static const unsigned char READING_UNKNOWN_SIZE_BODY_STATE;
	static const unsigned char PREPARING_ERROR_PAGE_STATE;
	static const unsigned char RESPONSE_COMPLETED_STATE;

	int _M_fd;

	int _M_tmpfile;

	http_connection* _M_client;

	off_t _M_left;

	unsigned short _M_status_code;

	unsigned short _M_reason_phrase;
	unsigned short _M_reason_phrase_len;

	size_t _M_body_offset;

	unsigned _M_state:4;
	unsigned _M_substate:4;

	unsigned _M_major_number:4;
	unsigned _M_minor_number:4;

	// Constructor.
	proxy_connection();

	// Destructor.
	virtual ~proxy_connection();

	// Free.
	virtual void free();

	// Reset.
	virtual void reset();

	// Loop.
	virtual bool loop(unsigned fd);

	// Read status line.
	bool read_status_line(unsigned fd, size_t& total);

	// Read headers.
	bool read_headers(unsigned fd, size_t& total);

	// Read body.
	bool read_body(unsigned fd, size_t& total);

	// Read chunked body.
	bool read_chunked_body(unsigned fd, size_t& total);

	// Read unknown size body.
	bool read_unknown_size_body(unsigned fd, size_t& total);

	enum parse_result {
		PARSE_ERROR,
		PARSING_NOT_COMPLETED,
		PARSING_COMPLETED
	};

	// Parse status line.
	parse_result parse_status_line(unsigned fd);

	// Parse headers.
	parse_result parse_headers(unsigned fd);

	// Process response.
	virtual bool process_response(unsigned fd);

	// Prepare HTTP response.
	bool prepare_http_response(unsigned fd);

	// Add chunked data.
	bool add_chunked_data(const char* buf, size_t len);

	// State to string.
	static const char* state_to_string(unsigned state);
};

inline proxy_connection::~proxy_connection()
{
}

inline void proxy_connection::free()
{
	tcp_connection::free();
}

inline bool proxy_connection::add_chunked_data(const char* buf, size_t len)
{
	if (!file_wrapper::write(_M_tmpfile, buf, len)) {
		return false;
	}

	_M_client->_M_filesize += len;

	return true;
}

#endif // PROXY_CONNECTION_H
