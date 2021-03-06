#include "ws_raw.h"

#ifdef ENVELOPE
#else

static const char *str_date_format = "%Y/%m/%d";
static const char *str_time_format = "%H:%M:%S";

char *ws_raw_step1(struct sock_ev_client_request *client_request) {
	char *ptr_query = NULL;
	char *str_response = NULL;
	client_request->arr_query = NULL;
	client_request->int_response_id = 0;
	char str_temp[101];
	int int_status = 1;

	client_request->arr_response = DArray_create(sizeof(char *), 1);

	ptr_query = client_request->ptr_query + 3;
	SFINISH_CHECK(*ptr_query != 0, "Invalid RAW request");
	ptr_query++;
	SFINISH_CHECK(*ptr_query != 0, "Invalid RAW request");

	client_request->arr_query = DArray_sql_split(ptr_query);
	memset(str_temp, 0, 101);
	if (client_request->arr_query == NULL) {
		client_request->int_response_id += 1;
		snprintf(str_temp, 100, "%zd", client_request->int_response_id);

		SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012");
		if (client_request->str_transaction_id) {
			SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
		}
		SFINISH_CAT_APPEND(str_response, "responsenumber = ", str_temp, "\012");
		SFINISH_CAT_APPEND(str_response, "EMPTY");
		WS_sendFrame(global_loop, client_request->parent, true, 0x01, str_response, strlen(str_response));
		DArray_push(client_request->arr_response, str_response);
		str_response = NULL;

		client_request->int_response_id += 1;
		memset(str_temp, 0, 101);
		snprintf(str_temp, 100, "%zd", client_request->int_response_id);

		SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012");
		if (client_request->str_transaction_id) {
			SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
		}
		SFINISH_CAT_APPEND(str_response, "responsenumber = ", str_temp, "\012");
		SFINISH_CAT_APPEND(str_response, "\\.");
		WS_sendFrame(global_loop, client_request->parent, true, 0x01, str_response, strlen(str_response));
		DArray_push(client_request->arr_response, str_response);
		str_response = NULL;

		client_request->int_response_id += 1;
		memset(str_temp, 0, 101);
		snprintf(str_temp, 100, "%zd", client_request->int_response_id);

		SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012");
		if (client_request->str_transaction_id) {
			SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
		}
		SFINISH_CAT_APPEND(str_response, "responsenumber = ", str_temp, "\012");
		SFINISH_CAT_APPEND(str_response, "TRANSACTION COMPLETED");
		WS_sendFrame(global_loop, client_request->parent, true, 0x01, str_response, strlen(str_response));
		DArray_push(client_request->arr_response, str_response);
		str_response = NULL;
		goto finish;
	}
	client_request->int_i = -1;
	client_request->int_len = (ssize_t)DArray_end(client_request->arr_query);

	int_status = PQsendQuery(client_request->parent->cnxn, "BEGIN");
	if (int_status != 1) {
		SFINISH("Query failed: %s", PQerrorMessage(client_request->parent->cnxn));
	}

	client_request->int_response_id = -1;

	query_callback(global_loop, client_request, ws_raw_step2);

	bol_error_state = false;
finish:
	if (str_response != NULL) {
		bol_error_state = false;
		memset(str_temp, 0, 101);
		client_request->int_response_id += 1;
		snprintf(str_temp, 100, "%zd", client_request->int_response_id);
		char *_str_response = str_response;
		SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012"
																					   "responsenumber = ",
			str_temp, "\012");
		if (client_request->str_transaction_id != NULL) {
			SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
		}
		SFINISH_CAT_APPEND(str_response, _str_response);
		SFREE(_str_response);
		WS_sendFrame(global_loop, client_request->parent, true, 0x01, str_response, strlen(str_response));
		DArray_push(client_request->arr_response, str_response);
		str_response = NULL;
		// client_request_free(client_request);
		ws_raw_step3(global_loop, NULL, 0, client_request);
	}
	bol_error_state = false;
	return str_response;
}

