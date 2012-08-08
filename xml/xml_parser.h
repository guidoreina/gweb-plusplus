#ifndef XML_PARSER_H
#define XML_PARSER_H

#define ELEMENT_NAME_MAX_LEN   256
#define ATTRIBUTE_NAME_MAX_LEN 256
#define TEXT_BUFFER_SIZE       (4 * 1024)

#include "xml/attribute_list.h"

class xml_parser {
	public:
		enum xml_error {
			NO_ERROR_YET,
			CANNOT_OPEN_FILE,
			ERROR_NO_MEMORY,
			ERROR_PARSING,
			ERROR_CALLBACK,
			PARSE_SUCCEEDED
		};

		// Get error.
		xml_error get_error() const;

		// Get line.
		unsigned get_line() const;

		// Get offset.
		unsigned get_offset() const;

	protected:
		char _M_element[ELEMENT_NAME_MAX_LEN + 1];
		size_t _M_element_len;

		char _M_attribute_name[ATTRIBUTE_NAME_MAX_LEN + 1];
		size_t _M_attribute_name_len;

		char _M_text[TEXT_BUFFER_SIZE];
		size_t _M_len;

		attribute_list _M_attribute_list;

		char _M_last_char;
		char _M_quotes;

		unsigned _M_state;
		unsigned _M_depth;
		bool _M_has_root;

		xml_error _M_error;
		unsigned _M_line;
		unsigned _M_offset;

		// Constructor.
		xml_parser();

		// Destructor.
		virtual ~xml_parser();

		// Parse file.
		bool parse_file(const char* filename);

		// Parse chunk.
		bool parse_chunk(const char* data, size_t len);

		// End of chunk.
		bool end_of_chunk();

		// Get depth.
		unsigned get_depth() const;

		// Has root.
		bool has_root() const;

		// On DOCTYPE.
		virtual bool on_doctype(const char* doctype, size_t len) = 0;

		// On processing instruction.
		virtual bool on_processing_instruction(const char* processing_instruction, size_t len) = 0;

		// On element start.
		virtual bool on_element_start(const char* element, size_t len, const attribute_list& attribute_list) = 0;

		// On element end.
		virtual bool on_element_end(const char* element, size_t len) = 0;

		// On empty element.
		virtual bool on_empty_element(const char* element, size_t len, const attribute_list& attribute_list) = 0;

		// On character data.
		virtual bool on_character_data(const char* data, size_t len) = 0;

		// On comment.
		virtual bool on_comment(const char* comment, size_t len) = 0;
};

inline xml_parser::~xml_parser()
{
}

inline xml_parser::xml_error xml_parser::get_error() const
{
	return _M_error;
}

inline unsigned xml_parser::get_line() const
{
	return _M_line;
}

inline unsigned xml_parser::get_offset() const
{
	return _M_offset;
}

inline bool xml_parser::end_of_chunk()
{
	if ((_M_state == 0) || (_M_state == 45)) {
		_M_error = PARSE_SUCCEEDED;
		return true;
	} else {
		_M_error = ERROR_PARSING;
		return false;
	}
}

inline unsigned xml_parser::get_depth() const
{
	return _M_depth;
}

inline bool xml_parser::has_root() const
{
	return _M_has_root;
}

#endif // XML_PARSER_H
