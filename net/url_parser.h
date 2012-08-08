#ifndef URL_PARSER_H
#define URL_PARSER_H

#include "net/scheme.h"
#include "string/buffer.h"

class url_parser {
	public:
		static const unsigned short HOST_MAX_LEN;
		static const unsigned short PATH_MAX_LEN;
		static const unsigned short QUERY_MAX_LEN;
		static const unsigned short FRAGMENT_MAX_LEN;

		static const unsigned short FTP_DEFAULT_PORT;
		static const unsigned short HTTP_DEFAULT_PORT;
		static const unsigned short HTTPS_DEFAULT_PORT;
		static const unsigned short TELNET_DEFAULT_PORT;

		// Constructor.
		url_parser();

		// Destructor.
		~url_parser();

		// Reset.
		void reset();

		// Get scheme.
		unsigned char get_scheme() const;

		// Get host.
		const char* get_host(unsigned short& len) const;

		// Get port.
		unsigned short get_port() const;

		// Get path.
		const char* get_path(unsigned short& len) const;

		// Get extension.
		const char* get_extension(unsigned short& len) const;

		// Get query.
		const char* get_query(unsigned short& len) const;

		// Get fragment.
		const char* get_fragment(unsigned short& len) const;

		// Parse URL.
		enum parse_result {
			ERROR_NO_MEMORY,
			PARSE_ERROR,
			FORBIDDEN,
			SUCCESS
		};

		parse_result parse(char* string, unsigned short& len, buffer& buf);

	private:
		unsigned char _M_scheme;

		const char* _M_host;
		unsigned short _M_hostlen;

		unsigned short _M_port;

		const char* _M_path;
		unsigned short _M_pathlen;

		const char* _M_extension;
		unsigned short _M_extensionlen;

		const char* _M_query;
		unsigned short _M_querylen;

		const char* _M_fragment;
		unsigned short _M_fragmentlen;

		// Parse path.
		parse_result parse_path(char* path, unsigned short& len, buffer& buf);
};

inline url_parser::url_parser()
{
	reset();
}

inline url_parser::~url_parser()
{
}

inline void url_parser::reset()
{
	_M_scheme = scheme::UNKNOWN;

	_M_host = NULL;
	_M_hostlen = 0;

	_M_port = 0;

	_M_path = NULL;
	_M_pathlen = 0;

	_M_extension = NULL;
	_M_extensionlen = 0;

	_M_query = NULL;
	_M_querylen = 0;

	_M_fragment = NULL;
	_M_fragmentlen = 0;
}

inline unsigned char url_parser::get_scheme() const
{
	return _M_scheme;
}

inline const char* url_parser::get_host(unsigned short& len) const
{
	len = _M_hostlen;
	return _M_host;
}

inline unsigned short url_parser::get_port() const
{
	return _M_port;
}

inline const char* url_parser::get_path(unsigned short& len) const
{
	len = _M_pathlen;
	return _M_path;
}

inline const char* url_parser::get_extension(unsigned short& len) const
{
	len = _M_extensionlen;
	return _M_extension;
}

inline const char* url_parser::get_query(unsigned short& len) const
{
	len = _M_querylen;
	return _M_query;
}

inline const char* url_parser::get_fragment(unsigned short& len) const
{
	len = _M_fragmentlen;
	return _M_fragment;
}

#endif // URL_PARSER_H
