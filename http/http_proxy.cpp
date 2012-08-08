#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <new>
#include "http_proxy.h"
#include "http_method.h"
#include "net/dnscache.h"
#include "string/memcasemem.h"
#include "macros/macros.h"

#define UNKNOWN_CONTENT_LENGTH (-1)

const unsigned http_proxy::STATUS_LINE_MAX_LEN = 256;

const unsigned http_proxy::BYTES_TO_ACCUMULATE_FROM_SERVER = 8 * 1024;
const unsigned http_proxy::BYTES_TO_ACCUMULATE_FROM_CLIENT = 2 * 1024;

http_proxy::http_proxy(unsigned short max_idle_time) : http_server(max_idle_time)
{
}

bool http_proxy::create(const char* address, unsigned short port)
{
	if (!tcp_server::create(address, port)) {
		return false;
	}

	if ((_M_connections = new (std::nothrow) connection[_M_size]) == NULL) {
		return false;
	}

	if ((_M_ready_list = (unsigned*) malloc(_M_size * sizeof(unsigned))) == NULL) {
		return false;
	}

	for (size_t i = 0; i < _M_size; i++) {
		connection* conn = &(_M_connections[i]);

		conn->fd = -1;

		conn->buf.set_buffer_increment(READ_BUFFER_SIZE);
		conn->host.set_buffer_increment(32);

		conn->headers.set_max_line_length(HEADER_MAX_LINE_LEN);

		conn->proxy = NO_PROXY;
	}

	return true;
}

bool http_proxy::on_read_from_client(unsigned fd, connection* client)
{
	connection* server;
	if (client->fd >= 0) {
		server = &(_M_connections[client->fd]);
	} else {
		server = NULL;
	}

	read_result res;
	size_t count;

	unsigned prev;
	size_t total = 0;

	do {
		prev = client->state;
		switch (client->state) {
			case BEGIN_REQUEST_STATE:
				if (!read_request_line(fd, client, total)) {
					delete_connection(fd, client);
					return false;
				}

				break;
			case READING_HEADERS_STATE:
				if (!read_headers(fd, client, total)) {
					delete_connection(fd, client);
					return false;
				}

				break;
			case PROCESSING_REQUEST_STATE:
				if (!process_request(fd, client)) {
					delete_connection(fd, client);
					return false;
				}

				if (client->error != http_error::OK) {
					client->state = PREPARING_ERROR_PAGE_STATE;
				} else {
					return true;
				}

				break;
			case READING_BODY_STATE:
				count = server->buf.count();
				res = read(fd, client, &server->buf, total);
				if (res == READ_ERROR) {
					// Delete server connection.
					server->fd = -1;
					delete_connection(client->fd, server);
					remove(client->fd);

					// Delete client connection.
					client->fd = -1;
					delete_connection(fd, client);

					return false;
				} else if ((res == PARTIAL_READ) || (res == FULL_READ)) {
					size_t diff = server->buf.count() - count;

					// If the client has sent more data than it was supposed to...
					if (diff > client->filesize) {
						server->buf.set_count(count + client->filesize);
						client->filesize = 0;

						return prepare_for_writing_to_server(server, client);
					} else if (diff == client->filesize) {
						client->filesize = 0;

						return prepare_for_writing_to_server(server, client);
					} else {
						client->filesize -= diff;

						// If we have enough data to send to the server...
						if (server->buf.count() >= BYTES_TO_ACCUMULATE_FROM_CLIENT) {
							return prepare_for_writing_to_server(server, client);
						}

						// If we couldn't read more...
						if (res == PARTIAL_READ) {
							return true;
						}
					}
				}

				break;
			case PREPARING_ERROR_PAGE_STATE:
				if ((!prepare_error_page(client)) || (!modify(fd, WRITE, client))) {
					delete_connection(fd, client);
					return false;
				}

				return true;
		}
	} while (prev != client->state);

	return true;
}

