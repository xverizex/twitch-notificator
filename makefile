LIBS=`pkg-config --libs gtk+-3.0,libnotify --cflags gtk+-3.0,libnotify`
all:
	gcc -g main.c $(LIBS) -pthread -o twitch-bot
install:
	install twitch-bot /usr/local/bin
