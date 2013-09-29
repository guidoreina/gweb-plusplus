#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <new>
#include "xml_document.h"

const off_t xml_document::MAX_FILE_SIZE = 1 * 1024 * 1024;
const size_t xml_document::NODE_ALLOC = 100;

xml_document::xml_document() : _M_buf(10 * 1024)
{
	_M_nodes = NULL;
	_M_size = 0;
	_M_used = 0;

	_M_parent = -1;
	_M_previous = -1;
	_M_last = -1;

	_M_reason = NO_REASON;
}

void xml_document::free()
{
	_M_buf.free();

	if (_M_nodes) {
		for (size_t i = 0; i < _M_used; i++) {
			if (_M_nodes[i].attributes) {
				delete _M_nodes[i].attributes;
			}
		}

		::free(_M_nodes);
		_M_nodes = NULL;
	}

	_M_size = 0;
	_M_used = 0;

	_M_parent = -1;
	_M_previous = -1;
	_M_last = -1;

	_M_reason = NO_REASON;
}

bool xml_document::load(const char* filename)
{
	struct stat buf;
	if ((stat(filename, &buf) < 0) || (!S_ISREG(buf.st_mode))) {
		_M_error = CANNOT_OPEN_FILE;
		return false;
	}

	if (buf.st_size > MAX_FILE_SIZE) {
		_M_error = CANNOT_OPEN_FILE;
		_M_reason = ERROR_FILE_TOO_BIG;
		return false;
	}

	return parse_file(filename);
}

bool xml_document::begin(iterator& it) const
{
	if (_M_used == 0) {
		return false;
	}

	it.idx = 0;

	struct node* node = &(_M_nodes[it.idx]);

	it.node.type = node->type;

	it.node.text = _M_buf.data() + node->text;
	it.node.len = node->len;

	it.node.attributes = node->attributes;

	return true;
}

bool xml_document::end(iterator& it) const
{
	if (_M_used == 0) {
		return false;
	}

	it.idx = _M_last;

	struct node* node = &(_M_nodes[it.idx]);

	it.node.type = node->type;

	it.node.text = _M_buf.data() + node->text;
	it.node.len = node->len;

	it.node.attributes = node->attributes;

	return true;
}

bool xml_document::next(iterator& it) const
{
	if ((it.idx < 0) || (it.idx >= (int) _M_used)) {
		return false;
	}

	if ((it.idx = _M_nodes[it.idx].next) < 0) {
		return false;
	}

	struct node* node = &(_M_nodes[it.idx]);

	it.node.type = node->type;

	it.node.text = _M_buf.data() + node->text;
	it.node.len = node->len;

	it.node.attributes = node->attributes;

	return true;
}

bool xml_document::previous(iterator& it) const
{
	if ((it.idx < 0) || (it.idx >= (int) _M_used)) {
		return false;
	}

	if ((it.idx = _M_nodes[it.idx].previous) < 0) {
		return false;
	}

	struct node* node = &(_M_nodes[it.idx]);

	it.node.type = node->type;

	it.node.text = _M_buf.data() + node->text;
	it.node.len = node->len;

	it.node.attributes = node->attributes;

	return true;
}

bool xml_document::child(iterator& it) const
{
	if ((it.idx < 0) || (it.idx >= (int) _M_used)) {
		return false;
	}

	if ((it.idx = _M_nodes[it.idx].child) < 0) {
		return false;
	}

	struct node* node = &(_M_nodes[it.idx]);

	it.node.type = node->type;

	it.node.text = _M_buf.data() + node->text;
	it.node.len = node->len;

	it.node.attributes = node->attributes;

	return true;
}

bool xml_document::last_child(iterator& it) const
{
	if ((it.idx < 0) || (it.idx >= (int) _M_used)) {
		return false;
	}

	if ((it.idx = _M_nodes[it.idx].last_child) < 0) {
		return false;
	}

	struct node* node = &(_M_nodes[it.idx]);

	it.node.type = node->type;

	it.node.text = _M_buf.data() + node->text;
	it.node.len = node->len;

	it.node.attributes = node->attributes;

	return true;
}

