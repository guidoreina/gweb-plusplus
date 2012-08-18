#include <stdlib.h>
#include "fastcgi.h"
#include "file/file_wrapper.h"
#include "logger/logger.h"
#include "macros/macros.h"

const int fastcgi::FCGI_LISTENSOCK_FILENO = 0;

const size_t fastcgi::FCGI_HEADER_LEN = 8;

const unsigned char fastcgi::FCGI_VERSION_1 = 1;

const unsigned char fastcgi::FCGI_BEGIN_REQUEST = 1;
const unsigned char fastcgi::FCGI_ABORT_REQUEST = 2;
const unsigned char fastcgi::FCGI_END_REQUEST = 3;
const unsigned char fastcgi::FCGI_PARAMS = 4;
const unsigned char fastcgi::FCGI_STDIN = 5;
const unsigned char fastcgi::FCGI_STDOUT = 6;
const unsigned char fastcgi::FCGI_STDERR = 7;
const unsigned char fastcgi::FCGI_DATA = 8;
const unsigned char fastcgi::FCGI_GET_VALUES = 9;
const unsigned char fastcgi::FCGI_GET_VALUES_RESULT = 10;
const unsigned char fastcgi::FCGI_UNKNOWN_TYPE = 11;
const unsigned char fastcgi::FCGI_MAXTYPE = fastcgi::FCGI_UNKNOWN_TYPE;

const unsigned short fastcgi::FCGI_NULL_REQUEST_ID = 0;

const unsigned char fastcgi::FCGI_KEEP_CONN = 1;

const unsigned short fastcgi::FCGI_RESPONDER = 1;
const unsigned short fastcgi::FCGI_AUTHORIZER = 2;
const unsigned short fastcgi::FCGI_FILTER = 3;

const unsigned char fastcgi::FCGI_REQUEST_COMPLETE = 0;
const unsigned char fastcgi::FCGI_CANT_MPX_CONN = 1;
const unsigned char fastcgi::FCGI_OVERLOADED = 2;
const unsigned char fastcgi::FCGI_UNKNOWN_ROLE = 3;

const char* fastcgi::FCGI_MAX_CONNS = "FCGI_MAX_CONNS";
const char* fastcgi::FCGI_MAX_REQS = "FCGI_MAX_REQS";
const char* fastcgi::FCGI_MPXS_CONNS = "FCGI_MPXS_CONNS";

bool fastcgi::begin_request(unsigned short requestId, unsigned short role, bool keep_alive, buffer& out)
{
	if (!out.allocate(sizeof(FCGI_BeginRequestRecord))) {
		return false;
	}

	FCGI_BeginRequestRecord* rec = (FCGI_BeginRequestRecord*) (out.data() + out.count());

	fill_header(FCGI_BEGIN_REQUEST, requestId, sizeof(FCGI_BeginRequestBody), 0, &rec->header);

	rec->body.roleB1 = (role & 0xff00) >> 8;
	rec->body.roleB0 = role & 0x00ff;

	rec->body.flags = keep_alive ? FCGI_KEEP_CONN : 0;

	unsigned char* ptr = rec->body.reserved;

	for (size_t i = sizeof(rec->body.reserved); i > 0; i--) {
		*ptr++ = 0;
	}

	out.increment_count(sizeof(FCGI_BeginRequestRecord));

	return true;
}

bool fastcgi::name_value_pairs::header(unsigned char type, unsigned short requestId)
{
	if (!_M_out.allocate(sizeof(fastcgi::FCGI_Header))) {
		return false;
	}

	_M_header = (fastcgi::FCGI_Header*) (_M_out.data() + _M_out.count());
	_M_contentLength = 0;

	fill_header(type, requestId, 0, 0, _M_header);

	_M_out.increment_count(sizeof(FCGI_Header));

	return true;
}

