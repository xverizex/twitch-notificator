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
#include <openssl/ssl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>

int sockhook;
const SSL_METHOD *method_hook;
SSL_CTX *ctx_hook;
SSL *ssl_hook;
char *data_hook;
extern char *opt_channel;
extern char *opt_client_id;
extern char *opt_callback;

void hook_connect_to ( const char *site, unsigned short port ) {
	struct hostent *ht = gethostbyname ( site );
	struct sockaddr_in s;
	s.sin_family = AF_INET;
	s.sin_port = htons ( port );
	s.sin_addr.s_addr = *( ( in_addr_t * ) ht->h_addr );

	sockhook = socket ( AF_INET, SOCK_STREAM, 0 );
	if ( sockhook == -1 ) {
		perror ( "socket" );
		exit ( EXIT_FAILURE );
	}
	int ret = connect ( sockhook, ( struct sockaddr * ) &s, sizeof ( s ) );
	if ( ret == -1 ) {
		perror ( "connect" );
		exit ( EXIT_FAILURE );
	}

	method_hook = SSLv23_client_method ( );
	ctx_hook = SSL_CTX_new ( method_hook );
	ssl_hook = SSL_new ( ctx_hook );
	SSL_set_fd ( ssl_hook, sockhook );
	SSL_connect ( ssl_hook );
}

char *channel_id;

static void find_channel ( const char *nick ) {
	char *data = calloc ( 1024, 1 );

	snprintf ( data, 1023,
			"GET /kraken/users?login=%s HTTP/1.1\r\n"
			"Client-ID: %s\r\n"
			"Accept: application/vnd.twitchtv.v5+json\r\n"
			"Host: api.twitch.tv\r\n"
			"\r\n"
			,
			opt_channel,
			opt_client_id
		 );
	int length_of_data = strlen ( data );
	SSL_write ( ssl_hook, data, length_of_data );
	SSL_read ( ssl_hook, data_hook, 1023 );
	const char *ok = "HTTP/1.1 200 OK";
	if ( strncmp ( data_hook, ok, strlen ( ok ) ) ) {
		free ( data );
		return;
	}
	memset ( data_hook, 0, 1024 );
	SSL_read ( ssl_hook, data_hook, 1023 );
	json_object *root = json_tokener_parse ( data_hook );
	json_object *json_follower;
	json_object *json_users;

	json_object_object_get_ex ( root, "users", &json_users );
	json_object *json_id = json_object_array_get_idx ( json_users, 0 );
	json_object_object_get_ex ( json_id, "_id", &json_follower );
	const char *cid = json_object_get_string ( json_follower );
	memset ( channel_id, 0, 512 );
	strncpy ( channel_id, cid, strlen ( cid ) );
	free ( data );
	json_object_put ( json_follower );
	json_object_put ( json_id );
	json_object_put ( json_users );
	json_object_put ( root );

}

int subscribe ( const int true ) {
	find_channel ( opt_channel );
	char *buf = calloc ( 1024, 1 );
	snprintf ( buf, 1023,
		"{"
		"\"hub.mode\":\"%s\","
		"\"hub.topic\":\"https://api.twitch.tv/helix/users/follows?first=1&to_id=%s\","
		"\"hub.lease_seconds\":\"864000\","
		"\"hub.callback\":\"%s\""
		"}"
		,
		true ? "subscribe" : "unsubscribe",
		channel_id,
		opt_callback
		)
		;

	int length = strlen ( buf );
	char *data = calloc ( 1024, 1 );
	snprintf ( data, 1023,
			"POST /helix/webhooks/hub HTTP/1.1\r\n"
			"Host: api.twitch.tv\r\n"
			"User-Agent: com.xverizex.twitch-bot\r\n"
			"Content-Type: application/json\r\n"
			"Client-ID: %s\r\n"
			"Content-Length: %d\r\n"
			"Accept: */*\r\n"
			"\r\n"
			"%s",
			opt_client_id,
			length,
			buf
			);
	SSL_write ( ssl_hook, data, strlen ( data ) );

	SSL_read ( ssl_hook, data_hook, 1023 );
	const char *ok = "HTTP/1.1 202 Accepted";
	if ( !strncmp ( data_hook, ok, strlen ( ok ) ) ) return 1;
	return 0;
}

void subscribe_init ( ) {
	channel_id = calloc ( 512, 1 );
	data_hook = calloc ( 1024, 1 );
}
void connect_for_webhook ( ) {
	hook_connect_to ( "api.twitch.tv", 443 );
}
