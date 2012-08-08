#include <stdlib.h>
#include <stdio.h>
#include "proxy_connection.h"
#include "http/http_server.h"
#include "http/version.h"
#include "net/socket_wrapper.h"
#include "net/tcp_connection.inl"
#include "util/number.h"
#include "util/now.h"
#include "string/memcasemem.h"
#include "logger/logger.h"
#include "macros/macros.h"

const unsigned short proxy_connection::STATUS_LINE_MAX_LEN = 1024;

const unsigned char proxy_connection::CONNECTING_STATE = 0;
const unsigned char proxy_connection::SENDING_HEADERS_STATE = 1;
const unsigned char proxy_connection::SENDING_BODY_STATE = 2;
const unsigned char proxy_connection::READING_STATUS_LINE_STATE = 3;
const unsigned char proxy_connection::READING_HEADERS_STATE = 4;
const unsigned char proxy_connection::PROCESSING_RESPONSE_STATE = 5;
const unsigned char proxy_connection::READING_BODY_STATE = 6;
const unsigned char proxy_connection::READING_CHUNKED_BODY_STATE = 7;
const unsigned char proxy_connection::READING_UNKNOWN_SIZE_BODY_STATE = 8;
const unsigned char proxy_connection::PREPARING_ERROR_PAGE_STATE = 9;
const unsigned char proxy_connection::RESPONSE_COMPLETED_STATE = 10;

proxy_connection::proxy_connection()
{
	_M_fd = -1;

	_M_tmpfile = -1;

	_M_client = NULL;

	_M_status_code = 0;

	_M_reason_phrase_len = 0;

	_M_state = CONNECTING_STATE;
	_M_substate = 0;
}

void proxy_connection::reset()
{
	tcp_connection::reset();
	chunked_parser::reset();

	_M_fd = -1;

	if (_M_tmpfile != -1) {
		static_cast<http_server*>(_M_server)->_M_tmpfiles.close(_M_tmpfile);
		_M_tmpfile = -1;
	}

	_M_client = NULL;

	_M_status_code = 0;

	_M_reason_phrase_len = 0;

	_M_state = CONNECTING_STATE;
	_M_substate = 0;
}