bool fastcgi::name_value_pairs::add(const char* name, unsigned short namelen, const char* value, unsigned short valuelen, bool prepend_HTTP_, bool to_uppercase)
{
	unsigned short prefixlen = prepend_HTTP_ ? 5 : 0;
	namelen += prefixlen;

	size_t size = namelen + valuelen;

	if (namelen <= 127) {
		size++;
	} else {
		size += 4;
	}

	if (valuelen <= 127) {
		size++;
	} else {
		size += 4;
	}

	if ((size_t) _M_contentLength + size > 0xffff) {
		return false;
	}

	if (!_M_out.allocate(size)) {
		return false;
	}

	char* data = _M_out.data();
	size_t count = _M_out.count();

	if (namelen <= 127) {
		data[count++] = namelen;
	} else {
		data[count++] = 0x80 | ((namelen & 0xff000000) >> 24);
		data[count++] = (namelen & 0x00ff0000) >> 16;
		data[count++] = (namelen & 0x0000ff00) >> 8;
		data[count++] = namelen & 0x000000ff;
	}

	if (valuelen <= 127) {
		data[count++] = valuelen;
	} else {
		data[count++] = 0x80 | ((valuelen & 0xff000000) >> 24);
		data[count++] = (valuelen & 0x00ff0000) >> 16;
		data[count++] = (valuelen & 0x0000ff00) >> 8;
		data[count++] = valuelen & 0x000000ff;
	}

	if (prepend_HTTP_) {
		memcpy(data + count, "HTTP_", 5);
		count += 5;
	}

	namelen -= prefixlen;

	if (to_uppercase) {
		for (unsigned short i = 0; i < namelen; i++) {
			unsigned char c = name[i];
			if (IS_ALPHA(c)) {
				c &= ~0x20; // To uppercase.
			} else if (c == '-') {
				c = '_';
			}

			data[count++] = c;
		}
	} else {
		memcpy(data + count, name, namelen);
		count += namelen;
	}

	if (valuelen > 0) {
		memcpy(data + count, value, valuelen);
		count += valuelen;
	}

	_M_out.increment_count(size);

	_M_contentLength += size;

	return true;
}

bool fastcgi::name_value_pairs::end()
{
	_M_header->contentLengthB1 = (_M_contentLength & 0xff00) >> 8;
	_M_header->contentLengthB0 = _M_contentLength & 0x00ff;

	unsigned char remainder = _M_contentLength % 8;
	_M_header->paddingLength = (remainder == 0) ? 0 : 8 - remainder;

	if (_M_header->paddingLength > 0) {
		if (!_M_out.allocate(_M_header->paddingLength)) {
			return false;
		}

		char* ptr = _M_out.data() + _M_out.count();

		for (size_t i = _M_header->paddingLength; i > 0; i--) {
			*ptr++ = 0;
		}

		_M_out.increment_count(_M_header->paddingLength);
	}

	return true;
}

bool fastcgi::abort_request(unsigned short requestId, buffer& out)
{
	if (!out.allocate(sizeof(FCGI_Header))) {
		return false;
	}

	FCGI_Header* header = (FCGI_Header*) (out.data() + out.count());

	fill_header(FCGI_ABORT_REQUEST, requestId, 0, 0, header);

	out.increment_count(sizeof(FCGI_Header));

	return true;
}

