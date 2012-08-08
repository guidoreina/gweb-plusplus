#ifndef FASTCGI_H
#define FASTCGI_H

#include <sys/types.h>
#include <string.h>
#include "string/buffer.h"

class fastcgi {
	friend class name_value_pairs;

	public:
		// Values for type component of FCGI_Header.
		static const unsigned char FCGI_BEGIN_REQUEST;
		static const unsigned char FCGI_ABORT_REQUEST;
		static const unsigned char FCGI_END_REQUEST;
		static const unsigned char FCGI_PARAMS;
		static const unsigned char FCGI_STDIN;
		static const unsigned char FCGI_STDOUT;
		static const unsigned char FCGI_STDERR;
		static const unsigned char FCGI_DATA;
		static const unsigned char FCGI_GET_VALUES;
		static const unsigned char FCGI_GET_VALUES_RESULT;
		static const unsigned char FCGI_UNKNOWN_TYPE;
		static const unsigned char FCGI_MAXTYPE;

		// Values for role component of FCGI_BeginRequestBody.
		static const unsigned short FCGI_RESPONDER;
		static const unsigned short FCGI_AUTHORIZER;
		static const unsigned short FCGI_FILTER;

		struct FCGI_Header {
			unsigned char version;
			unsigned char type;
			unsigned char requestIdB1;
			unsigned char requestIdB0;
			unsigned char contentLengthB1;
			unsigned char contentLengthB0;
			unsigned char paddingLength;
			unsigned char reserved;
		};

		// Begin request.
		static bool begin_request(unsigned short requestId, unsigned short role, bool keep_alive, buffer& out);

		class name_value_pairs {
			public:
				// Constructor.
				name_value_pairs(buffer& out);

				// Set header.
				bool header(unsigned char type, unsigned short requestId);

				// Add name-value pair.
				bool add(const char* name, unsigned short namelen, const char* value, unsigned short valuelen, bool prepend_HTTP_, bool to_uppercase);

				// End.
				virtual bool end();

			protected:
				buffer& _M_out;

				fastcgi::FCGI_Header* _M_header;
				unsigned short _M_contentLength;
		};

		class params : public name_value_pairs {
			public:
				// Constructor.
				params(buffer& out);

				// Set header.
				bool header(unsigned short requestId);

				// End.
				bool end();

			protected:
				unsigned short _M_requestId;
		};

		// stdin.
		static bool stdin_stream(unsigned short requestId, const void* buf, size_t len, buffer& out);
		static bool stdin_stream(unsigned short requestId, const void* buf, size_t len, int fd, off_t& filesize);

		// data.
		static bool data_stream(unsigned short requestId, const void* buf, size_t len, buffer& out);
		static bool data_stream(unsigned short requestId, const void* buf, size_t len, int fd, off_t& filesize);

		// Abort request.
		static bool abort_request(unsigned short requestId, buffer& out);

		// Process data received from the FastCGI application.
		bool process(buffer& buf);

	protected:
		// Listening socket file number.
		static const int FCGI_LISTENSOCK_FILENO;

		// Number of bytes in a FCGI_Header. Future versions of the protocol
		// will not reduce this number.
		static const size_t FCGI_HEADER_LEN;

		// Value for version component of FCGI_Header.
		static const unsigned char FCGI_VERSION_1;

		// Value for requestId component of FCGI_Header.
		static const unsigned short FCGI_NULL_REQUEST_ID;

		struct FCGI_BeginRequestBody {
			unsigned char roleB1;
			unsigned char roleB0;
			unsigned char flags;
			unsigned char reserved[5];
		};

		struct FCGI_BeginRequestRecord {
			FCGI_Header header;
			FCGI_BeginRequestBody body;
		};

		// Mask for flags component of FCGI_BeginRequestBody.
		static const unsigned char FCGI_KEEP_CONN;

		struct FCGI_EndRequestBody {
			unsigned char appStatusB3;
			unsigned char appStatusB2;
			unsigned char appStatusB1;
			unsigned char appStatusB0;
			unsigned char protocolStatus;
			unsigned char reserved[3];
		};

		struct FCGI_EndRequestRecord {
			FCGI_Header header;
			FCGI_EndRequestBody body;
		};

		// Values for protocolStatus component of FCGI_EndRequestBody.
		static const unsigned char FCGI_REQUEST_COMPLETE;
		static const unsigned char FCGI_CANT_MPX_CONN;
		static const unsigned char FCGI_OVERLOADED;
		static const unsigned char FCGI_UNKNOWN_ROLE;

