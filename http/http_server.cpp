#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <new>
#include "http_server.h"
#include "http/http_error.h"
#include "net/url_parser.h"
#include "util/number.h"

#ifndef HAVE_MEMRCHR
#include "string/memrchr.h"
#endif

http_server::http_server() : tcp_server(true)
{
	_M_connection_handlers = NULL;

	_M_http_connections = NULL;
	_M_proxy_connections = NULL;
	_M_fcgi_connections = NULL;

	_M_headers.set_max_line_length(http_connection::HEADER_MAX_LINE_LEN);

	http_error::set_headers(&_M_headers);

	_M_boundary = 0;

	_M_sync_count = 0;
}

bool http_server::create(const char* config_file, const char* mime_types_file)
{
	xmlconf conf;
	if (!conf.load(config_file)) {
		fprintf(stderr, "Couldn't load configuration file (%s:%u).\n", config_file, conf.get_line());
		return false;
	}

	general_conf general_conf;

	if (!load_general(conf, general_conf)) {
		return false;
	}

	if (general_conf.logfile) {
		if (!logger::instance().create(general_conf.level, general_conf.logdir, general_conf.logfile, general_conf.error_log_max_file_size * 1024L)) {
			return false;
		}
	}

	logger::instance().log(logger::LOG_INFO, "Loading hosts...");

	if (!load_hosts(conf, general_conf)) {
		logger::instance().log(logger::LOG_ERROR, "Couldn't load hosts.");
		return false;
	}

	logger::instance().log(logger::LOG_INFO, "Loading MIME types...");

	if (!_M_mime_types.load(mime_types_file)) {
		logger::instance().log(logger::LOG_ERROR, "Couldn't load MIME types.");
		return false;
	}

	logger::instance().log(logger::LOG_INFO, "Creating server...");

	if (!tcp_server::create(general_conf.address, general_conf.port)) {
		logger::instance().log(logger::LOG_ERROR, "Couldn't create server.");
		return false;
	}

	// Create cache of temporary files.
	if (general_conf.max_spare_files > _M_size) {
		general_conf.max_spare_files = _M_size;
	}

	if (!_M_tmpfiles.create(general_conf.payload_directory, _M_size, general_conf.max_spare_files)) {
		logger::instance().log(logger::LOG_ERROR, "Couldn't create cache of temporary files.");
		return false;
	}

	// Create backends.
	virtual_hosts::vhost* vhost;
	for (size_t i = 0; (vhost = _M_vhosts.get_host(i)) != NULL; i++) {
		rulelist::rule* rules;
		for (size_t j = 0; (rules = vhost->rules->get(j)) != NULL; j++) {
			if (!rules->backends.create(_M_size)) {
				logger::instance().log(logger::LOG_ERROR, "Couldn't create backends.");
				return false;
			}
		}
	}

	http_error::set_port(general_conf.port);

	logger::instance().log(logger::LOG_INFO, "Server started.");

	return true;
}

bool http_server::create_connections()
{
	if ((_M_connection_handlers = (unsigned char*) malloc(_M_size)) == NULL) {
		return false;
	}

	if ((_M_http_connections = new (std::nothrow) http_connection[_M_size]) == NULL) {
		return false;
	}

	if ((_M_proxy_connections = new (std::nothrow) proxy_connection[_M_size]) == NULL) {
		return false;
	}

	if ((_M_fcgi_connections = new (std::nothrow) fcgi_connection[_M_size]) == NULL) {
		return false;
	}

	for (size_t i = 0; i < _M_size; i++) {
		_M_connections[i] = &(_M_http_connections[i]);
	}

	return true;
}

void http_server::delete_connections()
{
	if (_M_connection_handlers) {
		free(_M_connection_handlers);
		_M_connection_handlers = NULL;
	}

	if (_M_http_connections) {
		delete [] _M_http_connections;
		_M_http_connections = NULL;
	}

	if (_M_proxy_connections) {
		delete [] _M_proxy_connections;
		_M_proxy_connections = NULL;
	}

	if (_M_fcgi_connections) {
		delete [] _M_fcgi_connections;
		_M_fcgi_connections = NULL;
	}
}

