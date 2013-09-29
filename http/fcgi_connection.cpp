#include <stdlib.h>
#include <stdio.h>
#include "fcgi_connection.h"
#include "http/http_server.h"
#include "http/version.h"
#include "net/socket_wrapper.h"
#include "net/tcp_connection.inl"
#include "util/now.h"
#include "logger/logger.h"

const unsigned char fcgi_connection::CONNECTING_STATE = 0;
const unsigned char fcgi_connection::SENDING_REQUEST_STATE = 1;
const unsigned char fcgi_connection::SENDING_STDIN_STATE = 2;
const unsigned char fcgi_connection::READING_HEADERS_STATE = 3;
const unsigned char fcgi_connection::READING_BODY_STATE = 4;
const unsigned char fcgi_connection::PREPARING_ERROR_PAGE_STATE = 5;
const unsigned char fcgi_connection::RESPONSE_COMPLETED_STATE = 6;

fcgi_connection::fcgi_connection()
{
	_M_fd = -1;

	_M_tmpfile = -1;

	_M_client = NULL;

	_M_state = CONNECTING_STATE;
}

void fcgi_connection::reset()
{
	tcp_connection::reset();

	_M_fd = -1;

	if (_M_tmpfile != -1) {
		static_cast<http_server*>(_M_server)->_M_tmpfiles.close(_M_tmpfile);
		_M_tmpfile = -1;
	}

	_M_client = NULL;

	_M_state = CONNECTING_STATE;
}

bool fcgi_connection::loop(unsigned fd)
{
	size_t total = 0;
	int error;

	do {
		logger::instance().log(logger::LOG_DEBUG, "[fcgi_connection::loop] (fd %d) State = %s.", fd, state_to_string(_M_state));

		switch (_M_state) {
			case CONNECTING_STATE:
				if (!_M_client) {
					return false;
				}

				if ((!socket_wrapper::get_socket_error(fd, error)) || (error)) {
					logger::instance().log(logger::LOG_WARNING, "(fd %d) Connection to backend failed with error: %d.", fd, error);

					_M_client->_M_rule->backends.connection_failed(fd);

					_M_client->_M_error = http_error::GATEWAY_TIMEOUT;
					_M_state = PREPARING_ERROR_PAGE_STATE;
				} else {
					if (!_M_client->_M_payload_in_memory) {
						socket_wrapper::cork(fd);
					}

					_M_state = SENDING_REQUEST_STATE;
				}

				break;
			case SENDING_REQUEST_STATE:
				if (!_M_writable) {
					return true;
				}

				if (!write(fd, total)) {
					_M_client->_M_error = http_error::GATEWAY_TIMEOUT;
					_M_state = PREPARING_ERROR_PAGE_STATE;
				} else {
					_M_client->_M_timestamp = now::_M_time;

					if (_M_outp == (off_t) _M_out.count()) {
						if (_M_client->_M_payload_in_memory) {
							if (!modify(fd, tcp_server::READ)) {
								_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
								_M_state = PREPARING_ERROR_PAGE_STATE;
							} else {
								_M_out.reset();

								// Reuse client's headers.
								_M_client->_M_headers.reset();

								_M_client->_M_body.reset();

								_M_state = READING_HEADERS_STATE;
							}
						} else {
							_M_outp = 0;

							_M_state = SENDING_STDIN_STATE;
						}
					}
				}

				break;
			case SENDING_STDIN_STATE:
				if (!_M_writable) {
					return true;
				}

				if (!sendfile(fd, _M_client->_M_tmpfile, _M_client->_M_tmpfilesize, total)) {
					_M_client->_M_error = http_error::GATEWAY_TIMEOUT;
					_M_state = PREPARING_ERROR_PAGE_STATE;
				} else {
					_M_client->_M_timestamp = now::_M_time;

					if (_M_outp == _M_client->_M_tmpfilesize) {
						// We don't need the client's temporary file anymore.
						static_cast<http_server*>(_M_server)->_M_tmpfiles.close(_M_client->_M_tmpfile);
						_M_client->_M_tmpfile = -1;

						socket_wrapper::uncork(fd);

						if (!modify(fd, tcp_server::READ)) {
							_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
							_M_state = PREPARING_ERROR_PAGE_STATE;
						} else {
							_M_out.reset();

							// Reuse client's headers.
							_M_client->_M_headers.reset();

							_M_client->_M_body.reset();

							_M_state = READING_HEADERS_STATE;
						}
					}
				}

				break;
			case READING_HEADERS_STATE:
			case READING_BODY_STATE:
				if (!_M_readable) {
					return true;
				}

				if (!read_response(fd, total)) {
					_M_state = PREPARING_ERROR_PAGE_STATE;
				}

				break;
			case PREPARING_ERROR_PAGE_STATE:
				if (!http_error::build_page(_M_client)) {
					return false;
				}

				_M_client->_M_response_header_size = _M_client->_M_out.count();

				if ((_M_client->_M_method == http_method::HEAD) || (_M_client->_M_bodyp->count() == 0)) {
					_M_client->_M_filesize = 0;
				} else {
					_M_client->_M_filesize = _M_client->_M_bodyp->count();
				}

				_M_client->_M_payload_in_memory = 1;

				_M_client->_M_in_ready_list = 1;

				_M_client->_M_state = http_connection::SENDING_BACKEND_HEADERS_STATE;

				return false;
			case RESPONSE_COMPLETED_STATE:
				if (!prepare_http_response(fd)) {
					_M_state = PREPARING_ERROR_PAGE_STATE;
				} else {
					_M_client->_M_payload_in_memory = 0;

					_M_client->_M_in_ready_list = 1;

					_M_client->_M_tmpfile = _M_tmpfile;
					_M_tmpfile = -1;

					_M_client->_M_state = http_connection::SENDING_BACKEND_HEADERS_STATE;

					socket_wrapper::cork(_M_fd);

					return false;
				}

				break;
		}
	} while (!_M_in_ready_list);

	return true;
}