bool proxy_connection::loop(unsigned fd)
{
	size_t total = 0;
	size_t count;
	int error;
	bool ret;

	do {
		logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::loop] (fd %d) State = %s.", fd, state_to_string(_M_state));

		switch (_M_state) {
			case CONNECTING_STATE:
				if (!_M_client) {
					return false;
				}

				if ((!socket_wrapper::get_socket_error(fd, error)) || (error)) {
					logger::instance().log(logger::LOG_WARNING, "(fd %d) Connection to backend failed with error: %d.", fd, error);

#if !PROXY
					_M_client->_M_rule->backends.connection_failed(fd);
#endif

					_M_client->_M_error = http_error::GATEWAY_TIMEOUT;
					_M_state = PREPARING_ERROR_PAGE_STATE;
				} else {
					if (!_M_client->_M_payload_in_memory) {
						socket_wrapper::cork(fd);
					}

					_M_state = SENDING_HEADERS_STATE;
				}

				break;
			case SENDING_HEADERS_STATE:
				if (!_M_writable) {
					return true;
				}

				if ((!_M_client->_M_payload_in_memory) || (_M_client->_M_request_body_size == 0)) {
					count = _M_out.count();

					ret = write(fd, total);
				} else {
					socket_wrapper::io_vector io_vector[2];

					io_vector[0].iov_base = _M_out.data();
					io_vector[0].iov_len = _M_out.count();

					io_vector[1].iov_base = _M_client->_M_in.data() + _M_client->_M_request_header_size;
					io_vector[1].iov_len = _M_client->_M_request_body_size;

					count = io_vector[0].iov_len + io_vector[1].iov_len;

					ret = writev(fd, io_vector, 2, total);
				}

				if (!ret) {
					_M_client->_M_error = http_error::GATEWAY_TIMEOUT;
					_M_state = PREPARING_ERROR_PAGE_STATE;
				} else {
					_M_client->_M_timestamp = now::_M_time;

					if (_M_outp == count) {
						if (_M_client->_M_payload_in_memory) {
							if (!modify(fd, tcp_server::READ)) {
								_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
								_M_state = PREPARING_ERROR_PAGE_STATE;
							} else {
								_M_client->_M_body.reset();

								_M_state = READING_STATUS_LINE_STATE;
							}
						} else {
							_M_outp = 0;

							_M_state = SENDING_BODY_STATE;
						}
					}
				}

				break;
			case SENDING_BODY_STATE:
				if (!_M_writable) {
					return true;
				}

				if (!sendfile(fd, _M_client->_M_tmpfile, _M_client->_M_request_body_size, total)) {
					_M_client->_M_error = http_error::GATEWAY_TIMEOUT;
					_M_state = PREPARING_ERROR_PAGE_STATE;
				} else {
					_M_client->_M_timestamp = now::_M_time;

					if (_M_outp == _M_client->_M_request_body_size) {
						// We don't need the client's temporary file anymore.
						static_cast<http_server*>(_M_server)->_M_tmpfiles.close(_M_client->_M_tmpfile);
						_M_client->_M_tmpfile = -1;

						socket_wrapper::uncork(fd);

						if (!modify(fd, tcp_server::READ)) {
							_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
							_M_state = PREPARING_ERROR_PAGE_STATE;
						} else {
							_M_client->_M_body.reset();

							_M_state = READING_STATUS_LINE_STATE;
						}
					}
				}

				break;
			case READING_STATUS_LINE_STATE:
				if (!_M_readable) {
					return true;
				}

				if (!read_status_line(fd, total)) {
					_M_state = PREPARING_ERROR_PAGE_STATE;
				}

				break;
			case READING_HEADERS_STATE:
				if (!_M_readable) {
					return true;
				}

				if (!read_headers(fd, total)) {
					_M_state = PREPARING_ERROR_PAGE_STATE;
				}

				break;
			case PROCESSING_RESPONSE_STATE:
				if (!process_response(fd)) {
					_M_state = PREPARING_ERROR_PAGE_STATE;
				}

				break;
			case READING_BODY_STATE:
				if (!_M_readable) {
					return true;
				}

				if (!read_body(fd, total)) {
					_M_state = PREPARING_ERROR_PAGE_STATE;
				}

				break;
			case READING_CHUNKED_BODY_STATE:
				if (!_M_readable) {
					return true;
				}

				if (!read_chunked_body(fd, total)) {
					_M_state = PREPARING_ERROR_PAGE_STATE;
				}

				break;
			case READING_UNKNOWN_SIZE_BODY_STATE:
				if (!_M_readable) {
					return true;
				}

				if (!read_unknown_size_body(fd, total)) {
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
					_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
					_M_state = PREPARING_ERROR_PAGE_STATE;
				} else {
					_M_client->_M_in_ready_list = 1;

					_M_client->_M_tmpfile = _M_tmpfile;
					_M_tmpfile = -1;

					_M_client->_M_state = http_connection::SENDING_BACKEND_HEADERS_STATE;

					if (!_M_client->_M_payload_in_memory) {
						socket_wrapper::cork(_M_fd);
					}

					return false;
				}

				break;
		}
	} while (!_M_in_ready_list);

	return true;
}

bool proxy_connection::read_status_line(unsigned fd, size_t& total)
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

		parse_result parse_result = parse_status_line(fd);
		if (parse_result == PARSE_ERROR) {
			_M_client->_M_error = http_error::BAD_GATEWAY;

			return false;
		} else if (parse_result == PARSING_COMPLETED) {
			// Reuse client's headers.
			_M_client->_M_headers.reset();

			parse_result = parse_headers(fd);
			if (parse_result == PARSE_ERROR) {
				_M_client->_M_error = http_error::BAD_GATEWAY;

				return false;
			} else if (parse_result == PARSING_NOT_COMPLETED) {
				_M_state = READING_HEADERS_STATE;
			} else {
				_M_state = PROCESSING_RESPONSE_STATE;
			}

			return true;
		}
	} while (res == IO_SUCCESS);

	return true;
}