bool http_server::on_event(unsigned fd, int events)
{
	// New connection?
	if ((int) fd == _M_listener) {
		int client;
		if ((client = on_new_connection()) != -1) {
			_M_connection_handlers[client] = rulelist::LOCAL_HANDLER;
		}
	} else {
		if (!process_connection(fd, events)) {
			return false;
		}

		if (_M_connections[fd]->_M_in_ready_list) {
			// Add to ready list.
			_M_ready_list[_M_nready++] = fd;

			logger::instance().log(logger::LOG_DEBUG, "Added fd %d to ready list.", fd);
		}
	}

	return true;
}

bool http_server::load_general(const xmlconf& conf, general_conf& general_conf)
{
	const char* value;
	size_t len;
	unsigned i;
	bool b;

	if (!conf.get_value(value, len, "config", "general", "address", NULL)) {
		strcpy(general_conf.address, socket_wrapper::ANY_ADDRESS);
	} else {
		if (len > 15) {
			logger::instance().log(logger::LOG_ERROR, "Invalid bind address %s.", value);
			return false;
		}

		memcpy(general_conf.address, value, len);
		general_conf.address[len] = 0;
	}

	if (!conf.get_value(i, "config", "general", "port", NULL)) {
		general_conf.port = url_parser::HTTP_DEFAULT_PORT;
	} else {
		if ((i < 1) || (i > 65535)) {
			logger::instance().log(logger::LOG_ERROR, "Invalid bind port.");
			return false;
		}

		general_conf.port = i;
	}

	if (!conf.get_value(i, "config", "general", "max_idle_time", NULL)) {
		_M_max_idle_time = MAX_IDLE_TIME;
	} else {
		if ((i < 1) || (i > 3600)) {
			_M_max_idle_time = MAX_IDLE_TIME;

			logger::instance().log(logger::LOG_INFO, "Invalid idle time, set to %d seconds.", _M_max_idle_time);
		} else {
			_M_max_idle_time = i;
		}
	}

	if (!conf.get_value(i, "config", "general", "max_idle_time_unknown_size_body", NULL)) {
		_M_max_idle_time_unknown_size_body = 3;
	} else {
		if ((i < 1) || (i > 3600)) {
			_M_max_idle_time_unknown_size_body = 3;

			logger::instance().log(logger::LOG_INFO, "Invalid idle time for unknown size body, set to %d seconds.", _M_max_idle_time_unknown_size_body);
		} else {
			_M_max_idle_time_unknown_size_body = i;
		}
	}

	if (!conf.get_value(i, "config", "general", "max_payload_in_memory", NULL)) {
		_M_max_payload_in_memory = 4 * 1024;
	} else {
		if (i > 64) {
			_M_max_payload_in_memory = 4 * 1024;

			logger::instance().log(logger::LOG_INFO, "Invalid maximum payload in memory, set to %d bytes.", _M_max_payload_in_memory);
		} else {
			_M_max_payload_in_memory = i * 1024;
		}
	}

	if (!conf.get_value(general_conf.payload_directory, len, "config", "general", "payload_directory", NULL)) {
		general_conf.payload_directory = "/tmp";
	}

	if (!conf.get_value(general_conf.max_spare_files, "config", "general", "max_spare_files", NULL)) {
		general_conf.max_spare_files = 32;
	}

	if (!conf.get_value(i, "config", "general", "backend_retry_interval", NULL)) {
		general_conf.backend_retry_interval = 300;
	} else {
		if (i > 3600) {
			general_conf.backend_retry_interval = 300;

			logger::instance().log(logger::LOG_INFO, "Invalid backend retry interval, set to %d seconds.", general_conf.backend_retry_interval);
		} else {
			general_conf.backend_retry_interval = i;
		}
	}

	if (!conf.get_value(value, len, "config", "general", "log_level", NULL)) {
		general_conf.level = logger::LOG_ERROR;
	} else {
		if (len == 4) {
			if (strncasecmp(value, "info", 4) == 0) {
				general_conf.level = logger::LOG_INFO;
			} else {
				logger::instance().log(logger::LOG_ERROR, "Invalid log level %s.", value);
				return false;
			}
		} else if (len == 5) {
			if (strncasecmp(value, "error", 5) == 0) {
				general_conf.level = logger::LOG_ERROR;
			} else if (strncasecmp(value, "debug", 5) == 0) {
				general_conf.level = logger::LOG_DEBUG;
			} else {
				logger::instance().log(logger::LOG_ERROR, "Invalid log level %s.", value);
				return false;
			}
		} else if (len == 7) {
			if (strncasecmp(value, "warning", 7) == 0) {
				general_conf.level = logger::LOG_WARNING;
			} else {
				logger::instance().log(logger::LOG_ERROR, "Invalid log level %s.", value);
				return false;
			}
		} else {
			logger::instance().log(logger::LOG_ERROR, "Invalid log level %s.", value);
			return false;
		}
	}

	if (!conf.get_value(value, len, "config", "general", "error_log", NULL)) {
		general_conf.logdir[0] = 0;
		general_conf.logfile = NULL;
	} else {
		if (!get_dirname_basename(value, len, general_conf.logdir, general_conf.logfile)) {
			return false;
		}
	}

	if (!conf.get_value(i, "config", "general", "error_log_max_file_size", NULL)) {
		general_conf.error_log_max_file_size = 1024 * 1024;
	} else {
		if (i < 32) {
			i = 32;
		} else if (i > 4 * 1024 * 1024) {
			i = 4 * 1024 * 1024;
		}

		general_conf.error_log_max_file_size = i;
	}

	if (!conf.get_value(b, "config", "general", "log_requests", NULL)) {
		general_conf.log_requests = TRIBOOL_UNDEFINED;
	} else {
		general_conf.log_requests = b ? TRIBOOL_TRUE : TRIBOOL_FALSE;
	}

	if (!conf.get_value(i, "config", "general", "log_buffer_size", NULL)) {
		general_conf.log_buffer_size = 32;
	} else {
		if (i > 1024) {
			i = 1024;
		}

		general_conf.log_buffer_size = i;
	}

	if (!conf.get_value(i, "config", "general", "log_sync_interval", NULL)) {
		_M_sync_interval = 30;
	} else {
		if (i < 1) {
			_M_sync_interval = 1;
		} else if (i > 3600) {
			_M_sync_interval = 3600;
		} else {
			_M_sync_interval = i;
		}
	}

	if (!conf.get_value(i, "config", "general", "access_log_max_file_size", NULL)) {
		general_conf.access_log_max_file_size = 1024 * 1024;
	} else {
		if (i < 32) {
			i = 32;
		} else if (i > 4 * 1024 * 1024) {
			i = 4 * 1024 * 1024;
		}

		general_conf.access_log_max_file_size = i;
	}

	if (!conf.get_value(value, len, "config", "general", "log_format", NULL)) {
		general_conf.log_format = NULL;
	} else {
		general_conf.log_format = value;
	}

	if (!conf.get_value(b, "config", "general", "directory_listing", NULL)) {
		general_conf.have_dirlisting = TRIBOOL_UNDEFINED;
	} else {
		general_conf.have_dirlisting = b ? TRIBOOL_TRUE : TRIBOOL_FALSE;
	}

	if (!conf.get_value(value, len, "config", "general", "directory_listing_footer", NULL)) {
		general_conf.footer_file = NULL;
	} else {
		general_conf.footer_file = value;
	}

	for (unsigned i = 0; conf.get_child(i, value, len, "config", "general", "index_files", NULL); i++) {
		// If the filename contains a '/'...
		if (memchr(value, '/', len)) {
			logger::instance().log(logger::LOG_ERROR, "Invalid index file name %s.", value);
			return false;
		}

		if (!_M_index_file_finder.add(value, len)) {
			return false;
		}
	}

	return true;
}

