#include <stdlib.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <limits.h>
#include <errno.h>
#include "file_wrapper.h"
#include "logger/logger.h"

int file_wrapper::open(const char* pathname, int flags)
{
	int fd = ::open(pathname, flags);
	if (fd < 0) {
		logger::instance().log(logger::LOG_ERROR, "Couldn't open file (%s).", pathname);
		return -1;
	}

	return fd;
}

int file_wrapper::open(const char* pathname, int flags, mode_t mode)
{
	int fd = ::open(pathname, flags, mode);
	if (fd < 0) {
		logger::instance().log(logger::LOG_ERROR, "Couldn't open file (%s).", pathname);
		return -1;
	}

	return fd;
}

bool file_wrapper::close(int fd)
{
	if (::close(fd) < 0) {
		logger::instance().perror("close");
		return false;
	}

	return true;
}

ssize_t file_wrapper::read(int fd, void* buf, size_t count)
{
	do {
		ssize_t ret = ::read(fd, buf, count);
		if (ret < 0) {
			if ((errno != EINTR) && (errno != EAGAIN)) {
				logger::instance().perror(logger::LOG_ERROR, "read");
				return -1;
			}
		} else {
			return ret;
		}
	} while (true);
}

ssize_t file_wrapper::pread(int fd, void* buf, size_t count, off_t offset)
{
#if HAVE_PREAD
	do {
		ssize_t ret = ::pread(fd, buf, count, offset);
		if (ret < 0) {
			if ((errno != EINTR) && (errno != EAGAIN)) {
				logger::instance().perror(logger::LOG_ERROR, "pread");
				return -1;
			}
		} else {
			return ret;
		}
	} while (true);
#else
	if (seek(fd, offset, SEEK_SET) != offset) {
		return -1;
	}

	return read(fd, buf, count);
#endif
}

ssize_t file_wrapper::readv(int fd, const io_vector* iov, int iovcnt)
{
	do {
		ssize_t ret = ::readv(fd, (const struct iovec*) iov, iovcnt);
		if (ret < 0) {
			if ((errno != EINTR) && (errno != EAGAIN)) {
				logger::instance().perror(logger::LOG_ERROR, "readv");
				return -1;
			}
		} else {
			return ret;
		}
	} while (true);
}

bool file_wrapper::write(int fd, const void* buf, size_t count)
{
	const char* ptr = (const char*) buf;
	size_t written = 0;

	do {
		ssize_t ret = ::write(fd, ptr + written, count - written);
		if (ret < 0) {
			if ((errno != EINTR) && (errno != EAGAIN)) {
				logger::instance().perror(logger::LOG_ERROR, "write");
				return false;
			}
		} else if (ret > 0) {
			written += ret;
		}
	} while (written < count);

	return true;
}

bool file_wrapper::pwrite(int fd, const void* buf, size_t count, off_t offset)
{
#if HAVE_PREAD
	const char* ptr = (const char*) buf;
	size_t written = 0;

	do {
		ssize_t ret = ::pwrite(fd, ptr + written, count - written, offset);
		if (ret < 0) {
			if ((errno != EINTR) && (errno != EAGAIN)) {
				logger::instance().perror(logger::LOG_ERROR, "pwrite");
				return false;
			}
		} else if (ret > 0) {
			offset += ret;
			written += ret;
		}
	} while (written < count);

	return true;
#else
	if (seek(fd, offset, SEEK_SET) != offset) {
		return false;
	}

	return write(fd, buf, count);
#endif
}

bool file_wrapper::writev(int fd, const io_vector* iov, int iovcnt)
{
	if ((iovcnt < 0) || (iovcnt > IOV_MAX)) {
		return false;
	} else if (iovcnt == 0) {
		return true;
	}

	struct iovec vec[IOV_MAX];
	size_t count = 0;

	for (int i = 0; i < iovcnt; i++) {
		vec[i].iov_base = iov[i].iov_base;
		vec[i].iov_len = iov[i].iov_len;

		count += vec[i].iov_len;
	}

	struct iovec* v = vec;
	size_t written = 0;

	do {
		ssize_t ret = ::writev(fd, v, iovcnt);
		if (ret < 0) {
			if ((errno != EINTR) && (errno != EAGAIN)) {
				logger::instance().perror(logger::LOG_ERROR, "writev");
				return false;
			}
		} else if (ret > 0) {
			written += ret;

			if (written == count) {
				return true;
			}

			while ((size_t) ret >= v->iov_len) {
				ret -= v->iov_len;

				v++;
				iovcnt--;
			}

			if (ret > 0) {
				v->iov_base = (char*) v->iov_base + ret;
				v->iov_len -= ret;
			}
		}
	} while (true);
}

off_t file_wrapper::seek(int fd, off_t offset, int whence)
{
	off_t off;
	if ((off = ::lseek(fd, offset, whence)) < 0) {
		logger::instance().perror(logger::LOG_ERROR, "lseek");
		return -1;
	}

	return off;
}

bool file_wrapper::truncate(int fd, off_t length)
{
	if (::ftruncate(fd, length) < 0) {
		logger::instance().perror(logger::LOG_ERROR, "ftruncate");
		return false;
	}

	return true;
}
