# rtstreamplayer
A stream player that tries to keep the delay with the source minimum, regardless of network hiccups

# Features

* **Plays an arbitrary audio WAV** (RIFF) stream provided by ./source.sh. This comes tipically from a `curl | mp3dec` or the like.
* **Guarantees a maximum delay**, thus keeping the delay between the min_buffer and max_buffer settings. 
* **Backup stream**. When the system is in buffer underrun for a certain amount of time (by default 5 seconds), a backup playing will be started, and then stopped when the system goes back to normality.
* Accessible via **Telegram bot**. https://telegram.org/blog/bot-revolution
* **MQTT** (Mosquitto) is used for the connection between the core and the tg bot, so that they are two independent processes, and the system is more modular and more reliable.

# Telegram bot service

* /start - Start command
* /help - Show this help
* /ip - Show server's IP
* /bot_version - Show this software's version
* /quitbot - Stop the Telegram bot
* /params - Get parameters
* /subscribe - Subscribe to events
* /unsubscribe - Subscribe to events

* /mute - Disable output
* /unmute - Reenable output
* /player_version - Show timestamp of core's binary
* /quit - Exit the core
* /reopen - Close the input stream and reopen it
* /status - Show internal status (statistics, uptime etc) 
* /temp - Show system's CPU temperature
* /uptime - Show system uptime

# Mosquitto service

`rtstreamplayer-main` listens for commands in *rtsp/command/+*.  The last element in the topic is used as ID for replies: *rtsp/response/<ID>*.

# Compile 

It is a CMake project using a couple of libraries: 

  * libboost
  * sndfile
  ...
  
# INSTALL

1. Create a ./source.sh file that outputs RIFF data.
1. You most certainly want to run it with `daemon`, a tool that relaunches your programs when they fail.

# Author

Written by Raúl Salinas-Monteagudo <rausalinas@gmail.com> for Ràdio Malva (http://radiomalva.org).
