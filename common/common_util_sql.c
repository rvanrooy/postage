#include "common_util_sql.h"

char *get_table_name(char *_str_query) {
	char *str_temp = NULL;
	char *str_temp1 = NULL;
	char *ptr_table_name = NULL;
	char *ptr_end_table_name = NULL;
	char *str_table_name = NULL;
	char *str_query = NULL;

	SERROR_CAT_CSTR(str_table_name, "");
	SERROR_CAT_CSTR(str_query, _str_query);

	ptr_table_name = str_query + 6;
	SERROR_CHECK(*ptr_table_name == '\t', "Invalid request");
	ptr_table_name += 1;
	ptr_end_table_name = ptr_table_name + strcspn(ptr_table_name, "\t\012");
	bool bol_schema = *ptr_end_table_name == '\t';
	*ptr_end_table_name = 0;

	SERROR_CAT_CSTR(str_temp, ptr_table_name);
	SERROR_REPLACE(str_temp, "\"", "\"\"", "");

	str_temp1 = unescape_value(str_temp);
	SERROR_CHECK(str_temp1 != NULL, "unescape_value failed");
	SFREE(str_temp);
	SERROR_CAT_APPEND(str_table_name, "\"", str_temp1, "\"");
	SFREE(str_temp1);

	if (bol_schema) {
		ptr_table_name = ptr_end_table_name + 1;
		ptr_end_table_name = strstr(ptr_table_name, "\012");
		*ptr_end_table_name = 0;

		SERROR_CAT_CSTR(str_temp, ptr_table_name);
		SERROR_REPLACE(str_temp, "\"", "\"\"", "");

		str_temp1 = unescape_value(str_temp);
		SERROR_CHECK(str_temp1 != NULL, "unescape_value failed");
		SFREE(str_temp);
		SERROR_CAT_APPEND(str_table_name, ".\"", str_temp1, "\"");
		SFREE(str_temp1);
	}

	SFREE(str_query);

	return str_table_name;
error:
	SFREE(str_temp1);
	SFREE(str_temp);
	SFREE(str_query);

	SFREE(str_table_name);
	return NULL;
}

char *get_return_columns(char *_str_query, char *str_table_name) {
	char *str_temp = NULL;
	char *str_temp1 = NULL;
	char *ptr_return_columns = NULL;
	char *ptr_end_return_columns = NULL;
	char *str_return_columns = NULL;
	char *str_query = NULL;
	SERROR_CAT_CSTR(str_query, _str_query);

	ptr_return_columns = strstr(str_query, "RETURN\t");
	SERROR_CHECK(ptr_return_columns != NULL, "strstr failed");
	ptr_return_columns += 7;
	ptr_end_return_columns = strstr(ptr_return_columns, "\012");
	SERROR_CHECK(ptr_end_return_columns != NULL, "strstr failed");
	*ptr_end_return_columns = 0;

	SERROR_CAT_CSTR(str_temp, ptr_return_columns);

	if (strncmp(str_temp, "*", 2) != 0) {
		SERROR_REPLACE(str_temp, "\"", "\"\"", "");
		SERROR_REPLACE(str_temp, "\t", "\", {{TABLE}}.\"", "g");

		str_temp1 = unescape_value(str_temp);
		SERROR_CHECK(str_temp1 != NULL, "unescape_value failed");
		SFREE(str_temp);
		SERROR_CAT_CSTR(str_return_columns, "{{TABLE}}.\"", str_temp1, "\"");
		SFREE(str_temp1);

		SERROR_REPLACE(str_return_columns, "{{TABLE}}", str_table_name, "g");
	} else {
		SERROR_CAT_CSTR(str_return_columns, str_temp);
	}
	SFREE(str_temp);
	SFREE(str_query);

	return str_return_columns;
error:
	SFREE(str_temp);
	SFREE(str_temp1);
	SFREE(str_query);

	SFREE(str_return_columns);
	return NULL;
}

