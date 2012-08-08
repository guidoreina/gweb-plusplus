#include <stdlib.h>
#include <stdio.h>
#include "http_error.h"
#include "http/http_connection.h"
#include "net/url_parser.h"
#include "util/now.h"
#include "macros/macros.h"
#include "version.h"

const unsigned short http_error::OK = 200;
const unsigned short http_error::MOVED_PERMANENTLY = 301;
const unsigned short http_error::NOT_MODIFIED = 304;
const unsigned short http_error::BAD_REQUEST = 400;
const unsigned short http_error::FORBIDDEN = 403;
const unsigned short http_error::NOT_FOUND = 404;
const unsigned short http_error::LENGTH_REQUIRED = 411;
const unsigned short http_error::REQUEST_ENTITY_TOO_LARGE = 413;
const unsigned short http_error::REQUEST_URI_TOO_LONG = 414;
const unsigned short http_error::REQUESTED_RANGE_NOT_SATISFIABLE = 416;
const unsigned short http_error::INTERNAL_SERVER_ERROR = 500;
const unsigned short http_error::NOT_IMPLEMENTED = 501;
const unsigned short http_error::BAD_GATEWAY = 502;
const unsigned short http_error::SERVICE_UNAVAILABLE = 503;
const unsigned short http_error::GATEWAY_TIMEOUT = 504;

http_error::error http_error::_M_errors[] = {
	{MOVED_PERMANENTLY, "Moved Permanently"},
	{NOT_MODIFIED, "Not Modified"},
	{BAD_REQUEST, "Bad Request"},
	{FORBIDDEN, "Forbidden"},
	{NOT_FOUND, "Not Found"},
	{LENGTH_REQUIRED, "Length Required"},
	{REQUEST_ENTITY_TOO_LARGE, "Request Entity Too Large"},
	{REQUEST_URI_TOO_LONG, "Request-URI Too Long"},
	{REQUESTED_RANGE_NOT_SATISFIABLE, "Requested Range Not Satisfiable"},
	{INTERNAL_SERVER_ERROR, "Internal Server Error"},
	{NOT_IMPLEMENTED, "Not Implemented"},
	{BAD_GATEWAY, "Bad Gateway"},
	{SERVICE_UNAVAILABLE, "Service Unavailable"},
	{GATEWAY_TIMEOUT, "Gateway Timeout"}
};

http_headers* http_error::_M_headers = NULL;
unsigned short http_error::_M_port = 0;

bool http_error::build_page(http_connection* conn)
{
	error* err = search(conn->_M_error);
	if (!err) {
		return false;
	}

	if ((err->body.count() == 0) && (conn->_M_error != NOT_MODIFIED)) {
		err->body.set_buffer_increment(256);

		if (!err->body.format(
			"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">"
			"<html>"
				"<head>"
					"<title>%s</title>"
				"</head>"
				"<body>"
					"<h1>"
						"HTTP/1.1 %d %s"
					"</h1>"
				"</body>"
			"</html>",
			err->reason_phrase,
			err->status_code,
			err->reason_phrase)) {
				return false;
		}
	}

	_M_headers->reset();

	if (!_M_headers->add_known_header(http_headers::DATE_HEADER, &now::_M_tm)) {
		return false;
	}

	if (conn->_M_keep_alive) {
		if (!_M_headers->add_known_header(http_headers::CONNECTION_HEADER, "Keep-Alive", 10, false)) {
			return false;
		}
	} else {
		if (!_M_headers->add_known_header(http_headers::CONNECTION_HEADER, "close", 5, false)) {
			return false;
		}
	}

	int len;

	if (conn->_M_error == MOVED_PERMANENTLY) {
		unsigned short pathlen;
		const char* urlpath = conn->_M_url.get_path(pathlen);

		char location[4096];
		if (_M_port == url_parser::HTTP_DEFAULT_PORT) {
			len = snprintf(location, sizeof(location), "http://%s%.*s/", conn->_M_vhost->name, pathlen, urlpath);
		} else {
			len = snprintf(location, sizeof(location), "http://%s:%d%.*s/", conn->_M_vhost->name, _M_port, pathlen, urlpath);
		}

		if (!_M_headers->add_known_header(http_headers::LOCATION_HEADER, location, len, false)) {
			return false;
		}
	}

	if (!_M_headers->add_known_header(http_headers::SERVER_HEADER, WEBSERVER_NAME, sizeof(WEBSERVER_NAME) - 1, false)) {
		return false;
	}

	if (conn->_M_error != NOT_MODIFIED) {
		char num[32];
		len = snprintf(num, sizeof(num), "%d", err->body.count());
		if (!_M_headers->add_known_header(http_headers::CONTENT_LENGTH_HEADER, num, len, false)) {
			return false;
		}

		if (!_M_headers->add_known_header(http_headers::CONTENT_TYPE_HEADER, "text/html; charset=UTF-8", 24, false)) {
			return false;
		}
	} else {
		if (!_M_headers->add_known_header(http_headers::LAST_MODIFIED_HEADER, &conn->_M_last_modified)) {
			return false;
		}
	}

	conn->_M_out.reset();
	if (!conn->_M_out.format("HTTP/1.1 %d %s\r\n", err->status_code, err->reason_phrase)) {
		return false;
	}

	if (!_M_headers->serialize(conn->_M_out)) {
		return false;
	}

	conn->_M_bodyp = &err->body;

	return true;
}

http_error::error* http_error::search(unsigned short status_code)
{
	int i = 0;
	int j = ARRAY_SIZE(_M_errors) - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		if (status_code < _M_errors[pivot].status_code) {
			j = pivot - 1;
		} else if (status_code == _M_errors[pivot].status_code) {
			return &(_M_errors[pivot]);
		} else {
			i = pivot + 1;
		}
	}

	return NULL;
}
