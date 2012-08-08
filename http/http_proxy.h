#ifndef HTTP_PROXY_H
#define HTTP_PROXY_H

#include "http_server.h"
#include "net/socket_wrapper.h"
#include "net/url_parser.h"

#define CONNECTING_TO_SERVER_STATE              10
#define SENDING_REQUEST_TO_SERVER_STATE         11
#define READING_RESPONSE_STATE                  12
#define UNKNOWN_CONTENT_LENGTH_RESPONSE_STATE   13
#define CHUNKED_RESPONSE_FROM_PROXY_STATE       14
#define CHUNKED_RESPONSE_FROM_SERVER_STATE      15
#define CHUNKED_TO_UNKNOWN_CONTENT_LENGTH_STATE 16
#define WRITING_RESPONSE_TO_CLIENT_STATE        17
#define WRITING_USER_DATA_TO_CLIENT_STATE       18
#define BAD_GATEWAY_STATE                       19

class http_proxy : public http_server {
	public:
		// Constructor.
		http_proxy(unsigned short max_idle_time = MAX_IDLE_TIME);

		// Destructor.
		virtual ~http_proxy() {};

		// Create.
		virtual bool create(const char* address, unsigned short port);

		virtual bool create(unsigned short port)
		{
			return create("0.0.0.0", port);
		}

	protected:
		static const unsigned STATUS_LINE_MAX_LEN;

		static const unsigned BYTES_TO_ACCUMULATE_FROM_SERVER;
		static const unsigned BYTES_TO_ACCUMULATE_FROM_CLIENT;

		// Allow.
		enum allow_result {
			ALLOW,
			FORBID,
			ALLOW_ERROR
		};

		virtual allow_result allow(struct sockaddr* addr)
		{
			return ALLOW;
		}

		virtual allow_result allow(connection* client)
		{
			return ALLOW;
		}

		// On event error.
		virtual void on_event_error(unsigned fd, void* data)
		{
			connection* conn = (connection*) data;

			if (conn->proxy == NO_PROXY) {
				delete_connection(fd, conn);
			} else {
				delete_both_connections(fd, conn->fd);
			}
		}

		// On new connection.
		virtual bool on_new_connection(unsigned fd, struct sockaddr* addr)
		{
			allow_result allow_result = allow(addr);
			if ((allow_result == FORBID) || (allow_result == ALLOW_ERROR)) {
				socket_wrapper::close(fd);
				return false;
			}

			return http_server::on_new_connection(fd, addr);
		}

		// On read.
		virtual bool on_read(unsigned fd, void* data)
		{
			connection* conn = (connection*) data;
			if ((conn->proxy == NO_PROXY) || (conn->proxy == CLIENT_CONNECTION)) {
				return on_read_from_client(fd, conn);
			} else {
				return on_read_from_server(fd, conn);
			}
		}

		// On read from client.
		virtual bool on_read_from_client(unsigned fd, connection* client);

		// On read from server.
		virtual bool on_read_from_server(unsigned fd, connection* server);

		// On write.
		virtual bool on_write(unsigned fd, void* data)
		{
			connection* conn = (connection*) data;
			if ((conn->proxy == NO_PROXY) || (conn->proxy == CLIENT_CONNECTION)) {
				return on_write_to_client(fd, conn);
			} else {
				return on_write_to_server(fd, conn);
			}
		}

		// On write to server.
		virtual bool on_write_to_server(unsigned fd, connection* server);

		// On write to client.
		virtual bool on_write_to_client(unsigned fd, connection* client);

		// Connected to HTTP server.
		virtual bool on_connected_to_server(connection* server);

		// Process request.
		virtual bool process_request(unsigned fd, connection* client);

		// Read headers from server.
		virtual bool read_headers_from_server(unsigned fd, connection* server, unsigned& total);

		// Prepare for reading from server.
		virtual bool prepare_for_reading_from_server(connection* server, connection* client);

		// Prepare for writing to server.
		virtual bool prepare_for_writing_to_server(connection* server, connection* client);

		// Prepare for reading from client.
		virtual bool prepare_for_reading_from_client(connection* client, connection* server);

		// Prepare for writing to client.
		virtual bool prepare_for_writing_to_client(connection* client, connection* server);

		// Process response.
		virtual bool process_response(connection* server);

		// Delete connection.
		virtual void delete_connection(unsigned fd, connection* conn)
		{
			conn->url.reset();
			http_server::delete_connection(fd, conn);
		}

		// Reset connection.
		virtual void reset_connection(connection* conn)
		{
			conn->url.reset();
			http_server::reset_connection(conn);
		}

		// Read status line.
		bool read_status_line(unsigned fd, connection* server, unsigned& total);

		// Parse status line.
		parse_result parse_status_line(connection* server);

		// Get Content-Length.
		enum content_length_result {
			CONTENT_LENGTH_NOT_PRESENT,
			INVALID_CONTENT_LENGTH,
			VALID_CONTENT_LENGTH
		};

		content_length_result get_content_length(connection* conn);

		// Chunked?
		bool chunked(connection* conn);

		// Proxy Keep-Alive?
		bool proxy_keep_alive(connection* conn);

		// Parse chunked body.
		parse_result parse_chunked_body(connection* conn, buffer* in, buffer* out = NULL);
};

#endif // HTTP_PROXY_H