#ifndef POSTAGE_INTERFACE_LIBPQ
char *get_return_escaped_columns(DB_driver driver, char *_str_query) {
	char *str_temp = NULL;
	char *str_temp1 = NULL;
	char *ptr_return_columns = NULL;
	char *ptr_end_return_columns = NULL;
	char *str_return_columns = NULL;
	char *str_query = NULL;
	SERROR_CAT_CSTR(str_query, _str_query);

	ptr_return_columns = strstr(str_query, "RETURN\t");
	SERROR_CHECK(ptr_return_columns != NULL, "strstr failed");
	ptr_return_columns += 7;
	ptr_end_return_columns = strstr(ptr_return_columns, "\012");
	SERROR_CHECK(ptr_end_return_columns != NULL, "strstr failed");
	*ptr_end_return_columns = 0;

	SERROR_CAT_CSTR(str_temp, ptr_return_columns);
	if (strncmp(str_temp, "*", 2) != 0) {
		SERROR_REPLACE(str_temp, "\"", "\"\"", "");

		SERROR_REPLACE(str_temp, "\\t", "TABHERE3141592653589793TABHERE", "g");
		str_temp1 = unescape_value(str_temp);
		SERROR_CHECK(str_temp1 != NULL, "unescape_value failed");
		SFREE(str_temp);
		if (driver == DB_DRIVER_POSTGRES) {
			SERROR_REPLACE(str_temp1, "\t", "\"::text, '\\N'), '\\\\', '\\\\\\\\'), '\t', '\\t'), chr(10), '\\n'), chr(13), '\\r') || E'\\t' || replace(replace(replace(replace(COALESCE(\"", "g");
		} else {
			SERROR_REPLACE(str_temp1, "\t",
				"\" AS nvarchar(MAX)), CAST('\\N' AS nvarchar(MAX))) AS nvarchar(MAX)), '\\\\', '\\\\\\\\'), '\t', '\\t'), CHAR(10), '\\n'), CHAR(13), '\\r') + "
				"CAST('\t' AS nvarchar(MAX)) + replace(replace(replace(replace(CAST(COALESCE(CAST(\"", "g");
		}
		SERROR_REPLACE(str_temp1, "TABHERE3141592653589793TABHERE", "\t", "g");
		if (driver == DB_DRIVER_POSTGRES) {
			SERROR_CAT_CSTR(str_return_columns, "replace(replace(replace(replace(COALESCE(\"", str_temp1, "\"::text, '\\N'), '\\\\', '\\\\\\\\'), '\t', '\\t'), chr(10), '\\n'), chr(13), '\\r') || E'\n'");
		} else {
			SERROR_CAT_CSTR(str_return_columns, "replace(replace(replace(replace(CAST(COALESCE(CAST(\"", str_temp1,
				"\" AS nvarchar(MAX)), CAST('\\N' AS nvarchar(MAX))) AS nvarchar(MAX)), '\\\\', '\\\\\\\\'), '\t', '\\t'), CHAR(10), '\\n'), CHAR(13), '\\r') + CAST(CHAR(10) AS nvarchar(MAX))");
		}
		SFREE(str_temp1);
	} else {
		SERROR_CAT_CSTR(str_return_columns, str_temp);
	}
	SFREE(str_temp);
	SFREE(str_query);

	return str_return_columns;
error:
	SFREE(str_temp);
	SFREE(str_temp1);
	SFREE(str_query);

	SFREE(str_return_columns);
	return NULL;
}
#endif

char *get_hash_columns(char *_str_query) {
	char *str_temp1 = NULL;
	char *ptr_hash_columns = NULL;
	char *ptr_end_hash_columns = NULL;
	char *str_hash_columns = NULL;
	char *str_query = NULL;
	SERROR_CAT_CSTR(str_query, _str_query);

	ptr_hash_columns = strstr(str_query, "HASH\t");
	SERROR_CHECK(ptr_hash_columns != NULL, "strstr failed");
	ptr_hash_columns += 5;
	ptr_end_hash_columns = strstr(ptr_hash_columns, "\012");
	SERROR_CHECK(ptr_end_hash_columns != NULL, "strstr failed");
	*ptr_end_hash_columns = 0;

	SERROR_CAT_CSTR(str_hash_columns, ptr_hash_columns);
	SFREE(str_query);

	return str_hash_columns;
error:
	SFREE(str_temp1);
	SFREE(str_query);

	SFREE(str_hash_columns);
	return NULL;
}

