#ifndef HTTP_METHOD_H
#define HTTP_METHOD_H

class http_method {
	public:
		static const unsigned char CONNECT;
		static const unsigned char COPY;
		static const unsigned char DELETE;
		static const unsigned char GET;
		static const unsigned char HEAD;
		static const unsigned char LOCK;
		static const unsigned char MKCOL;
		static const unsigned char MOVE;
		static const unsigned char OPTIONS;
		static const unsigned char POST;
		static const unsigned char PROPFIND;
		static const unsigned char PROPPATCH;
		static const unsigned char PUT;
		static const unsigned char TRACE;
		static const unsigned char UNLOCK;
		static const unsigned char UNKNOWN;

		// Get method.
		static const char* get_method(unsigned char method);

		static unsigned char get_method(const char* string, size_t len);

	private:
		struct string {
			const char* name;
			size_t len;
		};

		static const string _M_methods[];
};

inline const char* http_method::get_method(unsigned char method)
{
	if (method > UNLOCK) {
		return NULL;
	}

	return _M_methods[method].name;
}

#endif // HTTP_METHOD_H
