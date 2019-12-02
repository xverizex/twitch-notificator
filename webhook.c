/*
 * twitch-bot - бот показывает сообщения от twitch в области уведомления
 *
 * Copyright (C) 2019 Naidolinsky Dmitry <naidv88@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY of FITNESS for A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * -------------------------------------------------------------------/
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib-2.0/gio/gio.h>
#include <json-c/json.h>
#include <string.h>
#include "webhook.h"

#define GET_REQUEST         1
#define POST_REQUEST        2

extern struct data *dt;

int find_r ( const char *ss, const int length ) {
	for ( int i = 0; i < length; i++ ) {
		if ( *ss++ == '\r' ) {
			int res = i;
			return res;
		}
	}
	return length;
}

const char *str_get = "GET";
const char *str_post = "POST";
const char *str_http1 = "HTTP/1.1";
const char *str_http0 = "HTTP/1.0";

static char *get_var ( const char **p ) {
	const char *st = *p;
	int length = 0;
	for ( ; *(*p) != 0x0 && *(*p) != '='; (*p)++ ) length++;
	char *var = calloc ( length + 1, 1 );
	strncpy ( var, st, length );
	(*p)++;
	return var;
}

static char *get_value ( const char **p ) {
	const char *st = *p;
	int length = 0;
	for ( ; *(*p) != 0x0 && *(*p) != '&'; (*p)++ ) length++;
	char *value = calloc ( length + 1, 1 );
	strncpy ( value, st, length );
	(*p)++;
	return value;
}

static void parse_get_param ( const char **p ) {
	char *var = get_var ( p );
	if ( !var ) return;
	char *value = get_value ( p );
	if ( !value ) {
		free ( var );
		return;
	}
	int index = dt->get.max_count++;
	dt->get.var = realloc ( dt->get.var, sizeof ( char * ) * dt->get.max_count );
	dt->get.value = realloc ( dt->get.value, sizeof ( char * ) * dt->get.max_count );
	dt->get.var[index] = var;
	dt->get.value[index] = value;
}

static void parse_get_line_request ( const char **p ) {
	const char *com = strchr ( *p, '?' );
	if ( !com ) return;
	int length = com - *p;
	dt->get.line = realloc ( dt->get.line, length + 1 );
	memset ( dt->get.line, 0, length + 1 );
	strncpy ( dt->get.line, *p, length );
	(*p) += length + 1;
}

static int valid_hex ( const char *l ) {
	for ( int i = 0; i < 2; i++, l++ ) {
		if ( *l >= '0' && *l <= '9' ) continue;
		if ( *l >= 'a' && *l <= 'f' ) continue;
		if ( *l >= 'A' && *l <= 'F' ) continue;
		return 1;
	}
	return 0;
}

static char get_char_hex ( const char *l ) {
	char h = *l;
	l++;
	char n = *l;

	if ( n >= '0' && n <= '9' ) { n -= '0'; }
	else
	if ( n >= 'a' && n <= 'f' ) { n = n - 'a' + 10; }
	else
	if ( n >= 'A' && n <= 'F' ) { n = n - 'A' + 10; }

	if ( h >= '0' && h <= '9' ) { h -= '0'; }
	else
	if ( h >= 'a' && h <= 'f' ) { h = n - 'a' + 10; }
	else
	if ( h >= 'A' && h <= 'F' ) { h = n - 'A' + 10; }

	char res = n + h * 16;
	return res;
}

static char get_hex ( char *l ) {
	if ( *l == '%' ) l++;
	int ret = valid_hex ( l );
	if ( ret ) return 0;
	char s = get_char_hex ( l );
	return s;
}


static char *normalize_line ( char *l ) {
	int length = 0;
	char *st = l;
	int len = strlen ( st );
	for ( ; *l; ) {
		if ( *l == '%' ) {
			char s = get_hex ( l );
			*l = s;
			if ( *l == 0 ) {
				st = realloc ( st, 0 );
				l = st + length;
				*st = 0;
				return st;		
			}
			length = st + len - l;
			l++;
			memmove ( l, l + 2, length );
			st = realloc ( st, len );
			len -= 2;
			l  = st;
			if ( *l == '%' ) continue;
		}
		l++;
	}
	return st;
}

static void parse_get_request ( const char *str ) {
	const char *s = str;
	parse_get_line_request ( &s );
	while ( *s ) {
		parse_get_param ( &s );
	}

	dt->get.line = normalize_line ( dt->get.line );

	for ( int i = 0; i < dt->get.max_count; i++ ) {
//		dt->get.var[i] = normalize_line ( dt->get.var[i] );
		dt->get.value[i] = normalize_line ( dt->get.value[i] );
	}
}

int req_end;
int req_data;
int error_var;

static char *create_header_var ( const char **p ) {
	while ( *(*p) == ' ' ) (*p)++;
	const char *ss = *p;
	for ( ; *(*p) != ':' && *(*p) != '\n' && *(*p) != '\r' && *(*p) != 0x0; (*p)++ );
	int length = *p - ss;
	if ( length == 0 ) {
		error_var = 1;
		return NULL;
	}
	char *var = calloc ( length + 1, 1 );
	strncpy ( var, ss, length );
	if ( *(*p) == 0x0 ) { (*p) += 1; return var; }
	if ( *(*p) == '\r' && *(*p + 1) == '\n' ) (*p) += 2;
	else
	if ( *(*p) == '\r' && *(*p + 1) != '\n' ) (*p) += 1;
	else
	(*p) += 1;
	return var;
}
static char *create_header_val ( const char **p ) {
	while ( *(*p) == ' ' ) (*p)++;
	const char *ss = *p;
	for ( ; *(*p) != '\r' && *(*p) != '\n' && *(*p) != 0x0; (*p)++ );
	int length = *p - ss;
	if ( length == 0 ) {
		error_var = 1;
		return NULL;
	}
	char *val = calloc ( length + 1, 1 );
	strncpy ( val, ss, length );
	if ( *(*p) == 0x0 ) { return val; }
	if ( *(*p) == '\r' && *(*p + 1) == '\n' ) (*p) += length + 2;
	else
	if ( *(*p) == '\r' && *(*p + 1) != '\n' ) (*p) += length + 1;
	else
	(*p) += length;
	return val;
}

static void parse_header ( const char *s ) {
	if ( *s == '\r' && ( *( s + 1 ) == '\n' ) ) {
		req_data = 1;
		return;
	}
	if ( *s == '\n' ) {
		req_data = 1;
		return;
	}
	if ( *s == '\r' ) {
		return;
	}
	char *pp = strchr ( s, ':' );
	if ( !pp ) return;
	char *end = strchr ( s, '\n' );
	if ( !end ) return;
	if ( end - pp < 0 ) return;

	char *var = create_header_var ( &s );
	if ( error_var ) return;
	char *val = create_header_val ( &s );
	if ( error_var ) {
		if ( var ) free ( var );
		if ( val ) free ( val );
		return;
	}

	switch ( dt->type_of_request ) {
		case GET_REQUEST:
			{
				int index = dt->head.max_count++;

				dt->head.var = realloc ( dt->head.var, sizeof ( char * ) * dt->head.max_count );
				dt->head.value = realloc ( dt->head.value, sizeof ( char * ) * dt->head.max_count );

				dt->head.var[index] = var;
				dt->head.value[index] = val;
			}
			break;
		case POST_REQUEST:
			{
				int index = dt->head.max_count++;

				dt->head.var = realloc ( dt->head.var, sizeof ( char * ) * dt->head.max_count );
				dt->head.value = realloc ( dt->head.value, sizeof ( char * ) * dt->head.max_count );

				dt->head.var[index] = var;
				dt->head.value[index] = val;
			}
			break;
	}
}


static void copy_to_struct ( int *idx, const char *ss, const char *s ) {
	int length = s - ss;
	int pos = length;
	length = find_r ( ss, length );
	char *str = calloc ( length + 1, 1 );
	strncpy ( str, ss, length );

	if ( *idx == 0 && !strncmp ( str, str_get, strlen ( str_get ) + 1 ) ) {
		dt->type_of_request = GET_REQUEST;
		*idx += pos;
		free ( str );
		return;
	}
	if ( *idx == 0 && !strncmp ( str, str_post, strlen ( str_get ) + 1 ) ) {
		dt->type_of_request = POST_REQUEST;
		*idx += pos;
		free ( str );
		return;
	}
	if ( *idx == 0 ) {
		*idx += pos;
		free ( str );
		error_var = 1;
		return;
	}

	if ( *idx > 0 && !strncmp ( str, str_http1, strlen ( str_http1 ) + 1 ) ) {
		*idx += pos;
		free ( str );
		req_end = 1;
		return;
	}
	if ( *idx > 0 && !strncmp ( str, str_http0, strlen ( str_http0 ) + 1 ) ) {
		*idx += pos;
		free ( str );
		req_end = 1;
		return;
	}

	/* тело запроса */
	if ( *idx > 0 ) {
		switch ( dt->type_of_request ) {
			case GET_REQUEST:
				parse_get_request ( str );
				*idx += pos;
				return;
			case POST_REQUEST:
				dt->post.line = str;
				req_end = 1;
				break;
		}
	}
}