bool ws_raw_step2(EV_P, PGresult *res, ExecStatusType result, struct sock_ev_client_request *client_request) {
	SDEBUG("ws_raw_step2");
	char *str_response = NULL;
	char *str_sql = NULL;
	char *str_rows = NULL;
	char *str_oid_type = NULL;
	char *str_int_mod = NULL;
	bool bol_sfree_all = true;
	struct tm tm_time;
	struct timeval tv_time;
	char str_temp[101];
	int int_status = 0;
	SDEFINE_VAR_ALL(str_escaped_sql, str_sql_temp, str_sql_column_types);

	if (client_request->int_i >= 0 && client_request->int_i < client_request->int_len) {
		client_request->int_response_id += 1;
		memset(str_temp, 0, 101);
		snprintf(str_temp, 100, "%zd", client_request->int_response_id);
		SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012");
		if (client_request->str_transaction_id) {
			SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
		}
		SFINISH_CAT_APPEND(str_response, "responsenumber = ", str_temp, "\012");

		gettimeofday(&tv_time, NULL);
#ifdef _WIN32
		SFINISH_CHECK((errno = _gmtime32_s(&tm_time, &tv_time.tv_sec)) == 0, "_gmtime32_s failed");
#else
		gmtime_r(&tv_time.tv_sec, &tm_time);
#endif
		SFINISH_CHECK(strftime(str_temp, 100, str_date_format, &tm_time) != 0, "strftime() failed");
		SFINISH_CAT_APPEND(str_response, "END\t", str_temp);

		memset(str_temp, 0, 101);
		SFINISH_CHECK(strftime(str_temp, 100, str_time_format, &tm_time) != 0, "strftime() failed");
		SFINISH_CAT_APPEND(str_response, "\t", str_temp);

		memset(str_temp, 0, 101);

		SFINISH_CHECK(snprintf(str_temp, 100, "%ld", (unsigned long)tv_time.tv_usec) != 0, "snprintf() failed");
		SFINISH_CAT_APPEND(str_response, "\t", str_temp);

		SDEBUG("BEFORE str_response>%s<", str_response);


		SDEBUG("client_request->arr_response: %p", client_request->arr_response);
#ifdef UTIL_DEBUG
		if (client_request->arr_response != NULL) {
			int i = 0;
			int len = DArray_max(client_request->arr_response);
			while (i < len) {
				SDEBUG("client_request->arr_response[%d]: %s", i, DArray_get(client_request->arr_response, i));
				i++;
			}
		}
#endif

		WS_sendFrame(EV_A, client_request->parent, true, 0x01, str_response, strlen(str_response));
		DArray_push(client_request->arr_response, str_response);
		SDEBUG("AFTER");
		str_response = NULL;
	}

	client_request->int_response_id += 1;
	memset(str_temp, 0, 101);
	snprintf(str_temp, 100, "%zd", client_request->int_response_id);
	SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012");
	if (client_request->str_transaction_id) {
		SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
	}
	SFINISH_CAT_APPEND(str_response, "responsenumber = ", str_temp, "\012");

	SFINISH_CHECK(res != NULL, "query_callback failed!");
	client_request->int_i += 1;
	if (client_request->int_i <= client_request->int_len) {
		if (client_request->int_i == 0) {
			SFINISH_CHECK(result == PGRES_COMMAND_OK, "BEGIN failed: %s", PQerrorMessage(client_request->parent->cnxn));

		} else if (result == PGRES_FATAL_ERROR) {
			SFINISH("Query failed: %s", PQerrorMessage(client_request->parent->cnxn));

		} else if (result == PGRES_EMPTY_QUERY) {
			// FINISH("Query empty");
			SFINISH_CAT_APPEND(str_response, "EMPTY");
			WS_sendFrame(EV_A, client_request->parent, true, 0x01, str_response, strlen(str_response));
			DArray_push(client_request->arr_response, str_response);
			str_response = NULL;

			client_request->int_response_id += 1;
			memset(str_temp, 0, 101);
			snprintf(str_temp, 100, "%zd", client_request->int_response_id);

			SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012");
			if (client_request->str_transaction_id) {
				SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
			}
			SFINISH_CAT_APPEND(str_response, "responsenumber = ", str_temp, "\012");
			SFINISH_CAT_APPEND(str_response, "\\.");

		} else if (result == PGRES_BAD_RESPONSE) {
			SFINISH("Bad response from server: %s", PQerrorMessage(client_request->parent->cnxn));

		} else if (result == PGRES_NONFATAL_ERROR) {
			SFINISH("Nonfatal error: %s", PQerrorMessage(client_request->parent->cnxn));

		} else if (result == PGRES_COMMAND_OK) {
			str_rows = PQcmdTuples(res);
			str_rows = *str_rows == '\0' ? "0" : str_rows;
			SFINISH_CAT_APPEND(str_response, "Rows Affected\012", str_rows, "\012");

		} else if (result == PGRES_TUPLES_OK) {
			SDEBUG("PGRES_TUPLES_OK");
			client_request->res = res;
			SFINISH_CAT_CSTR(str_sql_column_types, "SELECT ");

			int int_column;
			int int_num_columns = PQnfields(res);
			for (int_column = 0; int_column < int_num_columns; int_column += 1) {
				memset(str_temp, 0, 101);
				sprintf(str_temp, "%d", PQftype(res, int_column));
				str_oid_type = PQescapeLiteral(client_request->parent->cnxn, str_temp, strlen(str_temp));

				memset(str_temp, 0, 101);
				sprintf(str_temp, "%d", PQfmod(res, int_column));
				str_int_mod = PQescapeLiteral(client_request->parent->cnxn, str_temp, strlen(str_temp));

				// get type
				SFINISH_CAT_APPEND(str_sql_column_types, "format_type(", str_oid_type, ", ", str_int_mod, ")",
					int_column < (int_num_columns - 1) ? "," : ";");
				PQfreemem(str_oid_type);
				str_oid_type = NULL;
				PQfreemem(str_int_mod);
				str_int_mod = NULL;
			}

			int_status = PQsendQuery(client_request->parent->cnxn, str_sql_column_types);
			SFREE(str_sql_column_types);
			if (int_status != 1) {
				SFINISH("PQsendQuery failed %s", PQerrorMessage(client_request->parent->cnxn));
			}
			query_callback(EV_A, client_request, _raw_tuples_callback);
		} else {
			SFINISH("Unexpected result status %s: %s", PQresStatus(result), PQerrorMessage(client_request->parent->cnxn));
		}
	} else {
		client_request->int_response_id -= 1;
		SFREE(str_response);
	}

	if (result != PGRES_TUPLES_OK) {
		PQclear(res);
		if (str_response != NULL && client_request->int_i != 0) {
			WS_sendFrame(EV_A, client_request->parent, true, 0x01, str_response, strlen(str_response));
			DArray_push(client_request->arr_response, str_response);
			str_response = NULL;
		} else {
			SFREE(str_response);
		}

		client_request->int_response_id += 1;
		memset(str_temp, 0, 101);
		snprintf(str_temp, 100, "%zd", client_request->int_response_id);

		if (client_request->int_i < client_request->int_len) {
			str_sql = (char *)DArray_get(client_request->arr_query, (size_t)client_request->int_i);
			SFINISH_CAT_CSTR(str_sql_temp, str_sql);
			str_escaped_sql = escape_value(str_sql_temp);
			SFREE(str_sql_temp);
			SFINISH_CHECK(str_escaped_sql != NULL, "escape_value failed");

			SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012"
																						   "responsenumber = ",
				str_temp, "\012");
			if (client_request->str_transaction_id != NULL) {
				SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
			}
			SFINISH_CAT_APPEND(str_response, "QUERY\t", str_escaped_sql);
			WS_sendFrame(EV_A, client_request->parent, true, 0x01, str_response, strlen(str_response));
			DArray_push(client_request->arr_response, str_response);

			client_request->int_response_id += 1;
			memset(str_temp, 0, 101);
			snprintf(str_temp, 100, "%zd", client_request->int_response_id);
			SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012"
																						   "responsenumber = ",
				str_temp, "\012");
			if (client_request->str_transaction_id != NULL) {
				SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
			}

			memset(str_temp, 0, 101);
			SFINISH_CHECK(gettimeofday(&tv_time, NULL) == 0, "gettimeofday failed");
			SDEBUG("tv_time.tv_sec: %d", tv_time.tv_sec);
#ifdef _WIN32
			SFINISH_CHECK((errno = _gmtime32_s(&tm_time, &tv_time.tv_sec)) == 0, "_gmtime32_s failed");
#else
			gmtime_r(&tv_time.tv_sec, &tm_time);
#endif
			SDEBUG("tm_time.tm_mon: %d", tm_time.tm_mon);
			SFINISH_CHECK(strftime(str_temp, 100, str_date_format, &tm_time) != 0, "strftime() failed");
			SFINISH_CAT_APPEND(str_response, "START\t", str_temp);

			memset(str_temp, 0, 101);
			SFINISH_CHECK(strftime(str_temp, 100, str_time_format, &tm_time) != 0, "strftime() failed");
			SFINISH_CAT_APPEND(str_response, "\t", str_temp);

			memset(str_temp, 0, 101);
			SFINISH_CHECK(snprintf(str_temp, 100, "%ld", (unsigned long)tv_time.tv_usec) != 0, "snprintf() failed");
			SFINISH_CAT_APPEND(str_response, "\t", str_temp);

			WS_sendFrame(EV_A, client_request->parent, true, 0x01, str_response, strlen(str_response));
			DArray_push(client_request->arr_response, str_response);
			str_response = NULL;

			int_status = PQsendQuery(client_request->parent->cnxn, str_sql);
			if (int_status != 1) {
				SFINISH("Query failed: %s", PQerrorMessage(client_request->parent->cnxn));
			}
			query_callback(EV_A, client_request, ws_raw_step2);
		} else if (client_request->int_i == client_request->int_len) {
			client_request->int_response_id -= 1;
			int_status = PQsendQuery(client_request->parent->cnxn, "COMMIT");
			if (int_status != 1) {
				SFINISH("Query failed: %s", PQerrorMessage(client_request->parent->cnxn));
			}
			query_callback(EV_A, client_request, ws_raw_step2);
		} else if (client_request->int_i > client_request->int_len) {
			SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012"
																						   "responsenumber = ",
				str_temp, "\012");
			if (client_request->str_transaction_id != NULL) {
				SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
			}
			SFINISH_CAT_APPEND(str_response, "TRANSACTION COMPLETED");
			WS_sendFrame(EV_A, client_request->parent, true, 0x01, str_response, strlen(str_response));
			DArray_push(client_request->arr_response, str_response);
			str_response = NULL;
			// client_request_free(client_request);
		}
	} else {
		client_request->int_response_id -= 1;
		SFREE(str_response);
	}
	SFREE(str_response);

	bol_error_state = false;
