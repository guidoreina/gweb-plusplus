#ifndef HTTP_HEADERS_H
#define HTTP_HEADERS_H

#include <string.h>
#include <time.h>
#include "string/buffer.h"

class http_headers {
	public:
		static const size_t MAX_HEADERS_SIZE;

		static const unsigned char ACCEPT_HEADER;
		static const unsigned char ACCEPT_CHARSET_HEADER;
		static const unsigned char ACCEPT_ENCODING_HEADER;
		static const unsigned char ACCEPT_LANGUAGE_HEADER;
		static const unsigned char ACCEPT_RANGES_HEADER;
		static const unsigned char AGE_HEADER;
		static const unsigned char ALLOW_HEADER;
		static const unsigned char AUTHORIZATION_HEADER;
		static const unsigned char CACHE_CONTROL_HEADER;
		static const unsigned char CONNECTION_HEADER;
		static const unsigned char CONTENT_ENCODING_HEADER;
		static const unsigned char CONTENT_LANGUAGE_HEADER;
		static const unsigned char CONTENT_LENGTH_HEADER;
		static const unsigned char CONTENT_LOCATION_HEADER;
		static const unsigned char CONTENT_MD5_HEADER;
		static const unsigned char CONTENT_RANGE_HEADER;
		static const unsigned char CONTENT_TYPE_HEADER;
		static const unsigned char COOKIE_HEADER;
		static const unsigned char DATE_HEADER;
		static const unsigned char ETAG_HEADER;
		static const unsigned char EXPECT_HEADER;
		static const unsigned char EXPIRES_HEADER;
		static const unsigned char FROM_HEADER;
		static const unsigned char HOST_HEADER;
		static const unsigned char IF_MATCH_HEADER;
		static const unsigned char IF_MODIFIED_SINCE_HEADER;
		static const unsigned char IF_NONE_MATCH_HEADER;
		static const unsigned char IF_RANGE_HEADER;
		static const unsigned char IF_UNMODIFIED_SINCE_HEADER;
		static const unsigned char KEEP_ALIVE_HEADER;
		static const unsigned char LAST_MODIFIED_HEADER;
		static const unsigned char LOCATION_HEADER;
		static const unsigned char MAX_FORWARDS_HEADER;
		static const unsigned char PRAGMA_HEADER;
		static const unsigned char PROXY_AUTHENTICATE_HEADER;
		static const unsigned char PROXY_AUTHORIZATION_HEADER;
		static const unsigned char PROXY_CONNECTION_HEADER;
		static const unsigned char RANGE_HEADER;
		static const unsigned char REFERER_HEADER;
		static const unsigned char RETRY_AFTER_HEADER;
		static const unsigned char SERVER_HEADER;
		static const unsigned char SET_COOKIE_HEADER;
		static const unsigned char STATUS_HEADER;
		static const unsigned char TE_HEADER;
		static const unsigned char TRAILER_HEADER;
		static const unsigned char TRANSFER_ENCODING_HEADER;
		static const unsigned char UPGRADE_HEADER;
		static const unsigned char USER_AGENT_HEADER;
		static const unsigned char VARY_HEADER;
		static const unsigned char VIA_HEADER;
		static const unsigned char WARNING_HEADER;
		static const unsigned char WWW_AUTHENTICATE_HEADER;
		static const unsigned char UNKNOWN_HEADER;

		static const unsigned char GENERAL_HEADER_FIELD;
		static const unsigned char REQUEST_HEADER_FIELD;
		static const unsigned char RESPONSE_HEADER_FIELD;
		static const unsigned char ENTITY_HEADER_FIELD;

		static const size_t BOUNDARY_WIDTH;

		// Constructor.
		http_headers();

		// Destructor.
		virtual ~http_headers();

		// Free.
		void free();

		// Reset.
		void reset();

		// Set maximum line length.
		void set_max_line_length(unsigned short max_line_len);

		// Ignore errors.
		void ignore_errors();

		// Don't ignore errors.
		void dont_ignore_errors();

		// Add header.
		bool add_header(const char* name, unsigned short namelen, const char* value, unsigned short valuelen, bool replace_value);

		// Add known header.
		bool add_known_header(unsigned char header, const char* value, unsigned short valuelen, bool replace_value);
		bool add_known_header(unsigned char header, const struct tm* timestamp);

		// Add Content-Type multipart.
		bool add_content_type_multipart(unsigned boundary);

		// Add unknown header.
		bool add_unknown_header(const char* name, unsigned short namelen, const char* value, unsigned short valuelen, bool replace_value);

		// Remove header.
		bool remove_header(const char* name, unsigned short namelen);

		// Remove known header.
		bool remove_known_header(unsigned char header);

		// Remove unknown header.
		bool remove_unknown_header(const char* name, unsigned short namelen);