bool ws_copy_check_cb(EV_P, bool bol_success, bool bol_last, void *cb_data, char *str_response, size_t int_len) {
	struct sock_ev_client_request *client_request = cb_data;
	SFREE(str_global_error);
	SDEBUG("str_response: %s", str_response);

	if (bol_success) {
		if (client_request->str_current_response == NULL && !bol_last) {
			SDEBUG("Build Message Headers...");
			client_request->int_response_id += 1;
			char str_temp[101] = {0};
			snprintf(str_temp, 100, "%zd", client_request->int_response_id);

			SFINISH_CAT_CSTR(client_request->str_current_response, "messageid = ", client_request->str_message_id,
				"\012"
				"responsenumber = ",
				str_temp, "\012");
			if (client_request->str_transaction_id != NULL) {
				SFINISH_CAT_APPEND(
					client_request->str_current_response, "transactionid = ", client_request->str_transaction_id, "\012");
			}
			client_request->int_current_response_length = strlen(client_request->str_current_response);
		}

		if (!bol_last) {
			SDEBUG("Add row to Message...");
			SFINISH_SREALLOC(client_request->str_current_response, client_request->int_current_response_length + int_len + 1);
			memcpy(client_request->str_current_response + client_request->int_current_response_length, str_response, int_len);
			client_request->str_current_response[client_request->int_current_response_length + int_len] = '\0';
			client_request->int_current_response_length += int_len;
		}

		client_request->int_row_num += 1;
		if ((client_request->int_row_num % 10) == 0 || (bol_last && client_request->str_current_response != NULL)) {
			SDEBUG("Send Message...");
			WS_sendFrame(EV_A, client_request->parent, true, 0x01, client_request->str_current_response,
				client_request->int_current_response_length);
			DArray_push(client_request->arr_response, client_request->str_current_response);
			client_request->str_current_response = NULL;
			client_request->int_current_response_length = 0;
		}

		if (bol_last) {
			SDEBUG("Build Message Headers...");
			client_request->int_response_id += 1;
			char str_temp[101] = {0};
			snprintf(str_temp, 100, "%zd", client_request->int_response_id);

			SFINISH_CAT_CSTR(client_request->str_current_response, "messageid = ", client_request->str_message_id,
				"\012"
				"responsenumber = ",
				str_temp, "\012");
			if (client_request->str_transaction_id != NULL) {
				SFINISH_CAT_APPEND(
					client_request->str_current_response, "transactionid = ", client_request->str_transaction_id, "\012");
			}

			SFINISH_CAT_APPEND(client_request->str_current_response, "TRANSACTION COMPLETED");
			client_request->int_current_response_length = strlen(client_request->str_current_response);

			SDEBUG("Send \"TRANSACTION COMPLETED\" Message...");
			WS_sendFrame(EV_A, client_request->parent, true, 0x01, client_request->str_current_response,
				client_request->int_current_response_length);
			DArray_push(client_request->arr_response, client_request->str_current_response);
			client_request->str_current_response = NULL;
			client_request->int_current_response_length = 0;

			if (client_request->int_req_type == POSTAGE_REQ_INSERT) {
				ws_insert_free(client_request->vod_request_data);
			} else if (client_request->int_req_type == POSTAGE_REQ_UPDATE) {
				ws_update_free(client_request->vod_request_data);
			} else if (client_request->int_req_type == POSTAGE_REQ_SELECT) {
				ws_select_free(client_request->vod_request_data);
			}
		}

	} else {
		SDEBUG("Error...");
		if (client_request->str_current_response != NULL) {
			SDEBUG("Send Message...");
			WS_sendFrame(EV_A, client_request->parent, true, 0x01, client_request->str_current_response,
				client_request->int_current_response_length);
			DArray_push(client_request->arr_response, client_request->str_current_response);
			client_request->str_current_response = NULL;
			client_request->int_current_response_length = 0;
		}

		client_request->int_response_id += 1;
		char str_temp[101] = {0};
		snprintf(str_temp, 100, "%zd", client_request->int_response_id);

		SDEBUG("Send Error...");
		SFINISH_CAT_CSTR(client_request->str_current_response, "messageid = ", client_request->str_message_id,
			"\012"
			"responsenumber = ",
			str_temp, "\012");
		if (client_request->str_transaction_id != NULL) {
			SFINISH_CAT_APPEND(
				client_request->str_current_response, "transactionid = ", client_request->str_transaction_id, "\012");
		}
		client_request->int_current_response_length = strlen(client_request->str_current_response);
		SFINISH_SREALLOC(client_request->str_current_response, client_request->int_current_response_length + int_len + 6 + 1);

		memcpy(client_request->str_current_response + client_request->int_current_response_length, "FATAL\012", 6);
		client_request->int_current_response_length += 6;

		memcpy(client_request->str_current_response + client_request->int_current_response_length, str_response, int_len);
		client_request->int_current_response_length += int_len;
		client_request->str_current_response[client_request->int_current_response_length] = 0;

		SDEBUG("client_request->str_current_response: >%s<", client_request->str_current_response);
		SDEBUG("client_request->arr_response: >%p<", client_request->arr_response);
		SDEBUG("client_request->arr_response->contents: >%p<", client_request->arr_response->contents);

		WS_sendFrame(EV_A, client_request->parent, true, 0x01, client_request->str_current_response,
			client_request->int_current_response_length);
		DArray_push(client_request->arr_response, client_request->str_current_response);

		client_request->str_current_response = NULL;
		client_request->int_current_response_length = 0;

		if (client_request->int_req_type == POSTAGE_REQ_INSERT) {
			ws_insert_free(client_request->vod_request_data);
		} else if (client_request->int_req_type == POSTAGE_REQ_UPDATE) {
			ws_update_free(client_request->vod_request_data);
		} else if (client_request->int_req_type == POSTAGE_REQ_SELECT) {
			ws_select_free(client_request->vod_request_data);
		}
	}

finish:
	if (bol_error_state == true) {
		bol_error_state = false;
		// Get an error message
		char *_str_response = str_response;
		if (client_request->str_transaction_id != NULL) {
			SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012", "transactionid = ",
				client_request->str_transaction_id, "\012", _str_response);
		} else {
			SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012", _str_response);
		}
		SFREE(_str_response);

		WS_sendFrame(EV_A, client_request->parent, true, 0x01, str_response, strlen(str_response));
		DArray_push(client_request->arr_response, str_response);
		str_response = NULL;

		if (client_request->int_req_type == POSTAGE_REQ_INSERT) {
			ws_insert_free(client_request->vod_request_data);
		} else if (client_request->int_req_type == POSTAGE_REQ_UPDATE) {
			ws_update_free(client_request->vod_request_data);
		} else if (client_request->int_req_type == POSTAGE_REQ_SELECT) {
			ws_select_free(client_request->vod_request_data);
		}
		return false;
	}
	return true;
}

