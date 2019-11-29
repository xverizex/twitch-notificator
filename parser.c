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
;
const char *str_token = "token=";
const char *str_channel = "channel=";
const char *str_nickname = "nickname=";
const char *str_audacious = "audacious=";

extern char *opt_oauth;
extern char *opt_channel;
extern char *opt_nickname;
extern int audacious;
char *opt_audacious;

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
static void parse_audacious ( const char *s ) {
	s += strlen ( str_audacious );
	for ( int i = 0; *s != 0x0 && *s != '\n' && i < 255 && *s != -1; i++ ) {
		opt_audacious[i] = *s++;
	}
	if ( !strncmp ( opt_audacious, "true", 5 ) ) { audacious = 1; return; }
	if ( !strncmp ( opt_audacious, "false", 6 ) ) { audacious = 0; return; }
}

static void parse_file ( const char *file ) {
	FILE *fp = fopen ( file, "r" );
	char *line = calloc ( 255, 1 );
	opt_audacious = calloc ( 255, 1 );
	char *s;

	while ( s = fgets ( line, 254, fp ) ) {
		if ( !strncmp ( str_token, s, strlen ( str_token ) ) ) { parse_token ( s ); continue; }
		if ( !strncmp ( str_channel, s, strlen ( str_channel ) ) ) { parse_channel ( s ); continue; }
		if ( !strncmp ( str_nickname, s, strlen ( str_nickname ) ) ) { parse_nickname ( s ); continue; }
		if ( !strncmp ( str_audacious, s, strlen ( str_audacious ) ) ) { parse_audacious ( s ); continue; }
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