bool http_proxy::on_read_from_server(unsigned fd, connection* server)
{
	connection* client = &(_M_connections[server->fd]);

	read_result res;
	size_t count;

	unsigned prev;
	size_t total = 0;

	do {
		prev = server->state;
		switch (server->state) {
			case READING_RESPONSE_STATE:
				if (!read_status_line(fd, server, total)) {
					client->error = http_error::BAD_GATEWAY;
					server->state = BAD_GATEWAY_STATE;
				}

				break;
			case READING_HEADERS_STATE:
				if (!read_headers_from_server(fd, server, total)) {
					client->error = http_error::BAD_GATEWAY;
					server->state = BAD_GATEWAY_STATE;
				}

				break;
			case PROCESSING_RESPONSE_STATE:
				if (!process_response(server)) {
					server->state = BAD_GATEWAY_STATE;
				}

				if (server->completed) {
					return prepare_for_writing_to_client(client, server);
				}

				break;
			case READING_BODY_STATE:
				// The "Content-Length" is known.

				count = client->buf.count();
				res = read(fd, server, &client->buf, total);
				if (res == READ_ERROR) {
					// Delete client connection.
					client->fd = -1;
					delete_connection(server->fd, client);
					remove(server->fd);

					// Delete server connection.
					server->fd = -1;
					delete_connection(fd, server);

					return false;
				} else if ((res == PARTIAL_READ) || (res == FULL_READ)) {
					size_t diff = client->buf.count() - count;

					// If the server has sent more data than it was supposed to...
					if (diff > server->filesize) {
						client->buf.set_count(count + server->filesize);
						server->completed = 1;

						return prepare_for_writing_to_client(client, server);
					} else if (diff == server->filesize) {
						server->completed = 1;

						return prepare_for_writing_to_client(client, server);
					} else {
						server->filesize -= diff;

						// If we have enough data to send to the client...
						if (client->buf.count() >= BYTES_TO_ACCUMULATE_FROM_SERVER) {
							return prepare_for_writing_to_client(client, server);
						}

						// If we couldn't read more...
						if (res == PARTIAL_READ) {
							return true;
						}
					}
				}

				break;
			case CHUNKED_RESPONSE_FROM_PROXY_STATE:
				// Client is HTTP/1.1 and the server didn't send the "Content-Length" header
				// and the transfer-coding received from the server is not chunked.

				res = read(fd, server, &server->buf, total);

				// If we couldn't read more from the server...
				if (res == READ_ERROR) {
					// We might have data in the buffer already.
					buffer* buf = &client->buf;
					count = server->buf.count() - server->offset;
					bool ret;
					if (count > 0) {
						ret = (buf->format("%x\r\n", count)) && (buf->append(server->buf.data() + server->offset, count)) && (buf->append("\r\n0\r\n\r\n", 7));
					} else {
						ret = buf->append("0\r\n\r\n", 5);
					}

					if (!ret) {
						// Delete client connection.
						client->fd = -1;
						delete_connection(server->fd, client);
						remove(server->fd);

						// Delete server connection.
						server->fd = -1;
						delete_connection(fd, server);

						return false;
					}

					server->completed = 1;
					return prepare_for_writing_to_client(client, server);
				} else if ((res == PARTIAL_READ) || (res == FULL_READ)) {
					count = server->buf.count() - server->offset;

					// If we have enough data to send to the client...
					if (count >= BYTES_TO_ACCUMULATE_FROM_SERVER) {
						buffer* buf = &client->buf;
						if ((!buf->format("%x\r\n", count)) || (!buf->append(server->buf.data() + server->offset, count)) || (!buf->append("\r\n", 2))) {
							// Delete client connection.
							client->fd = -1;
							delete_connection(server->fd, client);
							remove(server->fd);

							// Delete server connection.
							server->fd = -1;
							delete_connection(fd, server);

							return false;
						}

						server->buf.reset();
						server->offset = 0;

						return prepare_for_writing_to_client(client, server);
					}

					// If we couldn't read more...
					if (res == PARTIAL_READ) {
						return true;
					}
				}

				break;
			case CHUNKED_RESPONSE_FROM_SERVER_STATE:
				// Client is HTTP/1.1 and the transfer-coding received from the server is chunked.

				res = read(fd, server, &client->buf, total);
				if (res == READ_ERROR) {
					// Delete client connection.
					client->fd = -1;
					delete_connection(server->fd, client);
					remove(server->fd);

					// Delete server connection.
					server->fd = -1;
					delete_connection(fd, server);

					return false;
				} else if ((res == PARTIAL_READ) || (res == FULL_READ)) {
					parse_result parse_result = parse_chunked_body(client, &client->buf);
					if (parse_result == PARSE_ERROR) {
						// Delete client connection.
						client->fd = -1;
						delete_connection(server->fd, client);
						remove(server->fd);

						// Delete server connection.
						server->fd = -1;
						delete_connection(fd, server);

						return false;
					} else {
						if (parse_result == PARSING_NOT_COMPLETED) {
							// If we have enough data to send to the client...
							if (client->buf.count() >= BYTES_TO_ACCUMULATE_FROM_SERVER) {
								return prepare_for_writing_to_client(client, server);
							}

							// If we couldn't read more...
							if (res == PARTIAL_READ) {
								return true;
							}
						} else {
							server->completed = 1;

							return prepare_for_writing_to_client(client, server);
						}
					}
				}

				break;
			case CHUNKED_TO_UNKNOWN_CONTENT_LENGTH_STATE:
				// Client is HTTP/1.0 and the transfer-coding received from the server is chunked
				// (HTTP/1.0 clients don't support chunked transfer-coding).

				// If we have data in the buffer already...
				if (server->buf.count() - server->offset > 0) {
					res = FULL_READ;
				} else {
					res = read(fd, server, &server->buf, total);
				}

				if (res == READ_ERROR) {
					// Delete client connection.
					client->fd = -1;
					delete_connection(server->fd, client);
					remove(server->fd);

					// Delete server connection.
					server->fd = -1;
					delete_connection(fd, server);

					return false;
				} else if ((res == PARTIAL_READ) || (res == FULL_READ)) {
					parse_result parse_result = parse_chunked_body(server, &server->buf, &client->buf);
					if (parse_result == PARSE_ERROR) {
						// Delete client connection.
						client->fd = -1;
						delete_connection(server->fd, client);
						remove(server->fd);

						// Delete server connection.
						server->fd = -1;
						delete_connection(fd, server);

						return false;
					} else {
						if (parse_result == PARSING_NOT_COMPLETED) {
							server->buf.reset();
							server->offset = 0;

							// If we have enough data to send to the client...
							if (client->buf.count() >= BYTES_TO_ACCUMULATE_FROM_SERVER) {
								return prepare_for_writing_to_client(client, server);
							}

							// If we couldn't read more...
							if (res == PARTIAL_READ) {
								return true;
							}
						} else {
							server->completed = 1;

							return prepare_for_writing_to_client(client, server);
						}
					}
				}

				break;
			case UNKNOWN_CONTENT_LENGTH_RESPONSE_STATE:
				// Client is HTTP/1.0 and the server didn't send the "Content-Length" header
				// and the transfer-coding received from the server is not chunked.

				res = read(fd, server, &client->buf, total);
				if (res == READ_ERROR) {
					server->completed = 1;
					return prepare_for_writing_to_client(client, server);
				} else if ((res == PARTIAL_READ) || (res == FULL_READ)) {
					// If we have enough data to send to the client...
					if (client->buf.count() >= BYTES_TO_ACCUMULATE_FROM_SERVER) {
						return prepare_for_writing_to_client(client, server);
					}

					// If we couldn't read more...
					if (res == PARTIAL_READ) {
						return true;
					}
				}

				break;
			case BAD_GATEWAY_STATE:
				client->fd = -1;
				if ((!prepare_error_page(client)) || (!modify(server->fd, WRITE, client))) {
					// Delete client connection.
					delete_connection(server->fd, client);
					remove(server->fd);
				} else {
					client->proxy = NO_PROXY;
				}

				// Delete server connection.
				server->fd = -1;
				delete_connection(fd, server);

				return false;
		}
	} while (prev != server->state);

	return true;
}

