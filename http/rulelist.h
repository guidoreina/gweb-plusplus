#ifndef RULELIST_H
#define RULELIST_H

#include "util/pathlist.h"
#include "util/string_list.h"
#include "http/http_method.h"
#include "http/backend_list.h"

class rulelist {
	public:
		static const unsigned char HTTP_HANDLER;
		static const unsigned char FCGI_HANDLER;
		static const unsigned char LOCAL_HANDLER;

		static const unsigned char BY_PATH;
		static const unsigned char BY_EXTENSION;
		static const unsigned char BY_METHOD;

		struct rule {
			unsigned char handler;
			unsigned char criterion;

			pathlist paths;
			string_list extensions;
			unsigned methods;

			backend_list backends;

			// Constructor.
			rule();

			// Destructor.
			~rule();

			// Add method.
			void add(unsigned char method);

			// Match.
			bool match(unsigned char method, const char* path, unsigned short pathlen, const char* extension, unsigned short extensionlen) const;
		};

		// Constructor.
		rulelist();

		// Destructor.
		virtual ~rulelist();

		// Add rule.
		bool add(rule* rule);

		// Get rule.
		rule* get(size_t i);

		// Find rule.
		rule* find(unsigned char method, const char* path, unsigned short pathlen, const char* extension, unsigned short extensionlen);

	protected:
		static const size_t RULE_ALLOC;

		rule** _M_rules;
		size_t _M_size;
		size_t _M_used;

		rule _M_default;
};

inline rulelist::rule::rule()
{
	methods = 0;
}

inline rulelist::rule::~rule()
{
}

inline void rulelist::rule::add(unsigned char method)
{
	methods |= (1 << method);
}

inline rulelist::rule* rulelist::get(size_t i)
{
	if (i >= _M_used) {
		return NULL;
	}

	return _M_rules[i];
}

inline rulelist::rule* rulelist::find(unsigned char method, const char* path, unsigned short pathlen, const char* extension, unsigned short extensionlen)
{
	for (size_t i = 0; i < _M_used; i++) {
		if (_M_rules[i]->match(method, path, pathlen, extension, extensionlen)) {
			return _M_rules[i];
		}
	}

	return &_M_default;
}

#endif // RULELIST_H
