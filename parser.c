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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

char *home;
char *path_of_config;
const char *dir_config = ".twitch-bot";
const char *file_config = "conf";
const char *content = 
"token=\n"
"channel=\n"
"nickname=\n"
"audacious=false\n"
"client-id=\n"
"port-event=8000\n"
"callback=\n"
"user_uid=1000\n"
"interface=\n"
"new_message=\n"
"new_follower=\n"
"notify_frozen=\n"
"volume=50\n"
;

const char *str_token = "token=";
const char *str_channel = "channel=";
const char *str_nickname = "nickname=";
const char *str_audacious = "audacious=";
const char *str_client_id = "client-id=";
const char *str_port_event = "port-event=";
const char *str_callback = "callback=";
const char *str_uid = "user-uid=";
const char *str_iface = "interface=";
const char *str_new_message = "new_message=";
const char *str_new_follower = "new_follower=";
const char *str_notify_frozen = "notify_frozen=";
const char *str_volume = "volume=";

extern char *opt_oauth;
extern char *opt_channel;
extern char *opt_nickname;
extern char *opt_client_id;
extern char *opt_callback;
extern char *opt_iface;
extern char *opt_new_message;
extern char *opt_new_follower;
extern int n_client_id;
extern unsigned short port_event;
extern int notify_frozen;
extern int audacious;
extern int uid;
extern double opt_volume;
char *opt_audacious;
char *opt_notify_frozen;

static void create_config_file ( const char *file ) {
	FILE *fp = fopen ( file, "w" );
	fprintf ( fp, content, strlen ( content ) );
	fclose ( fp );
}

static void parse_token ( const char *s ) {
	s += strlen ( str_token );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_oauth[i] = *s++;
	}
}
static void parse_channel ( const char *s ) {
	s += strlen ( str_channel );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_channel[i] = *s++;
	}
}
static void parse_nickname ( const char *s ) {
	s += strlen ( str_nickname );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_nickname[i] = *s++;
	}
}
static void parse_interface ( const char *s ) {
	s += strlen ( str_iface );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_iface[i] = *s++;
	}
}
static void parse_callback ( const char *s ) {
	s += strlen ( str_callback );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_callback[i] = *s++;
	}
}
static void parse_new_message ( const char *s ) {
	s += strlen ( str_new_message );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_new_message[i] = *s++;
	}
}
static void parse_new_follower ( const char *s ) {
	s += strlen ( str_new_follower );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_new_follower[i] = *s++;
	}
}
static void parse_client_id ( const char *s ) {
	s += strlen ( str_client_id );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_client_id[i] = *s++;
	}
	n_client_id = 1;
}
static void parse_port_event ( const char *s ) {
	const char *must_be = s;
	s += strlen ( str_port_event );
	const char *port = s;
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++, s++ ) {
		if ( *s >= '0' && *s <= '9' ) continue;
		fprintf ( stderr, "It must be number %s\n", must_be );
		exit ( EXIT_FAILURE );
	}
	port_event = atoi ( port );
}
static void parse_uid ( const char *s ) {
	const char *must_be = s;
	s += strlen ( str_uid );
	const char *id = s;
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++, s++ ) {
		if ( *s >= '0' && *s <= '9' ) continue;
		fprintf ( stderr, "It must be number %s\n", must_be );
		exit ( EXIT_FAILURE );
	}
	uid = atoi ( id );
}

static void parse_volume ( const char *s ) {
	const char *must_be = s;
	s += strlen ( str_uid );
	const char *id = s;
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++, s++ ) {
		if ( *s >= '0' && *s <= '9' ) continue;
		fprintf ( stderr, "It must be number %s\n", must_be );
		exit ( EXIT_FAILURE );
	}
	int vol = atoi ( id );
	opt_volume = (double) ( (double) vol / 100.0 );
}

