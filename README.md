# twitch-notificator 1.1
This program is the bot of notifications. It has show messages from twitch chat in the desktop notifications. It has support audio notification from file sound.

Write a message is '@nickname help' for seeing available commands. Nickname is the name of a streamer.

Depending:
* glib-2.0
* gio-2.0
* appindicator3
* gstreamer-1.0

In beginning, execute `./configure`.

When the bot had installed, start it. The bot will create a file of configuration. The file of configuration will be stored in '/home/user/.twitch-bot/conf'.

* token = for option 'token' get token from site 'https://twitchapps.com/tmi/' and copy it with the word 'oauth:'.
* nickname = your nickname of token.
* channel = your channel name.
* audacious = if set 'true', to can control the music player such as 'audacious'.
* interface = set the network interface for tracking a connection.
* new_messages = set path for a file mp3. When will enter a new message, it will play sound this the file.
* notify_frozen = by default the timer of notifications is temporary. For notifications is frozen, set `notify_frozen=true`.
* volume - set volume level from 0 to 100 in percent.

Control music player 'audacious.
* next - turn on the next track.
* prev - turn on the previous track.
* track - to known what track playing.
* start - seek to start position track.

![](http://s1.uploadpics.ru/images/bJg51xc7BL.png)
![](http://s1.uploadpics.ru/images/WJGoJlcXrI.png)