bool http_copy_check_cb(EV_P, bool bol_success, bool bol_last, void *cb_data, char *str_response, size_t int_response_len) {
	if (EV_A != 0) {
	} // get rid of unused parameter warning
	struct sock_ev_client_request *client_request = cb_data;
	struct sock_ev_client *client = client_request->parent;
	size_t int_header_len = 0;
	ssize_t int_response_write_len = 0;
	char str_length[50];
	memset(str_length, 0, 50);
	SDEFINE_VAR_ALL(str_header, _str_response);
	SDEBUG("str_response: %s", str_response);
	SFREE(str_global_error);

	if (bol_success) {
		if (client_request->str_current_response == NULL) {
			SFINISH_SALLOC(client_request->str_current_response, int_response_len + 1);
			memcpy(client_request->str_current_response, str_response, int_response_len);
			client_request->str_current_response[int_response_len] = '\0';
			client_request->int_current_response_length = int_response_len;
		} else if (!bol_last) {
			SFINISH_SREALLOC(
				client_request->str_current_response, client_request->int_current_response_length + int_response_len + 1);
			memcpy(client_request->str_current_response + client_request->int_current_response_length, str_response,
				int_response_len);
			client_request->str_current_response[client_request->int_current_response_length + int_response_len] = '\0';
			client_request->int_current_response_length += int_response_len;
		}

		if (bol_last) {
			snprintf(str_length, 50, "%zd", client_request->int_current_response_length);
			SFINISH_CAT_CSTR(str_header, "HTTP/1.1 200 OK\015\012"
										 "Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
										 "Content-Length: ",
				str_length, "\015\012\015\012");
			int_header_len = strlen(str_header);
			SDEBUG("str_header: %s", str_header);

			SFINISH_SALLOC(_str_response, client_request->int_current_response_length + int_header_len + 1);
			memcpy(_str_response, str_header, int_header_len);
			memcpy(_str_response + int_header_len, client_request->str_current_response,
				client_request->int_current_response_length);
			_str_response[client_request->int_current_response_length + int_header_len] = '\0';

			SDEBUG("_str_response: %s", _str_response);
			if ((int_response_write_len = CLIENT_WRITE(
					 client_request->parent, _str_response, client_request->int_current_response_length + int_header_len)) < 0) {
				if (bol_tls) {
					SFINISH_LIBTLS_CONTEXT(client_request->parent->tls_postage_io_context, "tls_write() failed");
				} else {
					SERROR_NORESPONSE("write() failed");
				}
			}
			SDEBUG("int_response_write_len: %d", int_response_write_len);
			SFREE(_str_response);

			SFINISH_CLIENT_CLOSE(client);
		}

	} else {
		bol_error_state = true;
	}

finish:
	SFREE_ALL();
	if (bol_error_state == true) {
		bol_error_state = false;
		snprintf(str_length, 50, "%zu", strlen(str_response));
		SFINISH_CAT_CSTR(_str_response, "HTTP/1.1 500 Internal Server Error\015\012"
										"Server: " SUN_PROGRAM_LOWER_NAME "\015\012"
										"Content-Length: ",
			str_length, "\015\012\015\012", str_response);

		if ((int_response_write_len = CLIENT_WRITE(client_request->parent, _str_response, strlen(_str_response))) < 0) {
			if (bol_tls) {
				SERROR_NORESPONSE_LIBTLS_CONTEXT(client_request->parent->tls_postage_io_context, "tls_write() failed");
			} else {
				SERROR_NORESPONSE("write() failed");
			}
		}
		SFREE(_str_response);

		SFINISH_CLIENT_CLOSE(client);

		return false;
	}
	return true;
}