static int get_head_var ( const char *var ) {
	for ( int i = 0; i < dt->head.max_count; i++ ) {
		if ( !strncmp ( dt->head.var[i], var, strlen ( var ) + 1 ) ) return i;
	}
	return -1;
}

static int check_num ( const char *s ) {
	for ( ; *s != '\r' && *s != '\n' && *s != 0x0; s++ ) {
		if ( *s >= '0' && *s <= '9' ) continue;
		return -1;
	}
	return 0;
}

static void read_body ( const char *s, const int length ) {
	dt->post.body = realloc ( dt->post.body, length + 1 );
	memset ( dt->post.body, 0, length + 1 );
	strncpy ( dt->post.body, s, length );
}

static void parse_line ( const char **p ) {
	const char *s = *p;
	const char *ss = s;
	int idx = 0;
	int space = 0;
	for ( ; *s != 0x0; s++ ) {
		if ( *s == '\n' ) {
			copy_to_struct ( &idx, ss, s );
			if ( error_var ) return;
			req_end = 1;
			ss = s + 1;
			if ( req_end ) break;
			break;
		}
		if ( *s == ' ' ) {
			space++;
			if ( space >= 3 ) {
				error_var = 1;
				return;
			}
			copy_to_struct ( &idx, ss, s );
			if ( error_var ) return;
			ss = s + 1;
			if ( req_end ) break;
		}
	}
	if ( *s == 0x0 ) return;
	ss = s;
	for ( ; *s != 0x0; s++ ) {
		if ( *s == '\n' ) {
			if ( req_end && !req_data ) {
				parse_header ( ss );
				if ( error_var ) return;
				ss = s + 1;
				if ( req_data ) {
					s++;
					break;
				}
			}
		}
	}
	(*p) += s - *p;
	if ( dt->type_of_request == GET_REQUEST ) return;

	int content_length = get_head_var ( "Content-Length" );
	if ( content_length == -1 ) { 
		error_var = 1;
		return;
	}

	char *num = dt->head.value[content_length];

	int ret = check_num ( num );
	if ( ret ) return;
	unsigned int length = atoi ( num );
	dt->post.length = length;
	while ( *s == '\r' || *s == '\n' ) s++;
	read_body ( s, length );
	return;
}

