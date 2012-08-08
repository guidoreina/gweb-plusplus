#include <stdlib.h>
#include "memcasemem.h"

void* memcasemem(const void* haystack, size_t haystacklen, const void* needle, size_t needlelen)
{
	if (needlelen == 0) {
		return (void*) haystack;
	}

	if (haystacklen < needlelen) {
		return NULL;
	}

	const char* end = (const char*) haystack + haystacklen - needlelen;

	for (const char* ptr = (const char*) haystack; ptr <= end; ptr++) {
		size_t i;
		for (i = 0; i < needlelen; i++) {
			unsigned char c1 = ptr[i];
			c1 = ((c1 >= 'A') && (c1 <= 'Z')) ? (c1 | 0x20) : c1;

			unsigned char c2 = ((const char*) needle)[i];
			c2 = ((c2 >= 'A') && (c2 <= 'Z')) ? (c2 | 0x20) : c2;

			if (c1 != c2) {
				break;
			}
		}

		if (i == needlelen) {
			return (void*) ptr;
		}
	}

	return NULL;
}
