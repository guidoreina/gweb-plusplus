#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <unistd.h>
#include "net/tcp_connection.h"
#include "net/url_parser.h"
#include "http/chunked_parser.h"
#include "http/virtual_hosts.h"
#include "http/rulelist.h"
#include "http/http_headers.h"
#include "http/fastcgi.h"
#include "http/http_method.h"
#include "http/http_error.h"
#include "file/file_wrapper.h"

struct http_connection : public tcp_connection,
                         public chunked_parser {
	static const unsigned short URI_MAX_LEN;
	static const unsigned short REQUEST_LINE_MAX_LEN;
	static const unsigned short HEADER_MAX_LINE_LEN;

	static const size_t HOST_MEAN_SIZE;
	static const size_t PATH_MEAN_SIZE;
	static const size_t QUERY_STRING_MEAN_SIZE;
	static const size_t BODY_MEAN_SIZE;

	// HTTP states.
	static const unsigned char BEGIN_REQUEST_STATE;
	static const unsigned char READING_HEADERS_STATE;
	static const unsigned char PROCESSING_REQUEST_STATE;
	static const unsigned char READING_BODY_STATE;
	static const unsigned char READING_CHUNKED_BODY_STATE;
	static const unsigned char PREPARING_HTTP_REQUEST_STATE;
	static const unsigned char WAITING_FOR_BACKEND_STATE;
	static const unsigned char PREPARING_ERROR_PAGE_STATE;
	static const unsigned char SENDING_TWO_BUFFERS_STATE;
	static const unsigned char SENDING_HEADERS_STATE;
	static const unsigned char SENDING_BODY_STATE;
	static const unsigned char SENDING_PART_HEADER_STATE;
	static const unsigned char SENDING_MULTIPART_FOOTER_STATE;
	static const unsigned char SENDING_BACKEND_HEADERS_STATE;
	static const unsigned char SENDING_BACKEND_BODY_STATE;
	static const unsigned char REQUEST_COMPLETED_STATE;

	static const unsigned short REQUEST_ID;

	static url_parser _M_url;

	static struct tm _M_last_modified;

	int _M_fd;

	int _M_tmpfile;

	virtual_hosts::vhost* _M_vhost;

	rulelist::rule* _M_rule;

	static rulelist::rule _M_http_rule;

	http_headers _M_headers;

	buffer _M_host;
	buffer _M_path; // Contains the path as it has been received (including query string and fragment, if present).
	buffer _M_decoded_path;
	buffer _M_query_string;

	buffer _M_body;
	buffer* _M_bodyp;

	size_t _M_request_header_size;
	size_t _M_request_body_size;
	size_t _M_response_header_size;
	size_t _M_backend_response_header_size;

	off_t _M_filesize;
	off_t _M_tmpfilesize;

	range_list _M_ranges;
	size_t _M_nrange;

	unsigned _M_boundary;

	const char* _M_type;
	unsigned short _M_typelen;

	unsigned short _M_port;

	unsigned short _M_token;

	unsigned short _M_uri;
	unsigned short _M_urilen;

	unsigned _M_error:10;

	unsigned _M_state:5;
	unsigned _M_substate:4;

	unsigned _M_method:6;

	unsigned _M_major_number:4;
	unsigned _M_minor_number:4;

	unsigned _M_keep_alive:1;

	unsigned _M_payload_in_memory:1;

	// Constructor.
	http_connection();

	// Destructor.
	virtual ~http_connection();

	// Free.
	virtual void free();

	// Reset.
	virtual void reset();

	// Loop.
	virtual bool loop(unsigned fd);

	// Read request line.
	bool read_request_line(unsigned fd, size_t& total);

	// Read headers.
	bool read_headers(unsigned fd, size_t& total);

	// Read body.
	bool read_body(unsigned fd, size_t& total);

	// Read chunked body.
	bool read_chunked_body(unsigned fd, size_t& total);

	enum parse_result {
		PARSE_ERROR,
		PARSING_NOT_COMPLETED,
		PARSING_COMPLETED
	};

	// Parse request line.
	parse_result parse_request_line();

	// Parse headers.
	parse_result parse_headers(unsigned fd);

	// Process request.
	virtual bool process_request(unsigned fd);

	// Process request for non-local handler.
	virtual bool process_non_local_handler(unsigned fd);

	// Prepare HTTP request.
	bool prepare_http_request(unsigned fd);

	// Add FastCGI parameters.
	bool add_fcgi_params(unsigned fd, buffer* out);

	// Add chunked data.
	bool add_chunked_data(const char* buf, size_t len);

	// Keep-Alive?
	bool keep_alive();

	// Compute Content-Length.
	off_t compute_content_length(off_t filesize) const;

	// Build part header.
	bool build_part_header();

	// Prepare error page.
	bool prepare_error_page();

	// Moved permanently.
	void moved_permanently();

	// Not modified.
	void not_modified();

	// Not found.
	void not_found();

	// Requested Range Not Satisfiable.
	void requested_range_not_satisfiable();

	// State to string.
	static const char* state_to_string(unsigned state);
};

inline http_connection::~http_connection()
{
	if (_M_fd != -1) {
		close(_M_fd);
	}
}

inline void http_connection::free()
{
	reset();

	if (_M_in.size() > 2 * READ_BUFFER_SIZE) {
		_M_in.free();
	} else {
		_M_in.reset();
	}

	_M_inp = 0;

	_M_readable = 0;
	_M_writable = 0;
}

inline bool http_connection::add_chunked_data(const char* buf, size_t len)
{
	if (_M_rule->handler == rulelist::HTTP_HANDLER) {
		if (!file_wrapper::write(_M_tmpfile, buf, len)) {
			return false;
		}

		_M_tmpfilesize += len;
	} else {
		if (!fastcgi::stdin_stream(REQUEST_ID, buf, len, _M_tmpfile, _M_tmpfilesize)) {
			return false;
		}
	}

	_M_request_body_size += len;

	return true;
}

inline bool http_connection::build_part_header()
{
	const range_list::range* range = _M_ranges.get(_M_nrange);

	return _M_out.format("\r\n--%0*u\r\nContent-Type: %.*s\r\nContent-Range: bytes %lld-%lld/%lld\r\n\r\n", http_headers::BOUNDARY_WIDTH, _M_boundary, _M_typelen, _M_type, range->from, range->to, _M_filesize);
}

inline void http_connection::moved_permanently()
{
	_M_error = http_error::MOVED_PERMANENTLY;
	keep_alive();
}

inline void http_connection::not_modified()
{
	_M_error = http_error::NOT_MODIFIED;
	keep_alive();
}

inline void http_connection::not_found()
{
	_M_error = http_error::NOT_FOUND;
	keep_alive();
}

inline void http_connection::requested_range_not_satisfiable()
{
	_M_error = http_error::REQUESTED_RANGE_NOT_SATISFIABLE;
	keep_alive();
}

#endif // HTTP_CONNECTION_H
