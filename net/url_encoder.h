#ifndef URL_ENCODER_H
#define URL_ENCODER_H

#include "string/buffer.h"
#include "macros/macros.h"

class url_encoder {
	public:
		static int encode(const char* url, size_t len, buffer& buf);

	private:
		static bool is_unreserved(unsigned char c);
};

inline bool url_encoder::is_unreserved(unsigned char c)
{
	return ((IS_ALPHA(c)) || (IS_DIGIT(c)) || (c == '-') || (c == '.') || (c == '_') || (c == '~'));
}

#endif // URL_ENCODER_H
