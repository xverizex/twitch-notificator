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
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <glib-2.0/glib.h>
#include <glib-2.0/gio/gio.h>
#include <signal.h>
#include "config.h"
#include "parser.h"
#ifdef AUDIO_NOTIFICATIONS
#include <gst/gst.h>
#include "audio.h"
#endif


const char *prog = "com.xverizex.twitch-notificator";
gchar *g_id;

GNotification *notify;
GApplication *global_app;

int show_notify_frozen;
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

GDBusProxy *audacious_proxy;

#ifdef AUDIO_NOTIFICATIONS
struct play_notification play_message, play_follower;
#endif


const char *commands =
"next - ( audacious ) перключение песни вперед. "
"prev - ( audacious ) переключение песни назад. "
"track - ( audacious ) показывает информацию о текущей песне. "
"help - эта справка"
;

const char *opt_help;

char *opt_oauth;
char *opt_channel;
char *opt_nickname;
char *opt_client_id;
char *opt_callback;
char *opt_iface;
char *opt_new_message;
char *opt_new_follower;

int n_client_id;
unsigned short port_event;
int audacious;
int uid;
int notify_frozen;

gchar *player_next;
gchar *player_prev;
gchar *player_track;

pid_t pid_handle_irc;
pid_t pid_server_webhook;

gchar *body;

guint id_audacious;
GDBusConnection *con_audacious;

void sig_handle ( int sig ) {
	switch ( sig ) {
		case SIGINT:
			g_dbus_connection_signal_unsubscribe ( con_audacious, id_audacious );
			sleep ( 3 );
			exit ( EXIT_SUCCESS );
			break;
	}
}
static void handle_player_state ( GDBusConnection *con,
		const gchar *sender_name,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *signal_name,
		GVariant *param,
		gpointer data
		) {
	gint64 pos;
	g_variant_get ( param, "(x)", &pos );
	if ( pos == 0 ) {
		GVariant *var = g_dbus_proxy_get_cached_property ( audacious_proxy, "Metadata" );
		GVariant *title = NULL;
		GVariant *album = NULL;
		if ( var ) {
			title = g_variant_lookup_value ( var, "xesam:title", NULL );
			album = g_variant_lookup_value ( var, "xesam:album", NULL );
		}
		gsize length;

		if ( title && album ) {
			gchar *message = g_strdup_printf ( "Сейчас играет: альбом: %s. песня: %s", 
					g_variant_get_string ( album, &length ), 
					g_variant_get_string ( title, &length ) );
			gchar *body = g_strdup_printf ( "%s%s\n", line_for_message, message );
	
			write ( sockfd, body, strlen ( body ) );
			g_free ( body );
			g_free ( message );
		}
		if ( var ) g_variant_unref ( var );
		if ( title ) g_variant_unref ( title );
		if ( album ) g_variant_unref ( album );
	}
}
void init_for_irc_net ( ) {
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

		int once_player = 0;

		if ( audacious == 1 ) {
			con_audacious = g_dbus_proxy_get_connection ( audacious_proxy );
			id_audacious = g_dbus_connection_signal_subscribe ( 
				con_audacious,
				"org.atheme.audacious",
				"org.mpris.MediaPlayer2.Player",
				"Seeked",
				"/org/mpris/MediaPlayer2",
				NULL,
				G_DBUS_SIGNAL_FLAGS_NONE,
				handle_player_state,
				NULL,
				NULL
				);
			once_player = 1;
		}

		nick = calloc ( 255, 1 );
		room = calloc ( 255, 1 );
		message = calloc ( 1024, 1 );
		player_next = g_strdup_printf ( "@%s next", opt_nickname );
		player_prev = g_strdup_printf ( "@%s prev", opt_nickname );
		player_track = g_strdup_printf ( "@%s track", opt_nickname );
		opt_help = g_strdup_printf ( "@%s help", opt_nickname );
		line_for_message = g_strdup_printf ( "PRIVMSG #%s :", opt_channel );
		body = calloc ( 1024, 1 );
}

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

int trigger_player;
int trigger_player_run;

void audacious_manage_track ( ) {
	GVariant *var = g_dbus_proxy_get_cached_property ( audacious_proxy, "Metadata" );
	GVariant *title = NULL;
	GVariant *album = NULL;
	if ( var ) {
		title = g_variant_lookup_value ( var, "xesam:title", NULL );
		album = g_variant_lookup_value ( var, "xesam:album", NULL );
	}
	gsize length;

	if ( title && album ) {
		gchar *message = g_strdup_printf ( "альбом: %s. песня: %s", g_variant_get_string ( album, &length ), g_variant_get_string ( title, &length ) );
		gchar *body = g_strdup_printf ( "%s%s\n", line_for_message, message );

		write ( sockfd, body, strlen ( body ) );
		g_free ( body );
		g_free ( message );

		trigger_player = 1;
	}

	if ( album ) g_variant_unref ( album );
	if ( title ) g_variant_unref ( title );
	if ( var ) g_variant_unref ( var );

	trigger_player_run = 1;
}

char *body_help;