static void parse_audacious ( const char *s ) {
	s += strlen ( str_audacious );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_audacious[i] = *s++;
	}
	if ( !strncmp ( opt_audacious, "true", 5 ) ) { audacious = 1; return; }
	if ( !strncmp ( opt_audacious, "false", 6 ) ) { audacious = 0; return; }
}
static void parse_notify_frozen ( const char *s ) {
	s += strlen ( str_notify_frozen );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_notify_frozen[i] = *s++;
	}
	if ( !strncmp ( opt_notify_frozen, "true", 5 ) ) { notify_frozen = 1; return; }
	if ( !strncmp ( opt_notify_frozen, "false", 6 ) ) { notify_frozen = 0; return; }
}

static void parse_file ( const char *file ) {
	FILE *fp = fopen ( file, "r" );
	char *line = calloc ( 255, 1 );
	opt_audacious = calloc ( 255, 1 );
	opt_notify_frozen = calloc ( 255, 1 );
	char *s;

	while ( s = fgets ( line, 254, fp ) ) {
		if ( !strncmp ( str_token, s, strlen ( str_token ) ) ) { parse_token ( s ); continue; }
		if ( !strncmp ( str_channel, s, strlen ( str_channel ) ) ) { parse_channel ( s ); continue; }
		if ( !strncmp ( str_nickname, s, strlen ( str_nickname ) ) ) { parse_nickname ( s ); continue; }
		if ( !strncmp ( str_audacious, s, strlen ( str_audacious ) ) ) { parse_audacious ( s ); continue; }
		if ( !strncmp ( str_client_id, s, strlen ( str_client_id ) ) ) { parse_client_id ( s ); continue; }
		if ( !strncmp ( str_port_event, s, strlen ( str_port_event ) ) )  { parse_port_event ( s ); continue; }
		if ( !strncmp ( str_callback, s, strlen ( str_callback ) ) ) { parse_callback ( s ); continue; }
		if ( !strncmp ( str_uid, s, strlen ( str_uid ) ) ) { parse_uid ( s ); continue; }
		if ( !strncmp ( str_iface, s, strlen ( str_iface ) ) ) { parse_interface ( s ); continue; }
		if ( !strncmp ( str_new_message, s, strlen ( str_new_message ) ) ) { parse_new_message ( s ); continue; }
		if ( !strncmp ( str_new_follower, s, strlen ( str_new_follower ) ) ) { parse_new_follower ( s ); continue; }
		if ( !strncmp ( str_notify_frozen, s, strlen ( str_notify_frozen ) ) ) { parse_notify_frozen ( s ); continue; }
		if ( !strncmp ( str_volume, s, strlen ( str_volume ) ) ) { parse_volume ( s ); continue; }
	}
}

void parser_config_init ( ) {

	int new_file = 0;
	home = getenv ( "HOME" );
	path_of_config = calloc ( 256, 1 );

	snprintf ( path_of_config, 255, "%s/%s", home, dir_config );

	if ( !access ( path_of_config, F_OK ) ) {
		struct stat st;
		stat ( path_of_config, &st );
		if ( st.st_mode & S_IFMT != S_IFDIR ) {
			fprintf ( stderr, "%s it must be the directory.\n", path_of_config );
			exit ( EXIT_FAILURE );
		}
	} else {
		mkdir ( path_of_config, 0700 );
	}

	snprintf ( path_of_config, 255, "%s/%s/%s", home, dir_config, file_config );

	if ( !access ( path_of_config, F_OK ) ) {
		struct stat st;
		stat ( path_of_config, &st );
		if ( st.st_mode & S_IFMT == S_IFDIR ) {
			fprintf ( stderr, "%s it must be the file.\n", path_of_config );
			exit ( EXIT_FAILURE );
		}
	} else {
		create_config_file ( path_of_config );
		fprintf ( stderr, "Now fill in the file %s.\n", path_of_config );
		exit ( EXIT_SUCCESS );
	}

	parse_file ( path_of_config );
}
