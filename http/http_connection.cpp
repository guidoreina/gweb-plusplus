#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "http_connection.h"
#include "http/http_server.h"
#include "http/range_parser.h"
#include "http/access_log.h"
#include "http/version.h"
#include "net/tcp_connection.inl"
#include "net/socket_wrapper.h"
#include "util/date_parser.h"
#include "util/number.h"
#include "util/now.h"
#include "string/memcasemem.h"
#include "logger/logger.h"

#ifndef HAVE_MEMRCHR
#include "string/memrchr.h"
#endif

#include "macros/macros.h"

const unsigned short http_connection::URI_MAX_LEN = 8 * 1024;
const unsigned short http_connection::REQUEST_LINE_MAX_LEN = URI_MAX_LEN + 100;
const unsigned short http_connection::HEADER_MAX_LINE_LEN = 10 * 1024;

const size_t http_connection::HOST_MEAN_SIZE = 32;
const size_t http_connection::PATH_MEAN_SIZE = 128;
const size_t http_connection::QUERY_STRING_MEAN_SIZE = 64;
const size_t http_connection::BODY_MEAN_SIZE = 2 * 1024;

const unsigned char http_connection::BEGIN_REQUEST_STATE = 0;
const unsigned char http_connection::READING_HEADERS_STATE = 1;
const unsigned char http_connection::PROCESSING_REQUEST_STATE = 2;
const unsigned char http_connection::READING_BODY_STATE = 3;
const unsigned char http_connection::READING_CHUNKED_BODY_STATE = 4;
const unsigned char http_connection::PREPARING_HTTP_REQUEST_STATE = 5;
const unsigned char http_connection::WAITING_FOR_BACKEND_STATE = 6;
const unsigned char http_connection::PREPARING_ERROR_PAGE_STATE = 7;
const unsigned char http_connection::SENDING_TWO_BUFFERS_STATE = 8;
const unsigned char http_connection::SENDING_HEADERS_STATE = 9;
const unsigned char http_connection::SENDING_BODY_STATE = 10;
const unsigned char http_connection::SENDING_PART_HEADER_STATE = 11;
const unsigned char http_connection::SENDING_MULTIPART_FOOTER_STATE = 12;
const unsigned char http_connection::SENDING_BACKEND_HEADERS_STATE = 13;
const unsigned char http_connection::SENDING_BACKEND_BODY_STATE = 14;
const unsigned char http_connection::REQUEST_COMPLETED_STATE = 15;

const unsigned short http_connection::REQUEST_ID = 1;

url_parser http_connection::_M_url;
struct tm http_connection::_M_last_modified;
rulelist::rule http_connection::_M_http_rule;

http_connection::http_connection()
 : _M_host(HOST_MEAN_SIZE),
   _M_path(PATH_MEAN_SIZE),
   _M_decoded_path(PATH_MEAN_SIZE),
   _M_query_string(QUERY_STRING_MEAN_SIZE),
   _M_body(BODY_MEAN_SIZE)
{
	_M_fd = -1;

	_M_tmpfile = -1;

	_M_vhost = NULL;

	_M_headers.set_max_line_length(HEADER_MAX_LINE_LEN);

	_M_request_body_size = 0;

	_M_nrange = 0;

	_M_state = BEGIN_REQUEST_STATE;
	_M_substate = 0;

	_M_keep_alive = 0;
}

void http_connection::reset()
{
	chunked_parser::reset();

	_M_outp = 0;

	_M_in_ready_list = 0;

	if (_M_fd != -1) {
		if ((_M_state != WAITING_FOR_BACKEND_STATE) && (_M_state != SENDING_BACKEND_HEADERS_STATE) && (_M_state != SENDING_BACKEND_BODY_STATE)) {
			file_wrapper::close(_M_fd);
		}

		_M_fd = -1;
	}

	if (_M_tmpfile != -1) {
		static_cast<http_server*>(_M_server)->_M_tmpfiles.close(_M_tmpfile);
		_M_tmpfile = -1;
	}

	_M_vhost = NULL;

	_M_headers.reset();

	if (_M_host.size() > HOST_MEAN_SIZE) {
		_M_host.free();
	} else {
		_M_host.reset();
	}

	if (_M_path.size() > PATH_MEAN_SIZE) {
		_M_path.free();
	} else {
		_M_path.reset();
	}

	if (_M_decoded_path.size() > PATH_MEAN_SIZE) {
		_M_decoded_path.free();
	} else {
		_M_decoded_path.reset();
	}

	if (_M_query_string.size() > QUERY_STRING_MEAN_SIZE) {
		_M_query_string.free();
	} else {
		_M_query_string.reset();
	}

	if (_M_body.size() > BODY_MEAN_SIZE) {
		_M_body.free();
	} else {
		_M_body.reset();
	}

	if (_M_ranges.count() > 2) {
		_M_ranges.free();
	} else {
		_M_ranges.reset();
	}

	_M_nrange = 0;

	_M_state = BEGIN_REQUEST_STATE;
	_M_substate = 0;

	_M_keep_alive = 0;
}

