#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "access_log.h"
#include "net/tcp_server.h"
#include "http/http_connection.h"
#include "http/http_method.h"
#include "util/now.h"
#include "constants/months_and_days.h"
#include "macros/macros.h"

const char* access_log::COMMON_LOG_FORMAT = "$remote_address - - [$timestamp] \"$method $host$path HTTP/$http_version\" $status_code $response_body_size";
const off_t access_log::MAX_SIZE = 1024L * 1024L * 1024L;
const size_t access_log::TOKENS_ALLOC = 10;

const access_log::variable access_log::_M_variables[] = {
	{"PID", 3, log_PID},
	{"connection_status", 17, log_connection_status},
	{"host", 4, log_host},
	{"http_version", 12, log_http_version},
	{"local_address", 13, log_local_address},
	{"local_port", 10, log_local_port},
	{"method", 6, log_method},
	{"path", 4, log_path},
	{"remote_address", 14, log_remote_address},
	{"remote_port", 11, log_remote_port},
	{"request_body_size", 17, log_request_body_size},
	{"request_header_size", 19, log_request_header_size},
	{"response_body_size", 18, log_response_body_size},
	{"response_header_size", 20, log_response_header_size},
	{"scheme", 6, log_scheme},
	{"status_code", 11, log_status_code},
	{"timestamp", 9, log_timestamp},
	{"user_agent", 10, log_user_agent}
};

access_log::access_log()
{
	*_M_path = 0;
	_M_pathlen = 0;

	_M_fd = -1;

	_M_last_file = 0;

	_M_enabled = true;

	_M_max_size = MAX_SIZE;
	_M_size = 0;

	_M_bufsize = 0;

	_M_token_list.tokens = NULL;
	_M_token_list.size = 0;
	_M_token_list.used = 0;
}

access_log::~access_log()
{
	if (_M_fd != -1) {
		if (_M_buf.count() > 0) {
			flush();
		}

		close(_M_fd);
	}

	if (_M_token_list.tokens) {
		for (size_t i = 0; i < _M_token_list.used; i++) {
			if (_M_token_list.tokens[i].type == STRING) {
				free(_M_token_list.tokens[i].u.str.data);
			}
		}

		free(_M_token_list.tokens);
	}
}

bool access_log::create(const char* dir, const char* filename, const char* format, size_t bufsize, off_t max_size)
{
	size_t dirlen = strlen(dir);
	if (dirlen >= sizeof(_M_path)) {
		return false;
	}

	struct stat buf;
	if ((stat(dir, &buf) < 0) || (!S_ISDIR(buf.st_mode))) {
		return false;
	}

	size_t filenamelen = strlen(filename);
	if (filenamelen >= sizeof(_M_path)) {
		return false;
	}

	if (dir[dirlen - 1] == '/') {
		dirlen--;
	}

	if (dirlen + 1 + filenamelen >= sizeof(_M_path)) {
		return false;
	}

	if ((!format) || (!*format)) {
		format = COMMON_LOG_FORMAT;
	}

	if (!parse_format(format)) {
		return false;
	}

	memcpy(_M_path, dir, dirlen);
	_M_path[dirlen] = '/';
	memcpy(_M_path + dirlen + 1, filename, filenamelen);

	_M_pathlen = dirlen + 1 + filenamelen;

	// Search last file.
	do {
		snprintf(_M_path + _M_pathlen, sizeof(_M_path) - _M_pathlen, ".%u", _M_last_file + 1);
	} while ((stat(_M_path, &buf) == 0) && (++_M_last_file));

	_M_path[_M_pathlen] = 0;

	// Open file.
	if ((_M_fd = open(_M_path, O_CREAT | O_WRONLY, 0644)) < 0) {
		return false;
	}

	_M_max_size = max_size;
	_M_size = lseek(_M_fd, 0, SEEK_END);

	_M_bufsize = bufsize;
	_M_buf.set_buffer_increment(_M_bufsize);

	return true;
}

