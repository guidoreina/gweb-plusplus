#include <stdlib.h>
#include <string.h>
#include <new>
#include "virtual_hosts.h"

const size_t virtual_hosts::VIRTUAL_HOSTS_ALLOC = 8;

virtual_hosts::virtual_hosts() : _M_buf(512)
{
	_M_vhosts = NULL;
	_M_size = 0;
	_M_used = 0;

	_M_index.index = NULL;
	_M_index.size = 0;
	_M_index.used = 0;

	_M_default = -1;
}

void virtual_hosts::free()
{
	_M_buf.free();

	if (_M_vhosts) {
		for (size_t i = 0; i < _M_used; i++) {
			if (_M_vhosts[i].dir_listing) {
				delete _M_vhosts[i].dir_listing;
			}

			if (_M_vhosts[i].log) {
				delete _M_vhosts[i].log;
			}

			delete _M_vhosts[i].rules;
		}

		::free(_M_vhosts);
		_M_vhosts = NULL;
	}

	_M_size = 0;
	_M_used = 0;

	if (_M_index.index) {
		::free(_M_index.index);
		_M_index.index = NULL;
	}

	_M_index.size = 0;
	_M_index.used = 0;

	_M_default = -1;
}

virtual_hosts::vhost* virtual_hosts::add(const char* name, unsigned short namelen, const char* root, unsigned short rootlen, bool have_dirlisting, bool log_requests, bool default_host)
{
	if ((default_host) && (_M_default != -1)) {
		return NULL;
	}

	struct host_index* index;
	if ((index = add_host_index(name, namelen, _M_used)) == NULL) {
		return NULL;
	}

	size_t root_offset = _M_buf.count();

	if (!_M_buf.append_nul_terminated_string(root, rootlen)) {
		return NULL;
	}

	dirlisting* dir_listing;
	if (!have_dirlisting) {
		dir_listing = NULL;
	} else {
		if ((dir_listing = new (std::nothrow) dirlisting()) == NULL) {
			return NULL;
		}
	}

	access_log* log;
	if (!log_requests) {
		log = NULL;
	} else {
		if ((log = new (std::nothrow) access_log()) == NULL) {
			if (dir_listing) {
				delete dir_listing;
			}

			return NULL;
		}
	}

	rulelist* rules;
	if ((rules = new (std::nothrow) rulelist()) == NULL) {
		if (log) {
			delete log;
		}

		if (dir_listing) {
			delete dir_listing;
		}

		return NULL;
	}

	if (_M_used == _M_size) {
		size_t size = _M_size + VIRTUAL_HOSTS_ALLOC;
		struct vhost* vhosts = (struct vhost*) realloc(_M_vhosts, size * sizeof(struct vhost));
		if (!vhosts) {
			delete rules;

			if (log) {
				delete log;
			}

			if (dir_listing) {
				delete dir_listing;
			}

			return NULL;
		}

		_M_vhosts = vhosts;
		_M_size = size;
	}

	struct vhost* vhost = &(_M_vhosts[_M_used]);

	vhost->name = NULL;
	vhost->namelen = namelen;

	vhost->root = NULL;
	vhost->rootlen = rootlen;

	vhost->have_dirlisting = have_dirlisting;

	if ((vhost->dir_listing = dir_listing) != NULL) {
		dir_listing->set_root(root, rootlen);
	}

	vhost->log_requests = log_requests;
	vhost->log = log;

	vhost->rules = rules;

	vhost->_M_name = index->name;
	vhost->_M_root = root_offset;

	if (default_host) {
		_M_default = _M_used;
	}

	_M_used++;

	return vhost;
}

bool virtual_hosts::add_alias(const vhost* vhost, const char* name, unsigned short namelen)
{
	if (vhost < _M_vhosts) {
		return false;
	}

	size_t diff = (const char*) vhost - (const char*) _M_vhosts;
	if ((diff % sizeof(struct vhost)) != 0) {
		return false;
	}

	size_t idx = diff / sizeof(struct vhost);
	if (idx >= _M_used) {
		return false;
	}

	return (add_host_index(name, namelen, idx) != NULL);
}

virtual_hosts::vhost* virtual_hosts::get_host(const char* name, unsigned short namelen)
{
	size_t position;
	if (!search(name, namelen, position)) {
		return NULL;
	}

	struct vhost* vhost = &(_M_vhosts[_M_index.index[position].idx]);

	if (vhost->name) {
		return vhost;
	}

	vhost->name = _M_buf.data() + vhost->_M_name;
	vhost->root = _M_buf.data() + vhost->_M_root;

	return vhost;
}

bool virtual_hosts::search(const char* name, unsigned short namelen, size_t& position) const
{
	int i = 0;
	int j = _M_index.used - 1;

	const char* data = _M_buf.data();

	while (i <= j) {
		int pivot = (i + j) / 2;
		int ret = strncasecmp(name, data + _M_index.index[pivot].name, namelen);

		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (namelen < _M_index.index[pivot].namelen) {
				j = pivot - 1;
			} else if (namelen == _M_index.index[pivot].namelen) {
				position = (size_t) pivot;
				return true;
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	position = (size_t) i;

	return false;
}

virtual_hosts::host_index* virtual_hosts::add_host_index(const char* name, unsigned short namelen, size_t idx)
{
	size_t position;
	if (search(name, namelen, position)) {
		return NULL;
	}

	size_t name_offset = _M_buf.count();

	if (!_M_buf.append_nul_terminated_string(name, namelen)) {
		return NULL;
	}

	if (_M_index.used == _M_index.size) {
		size_t size = _M_index.size + VIRTUAL_HOSTS_ALLOC;
		struct host_index* index = (struct host_index*) realloc(_M_index.index, size * sizeof(struct host_index));
		if (!index) {
			return NULL;
		}

		_M_index.index = index;
		_M_index.size = size;
	}

	if (position < _M_index.used) {
		memmove(&(_M_index.index[position + 1]), &(_M_index.index[position]), (_M_index.used - position) * sizeof(struct host_index));
	}

	struct host_index* index = &(_M_index.index[position]);

	index->name = name_offset;
	index->namelen = namelen;

	index->idx = idx;

	_M_index.used++;

	return index;
}
