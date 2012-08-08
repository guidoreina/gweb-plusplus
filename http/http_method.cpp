#include <stdlib.h>
#include <string.h>
#include "http_method.h"
#include "macros/macros.h"

const unsigned char http_method::CONNECT = 0;
const unsigned char http_method::COPY = 1;
const unsigned char http_method::DELETE = 2;
const unsigned char http_method::GET = 3;
const unsigned char http_method::HEAD = 4;
const unsigned char http_method::LOCK = 5;
const unsigned char http_method::MKCOL = 6;
const unsigned char http_method::MOVE = 7;
const unsigned char http_method::OPTIONS = 8;
const unsigned char http_method::POST = 9;
const unsigned char http_method::PROPFIND = 10;
const unsigned char http_method::PROPPATCH = 11;
const unsigned char http_method::PUT = 12;
const unsigned char http_method::TRACE = 13;
const unsigned char http_method::UNLOCK = 14;
const unsigned char http_method::UNKNOWN = 63;

const http_method::string http_method::_M_methods[] = {
	{"CONNECT", 7},
	{"COPY", 4},
	{"DELETE", 6},
	{"GET", 3},
	{"HEAD", 4},
	{"LOCK", 4},
	{"MKCOL", 5},
	{"MOVE", 4},
	{"OPTIONS", 7},
	{"POST", 4},
	{"PROPFIND", 8},
	{"PROPPATCH", 9},
	{"PUT", 3},
	{"TRACE", 5},
	{"UNLOCK", 6}
};

unsigned char http_method::get_method(const char* string, size_t len)
{
	int i = 0;
	int j = ARRAY_SIZE(_M_methods) - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		int ret = strncmp(string, _M_methods[pivot].name, len);
		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (len < _M_methods[pivot].len) {
				j = pivot - 1;
			} else if (len == _M_methods[pivot].len) {
				return (unsigned char) pivot;
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	return UNKNOWN;
}
