Writing an IRC bot in a language is my way of feeling comfortable with
that language. Particularly complete IRC bots involve a few critical
data structures, and flex a language's networking and string processing
code. As a litmus test, they are more complete than Fibonacci sequence
generators and 99 bottles of beer programs, and give a good feel for
how a language looks and feels in a "real world" scenario.

However, the goal here is not to show off the bots I have written. The
goal is simply to collect IRC bots. Feel free to add yours, or to
contribute to existing bots. Existing projects like Eggdrop and rbot
will not be considered for inclusion.

For IRC-related functionality a bot is LIMITED to only the features it
needs to get a socket connection to an IRC server. Importing an IRC client
library is cheating. A bot is REQUIRED to allow the user to specify a
host and port to connect to, to use some sort of event-based polling to
receive data from the server (repeated calls to recv() on a non-blocking
socket so that the process uses 100% CPU is poor design and cheating),
and to respond to PINGs from the server. Ideally, the bot should also
be able to join a user-specified channel, and offer one or two basic
commands. Robust bots should use a user-specified nickname, have some
notion of bot administrators, be fault tolerant (if a nickname is in use,
etc), handle commands in private message and in a channel appropriately,
track their channel list and the channel list and status of visible
users, etc. The number of features that can be added to an IRC bot is
very large. However, for inclusion, the only requirement is that the bot

The current collection of bots is as follows:

 -  Linux i386 assembly, by Alex Iadicicco (unfinished)
 -  Python, by Alex Iadicicco
 -  Lua 5.1 with LuaSocket extension, by Alex Iadicicco
