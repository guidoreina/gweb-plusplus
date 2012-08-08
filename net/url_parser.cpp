#include <stdlib.h>
#include "url_parser.h"
#include "macros/macros.h"

const unsigned short url_parser::HOST_MAX_LEN = 255;
const unsigned short url_parser::PATH_MAX_LEN = 4096;
const unsigned short url_parser::QUERY_MAX_LEN = 4096;
const unsigned short url_parser::FRAGMENT_MAX_LEN = 4096;

const unsigned short url_parser::FTP_DEFAULT_PORT = 21;
const unsigned short url_parser::HTTP_DEFAULT_PORT = 80;
const unsigned short url_parser::HTTPS_DEFAULT_PORT = 443;
const unsigned short url_parser::TELNET_DEFAULT_PORT = 23;

url_parser::parse_result url_parser::parse(char* string, unsigned short& len, buffer& buf)
{
	char* ptr = string;
	char* out = string;
	unsigned char n = 0;
	unsigned port = 0;
	unsigned state = 0; // Initial state.

	while (*ptr) {
		unsigned char c = (unsigned char) *ptr;

		switch (state) {
			case 0: // Initial state.
				if (IS_ALPHA(c)) {
					*out++ = c;
					state = 1; // Scheme.
				} else if (c == '/') {
					_M_path = out;

					len = 0;
					return parse_path(ptr, len, buf);
				} else {
					return PARSE_ERROR;
				}

				break;
			case 1: // Scheme.
				// RFC 3986: 3.1. Scheme (http://tools.ietf.org/html/rfc3986#section-3.1)
				if ((IS_ALPHA(c)) || (IS_DIGIT(c)) || (c == '+') || (c == '-') || (c == '.')) {
					*out++ = c;
				} else if (c == ':') {
					if ((_M_scheme = scheme::get_scheme(string, ptr - string)) == scheme::UNKNOWN) {
						return PARSE_ERROR;
					}

					*out++ = c;
					state = 2; // <scheme>:// => :
				} else {
					return PARSE_ERROR;
				}

				break;
			case 2: // <scheme>:// => :
				if (c == '/') {
					*out++ = c;
					state = 3; // <scheme>:// => First /
				} else {
					return PARSE_ERROR;
				}

				break;
			case 3: // <scheme>:// => First /
				if (c == '/') {
					*out++ = c;
					state = 4; // <scheme>:// => Second /
				} else {
					return PARSE_ERROR;
				}

				break;
			case 4: // <scheme>:// => Second /
				if ((IS_ALPHA(c)) || (IS_DIGIT(c))) {
					_M_host = out;

					*out++ = c;
					state = 7; // Host.
				} else if (c == '%') {
					state = 5; // First character of host: %XY => X
				} else {
					return PARSE_ERROR;
				}

				break;
			case 5: // First character of host: %XY => X
				if (IS_DIGIT(c)) {
					n = (c - '0') * 16;
				} else if ((c >= 'A') && (c <= 'F')) {
					n = (c - 'A' + 10) * 16;
				} else if ((c >= 'a') && (c <= 'f')) {
					n = (c - 'a' + 10) * 16;
				} else {
					return PARSE_ERROR;
				}

				state = 6; // First character of host: %XY => Y

				break;
			case 6: // First character of host: %XY => Y
				if (IS_DIGIT(c)) {
					n += (c - '0');
				} else if ((c >= 'A') && (c <= 'F')) {
					n += (c - 'A' + 10);
				} else if ((c >= 'a') && (c <= 'f')) {
					n += (c - 'a' + 10);
				} else {
					return PARSE_ERROR;
				}

				if ((IS_ALPHA(n)) || (IS_DIGIT(n))) {
					_M_host = out;

					*out++ = n;
					state = 7; // Host.
				} else {
					return PARSE_ERROR;
				}

				break;
			case 7: // Host.
				if (c == '/') {
					// If the previous character of the output buffer was not alphanumeric...
					unsigned char prev = *(out - 1);
					if ((!IS_ALPHA(prev)) && (!IS_DIGIT(prev))) {
						return PARSE_ERROR;
					}

					_M_hostlen = out - _M_host;
					if (_M_hostlen > HOST_MAX_LEN) {
						return PARSE_ERROR;
					}

					if (_M_scheme == scheme::FTP) {
						_M_port = FTP_DEFAULT_PORT;
					} else if (_M_scheme == scheme::HTTP) {
						_M_port = HTTP_DEFAULT_PORT;
					} else if (_M_scheme == scheme::HTTPS) {
						_M_port = HTTPS_DEFAULT_PORT;
					} else if (_M_scheme == scheme::TELNET) {
						_M_port = TELNET_DEFAULT_PORT;
					}

					_M_path = out;

					len = out - string;
					return parse_path(ptr, len, buf);
				} else if (c == '%') {
					state = 8; // In host: %XY => X
				} else if (c == ':') {
					// If the previous character of the output buffer was not alphanumeric...
					unsigned char prev = *(out - 1);
					if ((!IS_ALPHA(prev)) && (!IS_DIGIT(prev))) {
						return PARSE_ERROR;
					}

					_M_hostlen = out - _M_host;
					if (_M_hostlen > HOST_MAX_LEN) {
						return PARSE_ERROR;
					}

					port = 0;

					*out++ = c;
					state = 10; // Port.
				} else if ((c > ' ') && (c != 127) && (c != 255)) {
					*out++ = c;
				} else {
					return PARSE_ERROR;
				}

				break;
			case 8: // In host: %XY => X
				if (IS_DIGIT(c)) {
					n = (c - '0') * 16;
				} else if ((c >= 'A') && (c <= 'F')) {
					n = (c - 'A' + 10) * 16;
				} else if ((c >= 'a') && (c <= 'f')) {
					n = (c - 'a' + 10) * 16;
				} else {
					return PARSE_ERROR;
				}

				state = 9; // In host: %XY => Y

				break;
			case 9: // In host: %XY => Y
				if (IS_DIGIT(c)) {
					n += (c - '0');
				} else if ((c >= 'A') && (c <= 'F')) {
					n += (c - 'A' + 10);
				} else if ((c >= 'a') && (c <= 'f')) {
					n += (c - 'a' + 10);
				} else {
					return PARSE_ERROR;
				}

				if ((n == ':') || (n == '/') || (n == '%')) {
					return PARSE_ERROR;
				} else if ((n > ' ') && (n != 127) && (n != 255)) {
					*out++ = n;
					state = 7; // Host.
				} else {
					return PARSE_ERROR;
				}

				break;
			case 10: // Port.
				if (IS_DIGIT(c)) {
					port = (port * 10) + (c - '0');
					if (port > 65535) {
						return PARSE_ERROR;
					}

					*out++ = c;
				} else if (c == '/') {
					if (port == 0) {
						return PARSE_ERROR;
					}

					_M_port = (unsigned short) port;

					_M_path = out;

					len = out - string;
					return parse_path(ptr, len, buf);
				} else {
					return PARSE_ERROR;
				}

				break;
		}

		ptr++;
	}

	if (state == 7) {
		// If the previous character of the output buffer was not alphanumeric...
		unsigned char prev = *(out - 1);
		if ((!IS_ALPHA(prev)) && (!IS_DIGIT(prev))) {
			return PARSE_ERROR;
		}

		_M_hostlen = out - _M_host;
		if (_M_hostlen > HOST_MAX_LEN) {
			return PARSE_ERROR;
		}

		if (_M_scheme == scheme::FTP) {
			_M_port = FTP_DEFAULT_PORT;
		} else if (_M_scheme == scheme::HTTP) {
			_M_port = HTTP_DEFAULT_PORT;
		} else if (_M_scheme == scheme::HTTPS) {
			_M_port = HTTPS_DEFAULT_PORT;
		} else if (_M_scheme == scheme::TELNET) {
			_M_port = TELNET_DEFAULT_PORT;
		}

		if (!buf.append('/')) {
			return ERROR_NO_MEMORY;
		}

		_M_path = "/";
		_M_pathlen = 1;

		len = out - string;

		*out = 0;

		return SUCCESS;
	} else if (state == 10) {
		if (port == 0) {
			return PARSE_ERROR;
		}

		_M_port = (unsigned short) port;

		if (!buf.append('/')) {
			return ERROR_NO_MEMORY;
		}

		_M_path = "/";
		_M_pathlen = 1;

		len = out - string;

		*out = 0;

		return SUCCESS;
	} else {
		return PARSE_ERROR;
	}
}

