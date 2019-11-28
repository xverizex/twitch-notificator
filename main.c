#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>
#include "oauth.h"

int sockfd;
int size = 16384;
char *rbuffer;
char *sbuffer;
const char *rping = "PING :tmi.twitch.tv";
const char *default_line = ":tmi.twitch.tv";
const char *join = "JOIN";
const char *msg = "PRIVMSG";
char *message;
char *nick;
char *room;

NotifyNotification *notify;

static void buffers_init ( ) {
	rbuffer = calloc ( size, 1 );
	sbuffer = calloc ( 1024, 1 );
}

void copy_to_nick ( char *n, char **s ) {
	(*s)++;
	while ( *(*s) != '!' && *(*s) != 0x0 ) {
		*n++ = *(*s)++;
	}
}
void copy_to_room ( char *n, char **s ) {
	(*s)++;
	while ( *(*s) != '\n' && *(*s) != ' ' && *(*s) != 0x0 ) {
		*n++ = *(*s)++;
	}
}
void copy_to_message ( char *n, char **s ) {
	(*s)++;
	while ( *(*s) != '\n' && *(*s) != 0x0 ) {
		*n++ = *(*s)++;
	}
}

void *handle ( void *data ) {
//	notify = g_notification_new ( "twitch" );

	notify_init ( "twitch-bot" );
	notify = notify_notification_new ( "twitch", "body", NULL );
	notify_notification_set_timeout ( notify, NOTIFY_EXPIRES_NEVER );
	nick = calloc ( 255, 1 );
	room = calloc ( 255, 1 );
	message = calloc ( 1024, 1 );
	while ( 1 ) {
		memset ( rbuffer, 0, size );
		int ret = read ( sockfd, rbuffer, size );
		if ( ret == -1 ) {
			perror ( "read" );
			continue;
		}
		if ( !strncmp ( rbuffer, rping, strlen ( rping ) ) ) {
			printf ( "Отправляется pong\n" );
			char *pong = "PONG :tmi.twitch.tv\n";
			int len = strlen ( pong );
			write ( sockfd, pong, len );
			continue;
		}
		if ( !strncmp ( rbuffer, default_line, strlen ( default_line ) ) ) { continue; }
		char *s = rbuffer;
		copy_to_nick ( nick, &s );
		s = strchr ( s, ' ' );
		if ( !s )  {
			memset ( rbuffer, 0, size );
			memset ( nick, 0, 255 );
			continue;
		}

		s++;
		int length_join = strlen ( join );
		int length_msg = strlen ( msg );
		if ( !strncmp ( s, join, length_join ) ) {
			s += length_join + 1;
			copy_to_room ( room, &s );
			//printf ( "%s вошёл в комнату %s\n", nick, room );
			gchar *body = g_strdup_printf ( "%s вошёл в комнату %s", nick, room );
			notify_notification_update ( notify, "twitch", body, NULL );
			notify_notification_show ( notify, NULL );
			g_free ( body );
		} else 
		if ( !strncmp ( s, msg, length_msg ) ) {
			s += length_msg + 1;
			copy_to_room ( room, &s );
			s ++;
			copy_to_message ( message, &s );
			printf ( "%s: %s\n", nick, message );
			gchar *body = g_strdup_printf ( "%s: %s", nick, message );
			notify_notification_update ( notify, "twitch", body, NULL );
			notify_notification_show ( notify, NULL );
			g_free ( body );
		}
#if 0
		FILE *fp = fopen ( "log", "a" );
		fprintf ( fp, rbuffer, strlen ( rbuffer ) );
		fclose ( fp );
#endif
		memset ( rbuffer, 0, size );
		memset ( nick, 0, 255 );
		memset ( room, 0, 255 );
		memset ( message, 0, 1024 );
	}
}

static void connect_to ( const char *site, unsigned short port ) {
	struct hostent *ht = gethostbyname ( site );
	struct sockaddr_in s;
	s.sin_family = AF_INET;
	s.sin_port = htons ( port );
	s.sin_addr.s_addr = *( ( in_addr_t * ) ht->h_addr );
	sockfd = socket ( AF_INET, SOCK_STREAM, 0 );
	if ( sockfd == -1 ) {
		perror ( "socket" );
		exit ( EXIT_FAILURE );
	}
	int ret = connect ( sockfd, ( struct sockaddr * ) &s, sizeof ( s ) );
	if ( ret == -1 ) {
		perror ( "connect" );
		exit ( EXIT_FAILURE );
	}
}

int main ( int argc, char **argv ) {
	daemon ( 1, 1 );
	buffers_init ( );
	pthread_t t1;
	connect_to ( "irc.chat.twitch.tv", 6667 );

	sprintf ( sbuffer, "PASS %s\n", oauth );
	int ret = write ( sockfd, sbuffer, strlen ( sbuffer ) );
	if ( ret == -1 ) {
		perror ( "write" );
	}
	memset ( sbuffer, 0, 1024 );
	sprintf ( sbuffer, "NICK %s\n", my_nick );
	ret = write ( sockfd, sbuffer, strlen ( sbuffer ) );
	if ( ret == -1 ) {
		perror ( "write" );
	}
	memset ( sbuffer, 0, 1024 );
	sprintf ( sbuffer, "JOIN #%s\n", channel );
	ret = write ( sockfd, sbuffer, strlen ( sbuffer ) );
	if ( ret == -1 ) {
		perror ( "write" );
	}
	pthread_create ( &t1, NULL, handle, NULL );

	GMainLoop *loop = g_main_loop_new ( NULL, TRUE );

	g_main_loop_run ( loop );
}
