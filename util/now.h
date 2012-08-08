#ifndef NOW_H
#define NOW_H

#include <time.h>

struct now {
	// Update.
	static void update();

	static time_t _M_time;
	static struct tm _M_tm;
};

inline void now::update()
{
	_M_time = time(NULL);
	gmtime_r(&_M_time, &_M_tm);
}

#endif // NOW_H