bool http_connection::loop(unsigned fd)
{
	socket_wrapper::io_vector io_vector[2];
	size_t total = 0;
	size_t count;
	bool ret;

	do {
		logger::instance().log(logger::LOG_DEBUG, "[http_connection::loop] (fd %d) State = %s.", fd, state_to_string(_M_state));

		switch (_M_state) {
			case BEGIN_REQUEST_STATE:
				if ((!_M_readable) && (_M_inp == (off_t) _M_in.count())) {
					return true;
				}

				if (!read_request_line(fd, total)) {
					return false;
				}

				break;
			case READING_HEADERS_STATE:
				if (!_M_readable) {
					return true;
				}

				if (!read_headers(fd, total)) {
					return false;
				}

				break;
			case PROCESSING_REQUEST_STATE:
				if (!process_request(fd)) {
					return false;
				}

				if (_M_error != http_error::OK) {
					total = 0;

					_M_state = PREPARING_ERROR_PAGE_STATE;
				} else {
					if ((_M_state != PREPARING_HTTP_REQUEST_STATE) && (_M_state != READING_BODY_STATE) && (_M_state != READING_CHUNKED_BODY_STATE)) {
						if (!modify(fd, tcp_server::WRITE)) {
							return false;
						}

						total = 0;
					}
				}

				break;
			case READING_BODY_STATE:
				if (!_M_readable) {
					return true;
				}

				if (!read_body(fd, total)) {
					return false;
				}

				break;
			case READING_CHUNKED_BODY_STATE:
				if (!_M_readable) {
					return true;
				}

				if (!read_chunked_body(fd, total)) {
					return false;
				}

				break;
			case PREPARING_HTTP_REQUEST_STATE:
				if (!prepare_http_request(fd)) {
					_M_state = PREPARING_ERROR_PAGE_STATE;
				} else {
					_M_state = WAITING_FOR_BACKEND_STATE;
				}

				break;
			case WAITING_FOR_BACKEND_STATE:
				return true;
			case PREPARING_ERROR_PAGE_STATE:
				if ((!prepare_error_page()) || (!modify(fd, tcp_server::WRITE))) {
					return false;
				}

				break;
			case SENDING_TWO_BUFFERS_STATE:
				if (!_M_writable) {
					return true;
				}

				io_vector[0].iov_base = _M_out.data();
				io_vector[0].iov_len = _M_out.count();

				io_vector[1].iov_base = _M_bodyp->data();
				io_vector[1].iov_len = _M_bodyp->count();

				if (!writev(fd, io_vector, 2, total)) {
					return false;
				} else if (_M_outp == (off_t) (_M_out.count() + _M_bodyp->count())) {
					_M_state = REQUEST_COMPLETED_STATE;
				}

				break;
			case SENDING_HEADERS_STATE:
				if (!_M_writable) {
					return true;
				}

				if (!write(fd, total)) {
					return false;
				} else if (_M_outp == (off_t) _M_out.count()) {
					if (_M_method == http_method::HEAD) {
						_M_state = REQUEST_COMPLETED_STATE;
					} else if (_M_error != http_error::OK) {
						_M_state = REQUEST_COMPLETED_STATE;
					} else if (_M_filesize == 0) {
						_M_state = REQUEST_COMPLETED_STATE;
					} else {
						if (_M_ranges.count() == 0) {
							_M_outp = 0;
						} else {
							const range_list::range* range = _M_ranges.get(0);
							_M_outp = range->from;
						}

						_M_state = SENDING_BODY_STATE;
					}
				}

				break;
			case SENDING_BODY_STATE:
				if (!_M_writable) {
					return true;
				}

				if (!sendfile(fd, _M_fd, _M_filesize, &_M_ranges, _M_nrange, total)) {
					return false;
				} else {
					size_t nranges = _M_ranges.count();

					if (nranges == 0) {
						if (_M_outp == _M_filesize) {
							socket_wrapper::uncork(fd);

							_M_state = REQUEST_COMPLETED_STATE;
						}
					} else {
						const range_list::range* range = _M_ranges.get(_M_nrange);
						if (_M_outp == range->to + 1) {
							if (nranges == 1) {
								socket_wrapper::uncork(fd);

								_M_state = REQUEST_COMPLETED_STATE;
							} else {
								_M_out.reset();

								// Last part?
								if (++_M_nrange == nranges) {
									if (!_M_out.format("\r\n--%0*u--\r\n", http_headers::BOUNDARY_WIDTH, _M_boundary)) {
										return false;
									}

									_M_state = SENDING_MULTIPART_FOOTER_STATE;
								} else {
									if (!build_part_header()) {
										return false;
									}

									_M_state = SENDING_PART_HEADER_STATE;
								}

								_M_outp = 0;
							}
						}
					}
				}

				break;
			case SENDING_PART_HEADER_STATE:
				if (!_M_writable) {
					return true;
				}

				if (!write(fd, total)) {
					return false;
				} else if (_M_outp == (off_t) _M_out.count()) {
					const range_list::range* range = _M_ranges.get(_M_nrange);
					_M_outp = range->from;

					_M_state = SENDING_BODY_STATE;
				}

				break;
			case SENDING_MULTIPART_FOOTER_STATE:
				if (!_M_writable) {
					return true;
				}

				if (!write(fd, total)) {
					return false;
				} else if (_M_outp == (off_t) _M_out.count()) {
					socket_wrapper::uncork(fd);

					_M_state = REQUEST_COMPLETED_STATE;
				}

				break;
			case SENDING_BACKEND_HEADERS_STATE:
				if (!_M_writable) {
					return true;
				}

				if ((!_M_payload_in_memory) || (_M_filesize == 0) || (_M_method == http_method::HEAD)) {
					count = _M_out.count();

					ret = write(fd, total);
				} else {
					io_vector[0].iov_base = _M_out.data();
					io_vector[0].iov_len = _M_out.count();

					if (_M_error == http_error::OK) {
						io_vector[1].iov_base = _M_body.data() + _M_backend_response_header_size;
					} else {
						io_vector[1].iov_base = _M_bodyp->data();
					}

					io_vector[1].iov_len = _M_filesize;

					count = io_vector[0].iov_len + io_vector[1].iov_len;

					ret = writev(fd, io_vector, 2, total);
				}

				if (!ret) {
					return false;
				} else if (_M_outp == (off_t) count) {
					if (_M_payload_in_memory) {
						_M_state = REQUEST_COMPLETED_STATE;
					} else {
						_M_outp = 0;

						_M_state = SENDING_BACKEND_BODY_STATE;
					}
				}

				break;
			case SENDING_BACKEND_BODY_STATE:
				if (!_M_writable) {
					return true;
				}

				if (!sendfile(fd, _M_tmpfile, _M_filesize, total)) {
					return false;
				} else if (_M_outp == _M_filesize) {
					socket_wrapper::uncork(fd);

					_M_state = REQUEST_COMPLETED_STATE;
				}

				break;
			case REQUEST_COMPLETED_STATE:
				if ((_M_vhost) && (_M_vhost->log_requests)) {
					_M_vhost->log->log(*this, fd);
				}

				// Close connection?
				if (!_M_keep_alive) {
					return false;
				} else {
					if (!modify(fd, tcp_server::READ)) {
						return false;
					}

					reset();

					// If there is more data in the input buffer...
					size_t left = _M_in.count() - _M_inp;
					if (left > 0) {
						// Move data at the beginning of the buffer.
						char* data = _M_in.data();
						memmove(data, data + _M_inp, left);
						_M_in.set_count(left);
					} else {
						_M_in.reset();
					}

					_M_inp = 0;
				}

				break;
		}
	} while (!_M_in_ready_list);

	return true;
}

