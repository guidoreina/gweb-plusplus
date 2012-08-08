#ifndef ISELECTOR_H
#define ISELECTOR_H

#include "net/fdmap.h"

class iselector : protected fdmap {
	protected:
		static const unsigned READ;
		static const unsigned WRITE;

		// Constructor.
		iselector();

		// Create.
		virtual bool create() = 0;

		// Add descriptor.
		virtual bool add(unsigned fd, int events, bool modifiable) = 0;

		// Remove descriptor.
		virtual bool remove(unsigned fd) = 0;

		// Modify descriptor.
		virtual bool modify(unsigned fd, int events) = 0;

		// Wait for event.
		virtual bool wait_for_event() = 0;
		virtual bool wait_for_event(unsigned timeout) = 0;

		// On event.
		virtual bool on_event(unsigned fd, int events) = 0;

		// On event error.
		virtual void on_event_error(unsigned fd) = 0;
};

inline iselector::iselector()
{
}

#endif // ISELECTOR_H