void print_help ( ) {
	memset ( body_help, 0, 4096 );
	snprintf ( body_help, 4095, "%s%s\n", line_for_message, commands );
	write ( sockfd, body_help, strlen ( body_help ) );
}

static void check_body ( const char *s ) {
	do {
		if ( audacious ) {
			if ( !strncmp ( s, player_next, strlen ( player_next ) + 1 ) ) { 
				audacious_manage_next ( ); 
				break;
			}
			if ( !strncmp ( s, player_prev, strlen ( player_prev ) + 1 ) ) { 
				audacious_manage_prev ( ); 
				break;
			}
			if ( !strncmp ( s, player_track, strlen ( player_track ) + 1 ) ) { 
				audacious_manage_track ( ); 
				break;
			}
		}
		break;
	} while ( 0 );

	if ( !strncmp ( s, opt_help, strlen ( opt_help ) + 1 ) ) { print_help ( ); return; }

	if ( trigger_player == 0 && trigger_player_run ) {
		gchar *message = g_strdup_printf ( "%s%s\n", line_for_message, "Ни один плеер не включен." );
		write ( sockfd, message, strlen ( message ) );
		g_free ( message );
	}

	trigger_player_run = 0;
	trigger_player = 0;
}

int run_once;

static void *handle ( void *data ) {
	pid_handle_irc = getpid ( );

	GApplication *app = global_app;

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
			memset ( body, 0, 1024 );
			snprintf ( body, 1023,
					"%s вошёл в комнату %s",
					nick,
					room
				 );
			g_notification_set_body ( notify, body );
			g_application_send_notification ( app, prog, notify );
		} else 
		if ( !strncmp ( s, msg, length_msg ) ) {
			s += length_msg + 1;
			copy_to_room ( room, &s );
			s++;
			copy_to_message ( message, &s );
			memset ( body, 0, 1024 );
			snprintf ( body, 1023,
					"%s: %s",
					nick,
					message );
			g_notification_set_body ( notify, body );
			g_application_send_notification ( app, prog, notify );
#ifdef AUDIO_NOTIFICATIONS
			gst_element_seek_simple ( play_message.pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, play_message.pos );
			gst_element_set_state ( play_message.pipeline, GST_STATE_PLAYING );
#endif
			check_body ( message );
		}
		memset ( rbuffer, 0, size );
		memset ( nick, 0, 255 );
		memset ( room, 0, 255 );
		memset ( message, 0, 1024 );
	}
}
void join_to_channel ( ) {
	memset ( sbuffer, 0, 1024 );
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
}

static void connect_to ( const char *site, unsigned short port ) {
	struct hostent *ht = gethostbyname ( site );
	if ( !ht ) {
		printf ( "oh no\n" );
		return;
	}
	struct sockaddr_in s;
	memset ( &s, 0, sizeof ( s ) );
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
	opt_client_id = calloc ( 255, 1 );
	opt_callback = calloc ( 255, 1 );
	opt_iface = calloc ( 255, 1 );

	body_help = calloc ( 4096, 1 );

	opt_new_message = calloc ( 255, 1 );
	opt_new_follower = calloc ( 255, 1 );
}
gchar *object_path_iface;

static void connect_for_net_device ( ) {
	GError *error = NULL;
	GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync (
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			"org.freedesktop.NetworkManager",
			"/org/freedesktop/NetworkManager",
			"org.freedesktop.NetworkManager",
			NULL,
			&error );

	if ( error ) {
		fprintf ( stderr, "%s %d: %s\n", __FILE__, __LINE__, error->message );
		g_error_free ( error );
		error = NULL;
		exit ( EXIT_FAILURE );
	}

	GVariant *var = g_dbus_proxy_call_sync (
			proxy,
			"GetDeviceByIpIface",
			g_variant_new ( "(s)", opt_iface ),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			&error );
	if ( error ) {
		fprintf ( stderr, "%s %d: %s\n", __FILE__, __LINE__, error->message );
		g_error_free ( error );
		error = NULL;
		exit ( EXIT_FAILURE );
	}
	if ( !var ) {
		fprintf ( stderr, "%s: такого сетевого интерфейса не существует.\n", opt_iface );
		exit ( EXIT_FAILURE );
	}

	object_path_iface = calloc ( 255, 1 );
	g_variant_get ( var, "(o)", &object_path_iface );
}


guint power_net;
guint trigger_net;

pthread_t main_handle;
pthread_t server_handle;

static void connect_to_network ( ) {
	sleep ( 3 );
	connect_to ( "irc.chat.twitch.tv", 6667 );
	join_to_channel ( );
	pthread_create ( &main_handle, NULL, handle, global_app );
	const char *body = "Соединение установлено";
	g_notification_set_body ( notify, body );
	g_application_send_notification ( global_app, prog, notify );
}

static void connection_close_all ( ) {
	pthread_cancel ( main_handle );
	const char *body = "Соединение разорвано";
	g_notification_set_body ( notify, body );
	g_application_send_notification ( global_app, prog, notify );
}

