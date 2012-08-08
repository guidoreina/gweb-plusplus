#include <stdlib.h>
#include <string.h>
#include "scheme.h"
#include "macros/macros.h"

const unsigned char scheme::FILE = 0;
const unsigned char scheme::FTP = 1;
const unsigned char scheme::HTTP = 2;
const unsigned char scheme::HTTPS = 3;
const unsigned char scheme::ICAP = 4;
const unsigned char scheme::IMAP = 5;
const unsigned char scheme::LDAP = 6;
const unsigned char scheme::MAILTO = 7;
const unsigned char scheme::NEWS = 8;
const unsigned char scheme::NFS = 9;
const unsigned char scheme::POP = 10;
const unsigned char scheme::TELNET = 11;
const unsigned char scheme::UNKNOWN = 0xff;

const scheme::string scheme::_M_schemes[] = {
	{"FILE", 4},
	{"FTP", 3},
	{"HTTP", 4},
	{"HTTPS", 5},
	{"ICAP", 4},
	{"IMAP", 4},
	{"LDAP", 4},
	{"MAILTO", 6},
	{"NEWS", 4},
	{"NFS", 3},
	{"POP", 3},
	{"TELNET", 6}
};

unsigned char scheme::get_scheme(const char* string, size_t len)
{
	int i = 0;
	int j = ARRAY_SIZE(_M_schemes) - 1;

	while (i <= j) {
		int pivot = (i + j) / 2;
		int ret = strncasecmp(string, _M_schemes[pivot].name, len);
		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (len < _M_schemes[pivot].len) {
				j = pivot - 1;
			} else if (len == _M_schemes[pivot].len) {
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