bool http_proxy::on_write_to_client(unsigned fd, connection* client)
{
	connection* server;
	if (client->fd >= 0) {
		server = &(_M_connections[client->fd]);
	} else {
		server = NULL;
	}

	write_result res;
	unsigned prev;
	size_t total = 0;

	do {
		prev = client->state;
		switch (client->state) {
			case SENDING_ERROR_PAGE_STATE:
				res = send_error_page(fd, client, total);
				if (res == WRITE_ERROR) {
					delete_connection(fd, client);
					return false;
				} else if (res == FULL_WRITE) {
					client->state = REQUEST_COMPLETED_STATE;
				}

				break;
			case WRITING_USER_DATA_TO_CLIENT_STATE:
				res = write(fd, client, &client->buf, total);
				if (res == WRITE_ERROR) {
					delete_connection(fd, client);
					return false;
				} else if (res == FULL_WRITE) {
					client->state = REQUEST_COMPLETED_STATE;
				}

				break;
			case WRITING_RESPONSE_TO_CLIENT_STATE:
				res = write(fd, client, &client->buf, total);
				if (res == WRITE_ERROR) {
					// Delete server connection.
					server->fd = -1;
					delete_connection(client->fd, server);
					remove(client->fd);

					// Delete client connection.
					client->fd = -1;
					delete_connection(fd, client);

					return false;
				} else if (res == FULL_WRITE) {
					if (server->completed) {
						client->state = REQUEST_COMPLETED_STATE;
					} else {
						return prepare_for_reading_from_server(server, client);
					}
				}

				break;
			case REQUEST_COMPLETED_STATE:
				if (server) {
					// Delete server connection.
					server->fd = -1;
					delete_connection(client->fd, server);
					remove(client->fd);

					client->fd = -1;
				}

				// Close connection?
				if ((!client->keep_alive) || (!modify(fd, READ, client))) {
					delete_connection(fd, client);
					return false;
				} else {
					reset_connection(client);
					return true;
				}

				break;
		}
	} while (prev != client->state);

	return true;
}

bool http_proxy::on_write_to_server(unsigned fd, connection* server)
{
	connection* client = &(_M_connections[server->fd]);

	write_result res;
	unsigned prev;
	size_t total = 0;

	do {
		prev = server->state;
		switch (server->state) {
			case CONNECTING_TO_SERVER_STATE:
				if (!on_connected_to_server(server)) {
					client->error = http_error::INTERNAL_SERVER_ERROR;
					server->state = BAD_GATEWAY_STATE;
				}

				break;
			case SENDING_REQUEST_TO_SERVER_STATE:
				res = write(fd, server, &server->buf, total);
				if (res == WRITE_ERROR) {
					client->error = http_error::BAD_GATEWAY;
					server->state = BAD_GATEWAY_STATE;
				} else if (res == FULL_WRITE) {
					server->buf.reset();

					server->offset = 0;

					if (client->filesize == 0) {
						server->state = READING_RESPONSE_STATE;

						if (!modify(fd, READ, server)) {
							client->error = http_error::INTERNAL_SERVER_ERROR;
							server->state = http_error::BAD_GATEWAY;
						} else {
							return true;
						}
					} else {
						return prepare_for_reading_from_client(client, server);
					}
				}

				break;
			case BAD_GATEWAY_STATE:
				client->fd = -1;
				if ((!prepare_error_page(client)) || (!modify(server->fd, WRITE, client))) {
					// Delete client connection.
					delete_connection(server->fd, client);
					remove(server->fd);
				} else {
					client->proxy = NO_PROXY;
				}

				// Delete server connection.
				server->fd = -1;
				delete_connection(fd, server);

				return false;
		}
	} while (prev != server->state);

	return true;
}