bool xml_document::to_string(buffer& buf, bool pretty_print, bool nul_terminate) const
{
	if (_M_used == 0) {
		return nul_terminate ? buf.append((char) 0) : true;
	}

	if (!to_string(buf, pretty_print, _M_nodes, 0)) {
		return false;
	}

	return nul_terminate ? buf.append((char) 0) : true;
}

bool xml_document::on_doctype(const char* doctype, size_t len)
{
	if (!create_node(DOCTYPE_NODE, doctype, len, NULL, _M_used)) {
		_M_reason = ERROR_NO_MEMORY;
		return false;
	}

	return true;
}

bool xml_document::on_processing_instruction(const char* processing_instruction, size_t len)
{
	if (!create_node(PROCESSING_INSTRUCTION_NODE, processing_instruction, len, NULL, _M_used)) {
		_M_reason = ERROR_NO_MEMORY;
		return false;
	}

	return true;
}

bool xml_document::on_element_start(const char* element, size_t len, const attribute_list& attributes)
{
	attribute_list* dest;
	if (attributes.count() > 0) {
		if ((dest = new (std::nothrow) attribute_list()) == NULL) {
			_M_reason = ERROR_NO_MEMORY;
			return false;
		}

		if (!copy_attribute_list(dest, &attributes)) {
			delete dest;

			_M_reason = ERROR_NO_MEMORY;
			return false;
		}
	} else {
		dest = NULL;
	}

	if (!create_node(ELEMENT_NODE, element, len, dest, -1)) {
		if (dest) {
			delete dest;
		}

		_M_reason = ERROR_NO_MEMORY;
		return false;
	}

	_M_parent = _M_used - 1;

	return true;
}

bool xml_document::on_element_end(const char* element, size_t len)
{
	struct node* node = &(_M_nodes[_M_parent]);

	if ((node->len != len) || (strncasecmp(_M_buf.data() + node->text, element, len) != 0)) {
		_M_reason = ERROR_PARSING;
		return false;
	}

	_M_previous = _M_parent;

	_M_parent = node->parent;

	return true;
}

bool xml_document::on_empty_element(const char* element, size_t len, const attribute_list& attributes)
{
	attribute_list* dest;
	if (attributes.count() > 0) {
		if ((dest = new (std::nothrow) attribute_list()) == NULL) {
			_M_reason = ERROR_NO_MEMORY;
			return false;
		}

		if (!copy_attribute_list(dest, &attributes)) {
			delete dest;

			_M_reason = ERROR_NO_MEMORY;
			return false;
		}
	} else {
		dest = NULL;
	}

	if (!create_node(ELEMENT_NODE, element, len, dest, _M_used)) {
		if (dest) {
			delete dest;
		}

		_M_reason = ERROR_NO_MEMORY;
		return false;
	}

	return true;
}

bool xml_document::on_character_data(const char* data, size_t len)
{
	if (!create_node(TEXT_NODE, data, len, NULL, _M_used)) {
		_M_reason = ERROR_NO_MEMORY;
		return false;
	}

	return true;
}

bool xml_document::on_comment(const char* comment, size_t len)
{
	if (!create_node(COMMENT_NODE, comment, len, NULL, _M_used)) {
		_M_reason = ERROR_NO_MEMORY;
		return false;
	}

	return true;
}

bool xml_document::to_string(buffer& buf, bool pretty_print, const struct node* node, unsigned depth) const
{
	do {
		if (pretty_print) {
			// If not the first node...
			if (node != _M_nodes) {
				if (!buf.append("\r\n", 2)) {
					return false;
				}
			}
		}

		if (!node_to_string(buf, pretty_print, node, depth)) {
			return false;
		}

		if (node->child != -1) {
			if (!to_string(buf, pretty_print, &(_M_nodes[node->child]), depth + 1)) {
				return false;
			}

			if (pretty_print) {
				if (!buf.append("\r\n", 2)) {
					return false;
				}

				for (unsigned i = 0; i < depth; i++) {
					if (!buf.append('\t')) {
						return false;
					}
				}
			}

			if (!buf.format("</%.*s>", node->len, _M_buf.data() + node->text)) {
				return false;
			}
		}

		if (node->next < 0) {
			return true;
		}

		node = &(_M_nodes[node->next]);
	} while (true);
}