url_parser::parse_result url_parser::parse_path(char* path, unsigned short& len, buffer& buf)
{
	if (!buf.append(path)) {
		return ERROR_NO_MEMORY;
	}

	unsigned char* in = (unsigned char*) path + 1;
	char* out = (char*) _M_path;
	unsigned state = 0; // "/"

	while (*in) {
		switch (state) {
			case 0: // "/"
				if (in[0] == '/') {
					// Skip duplicated '/'.
					in++;
				} else if (in[0] == '.') {
					in++;
					state = 1; // "/."
				} else if (in[0] == '?') {
					*out++ = '/';
					_M_pathlen = out - _M_path;
					if (_M_pathlen > PATH_MAX_LEN) {
						return PARSE_ERROR;
					}

					_M_query = out;
					*out++ = *in++;

					state = 4; // Query.
				} else if (in[0] == '#') {
					*out++ = '/';
					_M_pathlen = out - _M_path;
					if (_M_pathlen > PATH_MAX_LEN) {
						return PARSE_ERROR;
					}

					_M_fragment = out;
					*out++ = *in++;

					state = 5; // Fragment.
				} else if (in[0] == '%') {
					unsigned char c;
					if ((in[1] >= '0') && (in[1] <= '9')) {
						c = (in[1] - '0') * 16;
					} else if ((in[1] >= 'A') && (in[1] <= 'F')) {
						c = (in[1] - 'A' + 10) * 16;
					} else if ((in[1] >= 'a') && (in[1] <= 'f')) {
						c = (in[1] - 'a' + 10) * 16;
					} else {
						return PARSE_ERROR;
					}

					if ((in[2] >= '0') && (in[2] <= '9')) {
						c += (in[2] - '0');
					} else if ((in[2] >= 'A') && (in[2] <= 'F')) {
						c += (in[2] - 'A' + 10);
					} else if ((in[2] >= 'a') && (in[2] <= 'f')) {
						c += (in[2] - 'a' + 10);
					} else {
						return PARSE_ERROR;
					}

					if ((c == '.') || (c == '/')) {
						in += 2;
						*in = c;
					} else {
						*out++ = '/';
						*out++ = c;

						in += 3;

						state = 3; // "/<path>"
					}
				} else if (in[0] >= ' ') {
					*out++ = '/';
					*out++ = *in++;

					state = 3; // "/<path>"
				} else {
					return PARSE_ERROR;
				}

				break;
			case 1: // "/."
				if (in[0] == '/') {
					state = 0; // "/"
				} else if (in[0] == '.') {
					in++;
					state = 2; // "/.."
				} else if (in[0] == '?') {
					*out++ = '/';
					_M_pathlen = out - _M_path;
					if (_M_pathlen > PATH_MAX_LEN) {
						return PARSE_ERROR;
					}

					_M_query = out;
					*out++ = *in++;

					state = 4; // Query.
				} else if (in[0] == '#') {
					*out++ = '/';
					_M_pathlen = out - _M_path;
					if (_M_pathlen > PATH_MAX_LEN) {
						return PARSE_ERROR;
					}

					_M_fragment = out;
					*out++ = *in++;

					state = 5; // Fragment.
				} else if (in[0] == '%') {
					unsigned char c;
					if ((in[1] >= '0') && (in[1] <= '9')) {
						c = (in[1] - '0') * 16;
					} else if ((in[1] >= 'A') && (in[1] <= 'F')) {
						c = (in[1] - 'A' + 10) * 16;
					} else if ((in[1] >= 'a') && (in[1] <= 'f')) {
						c = (in[1] - 'a' + 10) * 16;
					} else {
						return PARSE_ERROR;
					}

					if ((in[2] >= '0') && (in[2] <= '9')) {
						c += (in[2] - '0');
					} else if ((in[2] >= 'A') && (in[2] <= 'F')) {
						c += (in[2] - 'A' + 10);
					} else if ((in[2] >= 'a') && (in[2] <= 'f')) {
						c += (in[2] - 'a' + 10);
					} else {
						return PARSE_ERROR;
					}

					if ((c == '.') || (c == '/')) {
						in += 2;
						*in = c;
					} else {
						*out++ = '/';
						*out++ = '.';
						*out++ = c;

						in += 3;

						state = 3; // "/<path>"
					}
				} else if (in[0] >= ' ') {
					*out++ = '/';
					*out++ = '.';
					*out++ = *in++;

					state = 3; // "/<path>"
				} else {
					return PARSE_ERROR;
				}

				break;
			case 2: // "/.."
				if (in[0] == '/') {
					if (out == _M_path) {
						return FORBIDDEN;
					}

					while (*--out != '/');

					state = 0; // "/"
				} else if (in[0] == '?') {
					if (out == _M_path) {
						return FORBIDDEN;
					}

					while (*(out - 1) != '/') {
						out--;
					}

					_M_pathlen = out - _M_path;
					if (_M_pathlen > PATH_MAX_LEN) {
						return PARSE_ERROR;
					}

					_M_query = out;
					*out++ = *in++;

					state = 4; // Query.
				} else if (in[0] == '#') {
					if (out == _M_path) {
						return FORBIDDEN;
					}

					while (*(out - 1) != '/') {
						out--;
					}

					_M_pathlen = out - _M_path;
					if (_M_pathlen > PATH_MAX_LEN) {
						return PARSE_ERROR;
					}

					_M_fragment = out;
					*out++ = *in++;

					state = 5; // Fragment.
				} else if (in[0] == '%') {
					unsigned char c;
					if ((in[1] >= '0') && (in[1] <= '9')) {
						c = (in[1] - '0') * 16;
					} else if ((in[1] >= 'A') && (in[1] <= 'F')) {
						c = (in[1] - 'A' + 10) * 16;
					} else if ((in[1] >= 'a') && (in[1] <= 'f')) {
						c = (in[1] - 'a' + 10) * 16;
					} else {
						return PARSE_ERROR;
					}

					if ((in[2] >= '0') && (in[2] <= '9')) {
						c += (in[2] - '0');
					} else if ((in[2] >= 'A') && (in[2] <= 'F')) {
						c += (in[2] - 'A' + 10);
					} else if ((in[2] >= 'a') && (in[2] <= 'f')) {
						c += (in[2] - 'a' + 10);
					} else {
						return PARSE_ERROR;
					}

					if ((c == '.') || (c == '/')) {
						in += 2;
						*in = c;
					} else {
						*out++ = '/';
						*out++ = '.';
						*out++ = '.';
						*out++ = c;

						in += 3;

						state = 3; // "/<path>"
					}
				} else if (in[0] >= ' ') {
					*out++ = '/';
					*out++ = '.';
					*out++ = '.';
					*out++ = *in++;

					state = 3; // "/<path>"
				} else {
					return PARSE_ERROR;
				}

				break;
			case 3: // "/<path>"
				if (in[0] == '/') {
					_M_extension = NULL;

					state = 0; // "/"
				} else if (in[0] == '%') {
					unsigned char c;
					if ((in[1] >= '0') && (in[1] <= '9')) {
						c = (in[1] - '0') * 16;
					} else if ((in[1] >= 'A') && (in[1] <= 'F')) {
						c = (in[1] - 'A' + 10) * 16;
					} else if ((in[1] >= 'a') && (in[1] <= 'f')) {
						c = (in[1] - 'a' + 10) * 16;
					} else {
						return PARSE_ERROR;
					}

					if ((in[2] >= '0') && (in[2] <= '9')) {
						c += (in[2] - '0');
					} else if ((in[2] >= 'A') && (in[2] <= 'F')) {
						c += (in[2] - 'A' + 10);
					} else if ((in[2] >= 'a') && (in[2] <= 'f')) {
						c += (in[2] - 'a' + 10);
					} else {
						return PARSE_ERROR;
					}

					if ((c == '.') || (c == '/')) {
						in += 2;
						*in = c;
					} else {
						*out++ = c;

						in += 3;
					}
				} else if (in[0] == '?') {
					_M_pathlen = out - _M_path;
					if (_M_pathlen > PATH_MAX_LEN) {
						return PARSE_ERROR;
					}

					_M_query = out;
					*out++ = *in++;

					state = 4; // Query.
				} else if (in[0] == '#') {
					_M_pathlen = out - _M_path;
					if (_M_pathlen > PATH_MAX_LEN) {
						return PARSE_ERROR;
					}

					_M_fragment = out;
					*out++ = *in++;

					state = 5; // Fragment.
				} else if (in[0] == '.') {
					*out++ = *in++;

					_M_extension = out;
					_M_extensionlen = 0;
				} else if (in[0] >= ' ') {
					if (_M_extension) {
						_M_extensionlen++;
					}

					*out++ = *in++;
				} else {
					return PARSE_ERROR;
				}

				break;
			case 4: // Query.
				if (in[0] == '#') {
					_M_querylen = out - _M_query;
					if (_M_querylen > QUERY_MAX_LEN) {
						return PARSE_ERROR;
					}

					_M_fragment = out;
					*out++ = *in++;

					state = 5; // Fragment.
				} else if (in[0] > ' ') {
					*out++ = *in++;
				} else {
					return PARSE_ERROR;
				}

				break;
			case 5: // Fragment.
				if (in[0] > ' ') {
					*out++ = *in++;
				} else {
					return PARSE_ERROR;
				}

				break;
		}
	}

	if ((state == 0) || (state == 1)) {
		*out++ = '/';
		_M_pathlen = out - _M_path;
		if (_M_pathlen > PATH_MAX_LEN) {
			return PARSE_ERROR;
		}
	} else if (state == 2) {
		if (out == _M_path) {
			return FORBIDDEN;
		}

		while (*(out - 1) != '/') {
			out--;
		}

		_M_pathlen = out - _M_path;
		if (_M_pathlen > PATH_MAX_LEN) {
			return PARSE_ERROR;
		}
	} else if (state == 3) {
		_M_pathlen = out - _M_path;
		if (_M_pathlen > PATH_MAX_LEN) {
			return PARSE_ERROR;
		}
	} else if (state == 4) {
		_M_querylen = out - _M_query;
		if (_M_querylen > QUERY_MAX_LEN) {
			return PARSE_ERROR;
		}
	} else if (state == 5) {
		_M_fragmentlen = out - _M_fragment;
		if (_M_fragmentlen > FRAGMENT_MAX_LEN) {
			return PARSE_ERROR;
		}
	}

	*out = 0;

	len += (out - _M_path);

	return SUCCESS;
}
