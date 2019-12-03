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
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <json-c/json.h>
#include "webhook.h"

int sockserver;
extern unsigned short port_event;
extern int uid;
extern pid_t pid_server_webhook;

pthread_key_t key;
static pthread_once_t once = PTHREAD_ONCE_INIT;

void destruct ( void *data ) {
	struct data *dt = ( struct data * ) data;

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

	free ( dt->data_buffer );

	struct json *js = &dt->json;

	if ( js->tok ) free ( js->tok );
	if ( js->root ) free ( js->root );
	if ( js->data ) free ( js->data );
	if ( js->array ) free ( js->array );
	if ( js->from_name ) free ( js->from_name );
	if ( js->to_name ) free ( js->to_name );

	free ( dt );
}

static void once_creator ( void ) {
	pthread_key_create ( &key, destruct );
}

void *handle_server ( void *usr_data ) {
#if 1
	pthread_once ( &once, once_creator );
	pthread_setspecific ( key, calloc ( 1, sizeof ( struct data ) ) );
	struct data *dt = pthread_getspecific ( key );
	memset ( dt, 0, sizeof ( struct data ) );
#endif
	GApplication *app = ( GApplication * ) usr_data;

	sockserver = socket ( AF_INET, SOCK_STREAM, 0 );
	if ( sockserver == -1 ) {
		perror ( "socket" );
		exit ( EXIT_FAILURE );
	}
	struct sockaddr_in s;
	memset ( &s, 0, sizeof ( s ) );
	s.sin_family = AF_INET;
	s.sin_port = htons ( port_event );

	int yes = 1;
	int ret;
	ret = setsockopt ( sockserver, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof ( int ) );
	if ( ret == -1 ) {
		perror ( "setsockopt" );
		exit ( EXIT_FAILURE );
	}

	ret = bind ( sockserver, ( struct sockaddr * ) &s, sizeof ( s ) );
	if ( ret == -1 ) {
		perror ( "bind" );
		exit ( EXIT_FAILURE );
	}

	listen ( sockserver, 0 );
	
	struct sockaddr_in client;
	socklen_t size = sizeof ( client );

	char *user_name = getenv ( "USER" );
	if ( user_name ) {
		struct passwd *pw = getpwnam ( user_name );
		if ( pw->pw_uid == 0 ) {
			setuid ( uid );
		}
	}

	char *data_input_server = calloc ( 4096, 1 );

	dt->data_buffer = data_input_server;
	while ( 1 ) {
		memset ( &data_input_server[0], 0, 4096 );
		int sockclient = accept ( sockserver, ( struct sockaddr * ) &client, &size );
		read ( sockclient, data_input_server, 4095 );
		handle_data ( sockclient, data_input_server, app, dt );
		close ( sockclient );
	}
}
