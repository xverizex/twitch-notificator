all:
	gcc  main.c parser.c subscribe.c server.c webhook.c -pthread -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/json-c -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lssl -ljson-c -pthread -o twitch-bot
install:
	install twitch-bot /usr/local/bin
clean:
	rm twitch-bot config.h
uninstall:
	rm /usr/local/bin/twitch-bot
