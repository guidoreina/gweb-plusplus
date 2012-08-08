#include <stdlib.h>
#include <stdio.h>
#include "http_headers.h"
#include "constants/months_and_days.h"
#include "macros/macros.h"

const size_t http_headers::MAX_HEADERS_SIZE = 8 * 1024;

const unsigned char http_headers::ACCEPT_HEADER               = 0;
const unsigned char http_headers::ACCEPT_CHARSET_HEADER       = 1;
const unsigned char http_headers::ACCEPT_ENCODING_HEADER      = 2;
const unsigned char http_headers::ACCEPT_LANGUAGE_HEADER      = 3;
const unsigned char http_headers::ACCEPT_RANGES_HEADER        = 4;
const unsigned char http_headers::AGE_HEADER                  = 5;
const unsigned char http_headers::ALLOW_HEADER                = 6;
const unsigned char http_headers::AUTHORIZATION_HEADER        = 7;
const unsigned char http_headers::CACHE_CONTROL_HEADER        = 8;
const unsigned char http_headers::CONNECTION_HEADER           = 9;
const unsigned char http_headers::CONTENT_ENCODING_HEADER     = 10;
const unsigned char http_headers::CONTENT_LANGUAGE_HEADER     = 11;
const unsigned char http_headers::CONTENT_LENGTH_HEADER       = 12;
const unsigned char http_headers::CONTENT_LOCATION_HEADER     = 13;
const unsigned char http_headers::CONTENT_MD5_HEADER          = 14;
const unsigned char http_headers::CONTENT_RANGE_HEADER        = 15;
const unsigned char http_headers::CONTENT_TYPE_HEADER         = 16;
const unsigned char http_headers::COOKIE_HEADER               = 17;
const unsigned char http_headers::DATE_HEADER                 = 18;
const unsigned char http_headers::ETAG_HEADER                 = 19;
const unsigned char http_headers::EXPECT_HEADER               = 20;
const unsigned char http_headers::EXPIRES_HEADER              = 21;
const unsigned char http_headers::FROM_HEADER                 = 22;
const unsigned char http_headers::HOST_HEADER                 = 23;
const unsigned char http_headers::IF_MATCH_HEADER             = 24;
const unsigned char http_headers::IF_MODIFIED_SINCE_HEADER    = 25;
const unsigned char http_headers::IF_NONE_MATCH_HEADER        = 26;
const unsigned char http_headers::IF_RANGE_HEADER             = 27;
const unsigned char http_headers::IF_UNMODIFIED_SINCE_HEADER  = 28;
const unsigned char http_headers::KEEP_ALIVE_HEADER           = 29;
const unsigned char http_headers::LAST_MODIFIED_HEADER        = 30;
const unsigned char http_headers::LOCATION_HEADER             = 31;
const unsigned char http_headers::MAX_FORWARDS_HEADER         = 32;
const unsigned char http_headers::PRAGMA_HEADER               = 33;
const unsigned char http_headers::PROXY_AUTHENTICATE_HEADER   = 34;
const unsigned char http_headers::PROXY_AUTHORIZATION_HEADER  = 35;
const unsigned char http_headers::PROXY_CONNECTION_HEADER     = 36;
const unsigned char http_headers::RANGE_HEADER                = 37;
const unsigned char http_headers::REFERER_HEADER              = 38;
const unsigned char http_headers::RETRY_AFTER_HEADER          = 39;
const unsigned char http_headers::SERVER_HEADER               = 40;
const unsigned char http_headers::SET_COOKIE_HEADER           = 41;
const unsigned char http_headers::STATUS_HEADER               = 42;
const unsigned char http_headers::TE_HEADER                   = 43;
const unsigned char http_headers::TRAILER_HEADER              = 44;
const unsigned char http_headers::TRANSFER_ENCODING_HEADER    = 45;
const unsigned char http_headers::UPGRADE_HEADER              = 46;
const unsigned char http_headers::USER_AGENT_HEADER           = 47;
const unsigned char http_headers::VARY_HEADER                 = 48;
const unsigned char http_headers::VIA_HEADER                  = 49;
const unsigned char http_headers::WARNING_HEADER              = 50;
const unsigned char http_headers::WWW_AUTHENTICATE_HEADER     = 51;
const unsigned char http_headers::UNKNOWN_HEADER              = 0xff;