bool access_log::log(const http_connection& conn, unsigned fd)
{
	if (!_M_enabled) {
		return true;
	}

	if (_M_fd < 0) {
		return false;
	}

	for (size_t i = 0; i < _M_token_list.used; i++) {
		const token* token = &(_M_token_list.tokens[i]);

		if (token->type == STRING) {
			if (!_M_buf.append(token->u.str.data, token->u.str.len)) {
				return false;
			}
		} else {
			if (!token->u.log(*this, conn, fd)) {
				return false;
			}
		}
	}

	if (!_M_buf.append('\n')) {
		return false;
	}

	if (_M_buf.count() <= _M_bufsize) {
		return true;
	}

	return flush();
}

bool access_log::parse_format(const char* format)
{
	const char* begin = format;
	const char* end = begin;
	int state = 0;

	unsigned char c;
	while ((c = *end) != 0) {
		if (state == 0) {
			if (c == '$') {
				if (begin < end) {
					if (!add_string(begin, end - begin)) {
						return false;
					}
				}

				begin = end;

				state = 1;
			}
		} else {
			if ((!IS_ALPHA(c)) && (c != '_')) {
				if (begin + 1 < end) {
					int variable = search(begin + 1, end - begin - 1);
					if (variable < 0) {
						if (c == '$') {
							if (!add_string(begin, end - begin)) {
								return false;
							}

							begin = end;
						} else {
							state = 0;
						}
					} else {
						if (!add_variable(variable)) {
							return false;
						}

						begin = end;

						state = (c == '$');
					}
				} else {
					if (c == '$') {
						if (!add_string(begin, 1)) {
							return false;
						}

						begin = end;
					} else {
						state = 0;
					}
				}
			}
		}

		end++;
	}

	if (state == 0) {
		if (!add_string(begin, end - begin)) {
			return false;
		}
	} else {
		if (begin + 1 == end) {
			if (!add_string(begin, 1)) {
				return false;
			}
		} else {
			int variable = search(begin + 1, end - begin - 1);
			if (variable < 0) {
				if (!add_string(begin, end - begin)) {
					return false;
				}
			} else {
				if (!add_variable(variable)) {
					return false;
				}
			}
		}
	}

	return true;
}

bool access_log::add_string(const char* data, size_t len)
{
	char* str;
	if ((str = (char*) malloc(len)) == NULL) {
		return false;
	}

	token* token;
	if ((token = add_token(STRING)) == NULL) {
		free(str);
		return false;
	}

	memcpy(str, data, len);

	token->u.str.data = str;
	token->u.str.len = len;

	return true;
}

access_log::token* access_log::add_token(access_log::token_type type)
{
	if (_M_token_list.used == _M_token_list.size) {
		size_t size = _M_token_list.size + TOKENS_ALLOC;
		struct token* tokens = (struct token*) realloc(_M_token_list.tokens, size * sizeof(struct token));
		if (!tokens) {
			return NULL;
		}

		_M_token_list.tokens = tokens;
		_M_token_list.size = size;
	}

	token* token = &(_M_token_list.tokens[_M_token_list.used]);

	token->type = type;

	_M_token_list.used++;

	return token;
}

