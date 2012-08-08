#ifndef LOGGER_H
#define LOGGER_H

#include <sys/types.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include "string/buffer.h"

class logger {
	public:
		enum level {LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_DEBUG};

		static const off_t MAX_SIZE;

		// Instance.
		static logger& instance();

		// Create.
		bool create(level level, const char* dir, const char* filename, off_t max_size = MAX_SIZE);

		// Enable.
		void enable();

		// Disable.
		void disable();

		// Set level.
		void set_level(level level);

		// Log.
		bool log(level level, const char* format, ...);
		bool log(level level, const char* format, va_list ap);

		// perror.
		bool perror(const char* s);
		bool perror(level level, const char* s);

	protected:
		buffer _M_buf;

		char _M_path[PATH_MAX + 1];
		size_t _M_pathlen;

		int _M_fd;

		unsigned _M_last_file;

		bool _M_enabled;

		level _M_level;

		off_t _M_max_size;
		off_t _M_size;

		// Constructor.
		logger();

		// Destructor.
		virtual ~logger();

		bool add(level level, const char* format, ...);
		bool add(level level, const char* format, va_list ap);

		bool rotate();
};

inline logger& logger::instance()
{
	static logger logger;
	return logger;
}

inline void logger::enable()
{
	_M_enabled = true;
}

inline void logger::disable()
{
	_M_enabled = false;
}

inline void logger::set_level(level level)
{
	_M_level = level;
}

inline bool logger::log(level level, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);

	bool ret = log(level, format, ap);

	va_end(ap);

	return ret;
}

inline bool logger::log(level level, const char* format, va_list ap)
{
	if ((_M_enabled) && (level <= _M_level)) {
		if (_M_fd < 0) {
			return false;
		}

		// Save errno.
		int err = errno;

		bool ret = add(level, format, ap);

		// Restore errno.
		errno = err;

		return ret;
	} else {
		return true;
	}
}

inline bool logger::perror(const char* s)
{
	return perror(LOG_ERROR, s);
}

inline bool logger::perror(level level, const char* s)
{
	if ((_M_enabled) && (level <= _M_level)) {
		if (_M_fd < 0) {
			return false;
		}

		// Save errno.
		int err = errno;

		bool ret = add(level, "%s: %s", s, strerror(err));

		// Restore errno.
		errno = err;

		return ret;
	} else {
		return true;
	}
}

#endif // LOGGER_H
