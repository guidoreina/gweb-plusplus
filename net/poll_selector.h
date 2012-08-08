#ifndef SELECTOR_H
#define SELECTOR_H

#include <poll.h>
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
		struct pollfd* _M_events;

		void process_events(unsigned nevents);
};

#endif // SELECTOR_H
