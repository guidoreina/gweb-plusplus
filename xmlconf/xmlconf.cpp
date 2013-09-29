#include <stdlib.h>
#include <string.h>
#include "xmlconf.h"
#include "util/number.h"

bool xmlconf::get_value(const char*& value, size_t& len, ...) const
{
	va_list ap;
	va_start(ap, len);

	xml_document::iterator it;
	if (!find_node(ap, it)) {
		va_end(ap);
		return false;
	}

	va_end(ap);

	return get_value(it, value, len);
}

bool xmlconf::get_value(unsigned& value, ...) const
{
	va_list ap;
	va_start(ap, value);

	xml_document::iterator it;
	if (!find_node(ap, it)) {
		va_end(ap);
		return false;
	}

	va_end(ap);

	const char* v;
	size_t l;

	if (!get_value(it, v, l)) {
		return false;
	}

	return (number::parse_unsigned(v, l, value) == number::PARSE_SUCCEEDED);
}

#if _LP64
bool xmlconf::get_value(size_t& value, ...) const
{
	va_list ap;
	va_start(ap, value);

	xml_document::iterator it;
	if (!find_node(ap, it)) {
		va_end(ap);
		return false;
	}

	va_end(ap);

	const char* v;
	size_t l;

	if (!get_value(it, v, l)) {
		return false;
	}

	return (number::parse_size_t(v, l, value) == number::PARSE_SUCCEEDED);
}
#endif // _LP64

bool xmlconf::get_value(bool& value, ...) const
{
	va_list ap;
	va_start(ap, value);

	xml_document::iterator it;
	if (!find_node(ap, it)) {
		va_end(ap);
		return false;
	}

	va_end(ap);

	const char* v;
	size_t l;

	if (!get_value(it, v, l)) {
		return false;
	}

	if (l == 1) {
		if (*v == '1') {
			value = true;
			return true;
		} else if (*v == '0') {
			value = false;
			return true;
		}
	} else if (l == 2) {
		if (((*v == 'N') || (*v == 'n')) && ((*(v + 1) == 'o') || (*(v + 1) == 'O'))) {
			value = false;
			return true;
		}
	} else if (l == 3) {
		if (((*v == 'Y') || (*v == 'y')) && ((*(v + 1) == 'e') || (*(v + 1) == 'E')) && ((*(v + 2) == 's') || (*(v + 2) == 'S'))) {
			value = true;
			return true;
		}
	} else if (l == 4) {
		if (((*v == 'T') || (*v == 't')) && ((*(v + 1) == 'r') || (*(v + 1) == 'R')) && ((*(v + 2) == 'u') || (*(v + 2) == 'U')) && ((*(v + 3) == 'e') || (*(v + 3) == 'E'))) {
			value = true;
			return true;
		}
	} else if (l == 5) {
		if (((*v == 'F') || (*v == 'f')) && ((*(v + 1) == 'a') || (*(v + 1) == 'A')) && ((*(v + 2) == 'l') || (*(v + 2) == 'L')) && ((*(v + 3) == 's') || (*(v + 3) == 'S')) && ((*(v + 4) == 'e') || (*(v + 4) == 'E'))) {
			value = false;
			return true;
		}
	}

	return false;
}

bool xmlconf::get_child(unsigned idx, const char*& value, size_t& len, ...) const
{
	va_list ap;
	va_start(ap, len);

	xml_document::iterator it;
	if (!find_node(ap, it)) {
		va_end(ap);
		return false;
	}

	va_end(ap);

	if (!_M_doc.child(it)) {
		return false;
	}

	if (it.node.type == xml_document::TEXT_NODE) {
		return get_token(it.node.text, it.node.text + it.node.len, idx, value, len);
	} else if (it.node.type == xml_document::ELEMENT_NODE) {
		unsigned i = 0;
		while (i < idx) {
			if (!_M_doc.next(it)) {
				return false;
			}

			if (it.node.type == xml_document::ELEMENT_NODE) {
				i++;
			} else if ((it.node.type != xml_document::PROCESSING_INSTRUCTION_NODE) && (it.node.type != xml_document::COMMENT_NODE)) {
				return false;
			}
		}

		value = it.node.text;
		len = it.node.len;

		return true;
	} else {
		return false;
	}
}

bool xmlconf::find_node(va_list ap, xml_document::iterator& it) const
{
	if (!_M_doc.begin(it)) {
		return false;
	}

	const char* node;
	while ((node = va_arg(ap, const char*)) != NULL) {
		// If not the first node...
		if (!it.first_node()) {
			if (!_M_doc.child(it)) {
				return false;
			}
		}

		size_t len = strlen(node);

		do {
			xml_document::node_type node_type = it.node.type;

			if (node_type == xml_document::ELEMENT_NODE) {
				if ((it.node.len == len) && (strncasecmp(it.node.text, node, len) == 0)) {
					break;
				}
			} else if ((node_type != xml_document::PROCESSING_INSTRUCTION_NODE) && (node_type != xml_document::COMMENT_NODE)) {
				return false;
			}

			if (!_M_doc.next(it)) {
				return false;
			}
		} while (true);
	}

	return true;
}

bool xmlconf::get_value(const xml_document::iterator& it, const char*& value, size_t& len) const
{
	xml_document::iterator child = it;
	if (!_M_doc.child(child)) {
		return false;
	}

	if (child.node.type != xml_document::TEXT_NODE) {
		return false;
	}

	value = child.node.text;
	len = child.node.len;

	return true;
}

bool xmlconf::get_token(const char* text, const char* end, unsigned idx, const char*& value, size_t& len) const
{
	for (unsigned i = 0; i <= idx; i++) {
		while ((text < end) && ((*text <= ' ') || (*text == ',') || (*text == ';'))) {
			text++;
		}

		if (text == end) {
			return false;
		}

		value = text++;

		while ((text < end) && ((*text > ' ') && (*text != ',') && (*text != ';'))) {
			text++;
		}
	}

	len = text - value;

	return true;
}
