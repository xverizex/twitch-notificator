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
#include <glib-2.0/gio/gio.h>
#include <json-c/json.h>

struct get_req {
	char *line;
	char **var;
	char **value;
	int max_count;
};
struct post_req {
	char *line;
	char *body;
	unsigned int length;
};
struct header {
	char **var;
	char **value;
	int max_count;
};

struct json {
	json_tokener *tok;
	json_object *root;
	json_object *data;
	json_object *array;
	json_object *from_name;
	json_object *to_name;
};
struct data {
	struct get_req get;
	struct post_req post;
	int type_of_request;
	struct header head;

	char *data_buffer;

	struct json json;
};

void handle_data ( const int sockclient, const char *buffer, GApplication *app, struct data *global_dt );