const unsigned char http_headers::GENERAL_HEADER_FIELD        = 0;
const unsigned char http_headers::REQUEST_HEADER_FIELD        = 1;
const unsigned char http_headers::RESPONSE_HEADER_FIELD       = 2;
const unsigned char http_headers::ENTITY_HEADER_FIELD         = 3;

const size_t http_headers::BOUNDARY_WIDTH                     = 11;

const size_t http_headers::HTTP_HEADER_ALLOC = 10;
const size_t http_headers::DATA_ALLOC = 1024;
const unsigned short http_headers::MAX_LINE_LEN = 76;

const http_headers::known_http_header http_headers::_M_known_http_headers[] = {
	{"Accept",               6, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"Accept-Charset",      14, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"Accept-Encoding",     15, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"Accept-Language",     15, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"Accept-Ranges",       13, RESPONSE_HEADER_FIELD, 1, 0, 0},
	{"Age",                  3, RESPONSE_HEADER_FIELD, 0, 1, 0},
	{"Allow",                5, ENTITY_HEADER_FIELD,   1, 0, 0},
	{"Authorization",       13, REQUEST_HEADER_FIELD,  0, 0, 0},
	{"Cache-Control",       13, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"Connection",          10, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"Content-Encoding",    16, ENTITY_HEADER_FIELD,   1, 0, 0},
	{"Content-Language",    16, ENTITY_HEADER_FIELD,   1, 0, 0},
	{"Content-Length",      14, ENTITY_HEADER_FIELD,   0, 1, 0},
	{"Content-Location",    16, ENTITY_HEADER_FIELD,   0, 1, 0},
	{"Content-MD5",         11, ENTITY_HEADER_FIELD,   0, 1, 0},
	{"Content-Range",       13, ENTITY_HEADER_FIELD,   0, 0, 0},
	{"Content-Type",        12, ENTITY_HEADER_FIELD,   0, 0, 0},
	{"Cookie",               6, REQUEST_HEADER_FIELD,  1, 0, 1},
	{"Date",                 4, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"ETag",                 4, RESPONSE_HEADER_FIELD, 0, 1, 0},
	{"Expect",               6, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"Expires",              7, ENTITY_HEADER_FIELD,   1, 0, 0},
	{"From",                 4, REQUEST_HEADER_FIELD,  0, 1, 0},
	{"Host",                 4, REQUEST_HEADER_FIELD,  0, 1, 0},
	{"If-Match",             8, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"If-Modified-Since",   17, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"If-None-Match",       13, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"If-Range",             8, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"If-Unmodified-Since", 19, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"Keep-Alive",          10, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"Last-Modified",       13, ENTITY_HEADER_FIELD,   1, 0, 0},
	{"Location",             8, RESPONSE_HEADER_FIELD, 0, 1, 0},
	{"Max-Forwards",        12, REQUEST_HEADER_FIELD,  0, 1, 0},
	{"Pragma",               6, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"Proxy-Authenticate",  18, RESPONSE_HEADER_FIELD, 1, 0, 0},
	{"Proxy-Authorization", 19, REQUEST_HEADER_FIELD,  0, 0, 0},
	{"Proxy-Connection",    16, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"Range",                5, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"Referer",              7, REQUEST_HEADER_FIELD,  0, 1, 0},
	{"Retry-After",         11, RESPONSE_HEADER_FIELD, 1, 0, 0},
	{"Server",               6, RESPONSE_HEADER_FIELD, 1, 0, 0},
	{"Set-Cookie",          10, RESPONSE_HEADER_FIELD, 1, 0, 1},
	{"Status",               6, RESPONSE_HEADER_FIELD, 1, 0, 0},
	{"TE",                   2, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"Trailer",              7, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"Transfer-Encoding",   17, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"Upgrade",              7, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"User-Agent",          10, REQUEST_HEADER_FIELD,  1, 0, 0},
	{"Vary",                 4, RESPONSE_HEADER_FIELD, 1, 0, 0},
	{"Via",                  3, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"Warning",              7, GENERAL_HEADER_FIELD,  1, 0, 0},
	{"WWW-Authenticate",    16, RESPONSE_HEADER_FIELD, 1, 0, 0}
};

