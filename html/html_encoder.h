#ifndef HTML_ENCODER_H
#define HTML_ENCODER_H

#include "string/buffer.h"

class html_encoder {
	public:
		static int encode(const char* string, size_t len, buffer& buf);
};

#endif // HTML_ENCODER_H