		// Variable names for FCGI_GET_VALUES / FCGI_GET_VALUES_RESULT records.
		static const char* FCGI_MAX_CONNS;
		static const char* FCGI_MAX_REQS;
		static const char* FCGI_MPXS_CONNS;

		struct FCGI_UnknownTypeBody {
			unsigned char type;    
			unsigned char reserved[7];
		};

		struct FCGI_UnknownTypeRecord {
			FCGI_Header header;
			FCGI_UnknownTypeBody body;
		};

		virtual bool get_values_result(const char* name, unsigned short namelen, const char* value, unsigned short valuelen) = 0;

		virtual bool stdout_stream(unsigned short requestId, const void* buf, unsigned short len) = 0;
		virtual bool stderr_stream(unsigned short requestId, const void* buf, unsigned short len) = 0;

		virtual bool end_request(unsigned short requestId, unsigned appStatus, unsigned char protocolStatus) = 0;

		// Fill header.
		static void fill_header(unsigned char type, unsigned short requestId, unsigned short contentLength, unsigned char paddingLength, FCGI_Header* header);

		// Decode name-value pairs.
		bool decode_name_value_pairs(const void* buf, unsigned short len);

		// Input stream.
		static bool input_stream(unsigned char type, unsigned short requestId, const void* buf, size_t len, buffer& out);
		static bool input_stream(unsigned char type, unsigned short requestId, const void* buf, size_t len, int fd, off_t& filesize);

		// Fill stream record.
		static void fill_stream_record(unsigned char type, unsigned short requestId, unsigned short len, unsigned char paddingLength, const void* buf, buffer& out);
		static bool fill_stream_record(unsigned char type, unsigned short requestId, unsigned short len, unsigned char paddingLength, const void* buf, int fd, off_t& filesize);
};

inline fastcgi::name_value_pairs::name_value_pairs(buffer& out) : _M_out(out)
{
}

inline fastcgi::params::params(buffer& out) : name_value_pairs(out)
{
}

inline bool fastcgi::params::header(unsigned short requestId)
{
	if (!fastcgi::name_value_pairs::header(FCGI_PARAMS, requestId)) {
		return false;
	}

	_M_requestId = requestId;

	return true;
}

inline bool fastcgi::params::end()
{
	if (!fastcgi::name_value_pairs::end()) {
		return false;
	}

	return fastcgi::name_value_pairs::header(FCGI_PARAMS, _M_requestId);
}

inline void fastcgi::fill_header(unsigned char type, unsigned short requestId, unsigned short contentLength, unsigned char paddingLength, FCGI_Header* header)
{
	header->version = FCGI_VERSION_1;
	header->type = type;

	header->requestIdB1 = (requestId & 0xff00) >> 8;
	header->requestIdB0 = requestId & 0x00ff;

	header->contentLengthB1 = (contentLength & 0xff00) >> 8;
	header->contentLengthB0 = contentLength & 0x00ff;

	header->paddingLength = paddingLength;

	header->reserved = 0;
}

inline bool fastcgi::stdin_stream(unsigned short requestId, const void* buf, size_t len, buffer& out)
{
	return input_stream(FCGI_STDIN, requestId, buf, len, out);
}

inline bool fastcgi::stdin_stream(unsigned short requestId, const void* buf, size_t len, int fd, off_t& filesize)
{
	return input_stream(FCGI_STDIN, requestId, buf, len, fd, filesize);
}

inline bool fastcgi::data_stream(unsigned short requestId, const void* buf, size_t len, buffer& out)
{
	return input_stream(FCGI_DATA, requestId, buf, len, out);
}

inline bool fastcgi::data_stream(unsigned short requestId, const void* buf, size_t len, int fd, off_t& filesize)
{
	return input_stream(FCGI_DATA, requestId, buf, len, fd, filesize);
}

inline void fastcgi::fill_stream_record(unsigned char type, unsigned short requestId, unsigned short len, unsigned char paddingLength, const void* buf, buffer& out)
{
	FCGI_Header* header = (FCGI_Header*) (out.data() + out.count());

	fill_header(type, requestId, len, paddingLength, header);

	char* ptr = (char*) header + sizeof(FCGI_Header);

	memcpy(ptr, buf, len);
	ptr += len;

	for (size_t i = paddingLength; i > 0; i--) {
		*ptr++ = 0;
	}

	out.increment_count(sizeof(FCGI_Header) + len + paddingLength);
}

#endif // FASTCGI_H
