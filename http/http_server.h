#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <limits.h>
#include "net/tcp_server.h"
#include "http/http_connection.h"
#include "http/proxy_connection.h"
#include "http/fcgi_connection.h"
#include "http/virtual_hosts.h"
#include "http/index_file_finder.h"
#include "http/http_headers.h"
#include "xmlconf/xmlconf.h"
#include "mime/mime_types.h"
#include "file/tmpfiles_cache.h"
#include "logger/logger.h"

class http_server : public tcp_server {
	friend struct http_connection;
	friend struct proxy_connection;
	friend struct fcgi_connection;

	public:
		// Constructor.
		http_server();

		// Destructor.
		virtual ~http_server();

		// Create.
		virtual bool create(const char* config_file, const char* mime_types_file = NULL);

	protected:
		unsigned char* _M_connection_handlers;

		http_connection* _M_http_connections;
		proxy_connection* _M_proxy_connections;
		fcgi_connection* _M_fcgi_connections;

		virtual_hosts _M_vhosts;

		index_file_finder _M_index_file_finder;

		http_headers _M_headers;

		mime_types _M_mime_types;

		tmpfiles_cache _M_tmpfiles;

		unsigned _M_max_idle_time_unknown_size_body;

		size_t _M_max_payload_in_memory;

		unsigned _M_boundary;

		unsigned _M_sync_interval;
		unsigned _M_sync_count;

		// Create connections.
		virtual bool create_connections();

		// Delete connections.
		virtual void delete_connections();

		// On event.
		virtual bool on_event(unsigned fd, int events);

		enum tribool {
			TRIBOOL_UNDEFINED,
			TRIBOOL_TRUE,
			TRIBOOL_FALSE
		};

		struct general_conf {
			char address[16];
			unsigned short port;

			const char* payload_directory;
			unsigned max_spare_files;

			unsigned backend_retry_interval;

			tribool log_requests;
			const char* log_format;
			size_t log_buffer_size; // [KB]
			size_t access_log_max_file_size; // [KB]

			tribool have_dirlisting;

			const char* footer_file;

			logger::level level;

			char logdir[PATH_MAX + 1];
			const char* logfile;
			size_t error_log_max_file_size; // [KB]
		};

		// Load general.
		bool load_general(const xmlconf& conf, general_conf& general_conf);

		// Load hosts.
		bool load_hosts(const xmlconf& conf, const general_conf& general_conf);

		// Load rules.
		bool load_rules(const xmlconf& conf, const general_conf& general_conf, const char* host, rulelist* rules);

		// Get directory and file name.
		bool get_dirname_basename(const char* path, size_t pathlen, char* dir, const char*& base);

		// Process ready list.
		virtual void process_ready_list();

		// Process connection.
		bool process_connection(unsigned fd, int events);

		// Handle alarm.
		virtual void handle_alarm();
};

inline http_server::~http_server()
{
	delete_connections();
}

#endif // HTTP_SERVER_H
