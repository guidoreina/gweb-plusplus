#ifndef SELECTOR_H
#define SELECTOR_H

#include <sys/epoll.h>
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
		int _M_fd;

		struct epoll_event* _M_events;

		void process_events(unsigned nevents);
};

inline bool selector::modify(unsigned fd, int events)
{
	return true;
}

#endif // SELECTOR_H