finish:
	SDEBUG("bol_error_state == %s", bol_error_state == true ? "true" : "false");
	// There is a possibility of SFREE_ALL being called twice, we don't want this
	// to happen
	if (bol_sfree_all == true) {
		SFREE_ALL();
		bol_sfree_all = false;
	}
	if (result == PGRES_FATAL_ERROR) {
		PQclear(res);
	}
	if (str_oid_type != NULL) {
		PQfreemem(str_oid_type);
		str_oid_type = NULL;
	}
	if (str_int_mod != NULL) {
		PQfreemem(str_int_mod);
		str_int_mod = NULL;
	}
	if (bol_error_state == true) {
		bol_error_state = false;
		char *_str_response = str_response;
		SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012"
																					   "responsenumber = ",
			str_temp, "\012");
		if (client_request->str_transaction_id != NULL) {
			SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
		}
		SFINISH_CAT_APPEND(str_response, _str_response);
		SFREE(_str_response);

		client_request->str_current_response = str_response;
		str_response = NULL;

		client_request->int_i = client_request->int_len + 10;
		int_status = PQsendQuery(client_request->parent->cnxn, "ROLLBACK");
		if (int_status != 1) {
			SFINISH("Query failed: %s", PQerrorMessage(client_request->parent->cnxn));
		}
		query_callback(EV_A, client_request, ws_raw_step3);
	} else {
		SFREE(str_response);
	}
	return true;
}

