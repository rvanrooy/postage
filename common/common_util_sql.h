#pragma once

#include "common_client.h"
#include "util/util_darray.h"
#include "util/util_error.h"
#include "util/util_string.h"
#include "ws_insert.h"
#include "ws_select.h"
#include "ws_update.h"

/*
This function takes a SELECT/INSERT/UPDATE/DELETE request and returns the table
name
*/
char *get_table_name(char *_str_query);
/*
This function takes a SELECT/INSERT/UPDATE request and returns the return
columns
*/
char *get_return_columns(char *_str_query, char *str_table_name);

#ifndef POSTAGE_INTERFACE_LIBPQ
/*
This function takes a SELECT/INSERT/UPDATE request and returns the return
columns in a format that concatinates and escapes them
*/
char *get_return_escaped_columns(DB_driver driver, char *_str_query);
#endif

/*
This function takes a INSERT/UPDATE request and returns the hash columns
*/
char *get_hash_columns(char *_str_query);
/*
This function handles the copy out functionality for websockets.
*/
bool ws_copy_check_cb(EV_P, bool bol_success, bool bol_last, void *cb_data, char *str_response, size_t int_len);
/*
This function handles the copy out functionality for html.
*/
bool http_copy_check_cb(EV_P, bool bol_success, bool bol_last, void *cb_data, char *str_response, size_t int_len);

typedef bool (*readable_cb_t)(EV_P, void *cb_data, bool bol_group);
typedef struct {
	void *cb_data;
	readable_cb_t readable_cb;
} DB_readable_poll;

#ifdef ENVELOPE
bool permissions_check(EV_P, DB_conn *conn, char *str_path, void *cb_data, readable_cb_t readable_cb);
bool permissions_write_check(EV_P, DB_conn *conn, char *str_path, void *cb_data, readable_cb_t readable_cb);
char *canonical_strip_start(char *str_path);
char *canonical_full_start(char *str_path);
#endif // ENVELOPE

/* Determine the group permissions of a user */
bool ddl_readable(EV_P, DB_conn *conn, char *str_path, bool bol_writeable, void *cb_data, readable_cb_t readable_cb);
