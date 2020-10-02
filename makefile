all:
	gcc  main.c parser.c -pthread -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -lgio-2.0 -lgobject-2.0 -lglib-2.0 -pthread -o twitch-bot
install:
	install twitch-bot /usr/local/bin
	mkdir -p /usr/local/share/applications/
	mkdir -p /usr/local/share/pixmaps/
	cp com.xverizex.twitch-notificator.desktop /usr/local/share/applications/
	cp twitch-notificator.png /usr/local/share/pixmaps/
clean:
	rm twitch-bot config.h makefile
uninstall:
	rm /usr/local/bin/twitch-bot