bool http_connection::read_request_line(unsigned fd, size_t& total)
{
	io_result res;

	// If we had already processed the data in the input buffer...
	if (_M_inp == (off_t) _M_in.count()) {
		if ((res = read(fd, total)) == IO_ERROR) {
			return false;
		} else if (res == IO_NO_DATA_READ) {
			return true;
		}
	}

	do {
		parse_result parse_result = parse_request_line();
		if (parse_result == PARSE_ERROR) {
			logger::instance().log(logger::LOG_DEBUG, "[http_connection::read_request_line] (fd %d) Invalid request line.", fd);

			_M_state = PREPARING_ERROR_PAGE_STATE;
			return true;
		} else if (parse_result == PARSING_COMPLETED) {
			parse_result = parse_headers(fd);
			if (parse_result == PARSE_ERROR) {
				_M_state = PREPARING_ERROR_PAGE_STATE;
			} else if (parse_result == PARSING_NOT_COMPLETED) {
				_M_state = READING_HEADERS_STATE;
			} else {
				_M_state = PROCESSING_REQUEST_STATE;
			}

			return true;
		}

		if (!_M_readable) {
			return true;
		}

		if ((res = read(fd, total)) == IO_ERROR) {
			return false;
		} else if (res == IO_NO_DATA_READ) {
			return true;
		}
	} while (true);
}

bool http_connection::read_headers(unsigned fd, size_t& total)
{
	io_result res;

	do {
		if ((res = read(fd, total)) == IO_ERROR) {
			return false;
		} else if (res == IO_NO_DATA_READ) {
			return true;
		}

		parse_result parse_result = parse_headers(fd);
		if (parse_result == PARSE_ERROR) {
			_M_state = PREPARING_ERROR_PAGE_STATE;
			return true;
		} else if (parse_result == PARSING_COMPLETED) {
			_M_state = PROCESSING_REQUEST_STATE;
			return true;
		}
	} while (res == IO_SUCCESS);

	return true;
}

bool http_connection::read_body(unsigned fd, size_t& total)
{
	io_result res;

	do {
		if ((res = read(fd, total)) == IO_ERROR) {
			return false;
		} else if (res == IO_NO_DATA_READ) {
			return true;
		}

		size_t count = MIN(_M_filesize, (off_t) _M_in.count() - _M_inp);

		if (!_M_payload_in_memory) {
			if (_M_rule->handler == rulelist::HTTP_HANDLER) {
				if (!file_wrapper::write(_M_tmpfile, _M_in.data(), count)) {
					_M_error = http_error::INTERNAL_SERVER_ERROR;

					_M_state = PREPARING_ERROR_PAGE_STATE;

					return true;
				}

				_M_tmpfilesize += count;
			} else {
				if (!fastcgi::stdin_stream(REQUEST_ID, _M_in.data(), count, _M_tmpfile, _M_tmpfilesize)) {
					_M_error = http_error::INTERNAL_SERVER_ERROR;

					_M_state = PREPARING_ERROR_PAGE_STATE;

					return true;
				}
			}
		}

		if ((off_t) count == _M_filesize) {
			_M_inp += count;

			_M_state = PREPARING_HTTP_REQUEST_STATE;

			return true;
		}

		_M_filesize -= count;

		if (_M_payload_in_memory) {
			_M_inp += count;
		} else {
			_M_in.reset();
		}
	} while (res == IO_SUCCESS);

	return true;
}

bool http_connection::read_chunked_body(unsigned fd, size_t& total)
{
	io_result res;

	do {
		if ((res = read(fd, total)) == IO_ERROR) {
			return false;
		} else if (res == IO_NO_DATA_READ) {
			return true;
		}

		if (_M_rule->handler == rulelist::HTTP_HANDLER) {
			size_t size;
			switch (parse_chunk(_M_in.data(), _M_in.count(), size)) {
				case chunked_parser::INVALID_CHUNKED_RESPONSE:
					_M_error = http_error::BAD_REQUEST;

					_M_state = PREPARING_ERROR_PAGE_STATE;

					return true;
				case chunked_parser::CALLBACK_FAILED:
					_M_error = http_error::INTERNAL_SERVER_ERROR;

					_M_state = PREPARING_ERROR_PAGE_STATE;

					return true;
				case chunked_parser::END_OF_RESPONSE:
					_M_inp += size;

					_M_state = PREPARING_HTTP_REQUEST_STATE;

					return true;
				default:
					;
			}
		}

		_M_in.reset();
	} while (res == IO_SUCCESS);

	return true;
}

