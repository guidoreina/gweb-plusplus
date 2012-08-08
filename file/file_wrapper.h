#ifndef FILE_WRAPPER_H
#define FILE_WRAPPER_H

#include <sys/types.h>
#include <unistd.h>

class file_wrapper {
	public:
		struct io_vector {
			void* iov_base;
			size_t iov_len;
		};

		// Open.
		static int open(const char* pathname, int flags);
		static int open(const char* pathname, int flags, mode_t mode);

		// Close.
		static bool close(int fd);

		// Read.
		static ssize_t read(int fd, void* buf, size_t count);

		// Read at a given offset.
		static ssize_t pread(int fd, void* buf, size_t count, off_t offset);

		// Read into multiple buffers.
		static ssize_t readv(int fd, const io_vector* iov, int iovcnt);

		// Write.
		static bool write(int fd, const void* buf, size_t count);

		// Write at a given offset.
		static bool pwrite(int fd, const void* buf, size_t count, off_t offset);

		// Write from multiple buffers.
		static bool writev(int fd, const io_vector* iov, int iovcnt);

		// Seek.
		static off_t seek(int fd, off_t offset, int whence);

		// Get offset.
		static off_t get_offset(int fd);

		// Truncate.
		static bool truncate(int fd, off_t length);
};

inline off_t file_wrapper::get_offset(int fd)
{
	return seek(fd, 0, SEEK_CUR);
}

#endif // FILE_WRAPPER_H