bool ws_raw_step3(EV_P, PGresult *res, ExecStatusType result, struct sock_ev_client_request *client_request) {
	if (EV_A != 0) {
	} // get rid of unused parameter warning
	if (result != 0) {
	} // get rid of unused parameter warning
	if (client_request != NULL) {
	} // get rid of unused parameter warning
	if (res != NULL) {
		PQclear(res);
	}
	if (client_request->str_current_response != NULL) {
		WS_sendFrame(EV_A, client_request->parent, true, 0x01, client_request->str_current_response, strlen(client_request->str_current_response));
		DArray_push(client_request->arr_response, client_request->str_current_response);
		client_request->str_current_response = NULL;
	}
	return true;
}

bool _raw_tuples_callback(EV_P, PGresult *res, ExecStatusType result, struct sock_ev_client_request *client_request) {
	if (result != 0) {
	} // get rid of unused parameter warning
	SDEBUG("_raw_tuples_callback");
	char *str_response = NULL;
	char str_temp[101];
	struct sock_ev_client_copy_check *client_copy_check = NULL;
	SFINISH_SALLOC(client_copy_check, sizeof(struct sock_ev_client_copy_check));
	client_copy_check->client_request = client_request;

	client_request->int_response_id += 1;
	memset(str_temp, 0, 101);
	snprintf(str_temp, 100, "%zd", client_request->int_response_id);

	if (close_client_if_needed(client_request->parent, (ev_watcher *)&client_request->check, EV_READ)) {
		SDEBUG("Client %p closed", client_request->parent);
		return false;
	}

	SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012"
																				   "responsenumber = ",
		str_temp, "\012");
	if (client_request->str_transaction_id != NULL) {
		SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
	}
	memset(str_temp, 0, 101);
	snprintf(str_temp, 100, "%d", PQntuples(client_request->res));
	SFINISH_CAT_APPEND(str_response, "ROWS\t", str_temp);
	WS_sendFrame(EV_A, client_request->parent, true, 0x01, str_response, strlen(str_response));
	DArray_push(client_request->arr_response, str_response);
	str_response = NULL;

	client_request->int_response_id += 1;
	memset(str_temp, 0, 101);
	snprintf(str_temp, 100, "%zd", client_request->int_response_id);
	SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012"
																				   "responsenumber = ",
		str_temp, "\012");
	if (client_request->str_transaction_id != NULL) {
		SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
	}
	SFINISH_CAT_APPEND(str_response, "COLUMNS\012");

	int int_num_columns = PQnfields(client_request->res);
	int int_column;
	for (int_column = 0; int_column < int_num_columns; int_column += 1) {
		SFINISH_CAT_APPEND(
			str_response, PQfname(client_request->res, int_column), int_column < (int_num_columns - 1) ? "\t" : "\012");
	}
	for (int_column = 0; int_column < int_num_columns; int_column += 1) {
		SFINISH_CAT_APPEND(str_response, PQgetvalue(res, 0, int_column), int_column < (int_num_columns - 1) ? "\t" : "\012");
	}
	PQclear(res);

	WS_sendFrame(EV_A, client_request->parent, true, 0x01, str_response, strlen(str_response));
	DArray_push(client_request->arr_response, str_response);
	str_response = NULL;

	// This will make sure that the event loop does not sleep
	increment_idle(EV_A);

	// This will run every iteration
	ev_check_init(&client_copy_check->check, _raw_tuples_check_callback);
	ev_check_start(EV_A, &client_copy_check->check);
	bol_error_state = false;
