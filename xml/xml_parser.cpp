#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "xml_parser.h"

#define IS_CHAR(x)            (                                 \
                                ((x) == 0x09) ||                \
                                ((x) == 0x0a) ||                \
                                ((x) == 0x0d) ||                \
                               (((x) >= 0x20) && ((x) <= 0xff)))

#define IS_WHITE_SPACE(x)     (                 \
                               ((x) == 0x20) || \
                               ((x) == 0x09) || \
                               ((x) == 0x0d) || \
                               ((x) == 0x0a))

#if !defined(ALLOW_DIGITS_AS_NAME_START_CHAR)
    #define IS_NAME_START_CHAR(x) (                                    \
                                    ((x) == ':')                    || \
                                   (((x) >= 'A') && ((x) <= 'Z'))   || \
                                    ((x) == '_')                    || \
                                   (((x) >= 'a') && ((x) <= 'z'))   || \
                                   (((x) >= 0xc0) && ((x) <= 0xd6)) || \
                                   (((x) >= 0xd8) && ((x) <= 0xf6)) || \
                                   (((x) >= 0xf8) && ((x) <= 0xff)))
#else
    #define IS_NAME_START_CHAR(x) (                                    \
                                    ((x) == ':')                    || \
                                   (((x) >= 'A') && ((x) <= 'Z'))   || \
                                    ((x) == '_')                    || \
                                   (((x) >= 'a') && ((x) <= 'z'))   || \
                                   (((x) >= '0') && ((x) <= '9'))   || \
                                   (((x) >= 0xc0) && ((x) <= 0xd6)) || \
                                   (((x) >= 0xd8) && ((x) <= 0xf6)) || \
                                   (((x) >= 0xf8) && ((x) <= 0xff)))
#endif // !defined(ALLOW_DIGITS_AS_NAME_START_CHAR)

#define IS_NAME_CHAR(x)       (                                  \
                                (IS_NAME_START_CHAR((x)))     || \
                                ((x) == '-')                  || \
                                ((x) == '.')                  || \
                               (((x) >= '0') && ((x) <= '9')) || \
                                ((x) == 0xb7))

xml_parser::xml_parser()
{
	*_M_element = 0;
	_M_element_len = 0;

	*_M_attribute_name = 0;
	_M_attribute_name_len = 0;

	*_M_text = 0;
	_M_len = 0;

	_M_last_char = 0;
	_M_quotes = 0;

	_M_state = 0;
	_M_depth = 0;
	_M_has_root = false;

	_M_error = NO_ERROR_YET;
	_M_line = 1;
	_M_offset = 1;
}

bool xml_parser::parse_file(const char* filename)
{
	int fd;
	if ((fd = open(filename, O_RDONLY)) < 0) {
		_M_error = CANNOT_OPEN_FILE;
		return false;
	}

	char buffer[10 * 1024];
	ssize_t ret;
	while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
		if (!parse_chunk(buffer, ret)) {
			close(fd);
			return false;
		}
	}

	close(fd);

	return end_of_chunk();
}

