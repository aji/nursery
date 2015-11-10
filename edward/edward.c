#include <mowgli.h>
#include <unicorn.h>

static mowgli_eventloop_t *eventloop;
static mowgli_linebuf_t *net_linebuf;
static mowgli_linebuf_t *red_out_linebuf;
static mowgli_linebuf_t *red_in_linebuf;

static irc_message_t *msg;
static irc_hook_table_t *net_hooks;

static char *cli_nick = "edvard";
static char *cli_ident = "ed";
static char *cli_gecos = "standard text editor";
static char *cli_chan = "#testing";

static pid_t child_pid = 0;

void line_out(mowgli_linebuf_t *linebuf, const char *fmt, ...)
{
	char buf[4096];
	int len;
	va_list va;

	va_start(va, fmt);
	len = vsnprintf(buf, 4096, fmt, va);
	va_end(va);

	mowgli_linebuf_write(linebuf, buf, len);
}

int sorcery_vio_read(mowgli_vio_t *vio, void *buf, size_t n)
{
	return read(mowgli_vio_getfd(vio), buf, n);
}

int sorcery_vio_write(mowgli_vio_t *vio, const void *buf, size_t n)
{
	return write(mowgli_vio_getfd(vio), buf, n);
}

pid_t sorcery_vio(mowgli_vio_t *from, mowgli_vio_t *to, char *const *cmd)
{
	int from_fd[2];
	int to_fd[2];
	pid_t pid;

	pipe(from_fd);
	pipe(to_fd);

	pid = fork();

	if (pid == 0) {
		dup2(to_fd[0], STDIN_FILENO);
		dup2(from_fd[1], STDOUT_FILENO);
		dup2(from_fd[1], STDERR_FILENO);

		execvp(cmd[0], cmd);
	} else {
		from->io.fd = from_fd[0];
		to->io.fd = to_fd[1];

		mowgli_vio_ops_set_op(from->ops, read, sorcery_vio_read);
		mowgli_vio_ops_set_op(to->ops, read, sorcery_vio_read);

		mowgli_vio_ops_set_op(from->ops, write, sorcery_vio_write);
		mowgli_vio_ops_set_op(to->ops, write, sorcery_vio_write);
	} 

	return pid;
}

int net_do_PING(int parc, const char *parv[], void *ctx)
{
	printf("Pong!\n");
	line_out(net_linebuf, "PONG :%s", parv[parc-1]);
	return 0;
}

int net_do_001(int parc, const char *parv[], void *ctx)
{
	printf("Registered! Joining channel %s...\n", cli_chan);
	line_out(net_linebuf, "JOIN %s", cli_chan);
	return 0;
}

int net_do_PRIVMSG(int parc, const char *parv[], void *ctx)
{
	if (!strcmp(parv[parc-2], cli_chan) && parv[parc-1][0] == '>')
		line_out(red_out_linebuf, "%s", parv[parc-1] + 1);
	return 0;
}

void net_add_hooks(void)
{
	irc_hook_add(net_hooks, "PING", net_do_PING);
	irc_hook_add(net_hooks, "001", net_do_001);
	irc_hook_add(net_hooks, "PRIVMSG", net_do_PRIVMSG);
}

int net_connect(char *host, char *port)
{
	/* a lot of this function was taken from create_client in
	   libmowgli-2/src/examples/linebuf/linebuf.c */
	bool use_ssl = false;
	struct addrinfo hints, *res;
	mowgli_vio_sockaddr_t addr;
	int err;

	if (*port == '+') {
		port++;
		use_ssl = true;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(host, port, &hints, &res) != 0)
		return -1;

	mowgli_vio_sockaddr_from_struct(&addr, res->ai_addr, res->ai_addrlen);

	if (use_ssl) {
		if (mowgli_vio_openssl_setssl(net_linebuf->vio, NULL, NULL) != 0)
			return -1;
	}

	if (mowgli_vio_socket(net_linebuf->vio, res->ai_family,
			res->ai_socktype, res->ai_protocol) != 0)
		return -1;

	mowgli_linebuf_attach_to_eventloop(net_linebuf, eventloop);

	if (mowgli_vio_connect(net_linebuf->vio, &addr) != 0)
		return -1;

	return 0;
}

void net_handshake(void)
{
	line_out(net_linebuf, "NICK %s", cli_nick);
	line_out(net_linebuf, "USER %s 0 0 :%s", cli_ident, cli_gecos);
}

void net_line(mowgli_linebuf_t *linebuf, char *line, size_t len, void *userdata)
{
	if (linebuf->flags & MOWGLI_LINEBUF_LINE_HASNULLCHAR)
		return;

	irc_message_parse(msg, line);
	irc_hook_ext_dispatch(net_hooks, msg, NULL);
}

int red_start(void)
{
	char *cmd[] = { "../ed", "-r", NULL };

	child_pid = sorcery_vio(red_in_linebuf->vio, red_out_linebuf->vio, cmd);

	red_in_linebuf->delim = "\n";
	red_out_linebuf->delim = "\n";

	mowgli_linebuf_attach_to_eventloop(red_in_linebuf, eventloop);
	mowgli_linebuf_attach_to_eventloop(red_out_linebuf, eventloop);

	return 0;
}

void red_ircify(char *buf, char *line)
{
	int i, j;
	i = j = 0;
	while (line[j]) {
		if (line[j] == '\t') {
			do {
				buf[i++] = ' ';
			} while (i % 8 != 0);
		} else {
			buf[i] = line[j];
			i++;
		}
		j++;
	}
	if (i == 0)
		buf[i++] = ' ';
	buf[i] = '\0';
}

void red_line(mowgli_linebuf_t *linebuf, char *line, size_t len, void *userdata)
{
	char buf[512];
	red_ircify(buf, line);
	line_out(net_linebuf, "PRIVMSG %s :%s", cli_chan, buf);
}

void signal_cb(int sig)
{
	mowgli_eventloop_break(eventloop);
}

void do_nothing(void *data)
{
}

int usage(char *argv0)
{
	printf("usage: %s SERVER [+]PORT [CHANNEL]\n", argv0);
	printf("CHANNEL defaults to #ed\n");
	return 1;
}

int main(int argc, char *argv[])
{
	if (argc < 3 || argc > 4)
		return usage(argv[0]);

	eventloop = mowgli_eventloop_create();
	net_linebuf = mowgli_linebuf_create(net_line, NULL);
	red_in_linebuf = mowgli_linebuf_create(red_line, NULL);
	red_out_linebuf = mowgli_linebuf_create(red_line, NULL);

	msg = irc_message_create();
	net_hooks = irc_hook_table_create();

	if (argc > 3)
		cli_chan = argv[3];

	if (red_start() < 0) {
		printf("Failed to start red, cannot continue\n");
		return 1;
	}

	net_add_hooks();

	if (net_connect(argv[1], argv[2]) < 0) {
		printf("Network connection failed, cannot continue\n");
		kill(child_pid, SIGTERM);
		return 1;
	}

	net_handshake();

	signal(SIGINT, signal_cb);
	signal(SIGTERM, signal_cb);

	mowgli_timer_add(eventloop, "null", do_nothing, NULL, 600);

	mowgli_eventloop_run(eventloop);

	kill(child_pid, SIGTERM);

	printf("Goodbye!\n");

	return 0;
}