static int get_get_var ( const char *var ) {
	for ( int i = 0; i < dt->get.max_count; i++ ) {
		if ( !strncmp ( dt->get.var[i], var, strlen ( var ) + 1 ) ) return i;
	}
	return -1;
}

static void parse_request ( const char *s ) {
	parse_line ( &s );
}

extern GNotification *notify;
extern int sockserver;

char follower[255];

void handle_data ( const int sockclient, const char *buffer, GApplication *app ) {

	dt->get.line = calloc ( 0, 1 );
	dt->get.var = calloc ( 0, 1 );
	dt->get.value = calloc ( 0, 1 );
	dt->post.line = calloc ( 0, 1 );
	dt->post.body = calloc ( 0, 1 );
	dt->head.var = calloc ( 0, 1 );
	dt->head.value = calloc ( 0, 1 );
	dt->get.max_count = 0;
	dt->head.max_count = 0;
	dt->post.length = 0;

	parse_request ( buffer );

	req_end = 0;
	req_data = 0;


	if ( dt->type_of_request == 0 || error_var ) {
		return;
	}

	switch ( dt->type_of_request ) {
		case GET_REQUEST:
			{
				int num = get_get_var ( "hub.challenge" );
				if ( num == -1 ) break;
				char *answer = calloc ( 255, 1 );
				int length = strlen ( dt->get.value[num] );
				snprintf ( answer, 254,
						"HTTP/1.0 200 OK\r\n"
						"\r\n"
						"%s"
						,
						dt->get.value[num]
					 );
				write ( sockclient, answer, strlen ( answer ) );
				free ( answer );
			}
			break;
		case POST_REQUEST:
			{
				json_tokener *tok = json_tokener_new ( );
				json_object *root;
				json_object *data;
				json_object *array;
				json_object *from_name;
				json_object *to_name;
				enum json_tokener_error jerr;
				root = json_tokener_parse_ex ( tok, dt->post.body, dt->post.length );
				while ( ( jerr = json_tokener_get_error ( tok ) ) == json_tokener_continue );
				do {
					if ( jerr != json_tokener_success ) {
						json_object_put ( root );
						json_tokener_free ( tok );
						break;
					}
					json_tokener_free ( tok );
					json_object_object_get_ex ( root, "data", &data );
					if ( !data ) {
						json_object_put ( root );
						break;
					}
					array = json_object_array_get_idx ( data, 0 );
					if ( !array ) {
						json_object_put ( data );
						json_object_put ( root );
						break;
					}
					json_object_object_get_ex ( array, "from_name", &from_name );
					if ( !from_name ) {
						json_object_put ( array );
						json_object_put ( data );
						json_object_put ( root );
						break;
					}
					json_object_object_get_ex ( array, "to_name", &to_name );
					if ( !to_name ) {
						json_object_put ( from_name );
						json_object_put ( array );
						json_object_put ( data );
						json_object_put ( root );
						break;
					}
					const char *str_from_name = json_object_get_string ( from_name );
					const char *str_to_name   = json_object_get_string ( to_name );
					memset ( follower, 0, 255 );
					snprintf ( follower, 254,
							"%s подписался на канал %s",
							str_from_name,
							str_to_name
						 );
					g_notification_set_body ( notify, follower );
					g_application_send_notification ( app, "com.xverizex.twitch-bot", notify );
					
				} while ( 0 );
			}
			break;
	}

	for ( int i = 0; i < dt->get.max_count; i++ ) {
		if ( dt->get.var[i] ) {
			free ( dt->get.var[i] );
			dt->get.var[i] = NULL;
		}
		if ( dt->get.value[i] ) {
			free ( dt->get.value[i] );
			dt->get.value[i] = NULL;
		}
	}
	dt->get.max_count = 0;
	if ( dt->get.line ) {
		free ( dt->get.line );
		dt->get.line = NULL;
	}
	if ( dt->post.line ) {
		free ( dt->post.line );
		dt->post.line = NULL;
	}
	dt->post.length = 0;
	
	if ( dt->post.body ) {
		free ( dt->post.body );
		dt->post.body = NULL;
	}

	for ( int i = 0; i < dt->head.max_count; i++ ) {
		if ( dt->head.var[i] ) {
			free ( dt->head.var[i] );
			dt->head.var[i] = NULL;
		}
		if ( dt->head.value[i] ) {
			free ( dt->head.value[i] );
			dt->head.value[i] = NULL;
		}
	}
	dt->head.max_count = 0;
}
