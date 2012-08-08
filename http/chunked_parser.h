#ifndef CHUNKED_PARSER_H
#define CHUNKED_PARSER_H

class chunked_parser {
	public:
		// Constructor.
		chunked_parser();

		// Destructor.
		virtual ~chunked_parser();

		// Reset.
		void reset();

		// Parse chunk.
		enum parse_result {
			INVALID_CHUNKED_RESPONSE,
			CALLBACK_FAILED,
			NOT_END_OF_RESPONSE,
			END_OF_RESPONSE
		};

		parse_result parse_chunk(const char* buf, size_t len);
		parse_result parse_chunk(const char* buf, size_t len, size_t& size);

		// Add chunked Data.
		virtual bool add_chunked_data(const char* buf, size_t len) = 0;

	private:
		unsigned char _M_state;
		size_t _M_chunk_size;
};

inline chunked_parser::chunked_parser()
{
	reset();
}

inline chunked_parser::~chunked_parser()
{
}

inline void chunked_parser::reset()
{
	_M_state = 0;
	_M_chunk_size = 0;
}

inline chunked_parser::parse_result chunked_parser::parse_chunk(const char* buf, size_t len)
{
	size_t size;
	return parse_chunk(buf, len, size);
}

#endif // CHUNKED_PARSER_H
