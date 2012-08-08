#ifndef UTF8_H
#define UTF8_H

class utf8 {
	public:
		static bool length(const char* string, size_t& count, size_t& utf8len);
		static bool length(const char* string, const char* end, size_t& utf8len);
};

#endif // UTF8_H
