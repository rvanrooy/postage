#include "http_action.h"

void http_action_step1(struct sock_ev_client *client) {
	SDEFINE_VAR_ALL(str_uri, str_uri_temp, str_action_name, str_temp, str_args, str_sql);
	char *str_response = NULL;
	char *ptr_end_uri = NULL;
	ssize_t int_len = 0;
	size_t int_uri_length = 0;
	size_t int_query_length = 0;

	str_uri = str_uri_path(client->str_request, client->int_request_len, &int_uri_length);
	SFINISH_CHECK(str_uri != NULL, "str_uri_path failed");
	ptr_end_uri = strchr(str_uri, '?');
	if (ptr_end_uri != NULL) {
		*ptr_end_uri = 0;
		ptr_end_uri = strchr(ptr_end_uri + 1, '#');
		if (ptr_end_uri != NULL) {
			*ptr_end_uri = 0;
		}
	}

	str_args = query(client->str_request, client->int_request_len, &int_query_length);
	if (str_args == NULL) {
		SFINISH_CAT_CSTR(str_args, "");
	}

#ifdef ENVELOPE
#else
	if (isdigit(str_uri[9])) {
		str_uri_temp = str_uri;
		char *ptr_temp = strchr(str_uri_temp + 9, '/');
		SFINISH_CHECK(ptr_temp != NULL, "strchr failed");
		SFINISH_CAT_CSTR(str_uri, "/postage/app", ptr_temp);
		SFREE(str_uri_temp);
	}
#endif

	SDEBUG("str_args: %s", str_args);

	str_temp = str_args;
	str_args = DB_escape_literal(client->conn, str_temp, strlen(str_args));
	SFINISH_CHECK(str_args != NULL, "DB_escape_literal failed");

	SDEBUG("str_args: %s", str_args);

#ifdef ENVELOPE
	SFINISH_CAT_CSTR(str_action_name, str_uri + strlen("/env/"));
#else
	SFINISH_CAT_CSTR(str_action_name, str_uri + strlen("/postage/app/"));
#endif
	if (DB_connection_driver(client->conn) == DB_DRIVER_POSTGRES) {
		SFINISH_CAT_CSTR(str_sql, "SELECT ", str_action_name, "(", str_args, ");");
	} else {
		SFINISH_CAT_CSTR(str_sql, "EXECUTE ", str_action_name, " ", str_args, ";");
	}
	SFINISH_CHECK(DB_exec(global_loop, client->conn, client, str_sql, http_action_step2), "DB_exec failed");
	SDEBUG("str_sql: %s", str_sql);

	bol_error_state = false;
finish:
	SFREE_ALL();
	if (bol_error_state) {
		char *_str_response = str_response;
		str_response = cat_cstr("HTTP/1.1 500 Internal Server Error\015\012"
								"Server: " SUN_PROGRAM_LOWER_NAME "\015\012\015\012",
			_str_response);
	}
	if (str_response != NULL && (int_len = CLIENT_WRITE(client, str_response, strlen(str_response))) < 0) {
		SFREE(str_response);
		if (bol_tls) {
			SERROR_NORESPONSE_LIBTLS_CONTEXT(client->tls_postage_io_context, "tls_write() failed");
		} else {
			SERROR_NORESPONSE("write() failed");
		}
	}
	SFREE(str_response);
	if (int_len != 0) {
		ev_io_stop(global_loop, &client->io);
		SFREE(client->str_request);
		SERROR_CHECK_NORESPONSE(client_close(client), "Error closing Client");
	}
}

bool http_action_step2(EV_P, void *cb_data, DB_result *res) {
	struct sock_ev_client_copy_io *client_copy_io = NULL;
	struct sock_ev_client_copy_check *client_copy_check = NULL;
	struct sock_ev_client *client = cb_data;
	char *str_response = NULL;
	char *_str_response = NULL;
	ssize_t int_len = 0;
	DArray *arr_row_values = NULL;

	SFINISH_CHECK(res != NULL, "DB_exec failed, res == NULL");
	SFINISH_CHECK(res->status == DB_RES_TUPLES_OK, "DB_exec failed, res->status == %d", res->status);

	SFINISH_CHECK(DB_fetch_row(res) == DB_FETCH_OK, "DB_fetch_row failed");
	arr_row_values = DB_get_row_values(res);
	SFINISH_CHECK(arr_row_values != NULL, "DB_get_row_values failed");

	_str_response = DArray_get(arr_row_values, 0);
	SFINISH_CAT_CSTR(str_response, "HTTP/1.1 200 OK\015\012"
								   "Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
								   "Content-Type: application/json; charset=UTF-8\015\012\015\012"
								   "{\"stat\":true, \"dat\": ",
		_str_response, "}");
	SFREE(_str_response);
	SDEBUG("str_response: %s", str_response);

	client->cur_request = create_request(client, NULL, NULL, NULL, NULL, 0, POSTAGE_REQ_ACTION);
	SFINISH_CHECK(client->cur_request != NULL, "create_request failed!");
	SFINISH_SALLOC(client_copy_check, sizeof(struct sock_ev_client_copy_check));

	client_copy_check->str_response = str_response;
	str_response = NULL;
	client_copy_check->int_response_len = (ssize_t)strlen(client_copy_check->str_response);
	client_copy_check->client_request = client->cur_request;

	SFINISH_SALLOC(client_copy_io, sizeof(struct sock_ev_client_copy_io));

	client_copy_io->client_copy_check = client_copy_check;

	ev_io_init(&client_copy_io->io, http_action_step3, GET_CLIENT_SOCKET(client), EV_WRITE);
	ev_io_start(EV_A, &client_copy_io->io);

	bol_error_state = false;
finish:
	if (arr_row_values != NULL) {
		DArray_destroy(arr_row_values);
	}
	SFREE(_str_response);
	_str_response = str_response;
	if (bol_error_state) {
		bol_error_state = false;

		str_response = cat_cstr("HTTP/1.1 500 Internal Server Error\015\012"
								"Server: " SUN_PROGRAM_LOWER_NAME "\015\012\015\012",
			_str_response);
		SFREE(_str_response);
		_str_response = DB_get_diagnostic(client->conn, res);
		SFINISH_CAT_APPEND(str_response, ":\n", _str_response);
		SFREE(_str_response);
	}
	if (str_response != NULL && (int_len = CLIENT_WRITE(client, str_response, strlen(str_response))) < 0) {
		SFREE(str_response);
		if (bol_tls) {
			SERROR_NORESPONSE_LIBTLS_CONTEXT(client->tls_postage_io_context, "tls_write() failed");
		} else {
			SERROR_NORESPONSE("write() failed");
		}
	}
	DB_free_result(res);
	SFREE(str_response);
	if (int_len != 0) {
		ev_io_stop(EV_A, &client->io);
		SFREE(client->str_request);
		SERROR_CHECK_NORESPONSE(client_close(client), "Error closing Client");
	}
	return true;
}