bool http_server::load_hosts(const xmlconf& conf, const general_conf& general_conf)
{
	const char* host;
	size_t hostlen;
	const char* value;
	size_t len;
	bool b;

	for (unsigned i = 0; conf.get_child(i, host, hostlen, "config", "hosts", NULL); i++) {
		bool def;
		if (!conf.get_value(def, "config", "hosts", host, "default", NULL)) {
			def = false;
		}

		const char* document_root;
		size_t document_root_len;
		if (!conf.get_value(document_root, document_root_len, "config", "hosts", host, "document_root", NULL)) {
			logger::instance().log(logger::LOG_ERROR, "Empty document root for host %s.", host);
			return false;
		} else {
			if (document_root[document_root_len - 1] == '/') {
				document_root_len--;
			}
		}

		bool log_requests;
		char dir[PATH_MAX + 1];
		const char* filename = NULL;
		const char* log_format = general_conf.log_format;
		size_t bufsize = general_conf.log_buffer_size;
		size_t access_log_max_file_size = general_conf.access_log_max_file_size;

		if (general_conf.log_requests == TRIBOOL_FALSE) {
			log_requests = false;
		} else {
			if (conf.get_value(b, "config", "hosts", host, "log_requests", NULL)) {
				log_requests = b;
			} else {
				log_requests = (general_conf.log_requests == TRIBOOL_TRUE) ? true : false;
			}

			if (log_requests) {
				if (!conf.get_value(value, len, "config", "hosts", host, "access_log", NULL)) {
					logger::instance().log(logger::LOG_ERROR, "Empty access log for host %s.", host);
					return false;
				}

				if (!get_dirname_basename(value, len, dir, filename)) {
					return false;
				}

				if (conf.get_value(value, len, "config", "hosts", host, "log_format", NULL)) {
					log_format = value;
				}

				if (conf.get_value(bufsize, "config", "hosts", host, "log_buffer_size", NULL)) {
					if (bufsize > 1024) {
						bufsize = 1024;

						logger::instance().log(logger::LOG_INFO, "Log buffer size too big, set to %u KB.", bufsize);
					}
				}

				if (conf.get_value(access_log_max_file_size, "config", "hosts", host, "access_log_max_file_size", NULL)) {
					if (access_log_max_file_size < 32) {
						access_log_max_file_size = 32;

						logger::instance().log(logger::LOG_INFO, "Access log maximum file size too small, set to %u KB.", access_log_max_file_size);
					} else if (access_log_max_file_size > 4 * 1024 * 1024) {
						access_log_max_file_size = 4 * 1024 * 1024;

						logger::instance().log(logger::LOG_INFO, "Access log maximum file size too big, set to %u KB.", access_log_max_file_size);
					}
				}
			}
		}

		bool have_dirlisting;
		if (general_conf.have_dirlisting == TRIBOOL_FALSE) {
			have_dirlisting = false;
		} else {
			if (conf.get_value(b, "config", "hosts", host, "directory_listing", NULL)) {
				have_dirlisting = b;
			} else {
				have_dirlisting = (general_conf.have_dirlisting == TRIBOOL_TRUE) ? true : false;
			}
		}

		virtual_hosts::vhost* vhost;
		if ((vhost = _M_vhosts.add(host, hostlen, document_root, document_root_len, have_dirlisting, log_requests, def)) == NULL) {
			return false;
		}

		if (log_requests) {
			if (!vhost->log->create(dir, filename, log_format, bufsize * 1024, access_log_max_file_size * 1024L)) {
				logger::instance().log(logger::LOG_ERROR, "Couldn't create access log for host %s.", host);
				return false;
			}
		}

		if (have_dirlisting) {
			if (conf.get_value(value, len, "config", "hosts", host, "directory_listing_footer", NULL)) {
				if (!vhost->dir_listing->load_footer(value)) {
					logger::instance().log(logger::LOG_ERROR, "Couldn't load footer file %s for host %s.", value, host);
					return false;
				}
			} else if (general_conf.footer_file) {
				if (!vhost->dir_listing->load_footer(general_conf.footer_file)) {
					logger::instance().log(logger::LOG_ERROR, "Couldn't load footer file %s for host %s.", general_conf.footer_file, host);
					return false;
				}
			}
		}

		for (unsigned j = 0; conf.get_child(j, value, len, "config", "hosts", host, "aliases", NULL); j++) {
			if (!_M_vhosts.add_alias(vhost, value, len)) {
				return false;
			}
		}

		if (!load_rules(conf, general_conf, host, vhost->rules)) {
			return false;
		}
	}

	return (_M_vhosts.count() > 0);
}

