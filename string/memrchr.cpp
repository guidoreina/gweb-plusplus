#include <stdlib.h>
#include "memrchr.h"

void *memrchr(const void *s, int c, size_t n)
{
	const char* begin = (const char*) s;
	char* ptr = (char*) s + n - 1;

	while (ptr >= begin) {
		if (*ptr == c) {
			return ptr;
		}

		ptr--;
	}

	return NULL;
}
