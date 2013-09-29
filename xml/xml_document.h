#ifndef XML_DOCUMENT_H
#define XML_DOCUMENT_H

#include "xml/xml_parser.h"
#include "string/buffer.h"

class xml_document : public xml_parser {
	public:
		// Node type.
		enum node_type {DOCTYPE_NODE, PROCESSING_INSTRUCTION_NODE, COMMENT_NODE, ELEMENT_NODE, TEXT_NODE};

		// Reason.
		enum reason {NO_REASON, ERROR_FILE_TOO_BIG, ERROR_NO_MEMORY, ERROR_PARSING};

		struct iterator {
			public:
				friend class xml_document;

				bool first_node() const;

				struct {
					node_type type;

					const char* text;
					size_t len;

					const attribute_list* attributes;
				} node;

			private:
				int idx;
		};

		// Constructor.
		xml_document();

		// Destructor.
		virtual ~xml_document();

		// Free object.
		void free();

		// Load file.
		virtual bool load(const char* filename);

		// Begin.
		virtual bool begin(iterator& it) const;

		// End.
		virtual bool end(iterator& it) const;

		// Next.
		virtual bool next(iterator& it) const;

		// Previous.
		virtual bool previous(iterator& it) const;

		// Child.
		virtual bool child(iterator& it) const;

		// Last child.
		virtual bool last_child(iterator& it) const;

		// Get reason.
		reason get_reason() const;

		// To string.
		virtual bool to_string(buffer& buf, bool pretty_print, bool nul_terminate = true) const;

	protected:
		static const off_t MAX_FILE_SIZE;

		static const size_t NODE_ALLOC;

		buffer _M_buf;

		struct node {
			node_type type;

			size_t text;
			size_t len;

			attribute_list* attributes;

			int parent;
			int child;
			int last_child;

			int next;
			int previous;
		};

		struct node* _M_nodes;
		size_t _M_size;
		size_t _M_used;

		int _M_parent;
		int _M_previous;
		int _M_last;

		reason _M_reason;

		// On DOCTYPE.
		bool on_doctype(const char* doctype, size_t len);

		// On processing instruction.
		bool on_processing_instruction(const char* processing_instruction, size_t len);

		// On element start.
		bool on_element_start(const char* element, size_t len, const attribute_list& attributes);

		// On element end.
		bool on_element_end(const char* element, size_t len);

		// On empty element.
		bool on_empty_element(const char* element, size_t len, const attribute_list& attributes);

		// On character data.
		bool on_character_data(const char* data, size_t len);

		// On comment.
		bool on_comment(const char* comment, size_t len);

		// To string.
		virtual bool to_string(buffer& buf, bool pretty_print, const struct node* node, unsigned depth) const;
		virtual bool node_to_string(buffer& buf, bool pretty_print, const struct node* node, unsigned depth) const;

	private:
		bool create_node(node_type type, const char* text, size_t len, attribute_list* attributes, int previous);

		bool allocate();

		bool copy_attribute_list(attribute_list* dest, const attribute_list* src);
};

inline bool xml_document::iterator::first_node() const
{
	return idx == 0;
}

inline xml_document::~xml_document()
{
	free();
}

inline xml_document::reason xml_document::get_reason() const
{
	return _M_reason;
}

#endif // XML_DOCUMENT_H