static void handle_net_state ( GDBusConnection *con,
		const gchar *sender_name,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *signal_name,
		GVariant *param,
		gpointer data
		) {
	guint32 num1, num2, num3;
	g_variant_get ( param, "(uuu)", &num1, &num2, &num3 );
	if ( num1 == 100 ) {
		power_net = 100;
		connect_to_network ( );
		trigger_net = 1;
	} else if ( num1 <= 30 ) {
		power_net = num1;
		if ( trigger_net ) {
			connection_close_all ( );
			trigger_net = 0;
		}
	}
}


static void set_signal_subscribe_to_net_status ( ) {
	GError *error = NULL;
	GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync (
			G_BUS_TYPE_SYSTEM,
			G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			"org.freedesktop.NetworkManager",
			object_path_iface,
			"org.freedesktop.NetworkManager.Device",
			NULL,
			&error );
	if ( error ) {
		fprintf ( stderr, "%s %d: %s\n", __FILE__, __LINE__, error->message );
		g_error_free ( error );
		error = NULL;
		exit ( EXIT_FAILURE );
	}

	GVariant *var = g_dbus_proxy_get_cached_property ( proxy, "State" );

	g_variant_get ( var, "u", &power_net );

	trigger_net = power_net == 100 ? 1 : 0;

	GDBusConnection *con = g_dbus_proxy_get_connection ( proxy );

	g_dbus_connection_signal_subscribe ( 
			con,
			"org.freedesktop.NetworkManager",
			"org.freedesktop.NetworkManager.Device",
			"StateChanged",
			object_path_iface,
			NULL,
			G_DBUS_SIGNAL_FLAGS_NONE,
			handle_net_state,
			NULL,
			NULL
			);
}
#ifdef AUDIO_NOTIFICATIONS
void on_pad_added ( GstElement *element, GstPad *pad, gpointer data ) {
	GstPad *sinkpad;
	GstElement *decoder = (GstElement *) data;

	sinkpad = gst_element_get_static_pad ( decoder, "sink" );

	gst_pad_link ( pad, sinkpad );

	gst_object_unref ( sinkpad );
}

void init_struct_play ( struct play_notification *pl, const char *opt_music ) {
	pl->pipeline = gst_pipeline_new ( "new_messages" );
	pl->source = gst_element_factory_make ( "filesrc", NULL );
	pl->demuxer = gst_element_factory_make ( "decodebin", NULL );
	pl->decoder = gst_element_factory_make ( "audioconvert", NULL );
	pl->volume = gst_element_factory_make ( "volume", NULL );
	pl->conv = gst_element_factory_make ( "audioconvert", NULL );
	pl->sink = gst_element_factory_make ( "autoaudiosink", NULL );

	if ( !pl->pipeline || !pl->source || !pl->demuxer || !pl->decoder || !pl->volume || !pl->conv || !pl->sink ) {
		fprintf ( stderr, "failed to init plugins\n" );
		exit ( EXIT_FAILURE );
	}

	gst_bin_add_many ( GST_BIN ( pl->pipeline ),
			pl->source,
			pl->demuxer,
			pl->decoder,
			pl->volume,
			pl->conv,
			pl->sink,
			NULL
			);
	gst_element_link ( pl->source, pl->demuxer );
	gst_element_link_many ( pl->decoder, pl->volume, pl->conv, pl->sink, NULL );
	g_signal_connect ( pl->demuxer, "pad-added", G_CALLBACK ( on_pad_added ), pl->decoder );

	g_object_set ( G_OBJECT ( pl->source ), "location", opt_music, NULL );

	pl->pos = 0;
}
void init_sounds ( ) {
	gst_init ( 0, 0 );

	init_struct_play ( &play_message, opt_new_message );
	init_struct_play ( &play_follower, opt_new_follower );

}
#endif

static void g_startup ( GApplication *app, gpointer data ) {
	if ( notify_frozen ) show_notify_frozen = G_NOTIFICATION_PRIORITY_URGENT;
	else show_notify_frozen = G_NOTIFICATION_PRIORITY_HIGH;

	notify = g_notification_new ( "twitch" );
	g_notification_set_priority ( notify, show_notify_frozen );

#ifdef AUDIO_NOTIFICATIONS
	init_sounds ( );
#endif
	connect_for_net_device ( );
	set_signal_subscribe_to_net_status ( );

	init_for_irc_net ( );

	if ( power_net == 100 ) {
		connect_to ( "irc.chat.twitch.tv", 6667 );

		join_to_channel ( );

		pthread_create ( &main_handle, NULL, handle, app );

	}


	GMainLoop *loop = g_main_loop_new ( NULL, FALSE );
	g_main_loop_run ( loop );
}


int main ( int argc, char **argv ) {
	init_opts ( );
	parser_config_init ( );

	signal ( SIGINT, sig_handle );



	buffers_init ( );

	daemon ( 1, 1 );

	int ret;


	GApplication *app;
	app = g_application_new ( prog, G_APPLICATION_FLAGS_NONE );
	g_application_register ( app, NULL, NULL );
	//const gchar *g_id = g_application_get_application_id ( app );
	global_app = app;
	g_signal_connect ( app, "activate", G_CALLBACK ( g_startup ), NULL );
	ret = g_application_run ( app, argc, argv );
	g_object_unref ( app );
	return ret;
}