bool proxy_connection::read_headers(unsigned fd, size_t& total)
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

		parse_result parse_result = parse_headers(fd);
		if (parse_result == PARSE_ERROR) {
			return false;
		} else if (parse_result == PARSING_COMPLETED) {
			_M_state = PROCESSING_RESPONSE_STATE;
			return true;
		}
	} while (res == IO_SUCCESS);

	return true;
}

bool proxy_connection::read_body(unsigned fd, size_t& total)
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

		size_t count = MIN(_M_left, _M_client->_M_body.count() - _M_inp);

		if (!_M_client->_M_payload_in_memory) {
			if (!file_wrapper::write(_M_tmpfile, _M_client->_M_body.data(), count)) {
				_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
				return false;
			}

			_M_client->_M_body.reset();
		} else {
			_M_inp += count;
		}

		if (count == _M_left) {
			_M_state = RESPONSE_COMPLETED_STATE;
			return true;
		}

		_M_left -= count;
	} while (res == IO_SUCCESS);

	return true;
}

bool proxy_connection::read_chunked_body(unsigned fd, size_t& total)
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

		switch (parse_chunk(_M_client->_M_body.data(), _M_client->_M_body.count())) {
			case chunked_parser::INVALID_CHUNKED_RESPONSE:
				_M_client->_M_error = http_error::BAD_GATEWAY;
				return false;
			case chunked_parser::CALLBACK_FAILED:
				_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
				return false;
			case chunked_parser::NOT_END_OF_RESPONSE:
				_M_client->_M_body.reset();
				break;
			default:
				_M_state = RESPONSE_COMPLETED_STATE;
				return true;
		}
	} while (res == IO_SUCCESS);

	return true;
}

bool proxy_connection::read_unknown_size_body(unsigned fd, size_t& total)
{
	io_result res;

	do {
		if ((res = read(fd, _M_client->_M_body, total)) == IO_ERROR) {
			_M_state = RESPONSE_COMPLETED_STATE;
			return true;
		} else if (res == IO_NO_DATA_READ) {
			return true;
		}

		_M_client->_M_timestamp = now::_M_time;

		size_t count = _M_client->_M_body.count();

		if (!file_wrapper::write(_M_tmpfile, _M_client->_M_body.data(), count)) {
			_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
			return false;
		}

		_M_client->_M_body.reset();

		_M_client->_M_filesize += count;
	} while (res == IO_SUCCESS);

	return true;
}

