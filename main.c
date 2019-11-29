/*
 * Copyright Dmitrii Naidolinskii
 */
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <glib-2.0/glib.h>
#include <glib-2.0/gio/gio.h>
#include "parser.h"

GNotification *notify;

int sockfd;
int size = 16384;
char *rbuffer;
char *sbuffer;
const char *rping = "PING :tmi.twitch.tv";
const char *default_line = ":tmi.twitch.tv";
const char *join = "JOIN";
const char *msg = "PRIVMSG";
char *line_for_message;
char *message;
char *nick;
char *room;

const char *commands =
"next - audacious перключение песни вперед. "
"prev - audacious переключение песни назад. "
"help - эта справка"
;

const char *opt_help;

char *opt_oauth;
char *opt_channel;
char *opt_nickname;

int audacious;

gchar *audacious_player_next;
gchar *audacious_player_prev;

static void buffers_init ( ) {
	rbuffer = calloc ( size, 1 );
	sbuffer = calloc ( 1024, 1 );
}

static void copy_to_nick ( char *n, char **s ) {
	(*s)++;
	while ( *(*s) != '!' && *(*s) != 0x0 && *(*s) != '\r' ) {
		*n++ = *(*s)++;
	}
}
static void copy_to_room ( char *n, char **s ) {
	(*s)++;
	while ( *(*s) != '\n' && *(*s) != ' ' && *(*s) != 0x0 && *(*s) != '\r' ) {
		*n++ = *(*s)++;
	}
}
static void copy_to_message ( char *n, char **s ) {
	(*s)++;
	while ( *(*s) != '\n' && *(*s) != 0x0 && *(*s) != '\r' ) {
		*n++ = *(*s)++;
	}
}

GDBusProxy *audacious_proxy;

void audacious_manage_next ( ) {
	g_dbus_proxy_call_sync ( audacious_proxy,
			"Next",
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL
			);
}
void audacious_manage_prev ( ) {
	g_dbus_proxy_call_sync ( audacious_proxy,
			"Previous",
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL
			);
}


void print_help ( ) {
	gchar *body = g_strdup_printf ( "%s%s\r\n", line_for_message, commands );
	write ( sockfd, body, strlen ( body ) );
	g_free ( body );
}

static void check_body ( const char *s ) {
	if ( !strncmp ( s, audacious_player_next, strlen ( audacious_player_next ) + 1 ) ) { audacious_manage_next ( ); return; }
	if ( !strncmp ( s, audacious_player_prev, strlen ( audacious_player_prev ) + 1 ) ) { audacious_manage_prev ( ); return; }
	if ( !strncmp ( s, opt_help, strlen ( opt_help ) + 1 ) ) { print_help ( ); return; }
}

static void *handle ( void *data ) {
	GApplication *app = ( GApplication * ) data;
	notify = g_notification_new ( "twitch" );
	g_notification_set_priority ( notify, G_NOTIFICATION_PRIORITY_HIGH );

	if ( audacious == 1 ) {
		audacious_proxy = g_dbus_proxy_new_for_bus_sync (
				G_BUS_TYPE_SESSION,
				G_DBUS_PROXY_FLAGS_NONE,
				NULL,
				"org.atheme.audacious",
				"/org/mpris/MediaPlayer2",
				"org.mpris.MediaPlayer2.Player",
				NULL,
				NULL
				);
	}

	nick = calloc ( 255, 1 );
	room = calloc ( 255, 1 );
	message = calloc ( 1024, 1 );
	audacious_player_next = g_strdup_printf ( "@%s next", opt_nickname );
	audacious_player_prev = g_strdup_printf ( "@%s prev", opt_nickname );
	opt_help = g_strdup_printf ( "@%s help", opt_nickname );
	line_for_message = g_strdup_printf ( "PRIVMSG #%s :", opt_channel );
			
	while ( 1 ) {
		memset ( rbuffer, 0, size );
		int ret = read ( sockfd, rbuffer, size );
		if ( ret == -1 ) {
			perror ( "read" );
			continue;
		}
		if ( !strncmp ( rbuffer, rping, strlen ( rping ) ) ) {
			//printf ( "Sending pong\n" );
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
			gchar *body = g_strdup_printf ( "%s вошёл в комнату %s", nick, room );
			g_notification_set_body ( notify, body );
			g_application_send_notification ( app, "com.xverizex.twitch-bot", notify );
			g_free ( body );
		} else 
		if ( !strncmp ( s, msg, length_msg ) ) {
			s += length_msg + 1;
			copy_to_room ( room, &s );
			s++;
			copy_to_message ( message, &s );
			gchar *body = g_strdup_printf ( "%s: %s", nick, message );
			g_notification_set_body ( notify, body );
			g_application_send_notification ( app, "com.xverizex.twitch-bot", notify );
			check_body ( message );
			g_free ( body );
		}
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

static void init_opts ( ) {
	opt_oauth = calloc ( 255, 1 );
	opt_channel = calloc ( 255, 1 );
	opt_nickname = calloc ( 255, 1 );
}

static void g_startup ( GApplication *app, gpointer data ) {
	pthread_t t1;
	pthread_create ( &t1, NULL, handle, app );
	pthread_join ( t1, NULL );
}

int main ( int argc, char **argv ) {
	init_opts ( );
	parser_config_init ( );
	daemon ( 1, 1 );
	buffers_init ( );
	connect_to ( "irc.chat.twitch.tv", 6667 );

	sprintf ( sbuffer, "PASS %s\n", opt_oauth );
	int ret = write ( sockfd, sbuffer, strlen ( sbuffer ) );
	if ( ret == -1 ) {
		perror ( "write" );
	}
	memset ( sbuffer, 0, 1024 );
	sprintf ( sbuffer, "NICK %s\n", opt_nickname );
	ret = write ( sockfd, sbuffer, strlen ( sbuffer ) );
	if ( ret == -1 ) {
		perror ( "write" );
	}
	memset ( sbuffer, 0, 1024 );
	sprintf ( sbuffer, "JOIN #%s\n", opt_channel );
	ret = write ( sockfd, sbuffer, strlen ( sbuffer ) );
	if ( ret == -1 ) {
		perror ( "write" );
	}

	GApplication *app;
	app = g_application_new ( "com.xverizex.twitch-bot", G_APPLICATION_FLAGS_NONE );
	const gchar *g_id = g_application_get_application_id ( app );
	g_application_register ( app, NULL, NULL );
	g_signal_connect ( app, "activate", G_CALLBACK ( g_startup ), NULL );
	ret = g_application_run ( app, argc, argv );
	g_object_unref ( app );
	return ret;
}