#ifdef ENVELOPE
bool permissions_check(EV_P, DB_conn *conn, char *str_path, void *cb_data, readable_cb_t readable_cb) {
	char *ptr_path = str_path;
	if (*ptr_path == '/') {
		ptr_path++;
	}

	if (strncmp(ptr_path, "role/", 5) == 0 || strncmp(ptr_path, "role", 5) == 0) {
		if (strlen(ptr_path) > 5) {
			return ddl_readable(EV_A, conn, ptr_path + 4, false, cb_data, readable_cb);
		} else {
			return readable_cb(EV_A, cb_data, true);
		}
	} else if (strncmp(ptr_path, "web_root/", 9) == 0 || strncmp(ptr_path, "web_root", 9) == 0) {
		return ddl_readable(EV_A, conn, "developer_g", false, cb_data, readable_cb);
	} else if (strncmp(ptr_path, "app/", 4) == 0 || strncmp(ptr_path, "app", 4) == 0) {
		return ddl_readable(EV_A, conn, "developer_g", false, cb_data, readable_cb);
	} else {
		return false;
	}

	return false;
}

bool permissions_write_check(EV_P, DB_conn *conn, char *str_path, void *cb_data, readable_cb_t readable_cb) {
	char *ptr_path = str_path;
	if (*ptr_path == '/') {
		ptr_path++;
	}

	if (strncmp(ptr_path, "role/", 5) == 0 || strncmp(ptr_path, "role", 5) == 0) {
		if (strlen(ptr_path) > 5) {
			return ddl_readable(EV_A, conn, ptr_path + 4, true, cb_data, readable_cb);
		} else {
			return readable_cb(EV_A, cb_data, true);
		}
	} else if (strncmp(ptr_path, "web_root/", 9) == 0 || strncmp(ptr_path, "web_root", 9) == 0) {
		return ddl_readable(EV_A, conn, "developer_g", false, cb_data, readable_cb);
	} else if (strncmp(ptr_path, "app/", 4) == 0 || strncmp(ptr_path, "app", 4) == 0) {
		return ddl_readable(EV_A, conn, "developer_g", false, cb_data, readable_cb);
	} else {
		return false;
	}

	return false;
}

