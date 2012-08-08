#ifndef SELECTOR_H
#define SELECTOR_H

#include <port.h>
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

		port_event_t* _M_port_events;

		int* _M_events;

		void process_events(unsigned nevents);
};

#endif // SELECTOR_H