finish:
	if (bol_error_state == true) {
		bol_error_state = false;
		// client_request_free(client_request);
	}
	SFREE(str_response);
	return true;
}

// continue copying the data
void _raw_tuples_check_callback(EV_P, ev_check *w, int revents) {
	if (revents != 0) {
	} // get rid of unused parameter warning
	SDEBUG("_raw_tuples_check_callback");
	struct sock_ev_client_copy_check *client_copy_check = (struct sock_ev_client_copy_check *)w;
	struct sock_ev_client_request *client_request = client_copy_check->client_request;
	struct sock_ev_client *client = client_request->parent;
	PGresult *res = client_request->res;

	char *str_sql_temp = NULL;
	char *str_sql = NULL;
	char *str_response = NULL;
	char str_temp[101];
	memset(str_temp, 0, 101);
	ssize_t int_column = 0;
	ssize_t int_num_columns = 0;
	ssize_t int_num_rows = 0;
	int int_status = 0;
	SDEFINE_VAR_ALL(str_temp1, str_escaped_sql);
	// This must not be freed

	if (close_client_if_needed(client_request->parent, (ev_watcher *)w, revents)) {
		SDEBUG("Client %p closed", client_request->parent);
		ev_check_stop(EV_A, w);
		return;
	}

	client_request->int_response_id += 1;
	snprintf(str_temp, 100, "%zd", client_request->int_response_id);
	SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012"
																				   "responsenumber = ",
		str_temp, "\012");
	if (client_request->str_transaction_id != NULL) {
		SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
	}

	int_num_columns = PQnfields(res);
	int_num_rows = PQntuples(res);

	do {
		if ((ssize_t)client_copy_check->int_i == int_num_rows) {
			if ((client_copy_check->int_i % 10) > 0) {
				SDEBUG("client_copy_check->str_message_id: %s", client_request->str_message_id);
				WS_sendFrame(EV_A, client, true, 0x01, str_response, strlen(str_response));
				DArray_push(client_request->arr_response, str_response);
				client_request->int_response_id += 1;
				memset(str_temp, 0, 101);
				snprintf(str_temp, 100, "%zd", client_request->int_response_id);
				SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012"
																							   "responsenumber = ",
					str_temp, "\012");
				if (client_request->str_transaction_id != NULL) {
					SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
				}
			}

			SFINISH_CAT_APPEND(str_response, "\\.");
			WS_sendFrame(EV_A, client, true, 0x01, str_response, strlen(str_response));
			DArray_push(client_request->arr_response, str_response);
			str_response = NULL;

			client_request->int_response_id += 1;
			memset(str_temp, 0, 101);
			snprintf(str_temp, 100, "%zd", client_request->int_response_id);

			decrement_idle(EV_A);

			SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012"
																						   "responsenumber = ",
				str_temp, "\012");
			if (client_request->str_transaction_id != NULL) {
				SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
			}
			PQclear(res);

			if (client_request->int_i < client_request->int_len) {
				str_sql = (char *)DArray_get(client_request->arr_query, (size_t)client_request->int_i);
				SFINISH_CAT_CSTR(str_sql_temp, str_sql);
				str_escaped_sql = escape_value(str_sql_temp);
				SFREE(str_sql_temp);
				SFINISH_CHECK(str_escaped_sql != NULL, "unescape_value failed");

				SFINISH_CAT_APPEND(str_response, "QUERY\t", str_escaped_sql);

				WS_sendFrame(EV_A, client_request->parent, true, 0x01, str_response, strlen(str_response));
				DArray_push(client_request->arr_response, str_response);

				client_request->int_response_id += 1;
				memset(str_temp, 0, 101);
				snprintf(str_temp, 100, "%zd", client_request->int_response_id);
				SFINISH_CAT_CSTR(str_response, "messageid = ", client_request->str_message_id, "\012");
				if (client_request->str_transaction_id) {
					SFINISH_CAT_APPEND(str_response, "transactionid = ", client_request->str_transaction_id, "\012");
				}
				SFINISH_CAT_APPEND(str_response, "responsenumber = ", str_temp, "\012");

				memset(str_temp, 0, 101);
				struct tm tm_time;
				struct timeval tv_time;
				gettimeofday(&tv_time, NULL);
#ifdef _WIN32
				SFINISH_CHECK((errno = _gmtime32_s(&tm_time, &tv_time.tv_sec)) == 0, "_gmtime32_s failed");
#else
				gmtime_r(&tv_time.tv_sec, &tm_time);
#endif
				SFINISH_CHECK(strftime(str_temp, 100, str_date_format, &tm_time) != 0, "strftime() failed");
				SFINISH_CAT_APPEND(str_response, "START\t", str_temp);

				memset(str_temp, 0, 101);
				SFINISH_CHECK(strftime(str_temp, 100, str_time_format, &tm_time) != 0, "strftime() failed");
				SFINISH_CAT_APPEND(str_response, "\t", str_temp);

				memset(str_temp, 0, 101);
				SFINISH_CHECK(snprintf(str_temp, 100, "%ld", (unsigned long)tv_time.tv_usec) != 0, "snprintf() failed");
				SFINISH_CAT_APPEND(str_response, "\t", str_temp);

				int_status = PQsendQuery(client_request->parent->cnxn, str_sql);
				if (int_status != 1) {
					SFINISH("Query failed: %s", PQerrorMessage(client_request->parent->cnxn));
				}
				query_callback(EV_A, client_request, ws_raw_step2);
			} else if (client_request->int_i == client_request->int_len) {
				client_request->int_response_id -= 1;
				SFREE(str_response);
				int_status = PQsendQuery(client_request->parent->cnxn, "COMMIT");
				if (int_status != 1) {
					SFINISH("Query failed: %s", PQerrorMessage(client_request->parent->cnxn));
				}
				query_callback(EV_A, client_request, ws_raw_step2);
			} else if (client_request->int_i > client_request->int_len) {
				SFINISH_CAT_APPEND(str_response, "TRANSACTION COMPLETED");
			}

			ev_check_stop(EV_A, &client_copy_check->check);
			SFREE(client_copy_check);
			break;
		}

		for (int_column = 0; int_column < int_num_columns; int_column += 1) {
			SFINISH_CAT_CSTR(str_sql_temp, PQgetvalue(res, (int)client_copy_check->int_i, (int)int_column));
			str_temp1 = escape_value(str_sql_temp);
			SFREE(str_sql_temp);
			SFINISH_CHECK(str_temp1 != NULL, "escape_value failed");

			SFINISH_CAT_APPEND(str_response, str_temp1,
				int_column < (int_num_columns - 1)
					? "\t"
					: (ssize_t)client_copy_check->int_i < (int_num_rows - 1) && (client_copy_check->int_i % 10) < 9 ? "\012"
																													: "");
			SFREE(str_temp1);
		}

		client_copy_check->int_i += 1;
	} while ((client_copy_check->int_i % 10) > 0);

finish:
	bol_error_state = false;
	SFREE_ALL();
	if (str_response != NULL) {
		WS_sendFrame(EV_A, client, true, 0x01, str_response, strlen(str_response));
		DArray_push(client_request->arr_response, str_response);
		str_response = NULL;
	}
}

#endif
