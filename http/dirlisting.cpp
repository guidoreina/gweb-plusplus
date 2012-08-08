#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include "dirlisting.h"
#include "net/url_encoder.h"
#include "html/html_encoder.h"
#include "string/utf8.h"
#include "constants/months_and_days.h"

const off_t dirlisting::MAX_FOOTER_SIZE = 32 * 1024;
const unsigned short dirlisting::WIDTH_OF_NAME_COLUMN = 32;

dirlisting::dirlisting()
{
	_M_rootlen = 0;

	*_M_path = 0;
	_M_pathlen = 0;

	_M_exact_size = false;
}

bool dirlisting::set_root(const char* root, size_t rootlen)
{
	if (rootlen >= sizeof(_M_path)) {
		return false;
	}

	_M_rootlen = rootlen;

	memcpy(_M_path, root, rootlen);
	_M_pathlen = rootlen;

	return true;
}

bool dirlisting::load_footer(const char* filename)
{
	struct stat buf;
	if (stat(filename, &buf) < 0) {
		return false;
	}

	if ((!S_ISREG(buf.st_mode)) && (!S_ISLNK(buf.st_mode))) {
		return false;
	}

	if (buf.st_size == 0) {
		return true;
	} else if (buf.st_size > MAX_FOOTER_SIZE) {
		return false;
	}

	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		return false;
	}

	if (!_M_footer.allocate(buf.st_size)) {
		close(fd);
		return false;
	}

	off_t size = buf.st_size;

	do {
		ssize_t ret = read(fd, _M_footer.data() + _M_footer.count(), size - _M_footer.count());
		if (ret <= 0) {
			close(fd);
			return false;
		}

		_M_footer.increment_count(ret);

		size -= ret;
	} while (size > 0);

	close(fd);

	return true;
}