char *canonical_strip_start(char *str_path) {
	char *str_return = NULL;
	char *ptr_path = str_path;
	if (*ptr_path == '/') {
		ptr_path++;
	}

	if (strncmp(ptr_path, "role/", 5) == 0) {
		ptr_path += 5;
	} else if (strncmp(ptr_path, "role", 5) == 0) {
		ptr_path += 4;
	} else if (strncmp(ptr_path, "web_root/", 9) == 0) {
		ptr_path += 9;
	} else if (strncmp(ptr_path, "web_root", 9) == 0) {
		ptr_path += 8;
	} else if (strncmp(ptr_path, "app/", 4) == 0) {
		ptr_path += 4;
	} else if (strncmp(ptr_path, "app", 4) == 0) {
		ptr_path += 3;
	} else {
		SERROR("Starting path not recognized.");
	}

	SERROR_CAT_CSTR(str_return, ptr_path);

	return str_return;
error:
	SFREE(str_return);
	return NULL;
}

char *canonical_full_start(char *str_path) {
	char *str_return = NULL;
	char *ptr_path = str_path;
	if (*ptr_path == '/') {
		ptr_path++;
	}

	if (strncmp(ptr_path, "role/", 5) == 0 || strncmp(ptr_path, "role", 5) == 0) {
		SERROR_CAT_CSTR(str_return, str_global_role_path);
	} else if (strncmp(ptr_path, "web_root/", 9) == 0 || strncmp(ptr_path, "web_root", 9) == 0) {
		SERROR_CAT_CSTR(str_return, str_global_web_root);
	} else if (strncmp(ptr_path, "app/", 4) == 0 || strncmp(ptr_path, "app", 4) == 0) {
		SERROR_CAT_CSTR(str_return, str_global_app_path);
	} else {
		SERROR("Starting path not recognized.");
	}

	return str_return;
error:
	SFREE(str_return);
	return NULL;
}
#endif // ENVELOPE