bool xml_parser::parse_chunk(const char* data, size_t len)
{
	const char* ptr = data;
	const char* end = data + len;

	while (ptr < end) {
		unsigned c = *ptr++;

		switch (_M_state) {
			case 0: // Initial state.
				if (c == '<') {
					_M_state = 1; // After '<'
				} else if (!IS_WHITE_SPACE(c)) {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 1: // After '<'
				if (IS_NAME_START_CHAR(c)) {
					_M_element[0] = c;
					_M_element_len = 1;

					_M_has_root = true;

					_M_state = 21; // Start tag.
				} else if (c == '!') {
					_M_state = 2; // DOCTYPE, comment or conditional section.
				} else if (c == '?') {
					_M_len = 0;

					_M_last_char = 0;

					_M_state = 19; // Processing instruction.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 2: // DOCTYPE, comment or conditional section.
				if (c == '-') {
					_M_state = 3; // <!-- => First -
				} else if ((c == 'D') || (c == 'd')) {
					_M_state = 7; // <!DOCTYPE => D
				} else if (c == '[') {
					_M_state = 16; // <![<data>]]> => <data>
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 3: // <!-- => First -
				if (c == '-') {
					_M_len = 0;

					_M_last_char = 0;

					_M_state = 4; // Comment.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 4: // Comment.
				if (c == '-') {
					_M_state = 5; // --> => First -
				} else if (IS_CHAR(c)) {
					if (!IS_WHITE_SPACE(c)) {
						if (_M_len + 2 > sizeof(_M_text)) {
							if (!on_comment(_M_text, _M_len)) {
								_M_error = ERROR_CALLBACK;
								return false;
							}

							_M_len = 0;
						}

						if ((IS_WHITE_SPACE(_M_last_char)) && (_M_len > 0)) {
							_M_text[_M_len++] = ' ';
						}

						_M_text[_M_len++] = c;
					}

					_M_last_char = c;
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 5: // --> => First -
				if (c == '-') {
					_M_state = 6; // --> => Second -
				} else if (IS_CHAR(c)) {
					if (_M_len + 3 > sizeof(_M_text)) {
						if (!on_comment(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}

						_M_len = 0;
					}

					if ((IS_WHITE_SPACE(_M_last_char)) && (_M_len > 0)) {
						_M_text[_M_len++] = ' ';
					}

					_M_text[_M_len++] = '-';

					if (!IS_WHITE_SPACE(c)) {
						_M_text[_M_len++] = c;
					}

					_M_last_char = c;

					_M_state = 4; // Comment.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 6: // --> => Second -
				if (c == '>') {
					if (_M_len > 0) {
						if (!on_comment(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}
					}

					if (!_M_has_root) {
						_M_state = 0; // Initial state.
					} else {
						if (_M_depth > 0) {
							_M_len = 0;

							_M_last_char = 0;

							_M_state = 30; // Character data.
						} else {
							_M_state = 45; // Misc.
						}
					}
				} else if (c == '-') {
					if (_M_len + 2 > sizeof(_M_text)) {
						if (!on_comment(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}

						_M_len = 0;
					}

					if ((IS_WHITE_SPACE(_M_last_char)) && (_M_len > 0)) {
						_M_text[_M_len++] = ' ';
					}

					_M_text[_M_len++] = '-';

					_M_last_char = '-';
				} else if (IS_CHAR(c)) {
					if (_M_len + 4 > sizeof(_M_text)) {
						if (!on_comment(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}

						_M_len = 0;
					}

					if ((IS_WHITE_SPACE(_M_last_char)) && (_M_len > 0)) {
						_M_text[_M_len++] = ' ';
					}

					_M_text[_M_len++] = '-';
					_M_text[_M_len++] = '-';

					if (!IS_WHITE_SPACE(c)) {
						_M_text[_M_len++] = c;
					}

					_M_last_char = c;

					_M_state = 4; // Comment.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 7: // <!DOCTYPE => D
				if ((c == 'O') || (c == 'o')) {
					_M_state = 8; // <!DOCTYPE => O
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 8: // <!DOCTYPE => O
				if ((c == 'C') || (c == 'c')) {
					_M_state = 9; // <!DOCTYPE => C
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 9: // <!DOCTYPE => C
				if ((c == 'T') || (c == 't')) {
					_M_state = 10; // <!DOCTYPE => T
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 10: // <!DOCTYPE => T
				if ((c == 'Y') || (c == 'y')) {
					_M_state = 11; // <!DOCTYPE => Y
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 11: // <!DOCTYPE => Y
				if ((c == 'P') || (c == 'p')) {
					_M_state = 12; // <!DOCTYPE => P
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 12: // <!DOCTYPE => P
				if ((c == 'E') || (c == 'e')) {
					_M_state = 13; // <!DOCTYPE => E
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 13: // <!DOCTYPE => E
				if (IS_WHITE_SPACE(c)) {
					_M_state = 14; // Space after <!DOCTYPE
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 14: // Space after <!DOCTYPE
				if (IS_NAME_START_CHAR(c)) {
					_M_text[0] = c;
					_M_len = 1;

					_M_last_char = c;

					_M_state = 15; // Doctype.
				} else if (!IS_WHITE_SPACE(c)) {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 15: // Doctype.
				if (c == '>') {
					if (!on_doctype(_M_text, _M_len)) {
						_M_error = ERROR_CALLBACK;
						return false;
					}

					_M_state = 0; // Initial state.
				} else if (IS_CHAR(c)) {
					if (!IS_WHITE_SPACE(c)) {
						if (_M_len + 2 > sizeof(_M_text)) {
							if (!on_doctype(_M_text, _M_len)) {
								_M_error = ERROR_CALLBACK;
								return false;
							}

							_M_len = 0;
						}

						if ((IS_WHITE_SPACE(_M_last_char)) && (_M_len > 0)) {
							_M_text[_M_len++] = ' ';
						}

						_M_text[_M_len++] = c;
					}

					_M_last_char = c;
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 16: // <![<data>]]> => <data>
				if (c == ']') {
					_M_state = 17; // <![<data>]]> => First ]
				} else if (!IS_CHAR(c)) {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 17: // <![<data>]]> => First ]
				if (c == ']') {
					_M_state = 18; // <![<data>]]> => Second ]
				} else if (IS_CHAR(c)) {
					_M_state = 16; // <![<data>]]> => <data>
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 18: // <![<data>]]> => Second ]
				if (c == '>') {
					_M_state = 0; // Initial state.
				} else if (IS_CHAR(c)) {
					if (c != ']') {
						_M_state = 16; // <![<data>]]> => <data>
					}
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 19: // Processing instruction.
				if (c == '?') {
					_M_state = 20; // ?> => ?
				} else if (IS_CHAR(c)) {
					if (!IS_WHITE_SPACE(c)) {
						if (_M_len + 2 > sizeof(_M_text)) {
							if (!on_processing_instruction(_M_text, _M_len)) {
								_M_error = ERROR_CALLBACK;
								return false;
							}

							_M_len = 0;
						}

						if ((IS_WHITE_SPACE(_M_last_char)) && (_M_len > 0)) {
							_M_text[_M_len++] = ' ';
						}

						_M_text[_M_len++] = c;
					}

					_M_last_char = c;
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 20: // ?> => ?
				if (c == '>') {
					if (_M_len > 0) {
						if (!on_processing_instruction(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}
					}

					if (!_M_has_root) {
						_M_state = 0; // Initial state.
					} else {
						if (_M_depth > 0) {
							_M_len = 0;

							_M_last_char = 0;

							_M_state = 30; // Character data.
						} else {
							_M_state = 45; // Misc.
						}
					}
				} else if (c == '?') {
					if (_M_len + 2 > sizeof(_M_text)) {
						if (!on_processing_instruction(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}

						_M_len = 0;
					}

					if ((IS_WHITE_SPACE(_M_last_char)) && (_M_len > 0)) {
						_M_text[_M_len++] = ' ';
					}

					_M_text[_M_len++] = '?';

					_M_last_char = '?';
				} else if (IS_CHAR(c)) {
					if (_M_len + 3 > sizeof(_M_text)) {
						if (!on_processing_instruction(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}

						_M_len = 0;
					}

					if ((IS_WHITE_SPACE(_M_last_char)) && (_M_len > 0)) {
						_M_text[_M_len++] = ' ';
					}

					_M_text[_M_len++] = '?';

					if (!IS_WHITE_SPACE(c)) {
						_M_text[_M_len++] = c;
					}

					_M_last_char = c;

					_M_state = 19; // Processing instruction.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 21: // Start tag.
				if (IS_WHITE_SPACE(c)) {
					_M_element[_M_element_len] = 0;
					_M_attribute_list.reset();

					_M_state = 22; // White space after start tag.
				} else if (c == '>') {
					_M_element[_M_element_len] = 0;
					_M_attribute_list.reset();

					if (!on_element_start(_M_element, _M_element_len, _M_attribute_list)) {
						_M_error = ERROR_CALLBACK;
						return false;
					}

					_M_len = 0;

					_M_depth++;

					_M_last_char = 0;

					_M_state = 30; // Character data.
				} else if (c == '/') {
					_M_element[_M_element_len] = 0;
					_M_attribute_list.reset();

					_M_state = 29; // /> => / (empty element tag)
				} else if (IS_NAME_CHAR(c)) {
					if (_M_element_len + 1 >= sizeof(_M_element)) {
						_M_error = ERROR_PARSING;
						return false;
					}

					_M_element[_M_element_len++] = c;
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 22: // White space after start tag.
				if (IS_NAME_START_CHAR(c)) {
					_M_attribute_name[0] = c;
					_M_attribute_name_len = 1;

					_M_state = 23; // Attribute name.
				} else if (c == '>') {
					if (!on_element_start(_M_element, _M_element_len, _M_attribute_list)) {
						_M_error = ERROR_CALLBACK;
						return false;
					}

					_M_len = 0;

					_M_depth++;

					_M_last_char = 0;

					_M_state = 30; // Character data.
				} else if (c == '/') {
					_M_state = 29; // /> => / (empty element tag)
				} else if (!IS_WHITE_SPACE(c)) {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 23: // Attribute name.
				if (IS_WHITE_SPACE(c)) {
					_M_attribute_name[_M_attribute_name_len] = 0;

					_M_state = 24; // White space after attribute name.
				} else if (c == '=') {
					_M_attribute_name[_M_attribute_name_len] = 0;

					_M_state = 25; // Attribute name = Attribute value => =
				} else if (IS_NAME_CHAR(c)) {
					if (_M_attribute_name_len + 1 >= sizeof(_M_attribute_name)) {
						_M_error = ERROR_PARSING;
						return false;
					}

					_M_attribute_name[_M_attribute_name_len++] = c;
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 24: // White space after attribute name.
				if (c == '=') {
					_M_state = 25; // Attribute name = Attribute value => =
				} else if (!IS_WHITE_SPACE(c)) {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 25: // Attribute name = Attribute value => =
				if ((c == '"') || (c == '\'')) {
					_M_quotes = c;

					_M_state = 26; // Attribute value: first letter.
				} else if (!IS_WHITE_SPACE(c)) {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 26: // Attribute value: first letter.
				if (c == (unsigned) _M_quotes) {
					if (!_M_attribute_list.add(_M_attribute_name, _M_attribute_name_len, "", 0)) {
						_M_error = ERROR_NO_MEMORY;
						return false;
					}

					_M_state = 28; // After quotes.
				} else if (IS_CHAR(c)) {
					_M_text[0] = c;
					_M_len = 1;

					_M_state = 27; // Attribute value.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 27: // Attribute value.
				if (c == (unsigned) _M_quotes) {
					if (!_M_attribute_list.add(_M_attribute_name, _M_attribute_name_len, _M_text, _M_len)) {
						_M_error = ERROR_NO_MEMORY;
						return false;
					}

					_M_state = 28; // After quotes.
				} else if (IS_CHAR(c)) {
					if (_M_len + 1 > sizeof(_M_text)) {
						_M_error = ERROR_PARSING;
						return false;
					}

					_M_text[_M_len++] = c;
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 28: // After quotes.
				if (IS_WHITE_SPACE(c)) {
					_M_state = 22; // White space after start tag.
				} else if (c == '/') {
					_M_state = 29; // /> => / (empty element tag)
				} else if (c == '>') {
					if (!on_element_start(_M_element, _M_element_len, _M_attribute_list)) {
						_M_error = ERROR_CALLBACK;
						return false;
					}

					_M_len = 0;

					_M_depth++;

					_M_last_char = 0;

					_M_state = 30; // Character data.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 29: // /> => / (empty element tag)
				if (c == '>') {
					if (!on_empty_element(_M_element, _M_element_len, _M_attribute_list)) {
						_M_error = ERROR_CALLBACK;
						return false;
					}

					if (_M_depth > 0) {
						_M_len = 0;

						_M_last_char = 0;

						_M_state = 30; // Character data.
					} else {
						_M_state = 45; // Misc.
					}
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 30: // Character data.
				if (c == '<') {
					if (_M_len > 0) {
						if (!on_character_data(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}
					}

					_M_state = 31; // character-data< => <
				} else {
					if (!IS_WHITE_SPACE(c)) {
						if (_M_len + 2 > sizeof(_M_text)) {
							if (!on_character_data(_M_text, _M_len)) {
								_M_error = ERROR_CALLBACK;
								return false;
							}

							_M_len = 0;
						}

						if ((IS_WHITE_SPACE(_M_last_char)) && (_M_len > 0)) {
							_M_text[_M_len++] = ' ';
						}

						_M_text[_M_len++] = c;
					}

					_M_last_char = c;
				}

				break;
			case 31: // character-data< => <
				if (IS_NAME_START_CHAR(c)) {
					_M_element[0] = c;
					_M_element_len = 1;

					_M_state = 21; // Start tag.
				} else if (c == '!') {
					_M_state = 32; // Comment or CDATA section.
				} else if (c == '?') {
					_M_len = 0;

					_M_last_char = 0;

					_M_state = 19; // Processing instruction.
				} else if (c == '/') {
					_M_state = 42; // End tag.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 32: // Comment or CDATA section.
				if (c == '[') {
					_M_state = 33; // <![CDATA[ => First [
				} else if (c == '-') {
					_M_state = 3; // <!-- => First -
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 33: // <![CDATA[ => First [
				if ((c == 'C') || (c == 'c')) {
					_M_state = 34; // <![CDATA[ => C
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 34: // <![CDATA[ => C
				if ((c == 'D') || (c == 'd')) {
					_M_state = 35; // <![CDATA[ => D
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 35: // <![CDATA[ => D
				if ((c == 'A') || (c == 'a')) {
					_M_state = 36; // <![CDATA[ => First A
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 36: // <![CDATA[ => First A
				if ((c == 'T') || (c == 't')) {
					_M_state = 37; // <![CDATA[ => T
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 37: // <![CDATA[ => T
				if ((c == 'A') || (c == 'a')) {
					_M_state = 38; // <![CDATA[ => Second A
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 38: // <![CDATA[ => Second A
				if (c == '[') {
					if ((IS_WHITE_SPACE(_M_last_char)) && (_M_len > 0)) {
						_M_text[0] = ' ';
						_M_len = 1;
					} else {
						_M_len = 0;
					}

					_M_state = 39; // CDATA section.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 39: // CDATA section.
				if (c == ']') {
					_M_state = 40; // ]]> => First ]
				} else if (IS_CHAR(c)) {
					if (_M_len + 1 > sizeof(_M_text)) {
						if (!on_character_data(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}

						_M_len = 0;
					}

					_M_text[_M_len++] = c;
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 40: // ]]> => First ]
				if (c == ']') {
					_M_state = 41; // ]]> => Second ]
				} else if (IS_CHAR(c)) {
					if (_M_len + 2 > sizeof(_M_text)) {
						if (!on_character_data(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}

						_M_len = 0;
					}

					_M_text[_M_len++] = ']';
					_M_text[_M_len++] = c;

					_M_state = 39; // CDATA section.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 41: // ]]> => Second ]
				if (c == '>') {
					_M_last_char = 0;

					_M_state = 30; // Character data.
				} else if (c == ']') {
					if (_M_len + 1 > sizeof(_M_text)) {
						if (!on_character_data(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}

						_M_len = 0;
					}

					_M_text[_M_len++] = ']';
				} else if (IS_CHAR(c)) {
					if (_M_len + 3 > sizeof(_M_text)) {
						if (!on_character_data(_M_text, _M_len)) {
							_M_error = ERROR_CALLBACK;
							return false;
						}

						_M_len = 0;
					}

					_M_text[_M_len++] = ']';
					_M_text[_M_len++] = ']';
					_M_text[_M_len++] = c;

					_M_state = 39; // CDATA section.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 42: // End tag.
				if (IS_NAME_START_CHAR(c)) {
					_M_element[0] = c;
					_M_element_len = 1;

					_M_state = 43; // First letter of end tag.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 43: // First letter of end tag.
				if (c == '>') {
					_M_element[_M_element_len] = 0;

					if (!on_element_end(_M_element, _M_element_len)) {
						_M_error = ERROR_CALLBACK;
						return false;
					}

					_M_depth--;

					if (_M_depth > 0) {
						_M_len = 0;

						_M_last_char = 0;

						_M_state = 30; // Character data.
					} else {
						_M_state = 45; // Misc.
					}
				} else if (IS_NAME_CHAR(c)) {
					if (_M_element_len + 1 >= sizeof(_M_element)) {
						_M_error = ERROR_PARSING;
						return false;
					}

					_M_element[_M_element_len++] = c;
				} else if (IS_WHITE_SPACE(c)) {
					_M_element[_M_element_len] = 0;

					_M_state = 44; // Space after end tag.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 44: // Space after end tag.
				if (c == '>') {
					if (!on_element_end(_M_element, _M_element_len)) {
						_M_error = ERROR_CALLBACK;
						return false;
					}

					_M_depth--;

					if (_M_depth > 0) {
						_M_len = 0;

						_M_last_char = 0;

						_M_state = 30; // Character data.
					} else {
						_M_state = 45; // Misc.
					}
				} else if (!IS_WHITE_SPACE(c)) {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 45: // Misc.
				if (c == '<') {
					_M_state = 46; // Comment or Processing instruction.
				} else if (!IS_WHITE_SPACE(c)) {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 46: // Comment or Processing instruction.
				if (c == '!') {
					_M_state = 47; // <!-- => !
				} else if (c == '?') {
					_M_len = 0;

					_M_last_char = 0;

					_M_state = 19; // Processing instruction.
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
			case 47: // <!-- => !
				if (c == '-') {
					_M_state = 3; // <!-- => First -
				} else {
					_M_error = ERROR_PARSING;
					return false;
				}

				break;
		}

		if (c == '\n') {
			_M_line++;

			_M_offset = 1;
		} else {
			_M_offset++;
		}
	}

	return true;
}