bool xml_document::node_to_string(buffer& buf, bool pretty_print, const struct node* node, unsigned depth) const
{
	if (pretty_print) {
		for (unsigned i = 0; i < depth; i++) {
			if (!buf.append('\t')) {
				return false;
			}
		}
	}

	switch (node->type) {
		case DOCTYPE_NODE:
			return buf.format("<!DOCTYPE %.*s>", node->len, _M_buf.data() + node->text);
		case PROCESSING_INSTRUCTION_NODE:
			return buf.format("<?%.*s?>", node->len, _M_buf.data() + node->text);
		case COMMENT_NODE:
			return buf.format("<!-- %.*s -->", node->len, _M_buf.data() + node->text);
		case ELEMENT_NODE:
			if (!buf.format("<%.*s", node->len, _M_buf.data() + node->text)) {
				return false;
			}

			if (node->attributes) {
				const char* name;
				size_t namelen;
				const char* value;
				size_t valuelen;

				for (size_t i = 0; node->attributes->get(i, name, namelen, value, valuelen); i++) {
					char quotes = (memchr(value, '"', valuelen)) ? '\'' : '"';
					if (!buf.format(" %.*s=%c%.*s%c", namelen, name, quotes, valuelen, value, quotes)) {
						return false;
					}
				}
			}

			if (node->child > 0) {
				return buf.append('>');
			} else {
				return buf.append("/>", 2);
			}

		case TEXT_NODE:
			return buf.format("%.*s", node->len, _M_buf.data() + node->text);
		default:
			return false;
	}
}

bool xml_document::create_node(node_type type, const char* text, size_t len, attribute_list* attributes, int previous)
{
	if ((_M_used > 0) && (_M_nodes[_M_used - 1].parent == _M_parent)) {
		if (((type == TEXT_NODE) && (_M_nodes[_M_used - 1].type == TEXT_NODE)) || ((type == COMMENT_NODE) && (_M_nodes[_M_used - 1].type == COMMENT_NODE))) {
			if (!_M_buf.allocate(len)) {
				return false;
			}

			struct node* node = &(_M_nodes[_M_used - 1]);

			memcpy(_M_buf.data() + node->text + node->len, text, len);
			node->len += len;
			_M_buf.data()[node->text + node->len] = 0;

			_M_buf.increment_count(len);

			return true;
		}
	}

	size_t offset = _M_buf.count();

	if (!_M_buf.append_nul_terminated_string(text, len)) {
		return false;
	}

	if (!allocate()) {
		return false;
	}

	struct node* node = &(_M_nodes[_M_used]);

	node->type = type;

	node->text = offset;
	node->len = len;

	node->attributes = attributes;

	if ((node->parent = _M_parent) != -1) {
		if (_M_nodes[_M_parent].child < 0) {
			_M_nodes[_M_parent].child = _M_used;
		}

		_M_nodes[_M_parent].last_child = _M_used;
	}

	node->child = -1;
	node->last_child = -1;

	node->next = -1;

	if ((node->previous = _M_previous) != -1) {
		_M_nodes[_M_previous].next = _M_used;
	}

	_M_previous = previous;

	if (_M_depth == 0) {
		_M_last = _M_used;
	}

	_M_used++;

	return true;
}

bool xml_document::allocate()
{
	if (_M_used == _M_size) {
		size_t size = _M_size + NODE_ALLOC;
		struct node* nodes = (struct node*) realloc(_M_nodes, size * sizeof(struct node));
		if (!nodes) {
			return false;
		}

		_M_nodes = nodes;
		_M_size = size;
	}

	return true;
}

bool xml_document::copy_attribute_list(attribute_list* dest, const attribute_list* src)
{
	const char* name;
	size_t namelen;
	const char* value;
	size_t valuelen;

	for (size_t i = 0; src->get(i, name, namelen, value, valuelen); i++) {
		if (!dest->add(name, namelen, value, valuelen)) {
			return false;
		}
	}

	return true;
}