http_headers::http_headers()
{
	_M_http_headers = NULL;
	_M_size = 0;
	_M_used = 0;

	_M_data.data = NULL;
	_M_data.size = 0;
	_M_data.used = 0;

	_M_max_line_len = MAX_LINE_LEN;

	_M_ignore_errors = true;

	_M_offset = 0;
	_M_token = 0;
	_M_end = 0;
	_M_name = 0;
	_M_namelen = 0;
	_M_state = 0;
	_M_header = UNKNOWN_HEADER;
	_M_nCRLFs = 1;
}

void http_headers::free()
{
	if (_M_http_headers) {
		::free(_M_http_headers);
		_M_http_headers = NULL;
	}

	_M_size = 0;
	_M_used = 0;

	if (_M_data.data) {
		::free(_M_data.data);
		_M_data.data = NULL;
	}

	_M_data.size = 0;
	_M_data.used = 0;

	_M_offset = 0;
	_M_token = 0;
	_M_end = 0;
	_M_name = 0;
	_M_namelen = 0;
	_M_state = 0;
	_M_header = UNKNOWN_HEADER;
	_M_nCRLFs = 1;
}

bool http_headers::add_header(const char* name, unsigned short namelen, const char* value, unsigned short valuelen, bool replace_value)
{
	unsigned char header = get_header(name, namelen);
	if (header != UNKNOWN_HEADER) {
		return add_known_header(header, value, valuelen, replace_value);
	} else {
		return add_unknown_header(name, namelen, value, valuelen, replace_value);
	}
}

bool http_headers::add_known_header(unsigned char header, const char* value, unsigned short valuelen, bool replace_value)
{
	http_header* http_header;
	if ((http_header = add_header(header, NULL, 0, valuelen, replace_value)) == NULL) {
		return false;
	}

	size_t len = clean_value(_M_data.data + http_header->value + http_header->valuelen, value, valuelen);
	http_header->valuelen += len;

	_M_data.used += (len + 1);

	return true;
}

bool http_headers::add_known_header(unsigned char header, const struct tm* timestamp)
{
	static const size_t datelen = 29;

	http_header* http_header;
	if ((http_header = add_header(header, NULL, 0, datelen, true)) == NULL) {
		return false;
	}

	sprintf(_M_data.data + http_header->value, "%s, %02d %s %d %02d:%02d:%02d GMT", days[timestamp->tm_wday], timestamp->tm_mday, months[timestamp->tm_mon], 1900 + timestamp->tm_year, timestamp->tm_hour, timestamp->tm_min, timestamp->tm_sec);

	http_header->valuelen = datelen;

	_M_data.used += (datelen + 1);

	return true;
}

bool http_headers::add_content_type_multipart(unsigned boundary)
{
	http_header* http_header;
	if ((http_header = add_header(CONTENT_TYPE_HEADER, NULL, 0, 33 + BOUNDARY_WIDTH, true)) == NULL) {
		return false;
	}

	sprintf(_M_data.data + http_header->value, "multipart/byteranges; boundary=\"%0*u\"", BOUNDARY_WIDTH, boundary);

	http_header->valuelen = 33 + BOUNDARY_WIDTH;

	_M_data.used += (33 + BOUNDARY_WIDTH + 1);

	return true;
}

bool http_headers::add_unknown_header(const char* name, unsigned short namelen, const char* value, unsigned short valuelen, bool replace_value)
{
	http_header* http_header;
	if ((http_header = add_header(UNKNOWN_HEADER, name, namelen, valuelen, replace_value)) == NULL) {
		return false;
	}

	size_t len = clean_value(_M_data.data + http_header->value + http_header->valuelen, value, valuelen);
	http_header->valuelen += len;

	_M_data.used += (len + 1);

	return true;
}

