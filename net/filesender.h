#ifndef FILESENDER_H
#define FILESENDER_H

#include <sys/types.h>

class filesender {
	public:
		static ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count, off_t filesize);
};

#endif // FILESENDER_H
