all:
	gcc  main.c parser.c  -pthread -I/usr/include/gstreamer-1.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -lgio-2.0 -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -pthread -o twitch-bot
install:
	install twitch-bot /usr/local/bin
clean:
	rm twitch-bot config.h
uninstall:
	rm /usr/local/bin/twitch-bot