bool http_headers::remove_header(const char* name, unsigned short namelen)
{
	unsigned char header = get_header(name, namelen);
	if (header != UNKNOWN_HEADER) {
		return remove_known_header(header);
	} else {
		return remove_unknown_header(name, namelen);
	}
}

bool http_headers::remove_known_header(unsigned char header)
{
	unsigned short pos;
	if (!search(header, pos)) {
		return false;
	}

	if (pos < _M_used - 1) {
		memmove(&(_M_http_headers[pos]), &(_M_http_headers[pos + 1]), (_M_used - pos - 1) * sizeof(struct http_header));
	}

	_M_used--;

	return true;
}

bool http_headers::remove_unknown_header(const char* name, unsigned short namelen)
{
	unsigned short pos;
	if (!search(name, namelen, pos)) {
		return false;
	}

	if (pos < _M_used - 1) {
		memmove(&(_M_http_headers[pos]), &(_M_http_headers[pos + 1]), (_M_used - pos - 1) * sizeof(struct http_header));
	}

	_M_used--;

	return true;
}

bool http_headers::get_value(const char* name, unsigned short namelen, const char*& value, unsigned short* valuelen) const
{
	unsigned char header = get_header(name, namelen);
	if (header != UNKNOWN_HEADER) {
		return get_value_known_header(header, value, valuelen);
	} else {
		return get_value_unknown_header(name, namelen, value, valuelen);
	}
}

bool http_headers::get_value_known_header(unsigned char header, const char*& value, unsigned short* valuelen) const
{
	http_header* http_header;
	unsigned short pos;

	if ((http_header = search(header, pos)) == NULL) {
		return false;
	}

	if (valuelen) {
		*valuelen = http_header->valuelen;
	}

	value = _M_data.data + http_header->value;

	return true;
}

bool http_headers::get_value_unknown_header(const char* name, unsigned short namelen, const char*& value, unsigned short* valuelen) const
{
	http_header* http_header;
	unsigned short pos;

	if ((http_header = search(name, namelen, pos)) == NULL) {
		return false;
	}

	if (valuelen) {
		*valuelen = http_header->valuelen;
	}

	value = _M_data.data + http_header->value;

	return true;
}

bool http_headers::serialize(buffer& buffer) const
{
	for (unsigned short i = 0; i < _M_used; i++) {
		const http_header* http_header = &(_M_http_headers[i]);

		// Add field name.
		const char* name;
		if (http_header->header != UNKNOWN_HEADER) {
			name = _M_known_http_headers[(unsigned) http_header->header].name;
		} else {
			name = _M_data.data + http_header->name;
		}

		if ((!buffer.append(name, http_header->namelen)) || (!buffer.append(": ", 2))) {
			return false;
		}

		size_t namelen = http_header->namelen + 2;
		size_t len = 0;

		// Add field value.
		const char* begin = _M_data.data + http_header->value;
		const char* end = begin + http_header->valuelen;
		const char* ptr = begin;
		const char* space = NULL;

		for (; ptr < end; ptr++, len++) {
			if (namelen + len >= _M_max_line_len) {
				if (space) {
					if ((!buffer.append(begin, space - begin)) || (!buffer.append("\r\n\t", 3))) {
						return false;
					}

					begin = space + 1;
					namelen = 1;
					len = ptr - begin;

					space = NULL;
				}
			}

			if (*ptr == ' ') {
				space = ptr;
			}
		}

		if ((!buffer.append(begin, len)) || (!buffer.append("\r\n", 2))) {
			return false;
		}
	}

	return buffer.append("\r\n", 2);
}