bool fcgi_connection::read_response(unsigned fd, size_t& total)
{
	io_result res;

	do {
		if ((res = read(fd, _M_client->_M_body, total)) == IO_ERROR) {
			_M_client->_M_error = http_error::GATEWAY_TIMEOUT;

			return false;
		} else if (res == IO_NO_DATA_READ) {
			return true;
		}

		_M_client->_M_timestamp = now::_M_time;

		if (!process(_M_client->_M_body)) {
			_M_client->_M_error = http_error::BAD_GATEWAY;

			return false;
		}
	} while ((res == IO_SUCCESS) && (_M_state != RESPONSE_COMPLETED_STATE));

	return true;
}

bool fcgi_connection::prepare_http_response(unsigned fd)
{
	http_headers* headers = &_M_client->_M_headers;

	const char* value;
	unsigned short valuelen;
	if (!headers->get_value_known_header(http_headers::STATUS_HEADER, value, &valuelen)) {
		value = "200 OK";
		valuelen = 6;
	}

	buffer* out = &_M_client->_M_out;

	out->reset();

	if ((!out->append("HTTP/1.1 ", 9)) || (!out->append(value, valuelen)) || (!out->append("\r\n", 2))) {
		_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
		return false;
	}

	headers->remove_known_header(http_headers::STATUS_HEADER);

	if (!headers->add_known_header(http_headers::DATE_HEADER, &now::_M_tm)) {
		_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
		return false;
	}

	if (_M_client->_M_keep_alive) {
		if (!headers->add_known_header(http_headers::CONNECTION_HEADER, "Keep-Alive", 10, true)) {
			_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
			return false;
		}
	} else {
		if (!headers->add_known_header(http_headers::CONNECTION_HEADER, "close", 5, true)) {
			_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
			return false;
		}
	}

	if (!headers->add_known_header(http_headers::SERVER_HEADER, WEBSERVER_NAME, sizeof(WEBSERVER_NAME) - 1, true)) {
		_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
		return false;
	}

	char num[32];
	int numlen = snprintf(num, sizeof(num), "%lld", _M_client->_M_filesize);
	if (!headers->add_known_header(http_headers::CONTENT_LENGTH_HEADER, num, numlen, true)) {
		_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
		return false;
	}

	if (!headers->serialize(*out)) {
		_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
		return false;
	}

	_M_client->_M_response_header_size = out->count();

	return true;
}

bool fcgi_connection::stdout_stream(unsigned short requestId, const void* buf, unsigned short len)
{
	if (_M_state == READING_HEADERS_STATE) {
		if (!_M_out.append((const char*) buf, len)) {
			return false;
		}

		size_t body_offset;
		http_headers::parse_result parse_result = _M_client->_M_headers.parse(_M_out.data(), _M_out.count(), body_offset);
		if (parse_result == http_headers::END_OF_HEADER) {
			// Open a temporary file.
			if ((_M_tmpfile = static_cast<http_server*>(_M_server)->_M_tmpfiles.open()) < 0) {
				return false;
			}

			size_t left = _M_out.count() - body_offset;
			if (left > 0) {
				if (!file_wrapper::write(_M_tmpfile, _M_out.data() + body_offset, left)) {
					return false;
				}
			}

			_M_client->_M_backend_response_header_size = body_offset;
			_M_client->_M_filesize = left;

			_M_state = READING_BODY_STATE;
		} else if (parse_result == http_headers::ERROR_WRONG_HEADERS) {
			logger::instance().log(logger::LOG_DEBUG, "[fcgi_connection::stdout_stream] Invalid headers.");

			return false;
		} else if (parse_result == http_headers::ERROR_HEADERS_TOO_LARGE) {
			logger::instance().log(logger::LOG_DEBUG, "[fcgi_connection::stdout_stream] Headers too large.");

			return false;
		} else if (parse_result == http_headers::ERROR_NO_MEMORY) {
			return false;
		}
	} else {
		if (!file_wrapper::write(_M_tmpfile, buf, len)) {
			return false;
		}

		_M_client->_M_filesize += len;
	}

	return true;
}

const char* fcgi_connection::state_to_string(unsigned state)
{
	switch (state) {
		case CONNECTING_STATE:
			return "CONNECTING_STATE";
		case SENDING_REQUEST_STATE:
			return "SENDING_REQUEST_STATE";
		case SENDING_STDIN_STATE:
			return "SENDING_STDIN_STATE";
		case READING_HEADERS_STATE:
			return "READING_HEADERS_STATE";
		case READING_BODY_STATE:
			return "READING_BODY_STATE";
		case PREPARING_ERROR_PAGE_STATE:
			return "PREPARING_ERROR_PAGE_STATE";
		case RESPONSE_COMPLETED_STATE:
			return "RESPONSE_COMPLETED_STATE";
		default:
			return "(unknown)";
	}
}
