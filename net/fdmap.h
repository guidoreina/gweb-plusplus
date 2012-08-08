#ifndef FDMAP_H
#define FDMAP_H

class fdmap {
	protected:
		int* _M_descriptors;
		size_t _M_size;
		size_t _M_used;

		unsigned* _M_index;

		// Constructor.
		fdmap();

		// Destructor.
		virtual ~fdmap();

		// Create.
		virtual bool create();

		// Add descriptor.
		virtual bool add(unsigned fd);

		// Remove descriptor.
		virtual bool remove(unsigned fd);
};

#endif // FDMAP_H