bool fastcgi::process(buffer& buf)
{
	size_t offset = 0;

	do {
		size_t count = buf.count() - offset;

		if (count < sizeof(FCGI_Header)) {
			if ((count > 0) && (offset > 0)) {
				memmove(buf.data(), buf.data() + offset, count);
			}

			buf.set_count(count);

			return true;
		}

		const FCGI_Header* header = (const FCGI_Header*) (buf.data() + offset);

		if ((header->version != FCGI_VERSION_1) || (header->type == 0) || (header->type > FCGI_UNKNOWN_TYPE)) {
			// Discard buffer.
			logger::instance().log(logger::LOG_ERROR, "Received invalid record (version = %u, type = %u).", header->version, header->type);

			buf.reset();
			return true;
		}

		unsigned short contentLength = (header->contentLengthB1 << 8) | header->contentLengthB0;
		if (count < sizeof(FCGI_Header) + contentLength + header->paddingLength) {
			// Record not complete.
			if (offset > 0) {
				memmove(buf.data(), buf.data() + offset, count);
			}

			buf.set_count(count);

			return true;
		}

		if (header->type == FCGI_GET_VALUES_RESULT) {
			if (contentLength > 0) {
				if (!decode_name_value_pairs((const char*) header + sizeof(FCGI_Header), contentLength)) {
					logger::instance().log(logger::LOG_ERROR, "get_values_result callback failed (content length = %u).", contentLength);
					return false;
				}
			}
		} else if (header->type == FCGI_STDOUT) {
			if (contentLength > 0) {
				unsigned short requestId = (header->requestIdB1 << 8) | header->requestIdB0;

				if (!stdout_stream(requestId, (const char*) header + sizeof(FCGI_Header), contentLength)) {
					logger::instance().log(logger::LOG_ERROR, "stdout_stream callback failed (requestId = %u, content length = %u).", requestId, contentLength);
					return false;
				}
			}
		} else if (header->type == FCGI_STDERR) {
			if (contentLength > 0) {
				unsigned short requestId = (header->requestIdB1 << 8) | header->requestIdB0;

				if (!stderr_stream(requestId, (const char*) header + sizeof(FCGI_Header), contentLength)) {
					logger::instance().log(logger::LOG_ERROR, "stderr_stream callback failed (requestId = %u, content length = %u).", requestId, contentLength);
					return false;
				}
			}
		} else if (header->type == FCGI_END_REQUEST) {
			unsigned short requestId = (header->requestIdB1 << 8) | header->requestIdB0;

			if (contentLength == sizeof(FCGI_EndRequestBody)) {
				const FCGI_EndRequestBody* end = (const FCGI_EndRequestBody*) ((const char*) header + sizeof(FCGI_Header));

				unsigned appStatus = (end->appStatusB3 << 24) | (end->appStatusB2 << 16) | (end->appStatusB1 << 8) | end->appStatusB0;

				if (!end_request(requestId, appStatus, end->protocolStatus)) {
					logger::instance().log(logger::LOG_ERROR, "end_request callback failed (requestId = %u).", requestId);
					return false;
				}
			} else {
				logger::instance().log(logger::LOG_ERROR, "Invalid content length for record FCGI_END_REQUEST (requestId = %u, content length = %u).", requestId, contentLength);
			}
		} else {
			unsigned short requestId = (header->requestIdB1 << 8) | header->requestIdB0;

			logger::instance().log(logger::LOG_ERROR, "Received unexpected record type (type = %u, requestId = %u, content length = %u).", header->type, requestId, contentLength);
		}

		offset += (sizeof(FCGI_Header) + contentLength + header->paddingLength);
	} while (true);
}