static bool ddl_readable_done(EV_P, void *cb_data, DB_result *res);
bool ddl_readable(EV_P, DB_conn *conn, char *str_path, bool bol_writeable, void *cb_data, readable_cb_t readable_cb) {
	SNOTICE("SQL.C READABLE");
	DB_readable_poll *readable_poll = NULL;
	SDEFINE_VAR_ALL(str_folder, str_folder_write, str_folder_literal, str_folder_write_literal, str_sql);

	SERROR_CHECK(cb_data != NULL, "cb_data == NULL");
	SERROR_CHECK(readable_cb != NULL, "readable_cb == NULL");

	char *ptr_path = str_path;
	while (*ptr_path == '/') {
		ptr_path++;
	}
	char *ptr_slash = strchr(ptr_path, '/');
	size_t slash_position;
	if (ptr_slash == 0) {
		slash_position = strlen(ptr_path);
	} else {
		slash_position = (size_t)(ptr_slash - ptr_path);
	}
	SDEBUG("str_path      : %p", str_path);
	SDEBUG("ptr_path      : %p", ptr_path);
	SDEBUG("ptr_slash     : %p", ptr_slash);
	SDEBUG("slash_position: %d", slash_position);

	if (bol_writeable) {
		SERROR_CAT_CSTR(str_folder_write, ptr_path);
		str_folder_write[slash_position - 1] = 'w';
		str_folder_write[slash_position] = '\0';
		str_folder_write_literal = DB_escape_literal(conn, str_folder_write, strlen(str_folder_write));
		SERROR_CHECK(str_folder_write_literal != NULL, "DB_escape_literal failed");
	}

	SERROR_CAT_CSTR(str_folder, ptr_path);
	str_folder[slash_position] = '\0';
	str_folder_literal = DB_escape_literal(conn, str_folder, strlen(str_folder));
	SERROR_CHECK(str_folder_literal != NULL, "DB_escape_literal failed");

	SDEBUG(">%s|%s<", ptr_path, str_folder);
	SFREE(str_folder);
	if (DB_connection_driver(conn) == DB_DRIVER_POSTGRES) {
		if (bol_writeable) {
			SERROR_CAT_CSTR(str_sql, "\
			SELECT CASE WHEN count(*) > 0 OR \
				   lower(",
				str_folder_literal, ") = 'all' OR \
				   lower(",
				str_folder_literal, ") = lower(session_user) THEN 'TRUE' ELSE 'FALSE' END::text \
			FROM pg_roles r \
			JOIN pg_auth_members ON r.oid=roleid \
			JOIN pg_roles u ON member = u.oid \
			WHERE (lower(r.rolname) = lower(",
				str_folder_write_literal, ") OR \
				  lower(r.rolname) = 'developer_g') AND lower(u.rolname) = lower(session_user); \
			");
		} else {
			SERROR_CAT_CSTR(str_sql, "\
			SELECT CASE WHEN count(*) > 0 OR 'all' = lower(",
				str_folder_literal, ") OR '' = ", str_folder_literal, " OR \
					lower(",
				str_folder_literal, ") = lower(session_user) THEN 'TRUE' ELSE 'FALSE' END::text \
			FROM pg_roles r \
			JOIN pg_auth_members ON r.oid=roleid \
			JOIN pg_roles u ON member = u.oid \
			WHERE (lower(r.rolname) = lower(",
				str_folder_literal, ") OR \
				  lower(r.rolname) = 'developer_g') AND lower(u.rolname) = lower(session_user);");
		}
	} else {
		if (bol_writeable) {
			SERROR_CAT_CSTR(str_sql, "SELECT CASE WHEN IS_MEMBER(", str_folder_write_literal, ") = 1 OR 'all' = lower(",
				str_folder_literal, ") OR '' = ", str_folder_literal, " OR lower(", str_folder_literal,
				") = lower(session_user) THEN 'TRUE' ELSE 'FALSE' END;");
		} else {
			SERROR_CAT_CSTR(str_sql, "SELECT CASE WHEN IS_MEMBER(", str_folder_literal, ") = 1 OR 'all' = lower(",
				str_folder_literal, ") OR '' = ", str_folder_literal, " OR lower(", str_folder_literal,
				") = lower(session_user) THEN 'TRUE' ELSE 'FALSE' END;");
		}
	}
	SDEBUG(">%s<", str_sql);
	SFREE(str_folder_literal);
	SFREE(str_folder_write_literal);

	SERROR_SALLOC(readable_poll, sizeof(DB_readable_poll));
	readable_poll->cb_data = cb_data;
	readable_poll->readable_cb = readable_cb;
	DB_exec(EV_A, conn, readable_poll, str_sql, ddl_readable_done);

	SFREE(str_sql);
	SFREE_ALL();
	return true;
error:
	SFREE_ALL();
	return false;
}

static bool ddl_readable_done(EV_P, void *cb_data, DB_result *res) {
	SINFO("SQL.C READABLE END");
	DB_readable_poll *readable_poll = cb_data;
	DArray *arr_row_values = NULL;
	DArray *arr_row_lengths = NULL;
	bool bol_return = true;
	bool bol_result = 0;

	SERROR_CHECK(res != NULL, "Query failed: res == NULL");
	SERROR_CHECK(res->status == DB_RES_TUPLES_OK, "Query failed: res->status = %d", res->status);

	SERROR_CHECK(DB_fetch_row(res) == DB_FETCH_OK, "DB_fetch_row failed: %s", DB_get_diagnostic(res->conn, res));
	arr_row_values = DB_get_row_values(res);
	arr_row_lengths = DB_get_row_lengths(res);

	SDEBUG("DArray_get(arr_row_values, 0): %s", DArray_get(arr_row_values, 0));
	SDEBUG("result: %s", strncmp(DArray_get(arr_row_values, 0), "TRUE", 4) == 0 ? "true" : "false");
	bol_result = strncmp(DArray_get(arr_row_values, 0), "TRUE", 4) == 0;

	DArray_clear_destroy(arr_row_values);
	DArray_clear_destroy(arr_row_lengths);

	DB_free_result(res);

	bol_return = readable_poll->readable_cb(EV_A, readable_poll->cb_data, bol_result);

	SFREE(readable_poll);
	return bol_return;
error:
	if (arr_row_values != NULL) {
		DArray_clear_destroy(arr_row_values);
	}
	if (arr_row_lengths != NULL) {
		DArray_clear_destroy(arr_row_lengths);
	}
	return false;
}
