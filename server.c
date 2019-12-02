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
extern char *data_input_server;

int close_thread;

void sig_handler ( int sig ) {
	close_thread = 1;
}

void *handle_server ( void *usr_data ) {
	pid_server_webhook = getpid ( );
	signal ( SIGTERM, sig_handler );
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
	while ( 1 ) {
		memset ( data_input_server, 0, 4096 );
		if ( close_thread ) { break; }
		int sockclient = accept ( sockserver, ( struct sockaddr * ) &client, &size );
		if ( close_thread ) {
			close ( sockclient );
			break;
		}
		int ret;
		read ( sockclient, data_input_server, 4095 );
		if ( close_thread ) {
			close ( sockclient );
			break;
		}
		handle_data ( sockclient, data_input_server, app );
		close ( sockclient );
	}

	close ( sockserver );
	close_thread = 0;
	exit ( EXIT_SUCCESS );
}
