#include <stdlib.h>
#include "rulelist.h"

const unsigned char rulelist::HTTP_HANDLER = 0;
const unsigned char rulelist::FCGI_HANDLER = 1;
const unsigned char rulelist::LOCAL_HANDLER = 2;

const unsigned char rulelist::BY_PATH = 0;
const unsigned char rulelist::BY_EXTENSION = 1;
const unsigned char rulelist::BY_METHOD = 2;

const size_t rulelist::RULE_ALLOC = 4;

bool rulelist::rule::match(unsigned char method, const char* path, unsigned short pathlen, const char* extension, unsigned short extensionlen) const
{
	switch (criterion) {
		case BY_PATH:
			return paths.exists(path, pathlen);
		case BY_EXTENSION:
			if (extensionlen == 0) {
				return false;
			} else {
				return extensions.exists(extension, extensionlen);
			}
		default:
			return ((methods & (1 << method)) != 0);
	}
}

rulelist::rulelist()
{
	_M_rules = NULL;
	_M_size = 0;
	_M_used = 0;

	_M_default.handler = LOCAL_HANDLER;
}

rulelist::~rulelist()
{
	if (_M_rules) {
		for (size_t i = 0; i < _M_used; i++) {
			delete _M_rules[i];
		}

		free(_M_rules);
	}
}

bool rulelist::add(rule* rule)
{
	if (_M_used == _M_size) {
		size_t size = _M_size + RULE_ALLOC;
		struct rule** rules = (struct rule**) realloc(_M_rules, size * sizeof(struct rule*));
		if (!rules) {
			return false;
		}

		_M_rules = rules;
		_M_size = size;
	}

	_M_rules[_M_used++] = rule;

	return true;
}