http_connection::parse_result http_connection::parse_request_line()
{
	const char* data = _M_in.data();
	size_t len = _M_in.count();

	while (_M_inp < (off_t) len) {
		unsigned char c = (unsigned char) data[_M_inp];
		switch (_M_substate) {
			case 0: // Initial state.
				if ((c >= 'A') && (c <= 'Z')) {
					_M_token = _M_inp;
					_M_substate = 2; // Method.
				} else if (c == '\r') {
					_M_substate = 1; // '\r' before method.
				} else if ((!IS_WHITE_SPACE(c)) && (c != '\n')) {
					_M_method = http_method::UNKNOWN;
					_M_error = http_error::BAD_REQUEST;

					return PARSE_ERROR;
				}

				break;
			case 1: // '\r' before method.
				if (c == '\n') {
					_M_substate = 0; // Initial state.
				} else {
					_M_method = http_method::UNKNOWN;
					_M_error = http_error::BAD_REQUEST;

					return PARSE_ERROR;
				}

				break;
			case 2: // Method.
				if (IS_WHITE_SPACE(c)) {
					if ((_M_method = http_method::get_method(data + _M_token, _M_inp - _M_token)) == http_method::UNKNOWN) {
						_M_error = http_error::BAD_REQUEST;
						return PARSE_ERROR;
					}

					_M_substate = 3; // Whitespace after method.
				} else if ((c < 'A') || (c > 'Z')) {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 3: // Whitespace after method.
				if (c > ' ') {
					_M_uri = _M_inp;
					_M_urilen = 1;

					_M_substate = 4; // URI.
				} else if (!IS_WHITE_SPACE(c)) {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 4: // URI.
				if (c > ' ') {
					if (++_M_urilen == URI_MAX_LEN) {
						_M_error = http_error::REQUEST_URI_TOO_LONG;
						return PARSE_ERROR;
					}
				} else if (IS_WHITE_SPACE(c)) {
					_M_substate = 5; // Whitespace after URI.
				} else {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 5: // Whitespace after URI.
				if ((c == 'H') || (c == 'h')) {
					_M_substate = 6; // [H]TTP/<major>.<minor>
				} else if (!IS_WHITE_SPACE(c)) {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 6: // [H]TTP/<major>.<minor>
				if ((c == 'T') || (c == 't')) {
					_M_substate = 7; // H[T]TP/<major>.<minor>
				} else {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 7: // H[T]TP/<major>.<minor>
				if ((c == 'T') || (c == 't')) {
					_M_substate = 8; // HT[T]P/<major>.<minor>
				} else {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 8: // HT[T]P/<major>.<minor>
				if ((c == 'P') || (c == 'p')) {
					_M_substate = 9; // HTT[P]/<major>.<minor>
				} else {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 9: // HTT[P]/<major>.<minor>
				if (c == '/') {
					_M_substate = 10; // HTTP[/]<major>.<minor>
				} else {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 10: // HTTP[/]<major>.<minor>
				if ((c >= '0') && (c <= '9')) {
					_M_major_number = c - '0';
					if (_M_major_number > 1) {
						_M_error = http_error::BAD_REQUEST;
						return PARSE_ERROR;
					}

					_M_substate = 11; // HTTP/[<major>].<minor>
				} else {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 11: // HTTP/[<major>].<minor>
				if (c == '.') {
					_M_substate = 12; // HTTP/<major>[.]<minor>
				} else {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 12: // HTTP/<major>[.]<minor>
				if ((c >= '0') && (c <= '9')) {
					_M_minor_number = c - '0';
					if ((_M_major_number == 1) && (_M_minor_number > 1)) {
						_M_error = http_error::BAD_REQUEST;
						return PARSE_ERROR;
					}

					_M_substate = 13; // HTTP/<major>.[<minor>]
				} else {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 13: // HTTP/<major>.[<minor>]
				if (c == '\r') {
					_M_substate = 14; // '\r' at the end of request line.
				} else if (c == '\n') {
					_M_inp++;

					return PARSING_COMPLETED;
				} else if (!IS_WHITE_SPACE(c)) {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
			case 14: // '\r' at the end of request line.
				if (c == '\n') {
					_M_inp++;

					return PARSING_COMPLETED;
				} else {
					_M_error = http_error::BAD_REQUEST;
					return PARSE_ERROR;
				}

				break;
		}

		if (++_M_inp == REQUEST_LINE_MAX_LEN) {
			_M_error = http_error::REQUEST_ENTITY_TOO_LARGE;
			return PARSE_ERROR;
		}
	}

	return PARSING_NOT_COMPLETED;
}

http_connection::parse_result http_connection::parse_headers(unsigned fd)
{
	size_t body_offset;
	http_headers::parse_result parse_result = _M_headers.parse(_M_in.data() + _M_inp, _M_in.count() - _M_inp, body_offset);
	if (parse_result == http_headers::END_OF_HEADER) {
		_M_inp += body_offset;

		_M_request_header_size = _M_inp;

		return PARSING_COMPLETED;
	} else if (parse_result == http_headers::ERROR_WRONG_HEADERS) {
		logger::instance().log(logger::LOG_DEBUG, "[http_connection::parse_headers] (fd %d) Invalid headers.", fd);

		_M_error = http_error::BAD_REQUEST;
		return PARSE_ERROR;
	} else if (parse_result == http_headers::ERROR_HEADERS_TOO_LARGE) {
		logger::instance().log(logger::LOG_DEBUG, "[http_connection::parse_headers] (fd %d) Headers too large.", fd);

		_M_error = http_error::REQUEST_ENTITY_TOO_LARGE;
		return PARSE_ERROR;
	} else if (parse_result == http_headers::ERROR_NO_MEMORY) {
		_M_error = http_error::INTERNAL_SERVER_ERROR;
		return PARSE_ERROR;
	}

	return PARSING_NOT_COMPLETED;
}

bool http_connection::process_request(unsigned fd)
{
	char* data = _M_in.data();
	data[_M_uri + _M_urilen] = 0;

	// Parse URL.
	_M_url.reset();
	url_parser::parse_result parse_result = _M_url.parse(data + _M_uri, _M_urilen, _M_path);
	if (parse_result == url_parser::ERROR_NO_MEMORY) {
		logger::instance().log(logger::LOG_INFO, "[http_connection::process_request] (fd %d) No memory, URL (%.*s).", fd, _M_urilen, data + _M_uri);

		_M_error = http_error::INTERNAL_SERVER_ERROR;
		return true;
	} else if (parse_result == url_parser::PARSE_ERROR) {
		if (_M_path.count() > 0) {
			logger::instance().log(logger::LOG_INFO, "[http_connection::process_request] (fd %d) URL parse error (%.*s).", fd, _M_path.count(), _M_path.data());
		} else {
			logger::instance().log(logger::LOG_INFO, "[http_connection::process_request] (fd %d) URL parse error (%.*s).", fd, _M_urilen, data + _M_uri);
		}

		_M_error = http_error::BAD_REQUEST;
		return true;
	} else if (parse_result == url_parser::FORBIDDEN) {
		logger::instance().log(logger::LOG_INFO, "[http_connection::process_request] (fd %d) Forbidden URL (%.*s).", fd, _M_path.count(), _M_path.data());

		_M_error = http_error::FORBIDDEN;
		return true;
	}

	// If an absolute URL has been received...
	unsigned short hostlen;
	const char* host = _M_url.get_host(hostlen);
	if (host) {
		if ((_M_major_number == 1) && (_M_minor_number == 1)) {
			// Ignore Host header (if present).
			const char* value;
			unsigned short valuelen;
			if (!_M_headers.get_value_known_header(http_headers::HOST_HEADER, value, &valuelen)) {
				_M_error = http_error::BAD_REQUEST;
				return true;
			}
		}

		if (!_M_host.append_nul_terminated_string(host, hostlen)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		_M_port = _M_url.get_port();

#if PROXY
		_M_http_rule.handler = rulelist::HTTP_HANDLER;
		_M_rule = &_M_http_rule;
		return process_non_local_handler(fd);
#endif // PROXY

		if (_M_url.get_port() != static_cast<http_server*>(_M_server)->_M_port) {
			not_found();
			return true;
		}

		_M_vhost = static_cast<http_server*>(_M_server)->_M_vhosts.get_host(host, hostlen);
	} else {
		if (_M_headers.get_value_known_header(http_headers::HOST_HEADER, host, &hostlen)) {
			// If the host includes port number...
			unsigned port;

			const char* semicolon = (const char*) memrchr(host, ':', hostlen);
			if (semicolon) {
				if (number::parse_unsigned(semicolon + 1, (host + hostlen) - (semicolon + 1), port, 1, 65535) != number::PARSE_SUCCEEDED) {
					_M_error = http_error::BAD_REQUEST;
					return true;
				}

				hostlen = semicolon - host;
			} else {
				port = url_parser::HTTP_DEFAULT_PORT;
			}

			if (port != static_cast<http_server*>(_M_server)->_M_port) {
				not_found();
				return true;
			}

			_M_vhost = static_cast<http_server*>(_M_server)->_M_vhosts.get_host(host, hostlen);
		} else {
			if ((_M_major_number == 1) && (_M_minor_number == 1)) {
				_M_error = http_error::BAD_REQUEST;
				return true;
			}

			_M_vhost = static_cast<http_server*>(_M_server)->_M_vhosts.get_default_host();
		}
	}

	if (!_M_vhost) {
		not_found();
		return true;
	}

	unsigned short pathlen;
	const char* urlpath = _M_url.get_path(pathlen);

	unsigned short extensionlen;
	const char* extension = _M_url.get_extension(extensionlen);

	_M_rule = _M_vhost->rules->find(_M_method, urlpath, pathlen, extension, extensionlen);

	if (_M_rule->handler != rulelist::LOCAL_HANDLER) {
		if (_M_rule->handler == rulelist::FCGI_HANDLER) {
			size_t len = _M_vhost->rootlen + pathlen;
			if (len > PATH_MAX) {
				_M_error = http_error::REQUEST_URI_TOO_LONG;
				return true;
			}

			// Save decoded path.
			if (!_M_decoded_path.append(urlpath, pathlen)) {
				_M_error = http_error::INTERNAL_SERVER_ERROR;
				return true;
			}

			unsigned short query_string_len;
			const char* query_string = _M_url.get_query(query_string_len);
			if (query_string_len > 1) {
				// Save query string.
				if (!_M_query_string.append(query_string + 1, query_string_len - 1)) {
					_M_error = http_error::INTERNAL_SERVER_ERROR;
					return true;
				}
			}
		}

		return process_non_local_handler(fd);
	}

	// Local handler.

	if ((_M_method != http_method::GET) && (_M_method != http_method::HEAD)) {
		_M_error = http_error::NOT_IMPLEMENTED;
		return true;
	}

	char path[PATH_MAX + 1];
	size_t len = _M_vhost->rootlen + pathlen;
	if (len >= sizeof(path)) {
		_M_error = http_error::REQUEST_URI_TOO_LONG;
		return true;
	}

	memcpy(path, _M_vhost->root, _M_vhost->rootlen);
	memcpy(path + _M_vhost->rootlen, urlpath, pathlen);
	path[len] = 0;

	struct stat buf;
	if (stat(path, &buf) < 0) {
		not_found();
		return true;
	}

	bool dirlisting;
	bool index_file;

	// If the URI points to a directory...
	if (S_ISDIR(buf.st_mode)) {
		// If the directory name doesn't end with '/'...
		if (path[len - 1] != '/') {
			moved_permanently();
			return true;
		}

		// Search index file.
		if (static_cast<http_server*>(_M_server)->_M_index_file_finder.search(path, len, &buf)) {
			// File.
			dirlisting = false;
			index_file = true;
		} else {
			// If we don't have directory listing...
			if (!_M_vhost->have_dirlisting) {
				not_found();
				return true;
			} else {
				// Build directory listing.
				if (!_M_vhost->dir_listing->build(urlpath, pathlen, _M_body)) {
					logger::instance().log(logger::LOG_WARNING, "[http_connection::process_request] (fd %d) Couldn't build directory listing for (%s).", fd, path);

					_M_error = http_error::INTERNAL_SERVER_ERROR;
					return true;
				}

				_M_bodyp = &_M_body;

				dirlisting = true;
				index_file = false;
			}
		}
	} else if ((S_ISREG(buf.st_mode)) || (S_ISLNK(buf.st_mode))) {
		// File.
		dirlisting = false;
		index_file = false;
	} else {
		not_found();
		return true;
	}

	if (!dirlisting) {
		const char* value;
		unsigned short valuelen;
		if (_M_headers.get_value_known_header(http_headers::IF_MODIFIED_SINCE_HEADER, value, &valuelen)) {
			time_t t;
			if ((t = date_parser::parse(value, valuelen, &_M_last_modified)) != (time_t) -1) {
				if (t == buf.st_mtime) {
					not_modified();
					return true;
				} else if (t > buf.st_mtime) {
					gmtime_r(&buf.st_mtime, &_M_last_modified);
					not_modified();

					return true;
				}
			}
		}

		if (_M_method == http_method::GET) {
			if ((_M_headers.get_value_known_header(http_headers::RANGE_HEADER, value, &valuelen)) && (valuelen > 6) && (strncasecmp(value, "bytes=", 6) == 0)) {
				if (!range_parser::parse(value + 6, valuelen - 6, buf.st_size, _M_ranges)) {
					requested_range_not_satisfiable();
					return true;
				}
			}

			if ((_M_fd = file_wrapper::open(path, O_RDONLY)) < 0) {
				logger::instance().log(logger::LOG_WARNING, "[http_connection::process_request] (fd %d) Couldn't open file (%s).", fd, path);

				_M_error = http_error::INTERNAL_SERVER_ERROR;
				return true;
			}
		}
	}

	http_headers* headers = &(static_cast<http_server*>(_M_server)->_M_headers);
	headers->reset();

	if (!headers->add_known_header(http_headers::DATE_HEADER, &now::_M_tm)) {
		_M_error = http_error::INTERNAL_SERVER_ERROR;
		return true;
	}

	if (keep_alive()) {
		if (!headers->add_known_header(http_headers::CONNECTION_HEADER, "Keep-Alive", 10, false)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}
	} else {
		if (!headers->add_known_header(http_headers::CONNECTION_HEADER, "close", 5, false)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}
	}

	if (!headers->add_known_header(http_headers::SERVER_HEADER, WEBSERVER_NAME, sizeof(WEBSERVER_NAME) - 1, false)) {
		_M_error = http_error::INTERNAL_SERVER_ERROR;
		return true;
	}

	unsigned short status_code;

	// Directory listing?
	if (dirlisting) {
		char num[32];
		int numlen = snprintf(num, sizeof(num), "%lu", _M_body.count());
		if (!headers->add_known_header(http_headers::CONTENT_LENGTH_HEADER, num, numlen, false)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		if (!headers->add_known_header(http_headers::CONTENT_TYPE_HEADER, "text/html; charset=UTF-8", 24, false)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		status_code = 200;
	} else {
		if (!headers->add_known_header(http_headers::ACCEPT_RANGES_HEADER, "bytes", 5, false)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		if (index_file) {
			const char* end = path + len;
			const char* ptr = end;
			extension = NULL;
			while ((ptr > path) && (*(ptr - 1) != '/')) {
				if (*(ptr - 1) == '.') {
					extension = ptr;
					extensionlen = end - extension;
					break;
				}

				ptr--;
			}
		}

		if ((extension) && (extensionlen > 0)) {
			_M_type = static_cast<http_server*>(_M_server)->_M_mime_types.get_mime_type(extension, extensionlen, _M_typelen);
		} else {
			_M_type = mime_types::DEFAULT_MIME_TYPE;
			_M_typelen = mime_types::DEFAULT_MIME_TYPE_LEN;
		}

		_M_filesize = compute_content_length(buf.st_size);

		char num[32];
		int numlen = snprintf(num, sizeof(num), "%lld", _M_filesize);
		if (!headers->add_known_header(http_headers::CONTENT_LENGTH_HEADER, num, numlen, false)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		const range_list::range* range;
		char content_range[128];

		switch (_M_ranges.count()) {
			case 0:
				if (!headers->add_known_header(http_headers::CONTENT_TYPE_HEADER, _M_type, _M_typelen, false)) {
					_M_error = http_error::INTERNAL_SERVER_ERROR;
					return true;
				}

				status_code = 200;

				break;
			case 1:
				if (!headers->add_known_header(http_headers::CONTENT_TYPE_HEADER, _M_type, _M_typelen, false)) {
					_M_error = http_error::INTERNAL_SERVER_ERROR;
					return true;
				}

				range = _M_ranges.get(0);

				len = snprintf(content_range, sizeof(content_range), "bytes %lld-%lld/%lld", range->from, range->to, buf.st_size);
				if (!headers->add_known_header(http_headers::CONTENT_RANGE_HEADER, content_range, len, false)) {
					_M_error = http_error::INTERNAL_SERVER_ERROR;
					return true;
				}

				status_code = 206;

				break;
			default:
				_M_boundary = ++(static_cast<http_server*>(_M_server)->_M_boundary);
				if (!headers->add_content_type_multipart(_M_boundary)) {
					_M_error = http_error::INTERNAL_SERVER_ERROR;
					return true;
				}

				status_code = 206;
		}

		// Add 'Last-Modified' header.

		struct tm timestamp;
		gmtime_r(&buf.st_mtime, &timestamp);

		if (!headers->add_known_header(http_headers::LAST_MODIFIED_HEADER, &timestamp)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}
	}

	_M_out.reset();

	if (status_code == 200) {
		if (!_M_out.append("HTTP/1.1 200 OK\r\n", 17)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}
	} else {
		if (!_M_out.append("HTTP/1.1 206 Partial Content\r\n", 30)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}
	}

	if (!headers->serialize(_M_out)) {
		_M_error = http_error::INTERNAL_SERVER_ERROR;
		return true;
	}

	_M_response_header_size = _M_out.count();

	// If multipart...
	if (_M_ranges.count() >= 2) {
		if (!build_part_header()) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}
	}

	_M_error = http_error::OK;

	if (_M_method == http_method::HEAD) {
		_M_filesize = 0;

		_M_state = SENDING_HEADERS_STATE;
	} else {
		if (dirlisting) {
			_M_filesize = _M_body.count();

			_M_state = SENDING_TWO_BUFFERS_STATE;
		} else {
			socket_wrapper::cork(fd);

			_M_state = SENDING_HEADERS_STATE;
		}
	}

	return true;
}

bool http_connection::process_non_local_handler(unsigned fd)
{
	if ((_M_method == http_method::GET) || (_M_method == http_method::HEAD)) {
		_M_request_body_size = 0;

		_M_payload_in_memory = 1;

		_M_state = PREPARING_HTTP_REQUEST_STATE;

		_M_error = http_error::OK;

		return true;
	} else if ((_M_method != http_method::POST) && (_M_method != http_method::PUT)) {
		_M_error = http_error::NOT_IMPLEMENTED;
		return true;
	}

	size_t count = _M_in.count() - _M_request_header_size;

	// Transfer-Encoding: chunked?
	const char* value;
	unsigned short valuelen;
	bool chunked;
	if ((_M_headers.get_value_known_header(http_headers::TRANSFER_ENCODING_HEADER, value, &valuelen)) && (memcasemem(value, valuelen, "chunked", 7))) {
		chunked = true;

		_M_request_body_size = 0;

		_M_payload_in_memory = 0;
	} else {
		chunked = false;

		// Get payload size (Content-Length).
		if (!_M_headers.get_value_known_header(http_headers::CONTENT_LENGTH_HEADER, value, &valuelen)) {
			_M_error = http_error::LENGTH_REQUIRED;
			return true;
		}

		if (number::parse_size_t(value, valuelen, _M_request_body_size) != number::PARSE_SUCCEEDED) {
			_M_error = http_error::BAD_REQUEST;
			return true;
		}

		// If the payload is already in memory...
		if (_M_request_header_size + _M_request_body_size <= _M_in.count()) {
			_M_payload_in_memory = 1;

			_M_inp += _M_request_body_size;

			_M_state = PREPARING_HTTP_REQUEST_STATE;
		} else {
			_M_payload_in_memory = (_M_request_body_size <= static_cast<http_server*>(_M_server)->_M_max_payload_in_memory);

			_M_inp += count;

			_M_filesize = _M_request_body_size - count;

			_M_state = READING_BODY_STATE;
		}
	}

	logger::instance().log(logger::LOG_DEBUG, "[http_connection::process_non_local_handler] (fd %d) %s payload will be saved in %s.", fd, chunked ? "Chunked" : "Not chunked", _M_payload_in_memory ? "memory" : "disk");

	// If the payload should be written to disk...
	if (!_M_payload_in_memory) {
		// Open a temporary file.
		if ((_M_tmpfile = static_cast<http_server*>(_M_server)->_M_tmpfiles.open()) < 0) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		_M_tmpfilesize = 0;

		// If there is some payload already in the input buffer...
		if (count > 0) {
			if (chunked) {
				size_t size;
				switch (parse_chunk(_M_in.data() + _M_request_header_size, count, size)) {
					case chunked_parser::INVALID_CHUNKED_RESPONSE:
						_M_error = http_error::BAD_REQUEST;
						return true;
					case chunked_parser::CALLBACK_FAILED:
						_M_error = http_error::INTERNAL_SERVER_ERROR;
						return true;
					case chunked_parser::NOT_END_OF_RESPONSE:
						_M_state = READING_CHUNKED_BODY_STATE;
						break;
					default:
						_M_inp += size;

						_M_state = PREPARING_HTTP_REQUEST_STATE;

						_M_error = http_error::OK;

						return true;
				}
			} else {
				if (_M_rule->handler == rulelist::HTTP_HANDLER) {
					if (!file_wrapper::write(_M_tmpfile, _M_in.data() + _M_request_header_size, count)) {
						_M_error = http_error::INTERNAL_SERVER_ERROR;
						return true;
					}
				} else {
					if (!fastcgi::stdin_stream(REQUEST_ID, _M_in.data() + _M_request_header_size, count, _M_tmpfile, _M_tmpfilesize)) {
						_M_error = http_error::INTERNAL_SERVER_ERROR;
						return true;
					}
				}
			}
		}

		_M_in.reset();
		_M_inp = 0;
	}

	_M_error = http_error::OK;

	return true;
}

bool http_connection::prepare_http_request(unsigned fd)
{
	// Connect to backend.
	const char* host;
	unsigned short hostlen;
	unsigned short port;

#if !PROXY
	if ((_M_fd = _M_rule->backends.connect(host, hostlen, port)) < 0) {
		_M_error = http_error::GATEWAY_TIMEOUT;
		return true;
	}
#else
	host = _M_host.data();
	hostlen = _M_host.count() - 1;

	port = _M_port;

	if ((_M_fd = socket_wrapper::connect(_M_host.data(), port)) < 0) {
		_M_error = http_error::GATEWAY_TIMEOUT;
		return true;
	}
#endif

	if (!static_cast<http_server*>(_M_server)->add(_M_fd, selector::WRITE, true)) {
		socket_wrapper::close(_M_fd);
		_M_fd = -1;

		_M_error = http_error::INTERNAL_SERVER_ERROR;
		return true;
	}

	// Get Connection header.
	keep_alive();

	if (_M_rule->handler == rulelist::HTTP_HANDLER) {
		buffer* out = &(static_cast<http_server*>(_M_server)->_M_proxy_connections[_M_fd]._M_out);

		out->reset();

		if (!out->format("%s %.*s HTTP/1.1\r\n", http_method::get_method(_M_method), _M_path.count(), _M_path.data())) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		if (!_M_headers.add_known_header(http_headers::CONNECTION_HEADER, "close", 5, true)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		_M_headers.remove_known_header(http_headers::KEEP_ALIVE_HEADER);
		_M_headers.remove_known_header(http_headers::PROXY_CONNECTION_HEADER);

		char string[512];
		size_t len;
		if (port == url_parser::HTTP_DEFAULT_PORT) {
			len = snprintf(string, sizeof(string), "%.*s", hostlen, host);
		} else {
			len = snprintf(string, sizeof(string), "%.*s:%u", hostlen, host, port);
		}

		if (!_M_headers.add_known_header(http_headers::HOST_HEADER, string, len, true)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		if (!_M_headers.serialize(*out)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		static_cast<http_server*>(_M_server)->_M_proxy_connections[_M_fd]._M_fd = fd;
		static_cast<http_server*>(_M_server)->_M_proxy_connections[_M_fd]._M_client = this;

		static_cast<http_server*>(_M_server)->_M_proxy_connections[_M_fd]._M_timestamp = now::_M_time;
	} else {
		buffer* out = &(static_cast<http_server*>(_M_server)->_M_fcgi_connections[_M_fd]._M_out);

		out->reset();

		if (!fastcgi::begin_request(REQUEST_ID, fastcgi::FCGI_RESPONDER, false, *out)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		if (!add_fcgi_params(fd, out)) {
			_M_error = http_error::INTERNAL_SERVER_ERROR;
			return true;
		}

		if (_M_payload_in_memory) {
			if (!fastcgi::stdin_stream(REQUEST_ID, _M_in.data() + _M_request_header_size, _M_request_body_size, *out)) {
				_M_error = http_error::INTERNAL_SERVER_ERROR;
				return true;
			}

			if (!fastcgi::stdin_stream(REQUEST_ID, NULL, 0, *out)) {
				_M_error = http_error::INTERNAL_SERVER_ERROR;
				return true;
			}
		} else {
			if (!fastcgi::stdin_stream(REQUEST_ID, NULL, 0, _M_tmpfile, _M_tmpfilesize)) {
				_M_error = http_error::INTERNAL_SERVER_ERROR;
				return true;
			}
		}

		static_cast<http_server*>(_M_server)->_M_fcgi_connections[_M_fd]._M_fd = fd;
		static_cast<http_server*>(_M_server)->_M_fcgi_connections[_M_fd]._M_client = this;

		static_cast<http_server*>(_M_server)->_M_fcgi_connections[_M_fd]._M_timestamp = now::_M_time;
	}

	static_cast<http_server*>(_M_server)->_M_connection_handlers[_M_fd] = _M_rule->handler;

	return true;
}

bool http_connection::add_fcgi_params(unsigned fd, buffer* out)
{
	fastcgi::params params(*out);

	if (!params.header(1)) {
		return false;
	}

	if (!params.add("SERVER_NAME", 11, _M_vhost->name, _M_vhost->namelen, false, false)) {
		return false;
	}

	char port[32];
	unsigned short valuelen = snprintf(port, sizeof(port), "%u", static_cast<http_server*>(_M_server)->_M_port);
	if (!params.add("SERVER_PORT", 11, port, valuelen, false, false)) {
		return false;
	}

	const char* value = http_method::get_method(_M_method);
	if (!params.add("REQUEST_METHOD", 14, value, strlen(value), false, false)) {
		return false;
	}

	const unsigned char* ip = (const unsigned char*) &(((struct sockaddr_in*) &_M_addr)->sin_addr);
	char addr[32];
	valuelen = snprintf(addr, sizeof(addr), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
	if (!params.add("REMOTE_ADDR", 11, addr, valuelen, false, false)) {
		return false;
	}

	valuelen = snprintf(port, sizeof(port), "%u", ntohs(((struct sockaddr_in*) &_M_addr)->sin_port));
	if (!params.add("REMOTE_PORT", 11, port, valuelen, false, false)) {
		return false;
	}

	if (!params.add("DOCUMENT_ROOT", 13, _M_vhost->root, _M_vhost->rootlen, false, false)) {
		return false;
	}

	if (!params.add("REQUEST_URI", 11, _M_path.data(), _M_path.count(), false, false)) {
		return false;
	}

	char path[PATH_MAX + 1];
	memcpy(path, _M_vhost->root, _M_vhost->rootlen);
	memcpy(path + _M_vhost->rootlen, _M_decoded_path.data(), _M_decoded_path.count());

	if (!params.add("SCRIPT_FILENAME", 15, path, _M_vhost->rootlen + _M_decoded_path.count(), false, false)) {
		return false;
	}

	if (!params.add("SCRIPT_NAME", 11, _M_decoded_path.data(), _M_decoded_path.count(), false, false)) {
		return false;
	}

	if (!params.add("QUERY_STRING", 12, _M_query_string.data(), _M_query_string.count(), false, false)) {
		return false;
	}

	if (!params.add("SERVER_PROTOCOL", 15, "HTTP/1.1", 8, false, false)) {
		return false;
	}

	if (!params.add("GATEWAY_INTERFACE", 17, "CGI/1.1", 7, false, false)) {
		return false;
	}

	unsigned char header;
	const char* name;
	unsigned short namelen;

	for (unsigned i = 0; _M_headers.get_header(i, header, name, namelen, value, valuelen); i++) {
		if (!params.add(name, namelen, value, valuelen, true, true)) {
			return false;
		}

		// Content-Type or Content-Length?
		if ((header == http_headers::CONTENT_TYPE_HEADER) || (header == http_headers::CONTENT_LENGTH_HEADER)) {
			if (!params.add(name, namelen, value, valuelen, false, true)) {
				return false;
			}
		}
	}

	return params.end();
}

bool http_connection::keep_alive()
{
	const char* value;
	unsigned short valuelen;
	if (_M_headers.get_value_known_header(http_headers::CONNECTION_HEADER, value, &valuelen)) {
		if (memcasemem(value, valuelen, "close", 5)) {
			_M_keep_alive = 0;
			return false;
		} else if (memcasemem(value, valuelen, "Keep-Alive", 10)) {
			_M_keep_alive = 1;
			return true;
		}
	}

	if ((_M_major_number == 1) && (_M_minor_number == 1)) {
		_M_keep_alive = 1;
		return true;
	} else {
		_M_keep_alive = 0;
		return false;
	}
}

off_t http_connection::compute_content_length(off_t filesize) const
{
	size_t nranges = _M_ranges.count();

	if (nranges == 0) {
		return filesize;
	} else if (nranges == 1) {
		const range_list::range* range = _M_ranges.get(0);

		return range->to - range->from + 1;
	}

	off_t content_length = 0;
	size_t sizelen = number::length(filesize);

	const range_list::range* range = _M_ranges.get(0);

	for (size_t i = 0; i < nranges; i++) {
		content_length += (2 + 2 + http_headers::BOUNDARY_WIDTH + 2 + 14 + _M_typelen + 2 + 21 + number::length(range->from) + 1 + number::length(range->to) + 1 + sizelen + 2 + 2);
		content_length += (range->to - range->from + 1);

		range++;
	}

	content_length += (2 + 2 + http_headers::BOUNDARY_WIDTH + 2 + 2);

	return content_length;
}

bool http_connection::prepare_error_page()
{
	if (!http_error::build_page(this)) {
		return false;
	}

	_M_response_header_size = _M_out.count();

	if ((_M_method == http_method::HEAD) || (_M_bodyp->count() == 0)) {
		_M_filesize = 0;

		_M_state = SENDING_HEADERS_STATE;
	} else {
		_M_filesize = _M_bodyp->count();

		_M_state = SENDING_TWO_BUFFERS_STATE;
	}

	return true;
}

const char* http_connection::state_to_string(unsigned state)
{
	switch (state) {
		case BEGIN_REQUEST_STATE:
			return "BEGIN_REQUEST_STATE";
		case READING_HEADERS_STATE:
			return "READING_HEADERS_STATE";
		case PROCESSING_REQUEST_STATE:
			return "PROCESSING_REQUEST_STATE";
		case READING_BODY_STATE:
			return "READING_BODY_STATE";
		case READING_CHUNKED_BODY_STATE:
			return "READING_CHUNKED_BODY_STATE";
		case PREPARING_HTTP_REQUEST_STATE:
			return "PREPARING_HTTP_REQUEST_STATE";
		case WAITING_FOR_BACKEND_STATE:
			return "WAITING_FOR_BACKEND_STATE";
		case PREPARING_ERROR_PAGE_STATE:
			return "PREPARING_ERROR_PAGE_STATE";
		case SENDING_TWO_BUFFERS_STATE:
			return "SENDING_TWO_BUFFERS_STATE";
		case SENDING_HEADERS_STATE:
			return "SENDING_HEADERS_STATE";
		case SENDING_BODY_STATE:
			return "SENDING_BODY_STATE";
		case SENDING_PART_HEADER_STATE:
			return "SENDING_PART_HEADER_STATE";
		case SENDING_MULTIPART_FOOTER_STATE:
			return "SENDING_MULTIPART_FOOTER_STATE";
		case SENDING_BACKEND_HEADERS_STATE:
			return "SENDING_BACKEND_HEADERS_STATE";
		case SENDING_BACKEND_BODY_STATE:
			return "SENDING_BACKEND_BODY_STATE";
		case REQUEST_COMPLETED_STATE:
			return "REQUEST_COMPLETED_STATE";
		default:
			return "(unknown)";
	}
}