proxy_connection::parse_result proxy_connection::parse_status_line(unsigned fd)
{
	const char* data = _M_client->_M_body.data();
	size_t len = _M_client->_M_body.count();

	while (_M_inp < len) {
		unsigned char c = (unsigned char) data[_M_inp];
		switch (_M_substate) {
			case 0: // Initial state.
				if ((c == 'H') || (c == 'h')) {
					_M_substate = 1; // [H]TTP/<major>.<minor> <status-code>
					break;
				} else if (!IS_WHITE_SPACE(c)) {
					return PARSE_ERROR;
				}

				break;
			case 1: // [H]TTP/<major>.<minor>
				if ((c == 'T') || (c == 't')) {
					_M_substate = 2; // H[T]TP/<major>.<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 2: // H[T]TP/<major>.<minor>
				if ((c == 'T') || (c == 't')) {
					_M_substate = 3; // HT[T]P/<major>.<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 3: // HT[T]P/<major>.<minor>
				if ((c == 'P') || (c == 'p')) {
					_M_substate = 4; // HTT[P]/<major>.<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 4: // HTT[P]/<major>.<minor>
				if (c == '/') {
					_M_substate = 5; // HTTP[/]<major>.<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 5: // HTTP[/]<major>.<minor>
				if ((c >= '0') && (c <= '9')) {
					_M_major_number = c - '0';
					if (_M_major_number > 1) {
						return PARSE_ERROR;
					}

					_M_substate = 6; // HTTP/[<major>].<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 6: // HTTP/[<major>].<minor>
				if (c == '.') {
					_M_substate = 7; // HTTP/<major>[.]<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 7: // HTTP/<major>[.]<minor>
				if ((c >= '0') && (c <= '9')) {
					_M_minor_number = c - '0';
					if ((_M_major_number == 1) && (_M_minor_number > 1)) {
						return PARSE_ERROR;
					}

					_M_substate = 8; // HTTP/<major>.[<minor>]
				} else {
					return PARSE_ERROR;
				}

				break;
			case 8: // HTTP/<major>.[<minor>]
				if (!IS_WHITE_SPACE(c)) {
					return PARSE_ERROR;
				}

				_M_substate = 9; // Whitespace after version.

				break;
			case 9: // Whitespace after version.
				if (IS_DIGIT(c)) {
					_M_status_code = c - '0';

					_M_substate = 10; // Status code.
				} else if (!IS_WHITE_SPACE(c)) {
					return PARSE_ERROR;
				}

				break;
			case 10: // Status code.
				if (IS_DIGIT(c)) {
					_M_status_code = (_M_status_code * 10) + (c - '0');
					if (_M_status_code >= 600) {
						return PARSE_ERROR;
					}
				} else if (IS_WHITE_SPACE(c)) {
					if (_M_status_code < 100) {
						return PARSE_ERROR;
					}

					_M_substate = 11; // Whitespace after status code.
				} else if (c == '\r') {
					if (_M_status_code < 100) {
						return PARSE_ERROR;
					}

					_M_substate = 13; // '\r' at the end of status line.
				} else if (c == '\n') {
					if (_M_status_code < 100) {
						return PARSE_ERROR;
					}

					_M_inp++;

					return PARSING_COMPLETED;
				}

				break;
			case 11: // Whitespace after status code.
				if (c > ' ') {
					_M_reason_phrase = _M_inp;

					_M_substate = 12; // Reason phrase.
				} else if (c == '\r') {
					if (_M_status_code < 100) {
						return PARSE_ERROR;
					}

					_M_substate = 13; // '\r' at the end of status line.
				} else if (c == '\n') {
					if (_M_status_code < 100) {
						return PARSE_ERROR;
					}

					_M_inp++;

					return PARSING_COMPLETED;
				}

				break;
			case 12: // Reason phrase.
				if (IS_WHITE_SPACE(c)) {
					if (_M_reason_phrase_len == 0) {
						_M_reason_phrase_len = _M_inp - _M_reason_phrase;
					}
				} else if (c > ' ') {
					_M_reason_phrase_len = 0;
				} else if (c == '\r') {
					if (_M_reason_phrase_len == 0) {
						_M_reason_phrase_len = _M_inp - _M_reason_phrase;
					}

					_M_substate = 13; // '\r' at the end of status line.
				} else if (c == '\n') {
					if (_M_reason_phrase_len == 0) {
						_M_reason_phrase_len = _M_inp - _M_reason_phrase;
					}

					_M_inp++;

					return PARSING_COMPLETED;
				} else if (c < ' ') {
					return PARSE_ERROR;
				}

				break;
			case 13: // '\r' at the end of status line.
				if (c == '\n') {
					_M_inp++;

					return PARSING_COMPLETED;
				} else {
					return PARSE_ERROR;
				}

				break;
		}

		if (++_M_inp == STATUS_LINE_MAX_LEN) {
			return PARSE_ERROR;
		}
	}

	return PARSING_NOT_COMPLETED;
}

proxy_connection::parse_result proxy_connection::parse_headers(unsigned fd)
{
	http_headers::parse_result parse_result = _M_client->_M_headers.parse(_M_client->_M_body.data() + _M_inp, _M_client->_M_body.count() - _M_inp, _M_body_offset);
	if (parse_result == http_headers::END_OF_HEADER) {
		_M_body_offset += _M_inp;
		_M_inp = _M_client->_M_body.count();

		return PARSING_COMPLETED;
	} else if (parse_result == http_headers::ERROR_WRONG_HEADERS) {
		logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::parse_headers] (fd %d) Invalid headers.", fd);

		_M_client->_M_error = http_error::BAD_GATEWAY;
		return PARSE_ERROR;
	} else if (parse_result == http_headers::ERROR_HEADERS_TOO_LARGE) {
		logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::parse_headers] (fd %d) Headers too large.", fd);

		_M_client->_M_error = http_error::BAD_GATEWAY;
		return PARSE_ERROR;
	} else if (parse_result == http_headers::ERROR_NO_MEMORY) {
		_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
		return PARSE_ERROR;
	}

	return PARSING_NOT_COMPLETED;
}

bool proxy_connection::process_response(unsigned fd)
{
	const char* value;
	unsigned short valuelen;

	if (_M_client->_M_method == http_method::HEAD) {
		_M_client->_M_payload_in_memory = 1;

		if (!_M_client->_M_headers.get_value_known_header(http_headers::CONTENT_LENGTH_HEADER, value, &valuelen)) {
			_M_client->_M_filesize = 0;
		} else {
			if (number::parse_off_t(value, valuelen, _M_client->_M_filesize, 0) != number::PARSE_SUCCEEDED) {
				logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::process_response] (fd %d) Invalid body size.", fd);

				_M_client->_M_error = http_error::BAD_GATEWAY;
				return false;
			}
		}

		_M_state = RESPONSE_COMPLETED_STATE;
	} else {
		size_t count = _M_client->_M_body.count() - _M_body_offset;

		// Transfer-Encoding: chunked?
		bool chunked;
		if ((_M_client->_M_headers.get_value_known_header(http_headers::TRANSFER_ENCODING_HEADER, value, &valuelen)) && (memcasemem(value, valuelen, "chunked", 7))) {
			logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::process_response] (fd %d) Chunked body.", fd);

			chunked = true;

			_M_client->_M_filesize = 0;

			_M_client->_M_payload_in_memory = 0;

			_M_state = READING_CHUNKED_BODY_STATE;
		} else {
			chunked = false;

			// Get Content-Length.
			if (!_M_client->_M_headers.get_value_known_header(http_headers::CONTENT_LENGTH_HEADER, value, &valuelen)) {
				if ((_M_status_code < 200) || (_M_status_code == 204) || (_M_status_code == 302) || (_M_status_code == 304)) {
					_M_client->_M_filesize = 0;

					_M_client->_M_payload_in_memory = 1;

					_M_state = RESPONSE_COMPLETED_STATE;
				} else {
					if (_M_client->_M_headers.get_value_known_header(http_headers::CONNECTION_HEADER, value, &valuelen)) {
						if (memcasemem(value, valuelen, "Keep-Alive", 10)) {
							// We cannot know the Content-Length.
							logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::process_response] (fd %d) !Content-Length && Keep-Alive.", fd);

							_M_client->_M_error = http_error::BAD_GATEWAY;
							return false;
						}
					} else {
						if ((_M_major_number == 1) && (_M_minor_number == 1)) {
							// We cannot know the Content-Length.
							logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::process_response] (fd %d) !Content-Length && !Connection && HTTP/1.1.", fd);

							_M_client->_M_error = http_error::BAD_GATEWAY;
							return false;
						}
					}

					logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::process_response] (fd %d) Unknown body size.", fd);

					_M_client->_M_filesize = count;

					_M_client->_M_payload_in_memory = 0;

					_M_state = READING_UNKNOWN_SIZE_BODY_STATE;
				}
			} else {
				if (number::parse_off_t(value, valuelen, _M_client->_M_filesize, 0) != number::PARSE_SUCCEEDED) {
					logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::process_response] (fd %d) Invalid body size.", fd);

					_M_client->_M_error = http_error::BAD_GATEWAY;
					return false;
				}

				logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::process_response] (fd %d) Body size %lld.", fd, _M_client->_M_filesize);

				// If we have received the whole body already...
				if (count >= _M_client->_M_filesize) {
					_M_client->_M_payload_in_memory = 1;

					_M_state = RESPONSE_COMPLETED_STATE;
				} else {
					_M_client->_M_payload_in_memory = (_M_client->_M_filesize <= static_cast<http_server*>(_M_server)->_M_max_payload_in_memory);

					_M_left = _M_client->_M_filesize - count;

					_M_state = READING_BODY_STATE;
				}
			}
		}

		logger::instance().log(logger::LOG_DEBUG, "[proxy_connection::process_response] (fd %d) Payload will be saved %s.", fd, _M_client->_M_payload_in_memory ? "in memory" : "to disk");

		if (!_M_client->_M_payload_in_memory) {
			// Open a temporary file.
			if ((_M_tmpfile = static_cast<http_server*>(_M_server)->_M_tmpfiles.open()) < 0) {
				_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
				return false;
			}

			if (count > 0) {
				if (!chunked) {
					if (!file_wrapper::write(_M_tmpfile, _M_client->_M_body.data() + _M_body_offset, count)) {
						_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
						return false;
					}
				} else {
					switch (parse_chunk(_M_client->_M_body.data() + _M_body_offset, count)) {
						case chunked_parser::INVALID_CHUNKED_RESPONSE:
							_M_client->_M_error = http_error::BAD_GATEWAY;
							return false;
						case chunked_parser::CALLBACK_FAILED:
							_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
							return false;
						case chunked_parser::END_OF_RESPONSE:
							_M_state = RESPONSE_COMPLETED_STATE;
							break;
						default: // NOT_END_OF_RESPONSE.
							;
					}
				}
			}

			// If there is reason phrase...
			if (_M_reason_phrase_len > 0) {
				_M_out.reset();
				if (!_M_out.append(_M_client->_M_body.data() + _M_reason_phrase, _M_reason_phrase_len)) {
					_M_client->_M_error = http_error::INTERNAL_SERVER_ERROR;
					return false;
				}
			}

			_M_client->_M_body.reset();
			_M_inp = 0;
		}
	}

	return true;
}

