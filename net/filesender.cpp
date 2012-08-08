#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "filesender.h"
#include "logger/logger.h"

#if HAVE_SENDFILE
	#if defined(__linux__)
		#include <sys/sendfile.h>

		ssize_t filesender::sendfile(int out_fd, int in_fd, off_t *offset, size_t count, off_t filesize)
		{
			ssize_t ret;

			if ((ret = ::sendfile(out_fd, in_fd, offset, count)) < 0) {
				logger::instance().perror("sendfile");
				return -1;
			}

			return ret;
		}
	#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
		#include <sys/socket.h>
		#include <sys/uio.h>

		ssize_t filesender::sendfile(int out_fd, int in_fd, off_t *offset, size_t count, off_t filesize)
		{
			off_t sbytes = 0;

			if (::sendfile(in_fd, out_fd, *offset, count, NULL, &sbytes, 0) < 0) {
				logger::instance().perror("sendfile");

				if ((errno == EAGAIN) || (errno == EINTR)) {
					*offset += sbytes;
				}

				return -1;
			}

			*offset += sbytes;

			return sbytes;
		}
	#elif defined(__sun__)
		#include <sys/sendfile.h>

		ssize_t filesender::sendfile(int out_fd, int in_fd, off_t *offset, size_t count, off_t filesize)
		{
			struct sendfilevec vec;
			size_t xferred;

			vec.sfv_fd = in_fd;
			vec.sfv_flag = 0;
			vec.sfv_off = *offset;
			vec.sfv_len = count;

			ssize_t ret = ::sendfilev(out_fd, &vec, 1, &xferred);
			if (ret < 0) {
				logger::instance().perror("sendfilev");
				return -1;
			}

			*offset += xferred;

			return xferred;
		}
	#endif
#elif HAVE_MMAP
	#include <sys/mman.h>
	#include "socket_wrapper.h"

	ssize_t filesender::sendfile(int out_fd, int in_fd, off_t *offset, size_t count, off_t filesize)
	{
		void* data = mmap(NULL, filesize, PROT_READ, MAP_SHARED, in_fd, 0);
		if (data == MAP_FAILED) {
			logger::instance().perror("mmap");

			errno = EIO;
			return -1;
		}

		ssize_t ret = socket_wrapper::write(out_fd, (const char*) data + *offset, count);

		// Save 'errno'.
		int error = errno;

		munmap(data, filesize);

		if (ret > 0) {
			*offset += ret;
		}

		// Restore 'errno'.
		errno = error;

		return ret;
	}
#else
	#include "socket_wrapper.h"
	#include "macros/macros.h"

	ssize_t filesender::sendfile(int out_fd, int in_fd, off_t *offset, size_t count, off_t filesize)
	{
		static const size_t READ_BUFFER_SIZE = 8 * 1024;

		size_t sent = 0;

	#if !HAVE_PREAD
		if (lseek(in_fd, *offset, SEEK_SET) != *offset) {
			logger::instance().perror("lseek");

			errno = EIO;
			return -1;
		}
	#endif // !HAVE_PREAD

		do {
			char buf[READ_BUFFER_SIZE];

			ssize_t bytes = MIN(sizeof(buf), count - sent);

	#if HAVE_PREAD
			bytes = pread(in_fd, buf, bytes, *offset);
	#else
			bytes = read(in_fd, buf, bytes);
	#endif

			if (bytes < 0) {
		#if HAVE_PREAD
				logger::instance().perror("pread");
		#else
				logger::instance().perror("read");
		#endif

				errno = EIO;
				return -1;
			} else if (bytes == 0) {
				logger::instance().log(logger::LOG_WARNING, "No data read from file descriptor %d.", in_fd);

				errno = EIO;
				return -1;
			}

			ssize_t ret = socket_wrapper::write(out_fd, buf, bytes);
			if (ret < 0) {
				return -1;
			} else if (ret == 0) {
				return sent;
			} else {
				sent += ret;
				*offset += ret;

				if (ret < bytes) {
					return sent;
				}
			}
		} while (sent < count);

		return sent;
	}
#endif