bool http_server::load_rules(const xmlconf& conf, const general_conf& general_conf, const char* host, rulelist* rules)
{
	for (unsigned i = 0; ; i++) {
		char name[64];
		snprintf(name, sizeof(name), "rule-%u", i);

		const char* value;
		size_t len;
		if (!conf.get_value(value, len, "config", "hosts", host, "request_handling", name, "handler", NULL)) {
			return true;
		}

		unsigned char handler;

		if (len == 4) {
			if (strncasecmp(value, "http", 4) == 0) {
				handler = rulelist::HTTP_HANDLER;
			} else {
				return false;
			}
		} else if (len == 5) {
			if (strncasecmp(value, "local", 5) == 0) {
				handler = rulelist::LOCAL_HANDLER;
			} else {
				return false;
			}
		} else if (len == 7) {
			if (strncasecmp(value, "fastcgi", 7) == 0) {
				handler = rulelist::FCGI_HANDLER;
			} else {
				return false;
			}
		} else {
			return false;
		}

		if (!conf.get_value(value, len, "config", "hosts", host, "request_handling", name, "criterion", NULL)) {
			return false;
		}

		unsigned char criterion;

		if (len == 4) {
			if (strncasecmp(value, "path", 4) == 0) {
				criterion = rulelist::BY_PATH;
			} else {
				return false;
			}
		} else if (len == 6) {
			if (strncasecmp(value, "method", 6) == 0) {
				criterion = rulelist::BY_METHOD;
			} else {
				return false;
			}
		} else if (len == 14) {
			if (strncasecmp(value, "file_extension", 14) == 0) {
				criterion = rulelist::BY_EXTENSION;
			} else {
				return false;
			}
		} else {
			return false;
		}

		rulelist::rule* rule;
		if ((rule = new (std::nothrow) rulelist::rule) == NULL) {
			return false;
		}

		unsigned j;

		if (criterion == rulelist::BY_PATH) {
			for (j = 0; conf.get_child(j, value, len, "config", "hosts", host, "request_handling", name, "values", NULL); j++) {
				if (!rule->paths.add(value, len)) {
					delete rule;
					return false;
				}
			}
		} else if (criterion == rulelist::BY_EXTENSION) {
			for (j = 0; conf.get_child(j, value, len, "config", "hosts", host, "request_handling", name, "values", NULL); j++) {
				if (!rule->extensions.add(value, len)) {
					delete rule;
					return false;
				}
			}
		} else {
			for (j = 0; conf.get_child(j, value, len, "config", "hosts", host, "request_handling", name, "values", NULL); j++) {
				unsigned char method;
				if ((method = http_method::get_method(value, len)) == http_method::UNKNOWN) {
					delete rule;
					return false;
				}

				rule->add(method);
			}
		}

		if (j == 0) {
			delete rule;
			return false;
		}

		rule->backends.set_retry_interval(general_conf.backend_retry_interval);

		if (handler != rulelist::LOCAL_HANDLER) {
			for (j = 0; conf.get_child(j, value, len, "config", "hosts", host, "request_handling", name, "backends", NULL); j++) {
				unsigned port;

				const char* semicolon = (const char*) memrchr(value, ':', len);
				if (semicolon) {
					if (number::parse_unsigned(semicolon + 1, (value + len) - (semicolon + 1), port, 1, 65535) != number::PARSE_SUCCEEDED) {
						delete rule;
						return false;
					}

					len = semicolon - value;
				} else {
					if (handler != rulelist::HTTP_HANDLER) {
						delete rule;
						return false;
					}

					port = url_parser::HTTP_DEFAULT_PORT;
				}

				if (len > url_parser::HOST_MAX_LEN) {
					delete rule;
					return false;
				}

				if (!rule->backends.add(value, len, port)) {
					delete rule;
					return false;
				}
			}

			if (j == 0) {
				delete rule;
				return false;
			}
		}

		rule->handler = handler;
		rule->criterion = criterion;

		if (!rules->add(rule)) {
			delete rule;
			return false;
		}
	}

	return true;
}