http_headers::parse_result http_headers::parse(const char* buffer, size_t len, size_t& body_offset)
{
	while (_M_offset < len) {
		unsigned char c = (unsigned char) buffer[_M_offset];

		switch (_M_state) {
			case 0: // Initial state.
				if (c == ':') {
					// Bad Request.
					return ERROR_WRONG_HEADERS;
				} else if ((c > ' ') && (c < 127)) {
					// RFC 822
					// 3.1.2. STRUCTURE OF HEADER FIELDS
					// The field-name must be composed of printable ASCII characters
					// (i.e., characters that have values between 33. and 126.,
					// decimal, except colon).
					_M_token = _M_offset;

					_M_nCRLFs = 0;

					_M_state = 1; // Header field.
				} else if (c == '\r') {
					_M_state = 5; // '\r'
				} else if (c == '\n') {
					body_offset = _M_offset + 1;
					return END_OF_HEADER;
				} else {
					// Bad Request.
					return ERROR_WRONG_HEADERS;
				}

				break;
			case 1: // Header field.
				if (c == ':') {
					_M_namelen = _M_offset - _M_token;
					if ((_M_header = get_header(buffer + _M_token, _M_namelen)) != UNKNOWN_HEADER) {
						if (!_M_ignore_errors) {
							// If the header field cannot have commas...
							if (!_M_known_http_headers[(unsigned) _M_header].might_have_commas) {
								unsigned char type = _M_known_http_headers[(unsigned) _M_header].type;
								for (unsigned short i = 0; i < _M_used; i++) {
									if (_M_header == _M_http_headers[i].header) {
										// Bad Request.
										return ERROR_WRONG_HEADERS;
									} else if ((_M_http_headers[i].header == UNKNOWN_HEADER) || (type < _M_known_http_headers[(unsigned) _M_http_headers[i].header].type)) {
										break;
									}
								}
							}
						}
					} else {
						_M_name = _M_token;
					}

					_M_token = 0;

					_M_state = 3; // ':' after header field.
				} else if ((c == ' ') || (c == '\t')) {
					_M_namelen = _M_offset - _M_token;

					_M_state = 2; // Whitespace after header field.
				} else if ((c < ' ') || (c >= 127)) {
					// Bad Request.
					return ERROR_WRONG_HEADERS;
				}

				break;
			case 2: // Whitespace after header field.
				if (c == ':') {
					if ((_M_header = get_header(buffer + _M_token, _M_namelen)) != UNKNOWN_HEADER) {
						if (!_M_ignore_errors) {
							// If the header field cannot have commas...
							if (!_M_known_http_headers[(unsigned) _M_header].might_have_commas) {
								unsigned char type = _M_known_http_headers[(unsigned) _M_header].type;
								for (unsigned short i = 0; i < _M_used; i++) {
									if (_M_header == _M_http_headers[i].header) {
										// Bad Request.
										return ERROR_WRONG_HEADERS;
									} else if ((_M_http_headers[i].header == UNKNOWN_HEADER) || (type < _M_known_http_headers[(unsigned) _M_http_headers[i].header].type)) {
										break;
									}
								}
							}
						}
					} else {
						_M_name = _M_token;
					}

					_M_token = 0;

					_M_state = 3; // ':' after header field.
				} else if ((c != ' ') && (c != '\t')) {
					// Bad Request.
					return ERROR_WRONG_HEADERS;
				}

				break;
			case 3: // ':' after header field.
				if (c == '\r') {
					_M_state = 5; // '\r'
				} else if (c == '\n') {
					_M_nCRLFs = 1;
					_M_state = 6; // '\n'
				} else if (c > ' ') {
					_M_token = _M_offset;
					_M_end = 0;

					_M_state = 4; // Header value.
				} else if ((c != ' ') && (c != '\t')) {
					// Bad Request.
					return ERROR_WRONG_HEADERS;
				}

				break;
			case 4: // Header value.
				if (c == '\r') {
					if (_M_end == 0) {
						_M_end = _M_offset;
					}

					_M_state = 5; // '\r'
				} else if (c == '\n') {
					if (_M_end == 0) {
						_M_end = _M_offset;
					}

					_M_nCRLFs = 1;
					_M_state = 6; // '\n'
				} else if ((c == ' ') || (c == '\t')) {
					if (_M_end == 0) {
						_M_end = _M_offset;
					}
				} else if (c > ' ') {
					// If a new word starts...
					if (_M_end != 0) {
						if ((_M_header != UNKNOWN_HEADER) && (_M_known_http_headers[(unsigned) _M_header].single_token)) {
							if (!_M_ignore_errors) {
								// Bad Request.
								return ERROR_WRONG_HEADERS;
							}
						} else {
							_M_end = 0;
						}
					}
				} else {
					// Bad Request.
					return ERROR_WRONG_HEADERS;
				}

				break;
			case 5: // '\r'
				if (c == '\n') {
					if (_M_nCRLFs == 1) {
						body_offset = _M_offset + 1;
						return END_OF_HEADER;
					}

					_M_nCRLFs = 1;
					_M_state = 6; // '\n'
				} else {
					// Bad Request.
					return ERROR_WRONG_HEADERS;
				}

				break;
			case 6: // '\n'
				if (c == '\r') {
					if (_M_token != 0) {
						if (_M_header != UNKNOWN_HEADER) {
							if (!add_known_header(_M_header, buffer + _M_token, _M_end - _M_token, false)) {
								return ERROR_NO_MEMORY;
							}
						} else {
							if (!add_unknown_header(buffer + _M_name, _M_namelen, buffer + _M_token, _M_end - _M_token, false)) {
								return ERROR_NO_MEMORY;
							}
						}
					}

					_M_state = 5; // '\r'
				} else if (c == '\n') {
					if (_M_token != 0) {
						if (_M_header != UNKNOWN_HEADER) {
							if (!add_known_header(_M_header, buffer + _M_token, _M_end - _M_token, false)) {
								return ERROR_NO_MEMORY;
							}
						} else {
							if (!add_unknown_header(buffer + _M_name, _M_namelen, buffer + _M_token, _M_end - _M_token, false)) {
								return ERROR_NO_MEMORY;
							}
						}
					}

					body_offset = _M_offset + 1;
					return END_OF_HEADER;
				} else if (c == ':') {
					// Bad Request.
					return ERROR_WRONG_HEADERS;
				} else if ((c > ' ') && (c < 127)) {
					if (_M_token != 0) {
						if (_M_header != UNKNOWN_HEADER) {
							if (!add_known_header(_M_header, buffer + _M_token, _M_end - _M_token, false)) {
								return ERROR_NO_MEMORY;
							}
						} else {
							if (!add_unknown_header(buffer + _M_name, _M_namelen, buffer + _M_token, _M_end - _M_token, false)) {
								return ERROR_NO_MEMORY;
							}
						}
					}

					_M_token = _M_offset;

					_M_nCRLFs = 0;

					_M_state = 1; // Header field.
				} else if ((c == ' ') || (c == '\t')) {
					_M_nCRLFs = 0;

					_M_state = 7; // Multiple lines value.
				} else {
					// Bad Request.
					return ERROR_WRONG_HEADERS;
				}

				break;
			case 7: // Multiple lines value.
				if (c == '\r') {
					_M_state = 5; // '\r'
				} else if (c == '\n') {
					_M_nCRLFs = 1;
					_M_state = 6; // '\n'
				} else if (c > ' ') {
					if (_M_token != 0) {
						if ((_M_header != UNKNOWN_HEADER) && (_M_known_http_headers[(unsigned) _M_header].single_token)) {
							if (!_M_ignore_errors) {
								// Bad Request.
								return ERROR_WRONG_HEADERS;
							} else {
								break;
							}
						}
					} else {
						_M_token = _M_offset;
					}

					_M_end = 0;

					_M_state = 4; // Header value.
				} else if ((c != ' ') && (c != '\t')) {
					// Bad Request.
					return ERROR_WRONG_HEADERS;
				}

				break;
		}

		if (++_M_offset > MAX_HEADERS_SIZE) {
			return ERROR_HEADERS_TOO_LARGE;
		}
	}

	return NOT_END_OF_HEADER;
}