bool proxy_connection::prepare_http_response(unsigned fd)
{
	buffer* out = &_M_client->_M_out;

	out->reset();

	if (_M_reason_phrase_len == 0) {
		if (!out->format("HTTP/1.1 %u\r\n", _M_status_code)) {
			return false;
		}
	} else {
		const char* reason_phrase = _M_client->_M_payload_in_memory ? _M_client->_M_body.data() + _M_reason_phrase : _M_out.data();

		if (!out->format("HTTP/1.1 %u %.*s\r\n", _M_status_code, _M_reason_phrase_len, reason_phrase)) {
			return false;
		}
	}

	http_headers* headers = &_M_client->_M_headers;

	if (_M_client->_M_keep_alive) {
		if (!headers->add_known_header(http_headers::CONNECTION_HEADER, "Keep-Alive", 10, true)) {
			return false;
		}
	} else {
		if (!headers->add_known_header(http_headers::CONNECTION_HEADER, "close", 5, true)) {
			return false;
		}
	}

	headers->remove_known_header(http_headers::TRANSFER_ENCODING_HEADER);

	if (!headers->add_known_header(http_headers::SERVER_HEADER, WEBSERVER_NAME, sizeof(WEBSERVER_NAME) - 1, true)) {
		return false;
	}

	char num[32];
	int numlen = snprintf(num, sizeof(num), "%lld", _M_client->_M_filesize);
	if (!headers->add_known_header(http_headers::CONTENT_LENGTH_HEADER, num, numlen, true)) {
		return false;
	}

	if (!headers->serialize(*out)) {
		return false;
	}

	_M_client->_M_response_header_size = out->count();

	if (_M_client->_M_payload_in_memory) {
		_M_client->_M_backend_response_header_size = _M_body_offset;
	}

	return true;
}

const char* proxy_connection::state_to_string(unsigned state)
{
	switch (state) {
		case CONNECTING_STATE:
			return "CONNECTING_STATE";
		case SENDING_HEADERS_STATE:
			return "SENDING_HEADERS_STATE";
		case SENDING_BODY_STATE:
			return "SENDING_BODY_STATE";
		case READING_STATUS_LINE_STATE:
			return "READING_STATUS_LINE_STATE";
		case READING_HEADERS_STATE:
			return "READING_HEADERS_STATE";
		case PROCESSING_RESPONSE_STATE:
			return "PROCESSING_RESPONSE_STATE";
		case READING_BODY_STATE:
			return "READING_BODY_STATE";
		case READING_CHUNKED_BODY_STATE:
			return "READING_CHUNKED_BODY_STATE";
		case READING_UNKNOWN_SIZE_BODY_STATE:
			return "READING_UNKNOWN_SIZE_BODY_STATE";
		case PREPARING_ERROR_PAGE_STATE:
			return "PREPARING_ERROR_PAGE_STATE";
		case RESPONSE_COMPLETED_STATE:
			return "RESPONSE_COMPLETED_STATE";
		default:
			return "(unknown)";
	}
}
