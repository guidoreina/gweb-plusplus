#ifndef ACCESS_LOG_H
#define ACCESS_LOG_H

#include <sys/types.h>
#include <limits.h>
#include "string/buffer.h"

struct http_connection;

class access_log {
	public:
		static const char* COMMON_LOG_FORMAT;

		static const off_t MAX_SIZE;

		// Constructor.
		access_log();

		// Destructor.
		virtual ~access_log();

		// Create.
		bool create(const char* dir, const char* filename, const char* format, size_t bufsize, off_t max_size = MAX_SIZE);

		// Enable.
		void enable();

		// Disable.
		void disable();

		// Sync.
		bool sync();

		// Log.
		bool log(const http_connection& conn, unsigned fd);

	protected:
		static const size_t TOKENS_ALLOC;

		buffer _M_buf;

		char _M_path[PATH_MAX + 1];
		size_t _M_pathlen;

		int _M_fd;

		unsigned _M_last_file;

		bool _M_enabled;

		off_t _M_max_size;
		off_t _M_size;

		size_t _M_bufsize;

		struct string {
			char* data;
			size_t len;
		};

		struct variable {
			const char* name;
			size_t len;

			bool (*log)(access_log& log, const http_connection& conn, unsigned fd);
		};

		static const variable _M_variables[];

		enum token_type {STRING, VARIABLE};

		struct token {
			token_type type;

			union {
				string str;
				bool (*log)(access_log& log, const http_connection& conn, unsigned fd);
			} u;
		};

		struct token_list {
			token* tokens;
			size_t size;
			size_t used;
		};

		token_list _M_token_list;

		bool parse_format(const char* format);

		bool add_string(const char* data, size_t len);
		bool add_variable(unsigned variable);
		token* add_token(token_type type);

		static int search(const char* name, size_t len);

		bool flush();

		bool rotate();

		static bool log_PID(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_connection_status(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_host(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_http_version(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_local_address(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_local_port(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_method(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_path(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_remote_address(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_remote_port(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_request_body_size(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_request_header_size(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_response_body_size(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_response_header_size(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_scheme(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_status_code(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_timestamp(access_log& log, const http_connection& conn, unsigned fd);
		static bool log_user_agent(access_log& log, const http_connection& conn, unsigned fd);
};

inline void access_log::enable()
{
	_M_enabled = true;
}

inline void access_log::disable()
{
	_M_enabled = false;
}

inline bool access_log::sync()
{
	if (_M_buf.count() == 0) {
		return true;
	}

	if (_M_fd < 0) {
		return false;
	}

	return flush();
}

inline bool access_log::add_variable(unsigned variable)
{
	token* token;
	if ((token = add_token(VARIABLE)) == NULL) {
		return false;
	}

	token->u.log = _M_variables[variable].log;

	return true;
}

#endif // ACCESS_LOG_H
