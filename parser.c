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
#include "parser.h"

char *home;
char *path_of_config;
const char *dir_config = ".twitch-bot";
const char *file_config = "conf";

extern struct conf cfg;

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
		FILE *fp = fopen ( path_of_config, "w" );
		fwrite ( &cfg, sizeof ( struct conf ), 1, fp );
		fclose ( fp );
		return;
	}

	unsigned long size = sizeof ( struct conf );

	struct stat st;
	stat ( path_of_config, &st );
	if ( st.st_size != size ) {
		FILE *fp = fopen ( path_of_config, "w" );
		fwrite ( &cfg, sizeof ( struct conf ), 1, fp );
		fclose ( fp );
	} else {
		FILE *fp = fopen ( path_of_config, "r" );
		fread ( &cfg, sizeof ( struct conf ), 1, fp );
		fclose ( fp );
	}
}

void config_write ( ) {
		FILE *fp = fopen ( path_of_config, "w" );
		fwrite ( &cfg, sizeof ( struct conf ), 1, fp );
		fclose ( fp );
}
