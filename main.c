/*
 * twitch-bot - бот показывает сообщения от twitch в области уведомления
 *
 * Copyright (C) 2020 Naidolinsky Dmitry <naidv88@gmail.com>
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
#include <gtk/gtk.h>
#include <signal.h>
#include "config.h"
#include "parser.h"
#ifdef AUDIO_NOTIFICATIONS
#include <gst/gst.h>
#include "audio.h"
#endif


const char *prog = "com.xverizex.twitch-notificator";
const gchar *g_id;

pthread_t thread_p_pubsub;

GNotification *notify;
GtkApplication *global_app;

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
double opt_volume;

struct conf cfg;
int volume;

struct widgets {
	GtkWidget *window_main;
	GtkWidget *box_window_main;
	GtkWidget *frame_current_login;
	GtkWidget *box_current_login;
	GtkWidget *label_current_login;
	GtkWidget *entry_current_login;
	GtkWidget *frame_current_channel;
	GtkWidget *box_current_channel;
	GtkWidget *label_current_channel;
	GtkWidget *entry_current_channel;
	GtkWidget *frame_current_token;
	GtkWidget *box_current_token;
	GtkWidget *label_current_token;
	GtkWidget *button_link_current_token;
	GtkWidget *entry_current_token;
	GtkWidget *frame_current_new_message;
	GtkWidget *box_current_new_message;
	GtkWidget *label_current_new_message;
	GtkWidget *button_current_new_message;
	GtkWidget *entry_current_new_message;
	GtkWidget *frame_sub_net_device;
	GtkWidget *box_sub_net_device;
	GtkWidget *label_sub_net_device;
	GtkWidget *entry_sub_net_device;
	GtkWidget *check_sub_net_device;
	GtkWidget *frame_volume;
	GtkWidget *box_volume;
	GtkWidget *label_volume;
	GtkWidget *scale_volume;
	GtkWidget *box_control;
	GtkWidget *button_connect;
	GtkWidget *header_bar;
	GtkWidget *window_oauth;
	GtkWidget *web_view_oauth;

	GtkWidget *STUBS;
} w;

const char *styles = "window#light { background-color: #ececec; } treeview#light { background-color: #ffffff; color: #000000; } treeview#light:selected  { background: #8c8c8c; color: #ffffff; } treeview#light header button { background: #ffffff; color: #000000; } entry#light { background-color: #ffffff; } label#light { color: #000000; } frame#light { border-radius: 8px; background-color: #dcdcdc; } textview#light.view text { background-color: #ffffff; } frame#light border { border-radius: 8px; } button#light.text-button { background: #ffffff; } notebook#light tab { background-color: #ececec; color: #ffffff; } window#dark { background-color: #3c3c3c; } treeview#dark { background-color: #4c4c4c; color: #ffffff; } treeview#dark:selected { background-color: #1c1c1c; color: #ffffff; } entry#dark { background-color: #5c5c5c; color: #ffffff; } label#dark { color: #ffffff; } frame#dark { border-radius: 8px; background-color: #8c8c8c; } textview#dark.view text { background-color: #5c5c5c; color: #ffffff } frame#dark border { border-radius: 8px; } button#dark.text-button { background: #8c8c8c; } treeview#dark header button { background: #7c7c7c; color: #ffffff; } label#light_info { font-size: 12px; } label#dark_info { font-size: 12px; } label#standard_info { font-size: 12px; } notebook#dark tab { background-color: #4c4c4c; color: #ffffff; }";

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

int audacious;

gchar *player_next;
gchar *player_prev;
gchar *player_track;

pid_t pid_handle_irc;
pid_t pid_server_webhook;

gchar *body;

guint id_audacious;
GDBusConnection *con_audacious;

static void set_theme_name ( const char *name ) {
	struct widgets **p = ( struct widgets ** ) &w;
	for ( int i = 0; p[i] != NULL; i++ ) {
		gtk_widget_set_name ( ( GtkWidget * ) p[i], name );
	}
}

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
#if 0
	gint64 pos;
	g_variant_get ( param, "(x)", &pos );
	if ( !pos ) {
		GVariant *var = g_dbus_proxy_get_cached_property ( audacious_proxy, "Metadata" );
		GVariant *title = NULL;
		GVariant *album = NULL;
		GVariant *sartist = NULL;
		GVariantIter *iter = NULL;
		char *artist = calloc ( 512, 1 );
		char *art = NULL;
		if ( var ) {
			title = g_variant_lookup_value ( var, "xesam:title", NULL );
			album = g_variant_lookup_value ( var, "xesam:album", NULL );
			sartist = g_variant_lookup_value ( var, "xesam:artist", NULL );
			g_variant_get ( sartist, "as", &iter );
			while ( g_variant_iter_loop ( iter, "s", &art ) ) {
				snprintf ( artist, 512,
						"%s %s",
						artist,
						art
					 );
			}
			g_variant_iter_free ( iter );
		}
		gsize length;

		if ( title && album ) {
			gchar *message = g_strdup_printf ( "Сейчас играет: альбом: %s. Исполнитель: %s. песня: %s", 
					g_variant_get_string ( album, &length ), 
					artist,
					g_variant_get_string ( title, &length ) );
			gchar *body = g_strdup_printf ( "%s%s\n", line_for_message, message );
	
			write ( sockfd, body, strlen ( body ) );
			g_free ( body );
			g_free ( message );
		}
		if ( var ) g_variant_unref ( var );
		if ( title ) g_variant_unref ( title );
		if ( album ) g_variant_unref ( album );
		if ( sartist ) g_variant_unref ( sartist );
		if ( artist ) free ( artist );
	}
#endif
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

}

static void buffers_init ( ) {
	rbuffer = calloc ( size, 1 );
	sbuffer = calloc ( 1024, 1 );
	body = calloc ( 1024, 1 );
	nick = calloc ( 255, 1 );
	room = calloc ( 255, 1 );
	message = calloc ( 1024, 1 );
	player_next = g_strdup_printf ( "@%s next", cfg.nickname );
	player_prev = g_strdup_printf ( "@%s prev", cfg.nickname );
	player_track = g_strdup_printf ( "@%s track", cfg.nickname );
	opt_help = g_strdup_printf ( "@%s help", cfg.nickname );
	line_for_message = g_strdup_printf ( "PRIVMSG #%s :", cfg.channel );
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
	GVariant *sartist = NULL;
	GVariantIter *iter = NULL;
	char *artist = calloc ( 512, 1 );
	char *art = NULL;
	if ( var ) {
		title = g_variant_lookup_value ( var, "xesam:title", NULL );
		album = g_variant_lookup_value ( var, "xesam:album", NULL );
		sartist = g_variant_lookup_value ( var, "xesam:artist", NULL );
		g_variant_get ( sartist, "as", &iter );
		while ( g_variant_iter_loop ( iter, "s", &art ) ) {
			snprintf ( artist, 512,
					"%s %s",
					artist,
					art
				 );
		}
		g_variant_iter_free ( iter );
		
	}
	gsize length;

	if ( title && album ) {
		gchar *message = g_strdup_printf ( "альбом: %s. исполнитель: %s. песня: %s", 
				g_variant_get_string ( album, &length ), 
				artist,
				g_variant_get_string ( title, &length ) );
		gchar *body = g_strdup_printf ( "%s%s\n", line_for_message, message );

		write ( sockfd, body, strlen ( body ) );
		g_free ( body );
		g_free ( message );

		trigger_player = 1;
	}

	if ( album ) g_variant_unref ( album );
	if ( title ) g_variant_unref ( title );
	if ( sartist ) g_variant_unref ( sartist );
	if ( var ) g_variant_unref ( var );
	if ( artist ) free ( artist );

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
		char message[512];
		snprintf ( message, 512, "%s%s\n", line_for_message, "Ни один плеер не включен." );
		write ( sockfd, message, strlen ( message ) );
	}

	trigger_player_run = 0;
	trigger_player = 0;
}

int run_once;
static gboolean send_message ( gpointer body ) {
	g_notification_set_body ( notify, ( char * ) body );
	g_application_send_notification ( ( GApplication * ) global_app, prog, notify );
	return FALSE;
}
static gboolean sound_message ( gpointer data ) {
	gst_element_seek_simple ( play_message.pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, play_message.pos );
	gst_element_set_state ( play_message.pipeline, GST_STATE_PLAYING );
	return FALSE;
}

static void *handle ( void *data ) {
	pid_handle_irc = getpid ( );

	GtkApplication *app = global_app;

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
			printf ( "%s\n", body );
			g_idle_add ( send_message, body );
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
			printf ( "%s\n", body );
			g_idle_add ( send_message, body );
#ifdef AUDIO_NOTIFICATIONS
			g_idle_add ( sound_message, NULL );
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
	sprintf ( sbuffer, "PASS %s\n", cfg.token );
	int ret = write ( sockfd, sbuffer, strlen ( sbuffer ) );
	if ( ret == -1 ) {
		perror ( "write" );
	}
	memset ( sbuffer, 0, 1024 );
	sprintf ( sbuffer, "NICK %s\n", cfg.nickname );
	ret = write ( sockfd, sbuffer, strlen ( sbuffer ) );
	if ( ret == -1 ) {
		perror ( "write" );
	}
	memset ( sbuffer, 0, 1024 );
	sprintf ( sbuffer, "JOIN #%s\n", cfg.channel );
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
	body_help = calloc ( 4096, 1 );
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
			g_variant_new ( "(s)", cfg.interface ),
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
		fprintf ( stderr, "%s: такого сетевого интерфейса не существует.\n", cfg.interface );
		exit ( EXIT_FAILURE );
	}

	if ( !object_path_iface ) object_path_iface = calloc ( 255, 1 );
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
	g_application_send_notification ( ( GApplication * ) global_app, prog, notify );
}

static void connection_close_all ( ) {
	pthread_cancel ( main_handle );
	const char *body = "Соединение разорвано";
	g_notification_set_body ( notify, body );
	g_application_send_notification ( ( GApplication * ) global_app, prog, notify );
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

struct play_notification *gpl;

void init_struct_play ( struct play_notification *pl, const char *opt_music ) {
	if ( !gpl ) {
		gpl = pl;

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
	}

	g_object_set ( G_OBJECT ( pl->source ), "location", cfg.new_message, NULL );
	g_object_set ( G_OBJECT ( pl->volume ), "volume", cfg.volume, NULL );

	pl->pos = 0;
}
void init_sounds ( ) {
	gst_init ( 0, 0 );

	init_struct_play ( &play_message, cfg.new_message );
	//init_struct_play ( &play_follower, opt_new_follower );

}
#endif

static void button_connect_clicked_cb ( GtkButton *button, gpointer data ) {
	const char *oauth_token = gtk_entry_get_text ( ( GtkEntry * ) w.entry_current_token );
	const char *login = gtk_entry_get_text ( ( GtkEntry * ) w.entry_current_login );
	const char *channel = gtk_entry_get_text ( ( GtkEntry * ) w.entry_current_channel );
	const char *new_message = gtk_entry_get_text ( ( GtkEntry * ) w.entry_current_new_message );
	const char *sub_net_device = gtk_entry_get_text ( ( GtkEntry * ) w.entry_sub_net_device );
	cfg.check_net_device = gtk_toggle_button_get_active ( ( GtkToggleButton * ) w.check_sub_net_device ) == TRUE ? 1 : 0;
	cfg.volume = gtk_range_get_value ( ( GtkRange * ) w.scale_volume );
	strncpy ( cfg.nickname, login, strlen ( login ) + 1 );
	strncpy ( cfg.channel, channel, strlen ( channel ) + 1 );
	strncpy ( cfg.token, oauth_token, strlen ( oauth_token ) + 1 );
	strncpy ( cfg.new_message, new_message, strlen ( new_message ) + 1 );
	strncpy ( cfg.interface, sub_net_device, strlen ( sub_net_device ) + 1 );
	config_write ( );

	if ( cfg.check_net_device ) {
		connect_for_net_device ( );
		set_signal_subscribe_to_net_status ( );
	}

	//init_for_irc_net ( );

	if ( strlen ( new_message ) ) { init_sounds ( ); volume = 1; }

#if 1
	if ( power_net == 100 ) {
		connect_to ( "irc.chat.twitch.tv", 6667 );

		join_to_channel ( );

		pthread_create ( &main_handle, NULL, handle, global_app );
	}
#endif
}

static void action_activate_select_light_theme ( GSimpleAction *simple, GVariant *parameter, gpointer data ) {
	set_theme_name ( "light" );
}
static void action_activate_select_dark_theme ( GSimpleAction *simple, GVariant *parameter, gpointer data ) {
	set_theme_name ( "dark" );
}
static void action_activate_select_quit ( GSimpleAction *simple, GVariant *parameter, gpointer data ) {
	exit ( EXIT_SUCCESS );
}

static void create_actions ( ) {
	const GActionEntry entries[] = {
		{ "select_light_theme", action_activate_select_light_theme },
		{ "select_dark_theme", action_activate_select_dark_theme },
		{ "select_quit", action_activate_select_quit }
	};

	g_action_map_add_action_entries ( G_ACTION_MAP ( global_app ), entries, G_N_ELEMENTS ( entries ), NULL );
}

static void scale_volume_value_changed_cb ( GtkRange *range, gpointer data ) {
	cfg.volume = gtk_range_get_value ( range );
	if ( volume ) g_object_set ( G_OBJECT ( gpl->volume ), "volume", cfg.volume, NULL );
	config_write ( );
}

static void check_net_device_toggled_cb ( GtkToggleButton *button, gpointer data ) {
	cfg.check_net_device = gtk_toggle_button_get_active ( button ) == TRUE ? 1 : 0;
	config_write ( );
}

static void g_startup ( GtkApplication *app, gpointer data ) {
	w.window_main = gtk_application_window_new ( app );
	gtk_window_set_default_size ( ( GtkWindow * ) w.window_main, 500, 600 );
	w.box_window_main = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );

	create_actions ( );

	GMenu *menu_app = g_menu_new ( );
	GMenu *menu_styles = g_menu_new ( );
	g_menu_append ( menu_styles, "Светлая тема", "app.select_light_theme" );
	g_menu_append ( menu_styles, "Тёмная тема", "app.select_dark_theme" );
	g_menu_append_submenu ( menu_app, "Выбрать тему", ( GMenuModel * ) menu_styles );
	g_menu_append ( menu_app, "Выход", "app.select_quit" );

	gtk_application_set_app_menu ( app, ( GMenuModel * ) menu_app );

	GtkStyleContext *context = gtk_style_context_new ( );
        GdkScreen *screen = gtk_style_context_get_screen ( context );
        GtkCssProvider *provider = gtk_css_provider_new ( );
        gtk_css_provider_load_from_data ( provider, styles, strlen ( styles ), NULL );
        gtk_style_context_add_provider_for_screen ( screen, ( GtkStyleProvider * ) provider, GTK_STYLE_PROVIDER_PRIORITY_USER );

	/* создать блок инфо - текущий пользователь */
	w.frame_current_login = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	gtk_frame_set_shadow_type ( ( GtkFrame * ) w.frame_current_login, GTK_SHADOW_OUT );
	w.box_current_login = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	w.label_current_login = gtk_label_new ( "login" );
	w.entry_current_login = gtk_entry_new ( );
	gtk_entry_set_alignment ( ( GtkEntry * ) w.entry_current_login, 1 );
	gtk_entry_set_width_chars ( ( GtkEntry * ) w.entry_current_login, 20 );
	gtk_box_pack_start ( ( GtkBox * ) w.box_current_login, w.label_current_login, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_current_login, w.entry_current_login, FALSE, FALSE, 0 );
	gtk_widget_set_margin_top ( w.label_current_login, 10 );
	gtk_widget_set_margin_bottom ( w.label_current_login, 10 );
	gtk_widget_set_margin_start ( w.label_current_login, 10 );
	gtk_widget_set_margin_end ( w.label_current_login, 10 );
	gtk_widget_set_margin_top ( w.entry_current_login, 10 );
	gtk_widget_set_margin_bottom ( w.entry_current_login, 10 );
	gtk_widget_set_margin_start ( w.entry_current_login, 10 );
	gtk_widget_set_margin_end ( w.entry_current_login, 10 );
	gtk_widget_set_margin_top ( w.frame_current_login, 32 );
	gtk_widget_set_margin_start ( w.frame_current_login, 32 );
	gtk_widget_set_margin_end ( w.frame_current_login, 32 );
	gtk_container_add ( ( GtkContainer * ) w.frame_current_login, w.box_current_login );

	/* создать блок инфо - текущий канал */
	w.frame_current_channel = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	gtk_frame_set_shadow_type ( ( GtkFrame * ) w.frame_current_channel, GTK_SHADOW_OUT );
	w.box_current_channel = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	w.label_current_channel = gtk_label_new ( "channel" );
	w.entry_current_channel = gtk_entry_new ( );
	gtk_entry_set_alignment ( ( GtkEntry * ) w.entry_current_channel, 1 );
	gtk_entry_set_width_chars ( ( GtkEntry * ) w.entry_current_channel, 20 );
	gtk_box_pack_start ( ( GtkBox * ) w.box_current_channel, w.label_current_channel, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_current_channel, w.entry_current_channel, FALSE, FALSE, 0 );
	gtk_widget_set_margin_top ( w.label_current_channel, 10 );
	gtk_widget_set_margin_bottom ( w.label_current_channel, 10 );
	gtk_widget_set_margin_start ( w.label_current_channel, 10 );
	gtk_widget_set_margin_end ( w.label_current_channel, 10 );
	gtk_widget_set_margin_top ( w.entry_current_channel, 10 );
	gtk_widget_set_margin_bottom ( w.entry_current_channel, 10 );
	gtk_widget_set_margin_start ( w.entry_current_channel, 10 );
	gtk_widget_set_margin_end ( w.entry_current_channel, 10 );
	gtk_widget_set_margin_top ( w.frame_current_channel, 10 );
	gtk_widget_set_margin_start ( w.frame_current_channel, 32 );
	gtk_widget_set_margin_end ( w.frame_current_channel, 32 );
	gtk_container_add ( ( GtkContainer * ) w.frame_current_channel, w.box_current_channel );

	/* создать блок инфо - текущий токен */
	w.frame_current_token = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	gtk_frame_set_shadow_type ( ( GtkFrame * ) w.frame_current_token, GTK_SHADOW_OUT );
	w.box_current_token = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	w.label_current_token = gtk_label_new ( "token" );
	w.entry_current_token = gtk_entry_new ( );
	gtk_entry_set_alignment ( ( GtkEntry * ) w.entry_current_token, 1 );
	gtk_entry_set_width_chars ( ( GtkEntry * ) w.entry_current_token, 20 );
	gtk_entry_set_invisible_char ( ( GtkEntry * ) w.entry_current_token, '*' );
	gtk_entry_set_visibility ( ( GtkEntry * ) w.entry_current_token, FALSE );
	w.button_link_current_token = gtk_link_button_new_with_label ( "https://twitchapps.com/tmi/", "get token" );
	gtk_box_pack_start ( ( GtkBox * ) w.box_current_token, w.label_current_token, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_current_token, w.entry_current_token, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_current_token, w.button_link_current_token, FALSE, FALSE, 0 );
	gtk_widget_set_margin_top ( w.label_current_token, 10 );
	gtk_widget_set_margin_bottom ( w.label_current_token, 10 );
	gtk_widget_set_margin_start ( w.label_current_token, 10 );
	gtk_widget_set_margin_end ( w.label_current_token, 10 );
	gtk_widget_set_margin_top ( w.button_link_current_token, 10 );
	gtk_widget_set_margin_bottom ( w.button_link_current_token, 10 );
	gtk_widget_set_margin_end ( w.button_link_current_token, 10 );
	gtk_widget_set_margin_top ( w.entry_current_token, 10 );
	gtk_widget_set_margin_bottom ( w.entry_current_token, 10 );
	gtk_widget_set_margin_start ( w.entry_current_token, 10 );
	gtk_widget_set_margin_end ( w.entry_current_token, 10 );
	gtk_widget_set_margin_top ( w.frame_current_token, 10 );
	gtk_widget_set_margin_start ( w.frame_current_token, 32 );
	gtk_widget_set_margin_end ( w.frame_current_token, 32 );
	gtk_container_add ( ( GtkContainer * ) w.frame_current_token, w.box_current_token );
	
	/* создать блок - текущий звук при новом входящем сообщении */
	w.frame_current_new_message = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	gtk_frame_set_shadow_type ( ( GtkFrame * ) w.frame_current_new_message, GTK_SHADOW_OUT );
	w.box_current_new_message = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	w.button_current_new_message = gtk_button_new_with_label ( "add" );
	w.label_current_new_message = gtk_label_new ( "sound of new message" );
	w.entry_current_new_message = gtk_entry_new ( );
	gtk_entry_set_alignment ( ( GtkEntry * ) w.entry_current_new_message, 1 );
	gtk_entry_set_width_chars ( ( GtkEntry * ) w.entry_current_new_message, 20 );
	gtk_box_pack_start ( ( GtkBox * ) w.box_current_new_message, w.label_current_new_message, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_current_new_message, w.entry_current_new_message, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_current_new_message, w.button_current_new_message, FALSE, FALSE, 0 );
	gtk_widget_set_margin_top ( w.label_current_new_message, 10 );
	gtk_widget_set_margin_bottom ( w.label_current_new_message, 10 );
	gtk_widget_set_margin_start ( w.label_current_new_message, 10 );
	gtk_widget_set_margin_end ( w.label_current_new_message, 10 );
	gtk_widget_set_margin_top ( w.button_current_new_message, 10 );
	gtk_widget_set_margin_bottom ( w.button_current_new_message, 10 );
	gtk_widget_set_margin_start ( w.button_current_new_message, 10 );
	gtk_widget_set_margin_end ( w.button_current_new_message, 10 );
	gtk_widget_set_margin_top ( w.entry_current_new_message, 10 );
	gtk_widget_set_margin_bottom ( w.entry_current_new_message, 10 );
	gtk_widget_set_margin_start ( w.entry_current_new_message, 10 );
	gtk_widget_set_margin_end ( w.entry_current_new_message, 10 );
	gtk_widget_set_margin_top ( w.frame_current_new_message, 10 );
	gtk_widget_set_margin_start ( w.frame_current_new_message, 32 );
	gtk_widget_set_margin_end ( w.frame_current_new_message, 32 );
	gtk_container_add ( ( GtkContainer * ) w.frame_current_new_message, w.box_current_new_message );

	/* создать блок - подписка на сетевое устройство */
	w.frame_sub_net_device = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	gtk_frame_set_shadow_type ( ( GtkFrame * ) w.frame_sub_net_device, GTK_SHADOW_OUT );
	w.box_sub_net_device = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	w.entry_sub_net_device = gtk_entry_new ( );
	gtk_entry_set_alignment ( ( GtkEntry * ) w.entry_sub_net_device, 1 );
	gtk_entry_set_width_chars ( ( GtkEntry * ) w.entry_sub_net_device, 20 );
	w.label_sub_net_device = gtk_label_new ( "subscribe on net device" );
	w.check_sub_net_device = gtk_check_button_new ( );
	gtk_box_pack_start ( ( GtkBox * ) w.box_sub_net_device, w.label_sub_net_device, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_sub_net_device, w.entry_sub_net_device, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_sub_net_device, w.check_sub_net_device, FALSE, FALSE, 0 );
	gtk_widget_set_margin_top ( w.label_sub_net_device, 10 );
	gtk_widget_set_margin_bottom ( w.label_sub_net_device, 10 );
	gtk_widget_set_margin_start ( w.label_sub_net_device, 10 );
	gtk_widget_set_margin_end ( w.label_sub_net_device, 10 );
	gtk_widget_set_margin_top ( w.check_sub_net_device, 10 );
	gtk_widget_set_margin_bottom ( w.check_sub_net_device, 10 );
	gtk_widget_set_margin_start ( w.check_sub_net_device, 10 );
	gtk_widget_set_margin_end ( w.check_sub_net_device, 10 );
	gtk_widget_set_margin_top ( w.entry_sub_net_device, 10 );
	gtk_widget_set_margin_bottom ( w.entry_sub_net_device, 10 );
	gtk_widget_set_margin_start ( w.entry_sub_net_device, 10 );
	gtk_widget_set_margin_end ( w.entry_sub_net_device, 10 );
	gtk_widget_set_margin_top ( w.frame_sub_net_device, 10 );
	gtk_widget_set_margin_start ( w.frame_sub_net_device, 32 );
	gtk_widget_set_margin_end ( w.frame_sub_net_device, 32 );
	gtk_container_add ( ( GtkContainer * ) w.frame_sub_net_device, w.box_sub_net_device );

	/* создать блок - уровень звука */
	w.frame_volume = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	gtk_frame_set_shadow_type ( ( GtkFrame * ) w.frame_volume, GTK_SHADOW_OUT );
	w.box_volume = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	w.label_volume = gtk_label_new ( "volume of sound new message" );
	GtkAdjustment *adj = gtk_adjustment_new ( 0.0, 0.0, 1.0, 1.0, 1.0, 0.0 );
	w.scale_volume = gtk_scale_new ( GTK_ORIENTATION_HORIZONTAL, adj );
	gtk_scale_set_draw_value ( ( GtkScale * ) w.scale_volume, FALSE );
	gtk_box_pack_start ( ( GtkBox * ) w.box_volume, w.label_volume, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_volume, w.scale_volume, TRUE, TRUE, 0 );
	gtk_widget_set_margin_top ( w.label_volume, 10 );
	gtk_widget_set_margin_bottom ( w.label_volume, 10 );
	gtk_widget_set_margin_start ( w.label_volume, 10 );
	gtk_widget_set_margin_end ( w.label_volume, 10 );
	gtk_widget_set_margin_top ( w.scale_volume, 10 );
	gtk_widget_set_margin_bottom ( w.scale_volume, 10 );
	gtk_widget_set_margin_start ( w.scale_volume, 10 );
	gtk_widget_set_margin_end ( w.scale_volume, 10 );
	gtk_widget_set_margin_top ( w.frame_volume, 10 );
	gtk_widget_set_margin_start ( w.frame_volume, 32 );
	gtk_widget_set_margin_end ( w.frame_volume, 32 );
	gtk_container_add ( ( GtkContainer * ) w.frame_volume, w.box_volume );

	/* создать блок кнопок */
	w.box_control = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	w.button_connect = gtk_button_new_with_label ( "ПОДКЛЮЧИТЬСЯ" );
	gtk_widget_set_margin_top ( w.button_connect, 10 );
	gtk_widget_set_margin_start ( w.button_connect, 10 );
	gtk_widget_set_margin_end ( w.button_connect, 10 );
	gtk_widget_set_margin_bottom ( w.button_connect, 10 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_control, w.button_connect, FALSE, FALSE, 0 );

	gtk_box_pack_start ( ( GtkBox * ) w.box_window_main, w.frame_current_login, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) w.box_window_main, w.frame_current_channel, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) w.box_window_main, w.frame_current_token, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) w.box_window_main, w.frame_current_new_message, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) w.box_window_main, w.frame_sub_net_device, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) w.box_window_main, w.frame_volume, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) w.box_window_main, w.box_control, FALSE, FALSE, 0 );

	w.header_bar = gtk_header_bar_new ( );
	gtk_header_bar_set_title ( ( GtkHeaderBar * ) w.header_bar, "twitch bot" );
	gtk_header_bar_set_show_close_button ( ( GtkHeaderBar * ) w.header_bar, TRUE );
	gtk_header_bar_set_decoration_layout ( ( GtkHeaderBar * ) w.header_bar, ":menu,minimize,close" );
	gtk_window_set_titlebar ( ( GtkWindow * ) w.window_main, w.header_bar );

	gtk_container_add ( ( GtkContainer * ) w.window_main, w.box_window_main );
	set_theme_name ( "light" );

	gtk_entry_set_text ( ( GtkEntry * ) w.entry_current_login, cfg.nickname );
	gtk_entry_set_text ( ( GtkEntry * ) w.entry_current_channel, cfg.channel );
	gtk_entry_set_text ( ( GtkEntry * ) w.entry_current_token, cfg.token );
	gtk_entry_set_text ( ( GtkEntry * ) w.entry_current_new_message, cfg.new_message );
	gtk_entry_set_text ( ( GtkEntry * ) w.entry_sub_net_device, cfg.interface );
	gtk_range_set_value ( ( GtkRange * ) w.scale_volume, cfg.volume );

	if ( cfg.check_net_device ) {
		gtk_toggle_button_set_active ( ( GtkToggleButton * ) w.check_sub_net_device, TRUE );
	}

	gtk_widget_show_all ( w.window_main );

	g_signal_connect ( w.button_connect, "clicked", G_CALLBACK ( button_connect_clicked_cb ), NULL );
	g_signal_connect ( w.scale_volume, "value-changed", G_CALLBACK ( scale_volume_value_changed_cb ), NULL );
	g_signal_connect ( w.check_sub_net_device, "toggled", G_CALLBACK ( check_net_device_toggled_cb ), NULL );

	notify = g_notification_new ( "twitch_bot" );
	g_notification_set_priority ( notify, G_NOTIFICATION_PRIORITY_HIGH );

}


int main ( int argc, char **argv ) {
	init_opts ( );
	parser_config_init ( );

	signal ( SIGINT, sig_handle );

	buffers_init ( );

	GtkApplication *app;
	app = gtk_application_new ( prog, G_APPLICATION_FLAGS_NONE );
	g_application_register ( ( GApplication * ) app, NULL, NULL );
	global_app = app;
	g_signal_connect ( app, "activate", G_CALLBACK ( g_startup ), NULL );
	return g_application_run ( ( GApplication * ) app, argc, argv );
}
