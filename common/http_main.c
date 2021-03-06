#include "http_main.h"

bool http_client_info_cb(EV_P, void *cb_data, DB_result *res);

/*
All this function does is check the database connection, then call a callback
*/
void http_main_cnxn_cb(EV_P, void *cb_data, DB_conn *conn) {
	struct sock_ev_client *client = cb_data;
	char *str_response = NULL;
	char *_str_response = NULL;
	char *str_uri = NULL;
	char *str_uri_temp = NULL;
	char *str_sql = NULL;
	char *ptr_end_uri = NULL;
	char str_length[50];
	size_t int_uri_length = 0;

	SFINISH_CHECK(conn->int_status == 1, "%s", conn->str_response);

	str_uri = str_uri_path(client->str_request, client->int_request_len, &int_uri_length);
	SFINISH_CHECK(str_uri, "str_uri_path failed");

#ifdef ENVELOPE
#else
	if (isdigit(str_uri[9])) {
		str_uri_temp = str_uri;
		char *str_temp = strchr(str_uri_temp + 9, '/');
		SFINISH_CHECK(str_temp != NULL, "strchr failed");
		SFINISH_CAT_CSTR(str_uri, "/postage/app", str_temp);
		SFREE(str_uri_temp);
	}
#endif
	ptr_end_uri = strchr(str_uri, '?');
	if (ptr_end_uri != NULL) {
		*ptr_end_uri = 0;
	}
	ptr_end_uri = strchr(str_uri, '#');
	if (ptr_end_uri != NULL) {
		*ptr_end_uri = 0;
	}

	SDEBUG("str_uri: %s", str_uri);

#ifdef ENVELOPE
	if (strncmp(str_uri, "/env/upload", 12) == 0) {
#else
	if (strncmp(str_uri, "/postage/app/upload", 22) == 0) {
#endif
		http_upload_step1(client);
#ifdef ENVELOPE
#else
#ifdef POSTAGE_INTERFACE_LIBPQ
	} else if (strncmp(str_uri, "/postage/app/export", 22) == 0) {
		http_export_step1(client);
#endif
#endif
#ifdef ENVELOPE
	} else if (strncmp(str_uri, "/env/action_ev", 15) == 0) {
		http_ev_step1(client);
	} else if (strncmp(str_uri, "/env/action_select", 19) == 0) {
		http_select_step1(client);
	} else if (strncmp(str_uri, "/env/action_insert", 19) == 0) {
		http_insert_step1(client);
	} else if (strncmp(str_uri, "/env/action_update", 19) == 0) {
		http_update_step1(client);
	} else if (strncmp(str_uri, "/env/action_delete", 19) == 0) {
		http_delete_step1(client);
	} else if (strncmp(str_uri, "/env/action_info", 17) == 0) {
#else
	} else if (strncmp(str_uri, "/postage/app/action_ev", 23) == 0) {
		http_ev_step1(client);
	} else if (strncmp(str_uri, "/postage/app/action_info", 25) == 0) {
#endif
		if (DB_connection_driver(client->conn) == DB_DRIVER_POSTGRES) {
			SFINISH_CAT_CSTR(str_sql,
				"SELECT version() "
				"UNION ALL "
				"SELECT SESSION_USER::text "
				"UNION ALL "
				"SELECT g.rolname "
				"	FROM pg_catalog.pg_roles g "
				"	LEFT JOIN pg_catalog.pg_auth_members m ON m.roleid = g.oid "
				"	LEFT JOIN pg_catalog.pg_roles u ON m.member = u.oid "
				"	WHERE g.rolcanlogin = FALSE AND u.rolname = SESSION_USER::text AND g.rolname IS NOT NULL");
		} else {
			SFINISH_CAT_CSTR(str_sql, "SELECT CAST(@@VERSION AS nvarchar(MAX)), '0' AS srt "
									  "UNION ALL "
									  "SELECT CAST(SYSTEM_USER AS nvarchar(MAX)) AS user_group_name, '1' AS srt "
									  "UNION ALL "
									  "SELECT CAST([name] AS nvarchar(MAX)) AS user_group_name, '2' AS srt "
									  "	FROM ( "
									  "		SELECT *, is_member(name) AS [is_member] "
									  "			FROM [sys].[database_principals] "
									  "	) em "
									  "	WHERE [is_member] = 1 "
									  "	ORDER BY 2, 1");
		}

		SFINISH_CHECK(DB_exec(EV_A, client->conn, client, str_sql, http_client_info_cb), "DB_exec failed");

#ifdef ENVELOPE
	} else if (strstr(str_uri, "accept_") != NULL || strstr(str_uri, "acceptnc_") != NULL) {
		char *ptr_dot = strstr(str_uri, ".");
		if (
			(
				ptr_dot != NULL &&
				(
					strncmp(ptr_dot + 1, "accept_", 7) == 0 ||
					strncmp(ptr_dot + 1, "acceptnc_", 9) == 0
				)
			) ||
			strncmp(str_uri, "/env/accept_", 12) == 0 ||
			strncmp(str_uri, "/env/acceptnc_", 14) == 0
		) {
			http_accept_step1(client);
		} else {
			http_file_step1(client);
		}
	} else if (strstr(str_uri, "action_") != NULL || strstr(str_uri, "actionnc_") != NULL) {
		char *ptr_dot = strstr(str_uri, ".");
		if (
			(
				ptr_dot != NULL &&
				(
					strncmp(ptr_dot + 1, "action_", 7) == 0 ||
					strncmp(ptr_dot + 1, "actionnc_", 9) == 0
				)
			) ||
			strncmp(str_uri, "/env/action_", 12) == 0 ||
			strncmp(str_uri, "/env/actionnc_", 14) == 0
		) {
			http_action_step1(client);
		} else {
			http_file_step1(client);
		}
#endif
	} else {
		http_file_step1(client);
	}
finish:
	bol_error_state = false;
	if (str_response != NULL) {
		_str_response = str_response;
		snprintf(str_length, 50, "%zu", strlen(_str_response));
		str_response = cat_cstr("HTTP/1.1 500 Internal Server Error\015\012"
								"Content-Length: ",
			str_length, "\015\012\015\012", _str_response);
		SFREE(_str_response);
		ssize_t int_response_len = 0;
		if ((int_response_len = CLIENT_WRITE(client, str_response, strlen(str_response))) < 0) {
			if (bol_tls) {
				SERROR_NORESPONSE_LIBTLS_CONTEXT(client->tls_postage_io_context, "tls_write() failed");
			} else {
				SERROR_NORESPONSE("write() failed");
			}
		}
		if (str_response == client->str_response) {
			client->str_response = NULL;
		}
		SFREE(str_response);

		SFINISH_CLIENT_CLOSE(client);
	}
	SFREE(str_uri_temp);
	SFREE(str_uri);
	SFREE(str_sql);
	bol_error_state = false;
}

void http_main(struct sock_ev_client *client) {
	ev_io_stop(global_loop, &client->io);
	SDEBUG("http_main %p", client);
	client->bol_request_in_progress = true;
	SDEFINE_VAR_ALL(str_uri, str_conninfo);
	char *str_response = NULL;
	char *ptr_end_uri = NULL;
	size_t int_uri_length = 0;
	// get path
	str_uri = str_uri_path(client->str_request, client->int_request_len, &int_uri_length);
	SFINISH_CHECK(str_uri, "str_uri_path failed");

	ptr_end_uri = strchr(str_uri, '?');
	if (ptr_end_uri != NULL) {
		*ptr_end_uri = 0;
	}
	ptr_end_uri = strchr(str_uri, '#');
	if (ptr_end_uri != NULL) {
		*ptr_end_uri = 0;
	}

	SDEBUG("#################################################################################################");
	SDEBUG("str_uri: %s", str_uri);
	SDEBUG("#################################################################################################");

	SDEBUG("str_uri: %s", str_uri);
#ifdef ENVELOPE
	if (strncmp(str_uri, "/env/auth", 10) == 0 || strncmp(str_uri, "/env/auth/", 11) == 0) {
		SDEBUG("str_uri: %s", str_uri);

		struct sock_ev_client_auth *client_auth;
		SFINISH_SALLOC(client_auth, sizeof(struct sock_ev_client_auth));
		SDEBUG("client_auth: %p", client_auth);
		client_auth->parent = client;

		str_response = http_auth(client_auth);
	} else if (strncmp(str_uri, "/env", 4) == 0) {
		// set_cnxn does its own error handling
		SDEBUG("str_uri: %s", str_uri);

		if ((client->conn = set_cnxn(client, client->str_request, http_main_cnxn_cb)) == NULL) {
			SFINISH_CLIENT_CLOSE(client);
		}
		// DEBUG("str_conninfo: %s", str_conninfo);
	} else {
		http_file_step1(client);
	}
#else
	if (strncmp(str_uri, "/postage", 8) != 0) {
		SFINISH_CAT_CSTR(str_response, "HTTP/1.1 303 See Other\015\012"
									   "Location: /postage",
			str_uri, "\015\012\015\012");
	} else if (strncmp(str_uri, "/postage/auth", 14) == 0) {
		SDEBUG("str_uri: %s", str_uri);

		struct sock_ev_client_auth *client_auth;
		SFINISH_SALLOC(client_auth, sizeof(struct sock_ev_client_auth));
		SDEBUG("client_auth: %p", client_auth);
		client_auth->parent = client;

		str_response = http_auth(client_auth);
	} else if (strncmp(str_uri, "/postage", 8) == 0 && isdigit(str_uri[9])) {
		// set_cnxn does its own error handling
		SDEBUG("str_uri: %s", str_uri);

		if ((client->conn = set_cnxn(client, client->str_request, http_main_cnxn_cb)) == NULL) {
			SFINISH_CLIENT_CLOSE(client);
		}
		// DEBUG("str_conninfo: %s", str_conninfo);
	} else {
		http_file_step1(client);
	}
#endif
	SDEBUG("str_response: %s", str_response);

	bol_error_state = false;
finish:
	bol_error_state = false;
	SFREE_PWORD_ALL();
	ssize_t int_response_len = 0;
	if (str_response != NULL && (int_response_len = CLIENT_WRITE(client, str_response, strlen(str_response))) < 0) {
		if (bol_tls) {
			SERROR_NORESPONSE_LIBTLS_CONTEXT(client->tls_postage_io_context, "tls_write() failed");
		} else {
			SERROR_NORESPONSE("write() failed");
		}
	}
	SFREE(str_response);
	if (int_response_len != 0) {
		SFINISH_CLIENT_CLOSE(client);
	}
}

// **************************************************************************************
// **************************************************************************************
// ************************************ INFO CALLBACK ***********************************
// **************************************************************************************
// **************************************************************************************

// wait for response from group list query
bool http_client_info_cb(EV_P, void *cb_data, DB_result *res) {
	struct sock_ev_client *client = cb_data;
	char *str_response = NULL;
	bool bol_ret = true;
	DArray *arr_values = NULL;
	int int_i = 0;
	DB_fetch_status status = DB_FETCH_OK;
	SDEFINE_VAR_ALL(str_conn_desc, str_conn_desc_enc, str_user, str_json_user, str_version, str_json_version, str_groups);

	SFINISH_CHECK(res != NULL, "DB_exec failed");
	SFINISH_CHECK(res->status == DB_RES_TUPLES_OK, "DB_exec failed");

	SFINISH_CAT_CSTR(str_groups, "[");

	while ((status = DB_fetch_row(res)) != DB_FETCH_END) {
		SFINISH_CHECK(status != DB_FETCH_ERROR, "DB_fetch_row failed");

		arr_values = DB_get_row_values(res);
		SFINISH_CHECK(arr_values != NULL, "DB_get_row_values failed");

		if (int_i == 0) {
			SFINISH_CAT_CSTR(str_version, DArray_get(arr_values, 0));
		} else if (int_i == 1) {
			SFINISH_CAT_CSTR(str_user, DArray_get(arr_values, 0));
		} else {
			SFINISH_CAT_APPEND(str_groups, int_i == 2 ? "" : ", ", "\"", DArray_get(arr_values, 0), "\"");
		}

		DArray_clear_destroy(arr_values);
		arr_values = NULL;
		int_i += 1;
	}

	SFINISH_CAT_APPEND(str_groups, "]");

	SFINISH_CHECK((str_json_version = jsonify(str_version)), "jsonify failed");
	SFINISH_CHECK((str_json_user = jsonify(str_user)), "jsonify failed");

	SFINISH_CAT_CSTR(str_response, "{\"stat\": true, \"dat\": {", "\"username\": ", str_json_user, ", ", "\"session_user\": ",
		str_json_user, ", ", "\"current_user\": ", str_json_user, ", ", "\"groups\": ", str_groups, ", ", "\"port\": ",
		str_global_port, ", ", "\"database_version\": ", str_json_version, ",", "\"" SUN_PROGRAM_LOWER_NAME "\": \"", VERSION,
		"\"", "}}");

	/*
	SFINISH_CAT_CSTR(str_conn_desc, client->str_connname, "\t",
		(client->str_conn != NULL ? client->str_conn : get_connection_info(client->str_connname, NULL)));
	str_conn_desc_enc = escape_value(str_conn_desc);
	SFINISH_CHECK(str_conn_desc_enc != NULL, "escape_value failed!");
	SFINISH_CAT_CSTR(str_response, "VERSION\t", VERSION, "\012CONNECTION\t", str_conn_desc_enc, "\012GROUPS\t[");

	while ((status = DB_fetch_row(res)) != DB_FETCH_END) {
		SFINISH_CHECK(status != DB_FETCH_ERROR, "DB_fetch_row failed");

		arr_values = DB_get_row_values(res);
		SFINISH_CHECK(arr_values != NULL, "DB_get_row_values failed");

		if (int_i == 0) {
			SFINISH_CAT_CSTR(str_user, DArray_get(arr_values, 0));
		} else {
			SFINISH_CAT_APPEND(str_response, int_i == 1 ? "" : ", ", "\"", DArray_get(arr_values, 0), "\"");
		}

		DArray_clear_destroy(arr_values);
		arr_values = NULL;
		int_i += 1;
	}
	SFINISH_CAT_APPEND(str_response, "]\012USER\t", str_user);*/

	bol_error_state = false;
	bol_ret = true;
finish:
	SFREE_ALL();
	if (arr_values != NULL) {
		DArray_clear_destroy(arr_values);
		arr_values = NULL;
	}

	char *_str_response = NULL;
	if (res != NULL && (res->status != DB_RES_TUPLES_OK || status == DB_FETCH_ERROR || status != DB_FETCH_END)) {
		_str_response = DB_get_diagnostic(client->conn, res);
		SFINISH_CAT_APPEND(str_response, ":\n", _str_response);
		SFREE(_str_response);
	} else if (res == NULL && client->conn->str_response != NULL) {
		SFINISH_CAT_APPEND(str_response, ":\n", client->conn->str_response);
		SFREE(client->conn->str_response);
	}

	_str_response = str_response;
	char str_length[50];
	snprintf(str_length, 50, "%zu", strlen(_str_response));
	if (bol_error_state) {
		str_response = cat_cstr("HTTP/1.1 500 Internal Server Error\015\012"
								"Content-Length: ",
			str_length, "\015\012\015\012", _str_response);
	} else {
		str_response = cat_cstr("HTTP/1.1 200 OK\015\012"
								"Content-Length: ",
			str_length, "\015\012Content-Type: application/json\015\012\015\012", _str_response);
	}
	SFREE(_str_response);
	ssize_t int_response_len = 0;
	if ((int_response_len = CLIENT_WRITE(client, str_response, strlen(str_response))) < 0) {
		if (bol_tls) {
			SERROR_NORESPONSE_LIBTLS_CONTEXT(client->tls_postage_io_context, "tls_write() failed");
		} else {
			SERROR_NORESPONSE("write() failed");
		}
	}
	DB_free_result(res);

	ev_io_stop(EV_A, &client->io);
	SFINISH_CLIENT_CLOSE(client);

	if (bol_error_state == true) {
		bol_ret = false;
		bol_error_state = false;
	}
	SFREE(str_response);
	return bol_ret;
}
