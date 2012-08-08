#ifndef XMLCONF_H
#define XMLCONF_H

#include <stdarg.h>
#include "xml/xml_document.h"

class xmlconf {
	public:
		// Constructor.
		xmlconf();

		// Destructor.
		virtual ~xmlconf();

		// Load.
		bool load(const char* filename);

		// Get error.
		xml_parser::xml_error get_error() const;

		// Get line.
		unsigned get_line() const;

		// Get offset.
		unsigned get_offset() const;

		// Get value.
		bool get_value(const char*& value, size_t& len, ...) const;
		bool get_value(unsigned& value, ...) const;
		bool get_value(bool& value, ...) const;

		// Get child.
		bool get_child(unsigned idx, const char*& value, size_t& len, ...) const;

	protected:
		xml_document _M_doc;

		// Find node.
		bool find_node(va_list ap, xml_document::iterator& it) const;

		// Get value.
		bool get_value(const xml_document::iterator& it, const char*& value, size_t& len) const;

		// Get token.
		bool get_token(const char* text, const char* end, unsigned idx, const char*& value, size_t& len) const;
};

inline xmlconf::xmlconf()
{
}

inline xmlconf::~xmlconf()
{
}

inline bool xmlconf::load(const char* filename)
{
	return _M_doc.load(filename);
}

inline xml_parser::xml_error xmlconf::get_error() const
{
	return _M_doc.get_error();
}

inline unsigned xmlconf::get_line() const
{
	return _M_doc.get_line();
}

inline unsigned xmlconf::get_offset() const
{
	return _M_doc.get_offset();
}

#endif // XMLCONF_H