bool http_proxy::prepare_for_reading_from_server(connection* server, connection* client)
{
	if ((!remove(server->fd, WRITE)) || (!add(client->fd, READ, server))) {
		unsigned fd = server->fd;

		// Delete server connection.
		server->fd = -1;
		delete_connection(client->fd, server);
		remove(client->fd);

		// Delete client connection.
		client->fd = -1;
		delete_connection(fd, client);

		return false;
	}

	client->offset = 0;
	client->buf.reset();

	return true;
}

bool http_proxy::prepare_for_writing_to_server(connection* server, connection* client)
{
	if ((!remove(server->fd, READ)) || (!add(client->fd, WRITE, server))) {
		unsigned fd = server->fd;

		// Delete server connection.
		server->fd = -1;
		delete_connection(client->fd, server);
		remove(client->fd);

		// Delete client connection.
		client->fd = -1;
		delete_connection(fd, client);

		return false;
	}

	return true;
}

bool http_proxy::prepare_for_reading_from_client(connection* client, connection* server)
{
	if ((!remove(client->fd, WRITE)) || (!add(server->fd, READ, client))) {
		unsigned fd = server->fd;

		// Delete server connection.
		server->fd = -1;
		delete_connection(client->fd, server);

		// Delete client connection.
		client->fd = -1;
		delete_connection(fd, client);
		remove(fd);

		return false;
	}

	return true;
}

bool http_proxy::prepare_for_writing_to_client(connection* client, connection* server)
{
	if ((!remove(client->fd, READ)) || (!add(server->fd, WRITE, client))) {
		unsigned fd = server->fd;

		// Delete server connection.
		server->fd = -1;
		delete_connection(client->fd, server);

		// Delete client connection.
		client->fd = -1;
		delete_connection(fd, client);
		remove(fd);

		return false;
	}

	client->offset = 0;

	return true;
}

bool http_proxy::process_request(unsigned fd, connection* client)
{
	char* data = client->buf.data();
	data[client->uri + client->urilen] = 0;

	url_parser* url = &client->url;
	url_parser::parse_result parse_result;
	parse_result = url->parse(data + client->uri, client->urilen, false);
	if (parse_result == url_parser::PARSE_ERROR) {
		client->error = http_error::BAD_REQUEST;
		return true;
	} else if (parse_result == url_parser::FORBIDDEN) {
		client->error = http_error::FORBIDDEN;
		return true;
	}

	// If not HTTP...
	if (url->get_scheme() != scheme::HTTP) {
		not_found(client);
		return true;
	}

	// Allow?
	allow_result allow_result = allow(client);
	if (allow_result == ALLOW_ERROR) {
		client->error = http_error::INTERNAL_SERVER_ERROR;
		return true;
	} else if (allow_result == FORBID) {
		if (!modify(fd, WRITE, client)) {
			client->error = http_error::INTERNAL_SERVER_ERROR;
		} else {
			client->offset = 0;
			client->state = WRITING_USER_DATA_TO_CLIENT_STATE;

			client->error = http_error::OK;
		}

		return true;
	}

	if ((client->method != http_method::GET) && (client->method != http_method::HEAD)) {
		// Get Content-Length.
		content_length_result content_length_result = get_content_length(client);
		if (content_length_result == INVALID_CONTENT_LENGTH) {
			client->error = http_error::BAD_REQUEST;
			return true;
		} else if (content_length_result == CONTENT_LENGTH_NOT_PRESENT) {
			client->error = http_error::LENGTH_REQUIRED;
			return true;
		}
	} else {
		client->filesize = 0;
	}

	// Disable READ events from the client.
	if (!remove(fd, READ)) {
		return false;
	}

	// Get pointer to the host name.
	unsigned short hostlen;
	char* host = (char*) url->get_host(hostlen);
	if (!host) {
		not_found(client);
		return true;
	}

	// Save host and port.
	if (!client->host.append_nul_terminated_string(host, hostlen)) {
		client->error = http_error::INTERNAL_SERVER_ERROR;
		return true;
	}

	client->port = url->get_port();

	// Connect to HTTP server.
	if ((client->fd = socket_wrapper::connect(client->host.data(), client->port, _M_now)) < 0) {
		client->error = http_error::BAD_GATEWAY;
		return true;
	}

	if (!socket_wrapper::disable_nagle_algorithm(client->fd)) {
		socket_wrapper::close(client->fd);
		client->fd = -1;

		client->error = http_error::INTERNAL_SERVER_ERROR;
		return true;
	}

	connection* server = &(_M_connections[client->fd]);
	if (!add(client->fd, WRITE, server)) {
		socket_wrapper::close(client->fd);
		client->fd = -1;

		client->error = http_error::INTERNAL_SERVER_ERROR;
		return true;
	}

	server->fd = fd;

	server->state = CONNECTING_TO_SERVER_STATE;

	server->timestamp = _M_now;

	client->proxy = CLIENT_CONNECTION;
	server->proxy = SERVER_CONNECTION;

	client->error = http_error::OK;

	return true;
}