bool http_server::get_dirname_basename(const char* path, size_t pathlen, char* dir, const char*& base)
{
	const char* slash = (const char*) memrchr(path, '/', pathlen);

	if (!slash) {
		*dir = '.';
		*(dir + 1) = 0;

		base = path;
	} else {
		size_t l = slash - path;
		if (l > PATH_MAX) {
			return false;
		}

		base = slash + 1;
		if (base == path + pathlen) {
			return false;
		}

		memcpy(dir, path, l);
		dir[l] = 0;
	}

	return true;
}

void http_server::process_ready_list()
{
	logger::instance().log(logger::LOG_DEBUG, "[http_server::process_ready_list] Processing %d connection(s) from the ready list.", _M_nready);

	unsigned nready = _M_nready;
	for (unsigned i = 0; i < nready; i++) {
		if (!process_connection(_M_ready_list[i], 0)) {
			remove(_M_ready_list[i]);
		}
	}

	for (unsigned i = nready; i < _M_nready; i++) {
		_M_ready_list[i - nready] = _M_ready_list[i];
	}

	_M_nready -= nready;
}

bool http_server::process_connection(unsigned fd, int events)
{
	tcp_connection* conn = _M_connections[fd];

	if (_M_connection_handlers[fd] == rulelist::LOCAL_HANDLER) {
		conn = &_M_http_connections[fd];
	} else if (_M_connection_handlers[fd] == rulelist::HTTP_HANDLER) {
		conn = &_M_proxy_connections[fd];
	} else if (_M_connection_handlers[fd] == rulelist::FCGI_HANDLER) {
		conn = &_M_fcgi_connections[fd];
	}

	if ((events & READ) != 0) {
		conn->_M_readable = 1;
	}

	if ((events & WRITE) != 0) {
		conn->_M_writable = 1;
	}

	if (events) {
		logger::instance().log(logger::LOG_DEBUG, "%s event for fd %d.", ((conn->_M_readable) && (conn->_M_writable)) ? "READ & WRITE" : conn->_M_readable ? "READ" : "WRITE", fd);
	} else {
		conn->_M_in_ready_list = 0;
	}

	if (!conn->loop(fd)) {
		if (_M_connection_handlers[fd] != rulelist::LOCAL_HANDLER) {
			int sock;
			if (_M_connection_handlers[fd] == rulelist::HTTP_HANDLER) {
				sock = static_cast<proxy_connection*>(conn)->_M_fd;
			} else {
				sock = static_cast<fcgi_connection*>(conn)->_M_fd;
			}

			http_connection* client = static_cast<http_connection*>(&_M_http_connections[sock]);

			client->_M_fd = -1;

			if (client->_M_in_ready_list) {
				client->_M_in_ready_list = 0;

				if (!client->loop(sock)) {
					client->free();
					remove(sock);
				}
			}
		}

		conn->free();

		return false;
	}

	return true;
}

