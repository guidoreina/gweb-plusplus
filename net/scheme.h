#ifndef SCHEME_H
#define SCHEME_H

class scheme {
	public:
		static const unsigned char FILE;
		static const unsigned char FTP;
		static const unsigned char HTTP;
		static const unsigned char HTTPS;
		static const unsigned char ICAP;
		static const unsigned char IMAP;
		static const unsigned char LDAP;
		static const unsigned char MAILTO;
		static const unsigned char NEWS;
		static const unsigned char NFS;
		static const unsigned char POP;
		static const unsigned char TELNET;
		static const unsigned char UNKNOWN;

		// Get scheme.
		static const char* get_scheme(unsigned char scheme);

		static unsigned char get_scheme(const char* string, size_t len);

	private:
		struct string {
			const char* name;
			size_t len;
		};

		static const string _M_schemes[];
};

inline const char* scheme::get_scheme(unsigned char scheme)
{
	if (scheme > TELNET) {
		return NULL;
	}

	return _M_schemes[scheme].name;
}

#endif // SCHEME_H