bool fastcgi::decode_name_value_pairs(const void* buf, unsigned short len)
{
	const unsigned char* ptr = (const unsigned char*) buf;

	do {
		size_t size;
		size_t nameLength;

		if (*ptr >= 0x80) {
			if (len < 4) {
				logger::instance().log(logger::LOG_ERROR, "Name-value record too small (%u bytes).", len);

				// Ignore record.
				return true;
			}

			nameLength = (*ptr++ & 0x7f) << 24;
			nameLength += (*ptr++ << 16);
			nameLength += (*ptr++ << 8);
			nameLength += *ptr++;

			if (nameLength > 0xffff) {
				logger::instance().log(logger::LOG_ERROR, "Name too long (%u bytes).", nameLength);

				// Ignore record.
				return true;
			}

			size = 4;
		} else {
			nameLength = *ptr++;

			size = 1;
		}

		size_t valueLength;

		if (*ptr >= 0x80) {
			if (len < size + 4) {
				logger::instance().log(logger::LOG_ERROR, "Name-value record too small (%u bytes).", len);

				// Ignore record.
				return true;
			}

			valueLength = (*ptr++ & 0x7f) << 24;
			valueLength += (*ptr++ << 16);
			valueLength += (*ptr++ << 8);
			valueLength += *ptr++;

			if (valueLength > 0xffff) {
				logger::instance().log(logger::LOG_ERROR, "Value too long (%u bytes).", valueLength);

				// Ignore record.
				return true;
			}

			size += 4;
		} else {
			if (len < size + 1) {
				logger::instance().log(logger::LOG_ERROR, "Name-value record too small (%u bytes).", len);

				// Ignore record.
				return true;
			}

			valueLength = *ptr++;

			size += 1;
		}

		size_t total = size + nameLength + valueLength;

		if (len < total) {
			logger::instance().log(logger::LOG_ERROR, "Name-value record too small (size: %u, expected: %u).", len, total);

			// Ignore record.
			return true;
		}

		if (!get_values_result((const char*) ptr, nameLength, (const char*) ptr + nameLength, valueLength)) {
			return false;
		}

		ptr += (nameLength + valueLength);
		len -= total;
	} while (len > 0);

	return true;
}

bool fastcgi::input_stream(unsigned char type, unsigned short requestId, const void* buf, size_t len, buffer& out)
{
	size_t nrecords = len / 0xffff;
	if ((len % 0xffff) != 0) {
		nrecords++;
	}

	unsigned char remainder = len % 8;
	unsigned char paddingLength = (remainder == 0) ? 0 : 8 - remainder;

	size_t size = (nrecords * sizeof(FCGI_Header)) + len + paddingLength;

	if (!out.allocate(size)) {
		return false;
	}

	size_t count = len;

	for (size_t i = 1; i < nrecords; i++) {
		len = count % 0xffff;

		fill_stream_record(type, requestId, len, 0, buf, out);

		buf = (const char*) buf + len;

		count -= len;
	}

	fill_stream_record(type, requestId, count, paddingLength, buf, out);

	return true;
}

bool fastcgi::input_stream(unsigned char type, unsigned short requestId, const void* buf, size_t len, int fd, off_t& filesize)
{
	size_t nrecords = len / 0xffff;
	if ((len % 0xffff) != 0) {
		nrecords++;
	}

	unsigned char remainder = len % 8;
	unsigned char paddingLength = (remainder == 0) ? 0 : 8 - remainder;

	size_t count = len;

	for (size_t i = 1; i < nrecords; i++) {
		len = count % 0xffff;

		fill_stream_record(type, requestId, len, 0, buf, fd, filesize);

		buf = (const char*) buf + len;

		count -= len;
	}

	return fill_stream_record(type, requestId, count, paddingLength, buf, fd, filesize);
}

bool fastcgi::fill_stream_record(unsigned char type, unsigned short requestId, unsigned short len, unsigned char paddingLength, const void* buf, int fd, off_t& filesize)
{
	FCGI_Header header;

	fill_header(type, requestId, len, paddingLength, &header);

	file_wrapper::io_vector iov[3];
	unsigned iovcnt = 2;

	iov[0].iov_base = &header;
	iov[0].iov_len = sizeof(FCGI_Header);

	iov[1].iov_base = (void*) buf;
	iov[1].iov_len = len;

	unsigned char padding[8];

	if (paddingLength > 0) {
		for (size_t i = 0; i < paddingLength; i++) {
			padding[i] = 0;
		}

		iov[2].iov_base = padding;
		iov[2].iov_len = paddingLength;

		iovcnt++;
	}

	if (!file_wrapper::writev(fd, iov, iovcnt)) {
		return false;
	}

	filesize += (sizeof(FCGI_Header) + len + paddingLength);

	return true;
}
