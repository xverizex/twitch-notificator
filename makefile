LIBS=`pkg-config --libs glib-2.0,gio-2.0 --cflags glib-2.0,gio-2.0`
all:
	gcc main.c parser.c $(LIBS) -pthread -o twitch-bot
install:
	install twitch-bot /usr/local/bin
clean:
	rm twitch-bot
