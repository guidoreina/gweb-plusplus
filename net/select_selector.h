#ifndef SELECTOR_H
#define SELECTOR_H

#include <sys/select.h>
#include "net/iselector.h"

class selector : protected iselector {
	protected:
		// Constructor.
		selector();

		// Destructor.
		virtual ~selector();

		// Create.
		virtual bool create();

		// Add descriptor.
		bool add(unsigned fd, int events, bool modifiable);

		// Remove descriptor.
		bool remove(unsigned fd);

		// Modify descriptor.
		bool modify(unsigned fd, int events);

		// Wait for event.
		bool wait_for_event();
		bool wait_for_event(unsigned timeout);

	private:
		static const int EMPTY_SET;
		static const int UNDEFINED;

		fd_set _M_rfds;
		fd_set _M_wfds;

		int _M_highest_fd;

		void process_events(const fd_set* rfds, const fd_set* wfds, unsigned nevents);

		int compute_highest_fd();
};

inline bool selector::create()
{
	return fdmap::create();
}

inline int selector::compute_highest_fd()
{
	if (_M_highest_fd == EMPTY_SET) {
		return -1;
	} else {
		if (_M_highest_fd == UNDEFINED) {
			for (size_t i = 0; i < _M_used; i++) {
				if ((int) _M_index[i] > _M_highest_fd) {
					_M_highest_fd = _M_index[i];
				}
			}
		}

		return _M_highest_fd;
	}
}

#endif // SELECTOR_H