		// Get header.
		bool get_header(unsigned idx, unsigned char& header, const char*& name, unsigned short& namelen, const char*& value, unsigned short& valuelen);

		// Get value of header.
		bool get_value(const char* name, unsigned short namelen, const char*& value, unsigned short* valuelen = NULL) const;

		// Get value of known header.
		bool get_value_known_header(unsigned char header, const char*& value, unsigned short* valuelen = NULL) const;

		// Get value of unknown header.
		bool get_value_unknown_header(const char* name, unsigned short namelen, const char*& value, unsigned short* valuelen = NULL) const;

		// Serialize.
		bool serialize(buffer& buffer) const;

		// Parse.
		enum parse_result {
			ERROR_NO_MEMORY,
			ERROR_WRONG_HEADERS,
			ERROR_HEADERS_TOO_LARGE,
			NOT_END_OF_HEADER,
			END_OF_HEADER
		};

		parse_result parse(const char* buffer, size_t len, size_t& body_offset);

	protected:
		static const size_t HTTP_HEADER_ALLOC;
		static const size_t DATA_ALLOC;

		static const unsigned short MAX_LINE_LEN;

		struct known_http_header {
			const char* name;
			size_t len;

			unsigned char type;

			unsigned char might_have_commas;
			unsigned char single_token;
			unsigned char force_multiple_header_fields;
		};

		static const known_http_header _M_known_http_headers[];

		static unsigned char get_header(const char* name, size_t len);

		struct http_header {
			unsigned char header;

			unsigned short name;
			unsigned short namelen;

			unsigned short value;
			unsigned short valuelen;
		};

		http_header* _M_http_headers;
		unsigned short _M_size;
		unsigned short _M_used;

		struct data {
			char* data;
			unsigned short size;
			unsigned short used;
		};

		data _M_data;

		unsigned short _M_max_line_len;

		bool _M_ignore_errors;

		unsigned short _M_offset;
		unsigned short _M_token;
		unsigned short _M_end;
		unsigned short _M_name;
		unsigned short _M_namelen;
		unsigned char _M_state;
		unsigned char _M_header;
		unsigned char _M_nCRLFs;

		bool allocate();
		bool allocate_data(size_t len);

		http_header* search(unsigned char header, unsigned short& pos) const;
		http_header* search(const char* name, unsigned short namelen, unsigned short& pos) const;

		http_header* add_header(unsigned char header, const char* name, unsigned short namelen, unsigned short valuelen, bool replace_value);

		unsigned short clean_value(char* data, const char* value, unsigned short valuelen);
};

inline http_headers::~http_headers()
{
	free();
}

inline void http_headers::reset()
{
	_M_used = 0;
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

inline void http_headers::set_max_line_length(unsigned short max_line_len)
{
	_M_max_line_len = max_line_len;
}

inline void http_headers::ignore_errors()
{
	_M_ignore_errors = true;
}

inline void http_headers::dont_ignore_errors()
{
	_M_ignore_errors = false;
}

inline bool http_headers::get_header(unsigned idx, unsigned char& header, const char*& name, unsigned short& namelen, const char*& value, unsigned short& valuelen)
{
	if (idx >= _M_used) {
		return false;
	}

	http_header* http_header = &_M_http_headers[idx];

	header = http_header->header;

	if (http_header->header != UNKNOWN_HEADER) {
		name = _M_known_http_headers[(unsigned) http_header->header].name;
	} else {
		name = _M_data.data + http_header->name;
	}

	namelen = http_header->namelen;

	value = _M_data.data + http_header->value;
	valuelen = http_header->valuelen;

	return true;
}

inline http_headers::http_header* http_headers::search(unsigned char header, unsigned short& pos) const
{
	const known_http_header* known_http_header = &(_M_known_http_headers[header]);
	unsigned char type = known_http_header->type;

	for (pos = 0; pos < _M_used; pos++) {
		if ((header == _M_http_headers[pos].header) && (!known_http_header->force_multiple_header_fields)) {
			return &(_M_http_headers[pos]);
		} else if ((_M_http_headers[pos].header == UNKNOWN_HEADER) || (type < _M_known_http_headers[(unsigned) _M_http_headers[pos].header].type)) {
			break;
		}
	}

	return NULL;
}

inline http_headers::http_header* http_headers::search(const char* name, unsigned short namelen, unsigned short& pos) const
{
	for (pos = 0; pos < _M_used; pos++) {
		if (_M_http_headers[pos].header == UNKNOWN_HEADER) {
			if ((namelen == _M_http_headers[pos].namelen) && (strncasecmp(name, _M_data.data + _M_http_headers[pos].name, namelen) == 0)) {
				return &(_M_http_headers[pos]);
			}
		}
	}

	return NULL;
}

#endif // HTTP_HEADERS_H
