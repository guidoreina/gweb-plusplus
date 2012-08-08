#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "mime_types.h"
#include "macros/macros.h"

const char* mime_types::MIME_TYPES_DEFAULT_FILE = "/etc/mime.types";

const char* mime_types::DEFAULT_MIME_TYPE = "application/octet-stream";
const unsigned short mime_types::DEFAULT_MIME_TYPE_LEN = 24;

const size_t mime_types::MIME_TYPES_ALLOC = 256;

mime_types::mime_types() : _M_buf(16 * 1024)
{
	_M_mime_types = NULL;
	_M_size = 0;
	_M_used = 0;
}

void mime_types::free()
{
	_M_buf.free();

	if (_M_mime_types) {
		::free(_M_mime_types);
		_M_mime_types = NULL;
	}

	_M_size = 0;
	_M_used = 0;
}

bool mime_types::load(const char* filename)
{
	struct stat buf;
	if ((stat(filename, &buf) < 0) || (!S_ISREG(buf.st_mode))) {
		return false;
	}

	FILE* file = fopen(filename, "r");
	if (!file) {
		return false;
	}

	const char* type = NULL;
	unsigned short typelen = 0;

	unsigned short extensionlen = 0;

	size_t offset = 0;

	char line[1024];
	while ((fgets(line, sizeof(line), file)) != NULL) {
		const char* ptr = line;

		const char* extension = NULL;

		unsigned count = 0;

		int state = 0;

		unsigned char c;
		while (((c = (unsigned char) *ptr) != 0) && (c != '\n') && (c != '\r')) {
			if (state == 0) {
				if (c == '#') {
					break;
				} else if (c > ' ') {
					type = ptr;
					typelen = 1;

					state = 1; // Parsing MIME type.
				} else if (!IS_WHITE_SPACE(c)) {
					break;
				}
			} else if (state == 1) { // Parsing MIME type.
				if (c == '#') {
					break;
				} else if (c > ' ') {
					typelen++;
				} else if (IS_WHITE_SPACE(c)) {
					state = 2; // Space after MIME type.
				} else {
					break;
				}
			} else if (state == 2) { // Space after MIME type.
				if (c == '#') {
					break;
				} else if (c > ' ') {
					extension = ptr;
					extensionlen = 1;

					state = 3; // Parsing extension.
				} else if (!IS_WHITE_SPACE(c)) {
					break;
				}
			} else { // Parsing extension.
				if (c == '#') {
					extension = NULL;
					break;
				} else if (c > ' ') {
					extensionlen++;
				} else if (IS_WHITE_SPACE(c)) {
					// First extension?
					if (count == 0) {
						offset = _M_buf.count();

						if (!_M_buf.append(type, typelen)) {
							fclose(file);
							return false;
						}
					}

					if (!add(offset, typelen, extension, extensionlen)) {
						fclose(file);
						return false;
					}

					extension = NULL;

					count++;

					state = 2; // Space after MIME type.
				} else {
					extension = NULL;
					break;
				}
			}

			ptr++;
		}

		if (extension) {
			// First extension?
			if (count == 0) {
				offset = _M_buf.count();

				if (!_M_buf.append(type, typelen)) {
					fclose(file);
					return false;
				}
			}

			if (!add(offset, typelen, extension, extensionlen)) {
				fclose(file);
				return false;
			}
		}
	}

	fclose(file);

	return true;
}

bool mime_types::search(const char* extension, unsigned short extensionlen, size_t& position) const
{
	int i = 0;
	int j = _M_used - 1;

	const char* data = _M_buf.data();

	while (i <= j) {
		int pivot = (i + j) / 2;
		const mime_type* mime_type = &(_M_mime_types[pivot]);

		int ret = strncasecmp(extension, data + mime_type->extension, MIN(extensionlen, mime_type->extensionlen));

		if (ret < 0) {
			j = pivot - 1;
		} else if (ret == 0) {
			if (extensionlen < _M_mime_types[pivot].extensionlen) {
				j = pivot - 1;
			} else if (extensionlen == _M_mime_types[pivot].extensionlen) {
				position = (size_t) pivot;

				return true;
			} else {
				i = pivot + 1;
			}
		} else {
			i = pivot + 1;
		}
	}

	position = (size_t) i;

	return false;
}

bool mime_types::add(size_t type, unsigned short typelen, const char* extension, unsigned short extensionlen)
{
	size_t position;
	if (search(extension, extensionlen, position)) {
		return true;
	}

	size_t offset = _M_buf.count();

	if (!_M_buf.append(extension, extensionlen)) {
		return false;
	}

	if (_M_used == _M_size) {
		size_t size = _M_size + MIME_TYPES_ALLOC;
		struct mime_type* types = (struct mime_type*) realloc(_M_mime_types, size * sizeof(struct mime_type));
		if (!types) {
			return false;
		}

		_M_mime_types = types;
		_M_size = size;
	}

	if (position < _M_used) {
		memmove(&(_M_mime_types[position + 1]), &(_M_mime_types[position]), (_M_used - position) * sizeof(struct mime_type));
	}

	mime_type* mime_type = &(_M_mime_types[position]);

	mime_type->extension = offset;
	mime_type->extensionlen = extensionlen;

	mime_type->type = type;
	mime_type->typelen = typelen;

	_M_used++;

	return true;
}
