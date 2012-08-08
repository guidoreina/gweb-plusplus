CC=g++
CXXFLAGS=-g -Wall -pedantic -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wno-format -Wno-long-long -I.
CXXFLAGS+=-DALLOW_DIGITS_AS_NAME_START_CHAR

ifeq ($(shell uname), Linux)
	CXXFLAGS+=-DHAVE_TCP_CORK -DHAVE_EPOLL -DHAVE_POLL -DHAVE_SENDFILE -DHAVE_MMAP -DHAVE_PREAD -DHAVE_MEMRCHR -DHAVE_TIMEGM
else
	ifeq ($(shell uname), FreeBSD)
		CXXFLAGS+=-DHAVE_TCP_NOPUSH -DHAVE_KQUEUE -DHAVE_POLL -DHAVE_SENDFILE -DHAVE_MMAP -DHAVE_PREAD -DHAVE_TIMEGM
	else
		ifeq ($(shell uname), SunOS)
			CXXFLAGS+=-DHAVE_PORT -DHAVE_POLL -DHAVE_SENDFILE -DHAVE_MMAP -DHAVE_PREAD
		endif
	endif
endif

LDFLAGS=
LIBS=

ifeq ($(shell uname), SunOS)
	LIBS+=-lsocket -lnsl -lsendfile
endif

MAKEDEPEND=${CC} -MM
PROGRAM=gweb++

OBJS =	constants/months_and_days.o \
	string/memcasemem.o string/buffer.o string/utf8.o \
	util/now.o util/number.o util/date_parser.o util/range_list.o \
	util/pathlist.o util/string_list.o \
	xml/attribute_list.o xml/xml_parser.o xml/xml_document.o xmlconf/xmlconf.o \
	file/file_wrapper.o file/tmpfiles_cache.o \
	mime/mime_types.o logger/logger.o \
	net/scheme.o net/url_encoder.o net/url_parser.o \
	net/socket_wrapper.o net/resolver.o net/filesender.o \
	net/fdmap.o net/tcp_server.o net/tcp_connection.o \
	net/sock.o \
	html/html_encoder.o http/http_method.o http/index_file_finder.o \
	http/filelist.o http/dirlisting.o http/range_parser.o \
	http/http_headers.o http/http_error.o http/virtual_hosts.o \
	http/access_log.o http/http_connection.o http/http_server.o http/rulelist.o \
	http/chunked_parser.o \
	http/fastcgi.o http/backend_list.o http/proxy_connection.o http/fcgi_connection.o \
	main.o

ifneq (,$(findstring HAVE_EPOLL, $(CXXFLAGS)))
	OBJS+=net/epoll_selector.o
else
	ifneq (,$(findstring HAVE_KQUEUE, $(CXXFLAGS)))
		OBJS+=net/kqueue_selector.o
	else
		ifneq (,$(findstring HAVE_PORT, $(CXXFLAGS)))
			OBJS+=net/port_selector.o
		else
			ifneq (,$(findstring HAVE_POLL, $(CXXFLAGS)))
				OBJS+=net/poll_selector.o
			else
				OBJS+=net/select_selector.o
			endif
		endif
	endif
endif

ifeq (,$(findstring HAVE_MEMRCHR, $(CXXFLAGS)))
	OBJS+=string/memrchr.o
endif

DEPS:= ${OBJS:%.o=%.d}

all: $(PROGRAM)

${PROGRAM}: ${OBJS}
	${CC} ${CXXFLAGS} ${LDFLAGS} ${OBJS} ${LIBS} -o $@

clean:
	rm -f ${PROGRAM} ${OBJS} ${OBJS} ${DEPS}

${OBJS} ${DEPS} ${PROGRAM} : Makefile

.PHONY : all clean

%.d : %.cpp
	${MAKEDEPEND} ${CXXFLAGS} $< -MT ${@:%.d=%.o} > $@

%.o : %.cpp
	${CC} ${CXXFLAGS} -c -o $@ $<

-include ${DEPS}