int access_log::search(const char* name, size_t len)
{
	int i = 0;
	int j = ARRAY_SIZE(_M_variables) - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		int ret = strncmp(name, _M_variables[pivot].name, len);

		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (len < _M_variables[pivot].len) {
				j = pivot - 1;
			} else if (len == _M_variables[pivot].len) {
				return pivot;
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	return -1;
}

bool access_log::flush()
{
	if (_M_size + _M_buf.count() > _M_max_size) {
		if (!rotate()) {
			_M_buf.reset();
			return false;
		}
	}

	size_t count = 0;

	do {
		ssize_t ret;
		if ((ret = write(_M_fd, _M_buf.data() + count, _M_buf.count() - count)) < 0) {
			close(_M_fd);
			_M_fd = -1;

			_M_buf.reset();

			return false;
		} else if (ret > 0) {
			count += ret;
		}
	} while (count < _M_buf.count());

	_M_size += _M_buf.count();

	_M_buf.reset();

	return true;
}

bool access_log::rotate()
{
	close(_M_fd);

	struct stat buf;

	char newpath[PATH_MAX + 1];
	memcpy(newpath, _M_path, _M_pathlen);

	do {
		snprintf(newpath + _M_pathlen, sizeof(newpath) - _M_pathlen, ".%u", ++_M_last_file);
	} while (stat(newpath, &buf) == 0);

	if (rename(_M_path, newpath) < 0) {
		_M_fd = -1;
		return false;
	}

	if ((_M_fd = open(_M_path, O_CREAT | O_WRONLY, 0644)) < 0) {
		return false;
	}

	_M_size = 0;

	return true;
}

bool access_log::log_PID(access_log& log, const http_connection& conn, unsigned fd)
{
	static pid_t pid = getpid();
	return log._M_buf.format("%u", pid);
}

bool access_log::log_connection_status(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.append(conn._M_keep_alive ? '+' : '-');
}

bool access_log::log_host(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.append(conn._M_vhost->name, conn._M_vhost->namelen);
}

bool access_log::log_http_version(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.format("%u.%u", conn._M_major_number, conn._M_minor_number);
}

bool access_log::log_local_address(access_log& log, const http_connection& conn, unsigned fd)
{
	struct sockaddr addr;
	socklen_t addrlen = sizeof(struct sockaddr);

	getsockname(fd, &addr, &addrlen);
	const unsigned char* ip = (const unsigned char*) &(((struct sockaddr_in*) &addr)->sin_addr);

	return log._M_buf.format("%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

bool access_log::log_local_port(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.format("%u", conn._M_server->get_port());
}

bool access_log::log_method(access_log& log, const http_connection& conn, unsigned fd)
{
	const char* method = http_method::get_method(conn._M_method);
	if (method) {
		return log._M_buf.append(method);
	} else {
		return log._M_buf.append('-');
	}
}

bool access_log::log_path(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.append(conn._M_in.data() + conn._M_uri, conn._M_urilen);
}

bool access_log::log_remote_address(access_log& log, const http_connection& conn, unsigned fd)
{
	const unsigned char* ip = (const unsigned char*) &(((struct sockaddr_in*) &conn._M_addr)->sin_addr);
	return log._M_buf.format("%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

bool access_log::log_remote_port(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.format("%u", ntohs(((struct sockaddr_in*) &conn._M_addr)->sin_port));
}

bool access_log::log_request_body_size(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.format("%lu", conn._M_request_body_size);
}

bool access_log::log_request_header_size(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.format("%lu", conn._M_request_header_size);
}

bool access_log::log_response_body_size(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.format("%lld", conn._M_filesize);
}

bool access_log::log_response_header_size(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.format("%lu", conn._M_response_header_size);
}

bool access_log::log_scheme(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.append("http://", 7);
}

bool access_log::log_status_code(access_log& log, const http_connection& conn, unsigned fd)
{
	if (conn._M_error != http_error::OK) {
		return log._M_buf.format("%u", conn._M_error);
	} else {
		return log._M_buf.format("%u", (conn._M_ranges.count() >= 1) ? 206 : 200);
	}
}

bool access_log::log_timestamp(access_log& log, const http_connection& conn, unsigned fd)
{
	return log._M_buf.format("%s, %02d %s %d %02d:%02d:%02d GMT", days[now::_M_tm.tm_wday], now::_M_tm.tm_mday, months[now::_M_tm.tm_mon], 1900 + now::_M_tm.tm_year, now::_M_tm.tm_hour, now::_M_tm.tm_min, now::_M_tm.tm_sec);
}

bool access_log::log_user_agent(access_log& log, const http_connection& conn, unsigned fd)
{
	const char* value;
	unsigned short valuelen;
	if (!conn._M_headers.get_value_known_header(http_headers::USER_AGENT_HEADER, value, &valuelen)) {
		return log._M_buf.append('-');
	} else {
		return log._M_buf.append(value, valuelen);
	}
}