void http_action_step3(EV_P, ev_io *w, int revents) {
	if (revents != 0) {
	} // get rid of unused parameter warning
	struct sock_ev_client_copy_io *client_copy_io = (struct sock_ev_client_copy_io *)w;
	struct sock_ev_client_copy_check *client_copy_check = client_copy_io->client_copy_check;
	struct sock_ev_client_request *client_request = client_copy_check->client_request;
	struct sock_ev_client *client = client_request->parent;
	char *str_response = NULL;
	char *_str_response = NULL;

	// SDEBUG("client_copy_check->str_response: %s", client_copy_check->str_response);

	ssize_t int_response_len =
		CLIENT_WRITE(client_request->parent, client_copy_check->str_response + client_copy_check->int_written,
		(size_t)(client_copy_check->int_response_len - client_copy_check->int_written));

	SDEBUG("write(%i, %p, %i): %z", client_request->parent->int_sock,
		client_copy_check->str_response + client_copy_check->int_written,
		client_copy_check->int_response_len - client_copy_check->int_written, int_response_len);

	if (int_response_len == -1 && errno != EAGAIN) {
		if (bol_tls) {
			SFINISH_LIBTLS_CONTEXT(client_request->parent->tls_postage_io_context, "tls_write() failed");
		} else {
			SFINISH("write(%i, %p, %i) failed: %i", client_request->parent->int_sock,
				client_copy_check->str_response + client_copy_check->int_written,
				client_copy_check->int_response_len - client_copy_check->int_written, int_response_len);
		}
	} else if (int_response_len == TLS_WANT_POLLIN) {
		ev_io_stop(EV_A, w);
		ev_io_set(w, GET_CLIENT_SOCKET(client_request->parent), EV_READ);
		ev_io_start(EV_A, w);
		bol_error_state = false;
		errno = 0;
		return;

	} else if (int_response_len == TLS_WANT_POLLOUT) {
		ev_io_stop(EV_A, w);
		ev_io_set(w, GET_CLIENT_SOCKET(client_request->parent), EV_WRITE);
		ev_io_start(EV_A, w);
		bol_error_state = false;
		errno = 0;
		return;

	} else {
		// int_response_len can't be negative at this point
		client_copy_check->int_written += (ssize_t)int_response_len;
	}

	if (client_copy_check->int_written == client_copy_check->int_response_len) {
		ev_io_stop(EV_A, w);

		SFREE(client_copy_check->str_response);
		SFREE(client_copy_check);
		SFREE(client_copy_io);

		SDEBUG("DONE");
		struct sock_ev_client *client = client_request->parent;
		SFINISH_CLIENT_CLOSE(client);
		client_request = NULL;
	}

	// If this line is not here, we close the client below
	// then libev calls a function on the socket and crashes and burns on windows
	// so... don't touch
	int_response_len = 0;

	bol_error_state = false;
finish:
	_str_response = str_response;
	if (bol_error_state) {
		if (client_copy_check != NULL) {
			ev_io_stop(EV_A, w);
			SFREE(client_copy_check->str_response);
			SFREE(client_copy_check);
			SFREE(client_copy_io);
		}
		str_response = NULL;
		bol_error_state = false;

		str_response = cat_cstr("HTTP/1.1 500 Internal Server Error\015\012"
			"Server: " SUN_PROGRAM_LOWER_NAME "\015\012\015\012",
			_str_response);
		SFREE(_str_response);
	}
	if (str_response != NULL) {
		int_response_len = CLIENT_WRITE(client_request->parent, str_response, strlen(str_response));
		SDEBUG("int_response_len: %d", int_response_len);
		if (int_response_len < 0) {
			if (bol_tls) {
				SERROR_NORESPONSE_LIBTLS_CONTEXT(client_request->parent->tls_postage_io_context, "tls_write() failed");
			} else {
				SERROR_NORESPONSE("write() failed");
			}
		}
	}
	SFREE(str_response);
	if (int_response_len != 0) {
		ev_io_stop(EV_A, &client_request->parent->io);
		SFREE(client_request->parent->str_request);
		SDEBUG("ERROR");
		SERROR_CHECK_NORESPONSE(client_close(client_request->parent), "Error closing Client");
	}
}