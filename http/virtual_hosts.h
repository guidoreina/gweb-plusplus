#ifndef VIRTUAL_HOSTS_H
#define VIRTUAL_HOSTS_H

#include "http/dirlisting.h"
#include "http/access_log.h"
#include "http/rulelist.h"
#include "string/buffer.h"

class virtual_hosts {
	public:
		struct vhost {
			friend class virtual_hosts;

			public:
				const char* name;
				unsigned short namelen;

				const char* root;
				unsigned short rootlen;

				bool have_dirlisting;
				dirlisting* dir_listing;

				bool log_requests;
				access_log* log;

				rulelist* rules;

			private:
				size_t _M_name;
				size_t _M_root;
		};

		// Constructor.
		virtual_hosts();

		// Destructor.
		virtual ~virtual_hosts();

		// Free.
		void free();

		// Reset.
		void reset();

		// Get count.
		size_t count() const;

		// Add virtual host.
		vhost* add(const char* name, unsigned short namelen, const char* root, unsigned short rootlen, bool have_dirlisting, bool log_requests, bool default_host);

		// Add alias.
		bool add_alias(const vhost* vhost, const char* name, unsigned short namelen);

		// Get host.
		vhost* get_host(const char* name, unsigned short namelen);
		vhost* get_host(size_t i);

		// Get default host.
		vhost* get_default_host();

		// Synchronize virtual hosts' access logs.
		void sync();

	protected:
		static const size_t VIRTUAL_HOSTS_ALLOC;

		buffer _M_buf;

		vhost* _M_vhosts;
		size_t _M_size;
		size_t _M_used;

		struct host_index {
			size_t name;
			unsigned short namelen;

			size_t idx;
		};

		struct index {
			struct host_index* index;
			size_t size;
			size_t used;
		};

		struct index _M_index;

		// Index of the default virtual host.
		int _M_default;

		// Search.
		bool search(const char* name, unsigned short namelen, size_t& position) const;

		// Add host index.
		host_index* add_host_index(const char* name, unsigned short namelen, size_t idx);
};

inline virtual_hosts::~virtual_hosts()
{
	free();
}

inline void virtual_hosts::reset()
{
	_M_buf.reset();
	_M_used = 0;

	_M_index.used = 0;

	_M_default = -1;
}

inline size_t virtual_hosts::count() const
{
	return _M_used;
}

inline virtual_hosts::vhost* virtual_hosts::get_host(size_t i)
{
	if (i >= _M_used) {
		return NULL;
	}

	struct vhost* vhost = &(_M_vhosts[i]);

	if (vhost->name) {
		return vhost;
	}

	vhost->name = _M_buf.data() + vhost->_M_name;
	vhost->root = _M_buf.data() + vhost->_M_root;

	return vhost;
}

inline virtual_hosts::vhost* virtual_hosts::get_default_host()
{
	if (_M_default < 0) {
		return NULL;
	}

	struct vhost* vhost = &(_M_vhosts[_M_default]);

	if (vhost->name) {
		return vhost;
	}

	vhost->name = _M_buf.data() + vhost->_M_name;
	vhost->root = _M_buf.data() + vhost->_M_root;

	return vhost;
}

inline void virtual_hosts::sync()
{
	for (size_t i = 0; i < _M_used; i++) {
		if (_M_vhosts[i].log) {
			_M_vhosts[i].log->sync();
		}
	}
}

#endif // VIRTUAL_HOSTS_H
