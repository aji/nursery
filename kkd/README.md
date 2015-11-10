# `kkd`

The short version: kkd is an ICMP "knock knock" daemon that uses the
ICMP protocol to listen for the secret code.

The long version: I find port knockers to be very interesting. My first
port knocker daemon was a cruddy little piece of Python that worked
okayish, but used the common TCP layer, which would `bind()` to a port,
wait for `accept()`, then immediately `close()` the socket, listening on
the next port in the sequence. It worked rather well, but had a single
flaw that made it entirely useless: an attempt to connect with netcat or
similar on a knock sequence port was not met with a "connection refused"
error, meaning *something* was listening. I later realized that with
`SOCK_RAW` you could watch for TCP packets indicating an attempt to
connect on the knock sequence ports, rather than using the TCP layer
itself. I went to implement this and decided that I could do it with
any IP protocol, not just TCP, so I settled on ICMP.

# Compiling

    $ gcc -o kkd kkd.c

No external dependencies or anything. Any sane system should be able to
compile this just fine.

# Installing

    # cp kkd /usr/local/sbin

Yeah, I'm trying to keep this as simple and straightforward as I can.

# Running

    # kkd [four byte code]

This starts the daemon. Pick a nice four byte code. There are over four
billion from which you can choose, so just pick one.

The first 8 bytes of echo request data are a timestamp used by ping
implementations to determine ping time. Many pings let you specify
optional ping data as well, which looks like it starts after the
8th byte. This is where `kkd` looks for the code. If I had the code
0xfff80085, I would knock with the following command:

    $ ping -p fff80085

Use your manpages to find out how your ping does this. I will someday
write a `kk` which does this in a uniform matter, but alas, I do not
care enough.
