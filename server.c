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

void *handle_server ( void *usr_data ) {
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
	char *data = calloc ( 16384, 1 );

	char *user_name = getenv ( "USER" );
	if ( user_name ) {
		struct passwd *pw = getpwnam ( user_name );
		if ( pw->pw_uid == 0 ) {
			setuid ( uid );
		}
	}
	while ( 1 ) {
		int sockclient = accept ( sockserver, ( struct sockaddr * ) &client, &size );
		int ret;
		read ( sockclient, data, 16383 );
		handle_data ( sockclient, data, app );
		memset ( data, 0, 16384 );
		close ( sockclient );
	}
}