bool dirlisting::build(const char* dir, size_t dirlen, buffer& buf)
{
	if (_M_rootlen + dirlen >= sizeof(_M_path)) {
		return false;
	}

	memcpy(_M_path + _M_rootlen, dir, dirlen);
	_M_pathlen = _M_rootlen + dirlen;
	_M_path[_M_pathlen] = 0;

	if (!build_file_lists()) {
		return false;
	}

#define FIRST  "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html><head><title>Index of "
#define SECOND "</title></head><body><h1>Index of "
#define THIRD  "</h1><pre>Name"

	if (!buf.append(FIRST, sizeof(FIRST) - 1)) {
		return false;
	}

	size_t offset = buf.count();
	int len;
	if ((len = html_encoder::encode(dir, dirlen, buf)) < 0) {
		return false;
	}

	if (!buf.append(SECOND, sizeof(SECOND) - 1)) {
		return false;
	}

	if (!buf.allocate(len)) {
		return false;
	}

	char* data = buf.data();
	memcpy(data + buf.count(), data + offset, len);
	buf.increment_count(len);

	if (!buf.append(THIRD, sizeof(THIRD) - 1)) {
		return false;
	}

	unsigned nspaces = WIDTH_OF_NAME_COLUMN - 4;
	if (!buf.format("%*s    Last modified        Size<hr>\n", nspaces, " ")) {
		return false;
	}

	// If not root directory...
	if (dirlen > 1) {
		const char* slash = dir + dirlen - 1;

		while (slash > dir) {
			if (*(slash - 1) == '/') {
				break;
			}

			slash--;
		}

		if (!buf.append("<a href=\"", 9)) {
			return false;
		}

		if (url_encoder::encode(dir, slash - dir, buf) < 0) {
			return false;
		}

		if (!buf.append("\">Parent directory</a>\n", 23)) {
			return false;
		}
	}

	// For each directory...
	const filelist::file* file;
	for (unsigned i = 0; ((file = _M_directories.get_file(i)) != NULL); i++) {
		if (!buf.append("<a href=\"", 9)) {
			return false;
		}

		if (url_encoder::encode(file->name, file->namelen, buf) < 0) {
			return false;
		}

		if (!buf.append("/\">", 3)) {
			return false;
		}

		if (file->utf8len + 1 > WIDTH_OF_NAME_COLUMN) {
			if (html_encoder::encode(file->name, WIDTH_OF_NAME_COLUMN - 4, buf) < 0) {
				return false;
			}

			if (!buf.append(".../</a>", 8)) {
				return false;
			}
		} else {
			if (html_encoder::encode(file->name, file->namelen, buf) < 0) {
				return false;
			}

			if (file->utf8len + 1 < WIDTH_OF_NAME_COLUMN) {
				if (!buf.format("/</a>%*s", WIDTH_OF_NAME_COLUMN - file->utf8len - 1, " ")) {
					return false;
				}
			} else {
				if (!buf.append("/</a>", 5)) {
					return false;
				}
			}
		}

		struct tm stm;
		gmtime_r(&file->mtime, &stm);

		if (!buf.format("    %02d-%s-%04d %02d:%02d     -\n", stm.tm_mday, months[stm.tm_mon], 1900 + stm.tm_year, stm.tm_hour, stm.tm_min)) {
			return false;
		}
	}

	// For each file...
	for (unsigned i = 0; ((file = _M_files.get_file(i)) != NULL); i++) {
		if (!buf.append("<a href=\"", 9)) {
			return false;
		}

		if (url_encoder::encode(file->name, file->namelen, buf) < 0) {
			return false;
		}

		if (!buf.append("\">", 2)) {
			return false;
		}

		if (file->utf8len > WIDTH_OF_NAME_COLUMN) {
			if (html_encoder::encode(file->name, WIDTH_OF_NAME_COLUMN - 3, buf) < 0) {
				return false;
			}

			if (!buf.append("...</a>", 7)) {
				return false;
			}
		} else {
			if (html_encoder::encode(file->name, file->namelen, buf) < 0) {
				return false;
			}

			if (file->utf8len < WIDTH_OF_NAME_COLUMN) {
				if (!buf.format("</a>%*s", WIDTH_OF_NAME_COLUMN - file->utf8len, " ")) {
					return false;
				}
			} else {
				if (!buf.append("</a>", 4)) {
					return false;
				}
			}
		}

		struct tm stm;
		gmtime_r(&file->mtime, &stm);

		if (!buf.format("    %02d-%s-%04d %02d:%02d    ", stm.tm_mday, months[stm.tm_mon], 1900 + stm.tm_year, stm.tm_hour, stm.tm_min)) {
			return false;
		}

		if (_M_exact_size) {
			if (!buf.format("%lld", file->size)) {
				return false;
			}
		} else {
			if (file->size > (off_t) 1024 * 1024 * 1024) {
				if (!buf.format("%.01fG\n", (float) file->size / (float) (1024.0 * 1024.0 * 1024.0))) {
					return false;
				}
			} else if (file->size > 1024 * 1024) {
				if (!buf.format("%.01fM\n", (float) file->size / (float) (1024.0 * 1024.0))) {
					return false;
				}
			} else if (file->size > 1024) {
				if (!buf.format("%.01fK\n", (float) file->size / (float) (1024.0))) {
					return false;
				}
			} else {
				if (!buf.format("%lu\n", file->size)) {
					return false;
				}
			}
		}
	}

	if (!buf.append("</pre><hr>", 10)) {
		return false;
	}

	if (_M_footer.count() > 0) {
		if (!buf.append(_M_footer.data(), _M_footer.count())) {
			return false;
		}
	}

	return buf.append("</body></html>", 14);
}

bool dirlisting::build_file_lists()
{
	DIR* dp = opendir(_M_path);
	if (!dp) {
		return false;
	}

	const char* end = _M_path + sizeof(_M_path) - 1;
	char* name = _M_path + _M_pathlen;

	_M_directories.reset();
	_M_files.reset();

	struct dirent* ep;
	while ((ep = readdir(dp)) != NULL) {
		// Hidden file/directory?
		if (ep->d_name[0] == '.') {
			continue;
		}

		const char* src = ep->d_name;
		char* dest = name;
		while ((dest < end) && (*src)) {
			*dest++ = *src++;
		}

		if (*src) {
			// Name too long.
			continue;
		}

		*dest = 0;

		struct stat buf;
		if (stat(_M_path, &buf) < 0) {
			continue;
		}

		// Directory?
		if (S_ISDIR(buf.st_mode)) {
			size_t utf8len;
			if (!utf8::length(name, dest, utf8len)) {
				continue;
			}

			if (!_M_directories.insert(name, dest - name, utf8len, buf.st_size, buf.st_mtime)) {
				closedir(dp);
				return false;
			}
		} else if ((S_ISREG(buf.st_mode)) || (S_ISLNK(buf.st_mode))) {
			size_t utf8len;
			if (!utf8::length(name, dest, utf8len)) {
				continue;
			}

			if (!_M_files.insert(name, dest - name, utf8len, buf.st_size, buf.st_mtime)) {
				closedir(dp);
				return false;
			}
		}
	}

	closedir(dp);

	return true;
}