bool http_proxy::on_connected_to_server(connection* server)
{
	connection* client = &(_M_connections[server->fd]);

	if (proxy_keep_alive(client)) {
		client->headers.remove_known_header(http_headers::KEEP_ALIVE_HEADER);
	}

	// Remove headers.
	client->headers.remove_known_header(http_headers::PROXY_AUTHENTICATE_HEADER);
	client->headers.remove_known_header(http_headers::PROXY_AUTHORIZATION_HEADER);
	client->headers.remove_known_header(http_headers::PROXY_CONNECTION_HEADER);

	// Modify "Connection" header.
	if (!client->headers.add_known_header(http_headers::CONNECTION_HEADER, "close", 5, true)) {
		return false;
	}

	// Modify "Host" header.
	if (client->port == url_parser::HTTP_DEFAULT_PORT) {
		if (!client->headers.add_known_header(http_headers::HOST_HEADER, client->host.data(), client->host.count() - 1, true)) {
			return false;
		}
	} else {
		char host[HOST_MAX_LEN + 32];
		int len = snprintf(host, sizeof(host), "%s:%d", client->host.data(), client->port);
		if (!client->headers.add_known_header(http_headers::HOST_HEADER, host, len, true)) {
			return false;
		}
	}

	url_parser* url = &client->url;
	unsigned short pathlen, querylen, fragmentlen;
	const char* path = url->get_path(pathlen);
	url->get_query(querylen);
	url->get_fragment(fragmentlen);

	buffer& buf = server->buf;
	if (!buf.format("%s %.*s HTTP/1.1\r\n", http_method::get_method(client->method), pathlen + querylen + fragmentlen, path)) {
		return false;
	}

	if (!client->headers.serialize(buf)) {
		return false;
	}

	// If we have message body...
	if (client->filesize > 0) {
		// If we have already received something from the client...
		size_t count = MIN(client->buf.count() - client->offset, client->filesize);
		if (count > 0) {
			if (!buf.append(client->buf.data() + client->offset, count)) {
				return false;
			}

			client->filesize -= count;
			if (client->filesize > 0) {
				client->state = READING_BODY_STATE;
			}
		} else {
			client->state = READING_BODY_STATE;
		}
	}

	server->offset = 0;
	server->state = SENDING_REQUEST_TO_SERVER_STATE;
	server->substate = 0;

	server->completed = 0;

	return true;
}

bool http_proxy::read_status_line(unsigned fd, connection* server, unsigned& total)
{
	do {
		read_result res = read(fd, server, &server->buf, total);
		if (res == READ_ERROR) {
			return false;
		} else if (res == NO_DATA_READ) {
			return true;
		}

		parse_result parse_result = parse_status_line(server);
		if (parse_result == PARSE_ERROR) {
			return false;
		} else if (parse_result == PARSING_COMPLETED) {
			parse_result = parse_headers(server);
			if (parse_result == PARSE_ERROR) {
				return false;
			} else if (parse_result == PARSING_NOT_COMPLETED) {
				server->state = READING_HEADERS_STATE;
			} else {
				server->state = PROCESSING_RESPONSE_STATE;
			}

			return true;
		}

		// If we couldn't read more...
		if (res == PARTIAL_READ) {
			return true;
		}
	} while (true);
}