void http_server::handle_alarm()
{
	logger::instance().log(logger::LOG_DEBUG, "Handling alarm...");

	// Drop connections without activity.
	size_t i = 1;
	while (i < _M_used) {
		unsigned fd = _M_index[i];

		tcp_connection* conn;

		if (_M_connection_handlers[fd] == rulelist::LOCAL_HANDLER) {
			conn = &_M_http_connections[fd];
		} else if (_M_connection_handlers[fd] == rulelist::HTTP_HANDLER) {
			conn = &_M_proxy_connections[fd];
		} else {
			conn = &_M_fcgi_connections[fd];
		}

		if (conn->_M_timestamp + (time_t) _M_max_idle_time < now::_M_time) {
			logger::instance().log(logger::LOG_INFO, "Connection fd %d timed out.", fd);

			remove(fd);
			conn->free();
		} else {
			if ((_M_connection_handlers[fd] == rulelist::HTTP_HANDLER) && \
			    (static_cast<proxy_connection*>(conn)->_M_state == proxy_connection::READING_UNKNOWN_SIZE_BODY_STATE) && \
			    (static_cast<proxy_connection*>(conn)->_M_timestamp + (time_t) _M_max_idle_time_unknown_size_body < now::_M_time) && \
			    (static_cast<proxy_connection*>(conn)->_M_client->_M_filesize > 0)) {
				logger::instance().log(logger::LOG_DEBUG, "[http_server::handle_alarm] Adding connection fd %d to the ready list, already read %lld bytes.", fd, static_cast<proxy_connection*>(conn)->_M_client->_M_filesize);

				conn->_M_in_ready_list = 1;

				// Add to ready list.
				_M_ready_list[_M_nready++] = fd;

				static_cast<proxy_connection*>(conn)->_M_state = proxy_connection::RESPONSE_COMPLETED_STATE;
			}

			i++;
		}
	}

	if (++_M_sync_count == _M_sync_interval) {
		_M_vhosts.sync();
		_M_sync_count = 0;
	}
}
