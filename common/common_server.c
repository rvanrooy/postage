#include "common_server.h"

struct ev_loop *global_loop = NULL;
bool bol_tls;
struct sock_ev_serv _server;

// set a socket to not blocking
int setnonblock(SOCKET int_sock) {
#ifdef _WIN32
	u_long mode = 1;
	return ioctlsocket(int_sock, FIONBIO, &mode);
#else
	int flags;
	flags = fcntl(int_sock, F_GETFL);
	flags |= O_NONBLOCK;
	return fcntl(int_sock, F_SETFL, flags);
#endif
}

// accept new client on a new connection
void server_cb(EV_P, ev_io *w, int revents) {
	if (revents != 0) {
	} // get rid of unused parameter warning
	SDEBUG("TCP stream socket has become readable");

	SOCKET int_client_sock;

	// since ev_io is the first member, watcher `w` has the address of the start
	// of the sock_ev_serv struct
	struct sock_ev_serv *server = (struct sock_ev_serv *)w;
	struct sockaddr_in client_address;
	socklen_t int_client_len = sizeof(client_address);

	// this will break out when there are no more clients waiting to connect
	while (1) {
		errno = 0;
		int_client_sock = accept(server->int_sock, (struct sockaddr *)&client_address, &int_client_len);
		if (int_client_sock == INVALID_SOCKET) {
			if (errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
				printf("accept() failed errno: %i (%s)\012", errno, strerror(errno));
				abort();
			} else {
				errno = 0;
			}

			// this is where it breaks out, notice if there is an error it doesn't get
			// here
			break;
		}
		SDEBUG("accepted a client");

		struct sock_ev_client *client = NULL;
		SERROR_SALLOC(client, sizeof(struct sock_ev_client));
		SERROR_CHECK(List_push(server->list_client, client) == true, "Failed to push client to array");
		SDEBUG("Client %p opened", client);
		client->_int_sock = int_client_sock;
		client->int_sock = (int)int_client_sock;
		client->server = server;
		client->bol_fast_close = false;
		client->int_last_activity_i = -1;
		client->node = server->list_client->last;
		client->bol_handshake = false;
		client->bol_connected = false;
		client->bol_socket_is_open = true;
		client->bol_upload = false;
		client->str_boundary = NULL;
		client->int_boundary_length = 0;
		client->conn = NULL;
		client->bol_is_open = true;
		SERROR_CAT_CSTR(client->str_request, "");
		client->str_response = NULL;
		client->str_message = NULL;
		client->str_cookie = NULL;
		client->str_conn = NULL;
		client->str_connname = NULL;
		client->que_message = NULL;
		client->client_paused_request = NULL;
		client->client_timeout_prepare = NULL;
		client->que_request = NULL;
		client->bol_request_in_progress = false;
		client->notify_watcher = NULL;
		client->int_request_len = 0;

		setnonblock(client->int_sock);

		if (inet_ntop(AF_INET, &client_address.sin_addr.s_addr, client->str_client_ip, sizeof(client->str_client_ip)) != NULL) {
			SDEBUG("Got connection from %s:%d", client->str_client_ip, ntohs(client_address.sin_port));
		} else {
			SDEBUG("Unable to get address");
		}

		errno = 0;

		// DEBUG("client->bol_handshake == %s", client->bol_handshake ? "true" :
		// "false");
		if (bol_tls) {
			// The tls accept command takes the socket you send and makes a tls I/O
			// context out of it
			int int_status =
				tls_accept_socket(client->server->tls_postage_context, &client->tls_postage_io_context, client->int_sock);
			if (int_status != 0) {
				SERROR_LIBTLS_CONTEXT(client->server->tls_postage_context, "tls_accept_fds() failed");
			}
			// we don't need to use tls_handshake becuase tls_read/write handle it
			// automatically
			SDEBUG("client->tls_postage_io_context: %p", client->tls_postage_io_context);
		}

#ifdef _WIN32
		// HANDLE h_dup_handle = 0;
		// SERROR_CHECK(DuplicateHandle(GetCurrentProcess(), client->_int_sock,
		// GetCurrentProcess(), &h_dup_handle, 0, FALSE,
		// DUPLICATE_SAME_ACCESS), "WSADuplicateHandle failed: %x",
		// WSAGetLastError());

		client->int_sock = _open_osfhandle(client->_int_sock, 0); // h_dup_handle
		SERROR_CHECK(client->int_sock != -1, "_open_osfhandle failed!");
		SDEBUG("Windows fd: %d", client->int_sock);
#endif
		ev_io_init(&client->io, client_cb, GET_CLIENT_SOCKET(client), EV_READ);
		ev_io_start(EV_A, &client->io);
	}
	bol_error_state = false;
error:
	return;
}

void client_last_activity_free(struct sock_ev_client_last_activity *client_last_activity) {
	if (client_last_activity != NULL) {
		SFREE(client_last_activity->str_cookie);
	}
	SFREE(client_last_activity);
}