http_proxy::parse_result http_proxy::parse_status_line(connection* server)
{
	const char* data = server->buf.data();
	size_t len = server->buf.count();
	unsigned short offset = server->offset;

	while (offset < len) {
		unsigned char c = (unsigned char) data[offset];
		switch (server->substate) {
			case 0: // Initial state.
				if ((c == 'H') || (c == 'h')) {
					server->substate = 1; // [H]TTP/<major>.<minor>
				} else if (!IS_WHITE_SPACE(c)) {
					return PARSE_ERROR;
				}

				break;
			case 1: // [H]TTP/<major>.<minor>
				if ((c == 'T') || (c == 't')) {
					server->substate = 2; // H[T]TP/<major>.<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 2: // H[T]TP/<major>.<minor>
				if ((c == 'T') || (c == 't')) {
					server->substate = 3; // HT[T]P/<major>.<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 3: // HT[T]P/<major>.<minor>
				if ((c == 'P') || (c == 'p')) {
					server->substate = 4; // HTT[P]/<major>.<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 4: // HTT[P]/<major>.<minor>
				if (c == '/') {
					server->substate = 5; // HTTP[/]<major>.<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 5: // HTTP[/]<major>.<minor>
				if ((c >= '0') && (c <= '9')) {
					server->major_number = c - '0';
					if (server->major_number > 1) {
						return PARSE_ERROR;
					}

					server->substate = 6; // HTTP/[<major>].<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 6: // HTTP/[<major>].<minor>
				if (c == '.') {
					server->substate = 7; // HTTP/<major>[.]<minor>
				} else {
					return PARSE_ERROR;
				}

				break;
			case 7: // HTTP/<major>[.]<minor>
				if ((c >= '0') && (c <= '9')) {
					server->minor_number = c - '0';
					if ((server->major_number == 1) && (server->minor_number > 1)) {
						return PARSE_ERROR;
					}

					server->substate = 8; // HTTP/<major>.[<minor>]
				} else {
					return PARSE_ERROR;
				}

				break;
			case 8: // HTTP/<major>.[<minor>]
				if (IS_WHITE_SPACE(c)) {
					server->substate = 9; // Whitespace after HTTP-Version.
				} else {
					return PARSE_ERROR;
				}

				break;
			case 9: // Whitespace after HTTP-Version.
				if ((c >= '0') && (c <= '9')) {
					server->status_code = c - '0';

					server->status_code_offset = offset;

					server->substate = 10; // Status-Code.
				} else if (!IS_WHITE_SPACE(c)) {
					return PARSE_ERROR;
				}

				break;
			case 10: // Status-Code.
				if ((c >= '0') && (c <= '9')) {
					server->status_code = (server->status_code * 10) + (c - '0');
					if (server->status_code > 999) {
						return PARSE_ERROR;
					}
				} else if (IS_WHITE_SPACE(c)) {
					server->substate = 11; // Whitespace after Status-Code.
				} else if (c == '\r') {
					server->substate = 12; // '\r' at the end of status line.
				} else if (c == '\n') {
					server->end_of_line = LF;

					server->offset = ++offset;
					server->headers_offset = server->offset;

					return PARSING_COMPLETED;
				} else {
					return PARSE_ERROR;
				}

				break;
			case 11: // Whitespace after Status-Code.
				if (c == '\r') {
					server->substate = 12; // '\r' at the end of status line.
				} else if (c == '\n') {
					server->end_of_line = LF;

					server->offset = ++offset;
					server->headers_offset = server->offset;

					return PARSING_COMPLETED;
				} else if (c < ' ') {
					return PARSE_ERROR;
				}

				break;
			case 12: // '\r' at the end of status line.
				if (c == '\n') {
					server->end_of_line = CRLF;

					server->offset = ++offset;
					server->headers_offset = server->offset;

					return PARSING_COMPLETED;
				} else {
					return PARSE_ERROR;
				}

				break;
		}

		if (++offset == STATUS_LINE_MAX_LEN) {
			return PARSE_ERROR;
		}
	}

	server->offset = offset;

	return PARSING_NOT_COMPLETED;
}

bool http_proxy::read_headers_from_server(unsigned fd, connection* server, unsigned& total)
{
	do {
		read_result res = read(fd, server, &server->buf, total);
		if (res == READ_ERROR) {
			return false;
		} else if (res == NO_DATA_READ) {
			return true;
		}

		parse_result parse_result = parse_headers(server);
		if (parse_result == PARSE_ERROR) {
			return false;
		} else if (parse_result == PARSING_COMPLETED) {
			server->state = PROCESSING_RESPONSE_STATE;
			return true;
		}

		// If we couldn't read more...
		if (res == PARTIAL_READ) {
			return true;
		}
	} while (true);
}

bool http_proxy::process_response(connection* server)
{
	connection* client = &(_M_connections[server->fd]);
	http_headers* headers = &server->headers;

	if ((client->method != http_method::HEAD) && (server->status_code >= 200) && (server->status_code != 204) && (server->status_code != 304)) {
		// Chunked response?
		if (chunked(server)) {
			server->filesize = UNKNOWN_CONTENT_LENGTH;

			// If the client is not HTTP/1.1...
			if ((client->major_number != 1) || (client->minor_number != 1)) {
				headers->remove_known_header(http_headers::TRANSFER_ENCODING_HEADER);

				client->keep_alive = 0;

				server->state = CHUNKED_TO_UNKNOWN_CONTENT_LENGTH_STATE;
			} else {
				server->state = CHUNKED_RESPONSE_FROM_SERVER_STATE;
			}
		} else {
			// Get Content-Length.
			content_length_result content_length_result = get_content_length(server);
			if (content_length_result == INVALID_CONTENT_LENGTH) {
				client->error = http_error::BAD_GATEWAY;
				return false;
			} else if (content_length_result == CONTENT_LENGTH_NOT_PRESENT) {
				// If Keep-Alive...
				if (keep_alive(server)) {
					if (server->status_code != 302) {
						client->error = http_error::BAD_GATEWAY;
						return false;
					} else {
						server->filesize = 0;

						server->completed = 1;
					}
				} else {
					server->filesize = UNKNOWN_CONTENT_LENGTH;

					// If the client is not HTTP/1.1...
					if ((client->major_number != 1) || (client->minor_number != 1)) {
						client->keep_alive = 0;

						server->state = UNKNOWN_CONTENT_LENGTH_RESPONSE_STATE;
					} else {
						if (!headers->add_known_header(http_headers::TRANSFER_ENCODING_HEADER, "chunked", 7, true)) {
							client->error = http_error::INTERNAL_SERVER_ERROR;
							return false;
						}

						server->state = CHUNKED_RESPONSE_FROM_PROXY_STATE;
					}
				}
			} else {
				server->state = READING_BODY_STATE;
			}
		}
	} else {
		server->filesize = 0;

		server->completed = 1;
	}

	buffer& buf = client->buf;
	buf.reset();

	// Copy Status-Line to client's buffer.
	if (!buf.append("HTTP/1.1 ", 9)) {
		client->error = http_error::INTERNAL_SERVER_ERROR;
		return false;
	}

	if (server->end_of_line == CRLF) {
		if (!buf.append(server->buf.data() + server->status_code_offset, server->headers_offset - server->status_code_offset)) {
			client->error = http_error::INTERNAL_SERVER_ERROR;
			return false;
		}
	} else {
		if ((!buf.append(server->buf.data() + server->status_code_offset, server->headers_offset - server->status_code_offset - 1)) || (!buf.append("\r\n", 2))) {
			client->error = http_error::INTERNAL_SERVER_ERROR;
			return false;
		}
	}

	// Remove "Connection" header.
	headers->remove_known_header(http_headers::CONNECTION_HEADER);

	// Add "Proxy-Connection" header.
	const char* value;
	unsigned short valuelen;
	if (client->keep_alive) {
		value = "Keep-Alive";
		valuelen = 10;
	} else {
		value = "close",
		valuelen = 5;
	}

	if (!headers->add_known_header(http_headers::PROXY_CONNECTION_HEADER, value, valuelen, true)) {
		client->error = http_error::INTERNAL_SERVER_ERROR;
		return false;
	}

	if (!headers->serialize(buf)) {
		client->error = http_error::INTERNAL_SERVER_ERROR;
		return false;
	}

	if (server->completed) {
		client->state = WRITING_RESPONSE_TO_CLIENT_STATE;
		return true;
	}

	server->substate = 0;

	if (server->state == UNKNOWN_CONTENT_LENGTH_RESPONSE_STATE) {
		size_t count = server->buf.count() - server->offset;
		if (count > 0) {
			if (!buf.append(server->buf.data() + server->offset, count)) {
				client->error = http_error::INTERNAL_SERVER_ERROR;
				return false;
			}
		}
	} else if (server->state == CHUNKED_RESPONSE_FROM_SERVER_STATE) {
		off_t offset = server->offset;
		size_t count = server->buf.count() - offset;
		if (count > 0) {
			parse_result parse_result = parse_chunked_body(server, &server->buf);
			if (parse_result == PARSE_ERROR) {
				client->error = http_error::BAD_GATEWAY;
				return false;
			} else {
				count = server->offset - offset;
				if (!buf.append(server->buf.data() + offset, count)) {
					client->error = http_error::INTERNAL_SERVER_ERROR;
					return false;
				}

				if (parse_result == PARSING_NOT_COMPLETED) {
					client->filesize = server->filesize;
					client->offset = buf.count();
					client->substate = server->substate;
				} else {
					server->completed = 1;
				}
			}
		} else {
			client->filesize = server->filesize;
			client->offset = buf.count();
			client->substate = 0;
		}
	} else if (server->state == READING_BODY_STATE) {
		size_t count = MIN(server->buf.count() - server->offset, server->filesize);
		if (count > 0) {
			if (!buf.append(server->buf.data() + server->offset, count)) {
				client->error = http_error::INTERNAL_SERVER_ERROR;
				return false;
			}

			server->filesize -= count;
		}

		if (server->filesize == 0) {
			server->completed = 1;
		}
	}

	client->state = WRITING_RESPONSE_TO_CLIENT_STATE;

	return true;
}

http_proxy::content_length_result http_proxy::get_content_length(connection* conn)
{
	const char* value;
	if (!conn->headers.get_value_known_header(http_headers::CONTENT_LENGTH_HEADER, value)) {
		return CONTENT_LENGTH_NOT_PRESENT;
	}

	conn->filesize = 0;
	do {
		unsigned char c = *value;
		if (!c) {
			return VALID_CONTENT_LENGTH;
		}

		if ((c < '0') || (c > '9')) {
			return INVALID_CONTENT_LENGTH;
		}

		conn->filesize = (conn->filesize * 10) + (c - '0');

		value++;
	} while (true);
}

bool http_proxy::chunked(connection* conn)
{
	const char* value;
	unsigned short valuelen;
	if (conn->headers.get_value_known_header(http_headers::TRANSFER_ENCODING_HEADER, value, &valuelen)) {
		if (memcasemem(value, valuelen, "chunked", 7)) {
			return true;
		}
	}

	return false;
}

bool http_proxy::proxy_keep_alive(connection* conn)
{
	const char* value;
	unsigned short valuelen;
	if (conn->headers.get_value_known_header(http_headers::PROXY_CONNECTION_HEADER, value, &valuelen)) {
		if (memcasemem(value, valuelen, "close", 5)) {
			conn->keep_alive = 0;
			return false;
		} else if (memcasemem(value, valuelen, "Keep-Alive", 10)) {
			conn->keep_alive = 1;
			return true;
		}
	}

	if ((conn->major_number == 1) && (conn->minor_number == 1)) {
		conn->keep_alive = 1;
		return true;
	} else {
		conn->keep_alive = 0;
		return false;
	}
}

http_proxy::parse_result http_proxy::parse_chunked_body(connection* conn, buffer* in, buffer* out)
{
	const char* data = in->data() + conn->offset;
	const char* end = in->data() + in->count();
	const char* ptr = data;
	size_t pending;

	while (ptr < end) {
		unsigned char c = (unsigned char) *ptr;
		switch (conn->substate) {
			case 0: // Chunk size.
				if ((c >= '0') && (c <= '9')) {
					conn->filesize = c - '0';

					conn->substate = 1; // Reading chunk size.
				} else if ((c >= 'A') && (c <= 'F')) {
					conn->filesize = c - 'A' + 10;

					conn->substate = 1; // Reading chunk size.
				} else if ((c >= 'a') && (c <= 'f')) {
					conn->filesize = c - 'a' + 10;

					conn->substate = 1; // Reading chunk size.
				} else {
					return PARSE_ERROR;
				}

				break;
			case 1: // Reading chunk size.
				if ((c >= '0') && (c <= '9')) {
					conn->filesize = (conn->filesize * 16) + (c - '0');
				} else if ((c >= 'A') && (c <= 'F')) {
					conn->filesize = (conn->filesize * 16) + (c - 'A' + 10);
				} else if ((c >= 'a') && (c <= 'f')) {
					conn->filesize = (conn->filesize * 16) + (c - 'a' + 10);
				} else if ((c == ' ') || (c == '\t') || (c == ';')) {
					conn->substate = 2; // Chunk extension.
				} else if (c == '\r') {
					conn->substate = 3; // '\r' after chunk size.
				} else {
					return PARSE_ERROR;
				}

				break;
			case 2: // Chunk extension.
				if (c == '\r') {
					conn->substate = 3; // '\r' after chunk size.
				} else if ((c < ' ') && (c != '\t')) {
					return PARSE_ERROR;
				}

				break;
			case 3: // '\r' after chunk size.
				if (c == '\n') {
					if (conn->filesize > 0) {
						conn->substate = 4; // Chunk data.
					} else {
						conn->substate = 7; // Trailer.
					}
				} else {
					return PARSE_ERROR;
				}

				break;
			case 4: // Chunk data.
				pending = end - ptr;
				if (conn->filesize <= pending) {
					if ((out) && (!out->append(ptr, conn->filesize))) {
						return PARSE_ERROR;
					}

					ptr += conn->filesize;

					conn->substate = 5; // After chunk data.

					continue;
				} else {
					if ((out) && (!out->append(ptr, pending))) {
						return PARSE_ERROR;
					}

					conn->filesize -= pending;

					conn->offset += (end - data);

					return PARSING_NOT_COMPLETED;
				}

				break;
			case 5: // After chunk data.
				if (c == '\r') {
					conn->substate = 6; // '\r' after chunk data.
				} else {
					return PARSE_ERROR;
				}

				break;
			case 6: // '\r' after chunk data.
				if (c == '\n') {
					conn->substate = 0; // Chunk size.
				} else {
					return PARSE_ERROR;
				}

				break;
			case 7: // Trailer.
				if (c == '\r') {
					conn->substate = 10; // '\r' after trailer.
				} else if ((c >= ' ') || (c == '\t')) {
					conn->substate = 8; // Entity header.
				} else {
					return PARSE_ERROR;
				}

				break;
			case 8: // Entity header.
				if (c == '\r') {
					conn->substate = 9; // '\r' after entity header.
				} else if ((c < ' ') && (c != '\t')) {
					return PARSE_ERROR;
				}

				break;
			case 9: // '\r' after entity header.
				if (c == '\n') {
					conn->substate = 7; // Trailer.
				} else {
					return PARSE_ERROR;
				}

				break;
			case 10: // '\r' after trailer.
				if (c == '\n') {
					conn->offset += (++ptr - data);

					// Drop trailing characters (if any).
					in->set_count(ptr - in->data());

					return PARSING_COMPLETED;
				} else {
					return PARSE_ERROR;
				}

				break;
		}

		ptr++;
	}

	conn->offset += (end - data);

	return PARSING_NOT_COMPLETED;
}
