This bot will only run properly on x86 Linux systems. It may work on
other architectures, but the syscall convention used is only guaranteed
to work on the one the bot was written on.

The rule here is that the standard C library is off limits, since that
is a C feature, not an assembly feature! Interacting with the socket
should be done entirely through raw system calls.

However, this particular bot cheats a bit. It uses a C loader program
to create the actual socket, since interacting with the getaddrinfo and
socket APIs using raw system calls is gross. However, once the socket
has been created, the bot is `exec()`d and continues from there (ideally).

Run `make`, and invoke the loader as follows:

    ./loader chat.freenode.us 6667 foobot