unsigned char http_headers::get_header(const char* name, size_t len)
{
	int i = 0;
	int j = ARRAY_SIZE(_M_known_http_headers) - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		int ret = strncasecmp(name, _M_known_http_headers[pivot].name, len);
		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (len < _M_known_http_headers[pivot].len) {
				j = pivot - 1;
			} else if (len == _M_known_http_headers[pivot].len) {
				return pivot;
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	return UNKNOWN_HEADER;
}

bool http_headers::allocate()
{
	if (_M_used == _M_size) {
		size_t size = _M_size + HTTP_HEADER_ALLOC;
		if (size > 0xffff) {
			return false;
		}

		http_header* http_headers = (http_header*) realloc(_M_http_headers, size * sizeof(http_header));
		if (!http_headers) {
			return false;
		}

		_M_http_headers = http_headers;
		_M_size = (unsigned short) size;
	}

	return true;
}

bool http_headers::allocate_data(size_t len)
{
	size_t size = _M_data.used + len;
	if (size > (size_t) _M_data.size) {
		size_t quotient = size / DATA_ALLOC;
		size_t remainder = size % DATA_ALLOC;
		if (remainder != 0) {
			quotient++;
		}

		size = quotient * DATA_ALLOC;
		if (size > 0xffff) {
			return false;
		}

		char* data = (char*) realloc(_M_data.data, size);
		if (!data) {
			return false;
		}

		_M_data.data = data;
		_M_data.size = (unsigned short) size;
	}

	return true;
}

http_headers::http_header* http_headers::add_header(unsigned char header, const char* name, unsigned short namelen, unsigned short valuelen, bool replace_value)
{
	http_header* http_header;
	unsigned short pos;
	size_t len;

	if (header == UNKNOWN_HEADER) {
		http_header = search(name, namelen, pos);

		len = namelen + 1 + valuelen + 1;
	} else {
		http_header = search(header, pos);

		len = valuelen + 1;
	}

	if (!http_header) {
		if ((!allocate()) || (!allocate_data(len))) {
			return NULL;
		}

		if (pos < _M_used) {
			memmove(&(_M_http_headers[pos + 1]), &(_M_http_headers[pos]), (_M_used - pos) * sizeof(struct http_header));
		}

		http_header = &(_M_http_headers[pos]);

		if ((http_header->header = header) == UNKNOWN_HEADER) {
			memcpy(_M_data.data + _M_data.used, name, namelen);
			http_header->name = _M_data.used;
			_M_data.used += namelen;
			_M_data.data[_M_data.used++] = 0;

			http_header->namelen = namelen;
		} else {
			http_header->namelen = _M_known_http_headers[header].len;
		}

		http_header->value = _M_data.used;
		http_header->valuelen = 0;

		_M_used++;
	} else {
		if (replace_value) {
			if (!allocate_data(valuelen + 1)) {
				return NULL;
			}

			http_header->value = _M_data.used;
			http_header->valuelen = 0;
		} else {
			if (!allocate_data(http_header->valuelen + valuelen + 3)) {
				return NULL;
			}

			memcpy(_M_data.data + _M_data.used, _M_data.data + http_header->value, http_header->valuelen);

			http_header->value = _M_data.used;
			_M_data.used += http_header->valuelen;

			_M_data.data[_M_data.used++] = ',';
			_M_data.data[_M_data.used++] = ' ';

			http_header->valuelen += 2;
		}
	}

	return http_header;
}

unsigned short http_headers::clean_value(char* data, const char* value, unsigned short valuelen)
{
	char* dest = data;
	const char* end = value + valuelen;
	int state = 0;

	while (value < end) {
		unsigned char c = (unsigned char) *value++;

		switch (state) {
			case 0:
				if (c > ' ') {
					*dest++ = c;
					state = 1;
				}

				break;
			case 1:
				if (c <= ' ') {
					*dest++ = ' ';
					state = 2;
				} else {
					*dest++ = c;
				}

				break;
			case 2:
				if (c > ' ') {
					*dest++ = c;
					state = 1;
				}

				break;
		}
	}

	*dest = 0;

	return (unsigned short) (dest - data);
}
