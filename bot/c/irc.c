#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include "io.h"
#include "version.h"

#define MSGBUFSIZE 4096
#define MAXARGS 17

#define MAXFIELDLEN 128

extern time_t started;
extern void update(char *errbuf, size_t errlen);

struct ctl_command {
	char name[16];
	void (*cb)(char *args);
	const char *usage, *help;
};

char ctl_me[MAXFIELDLEN] = "foobot";
char ctl_me_want[MAXFIELDLEN] = "foobot";
char ctl_chan[MAXFIELDLEN] = "#foobot";
char ctl_admin[MAXFIELDLEN] = "";
char ctl_prefix = '.';
char ctl_status_default[MAXFIELDLEN] = "u";
int ctl_sh_enabled = 0;

struct irc_command {
	char name[16];
	void (*cb)(void);
};

struct linebuf *irc;

char irc_host[MAXFIELDLEN];
char irc_port[MAXFIELDLEN] = "6667";
bool irc_reconnect = false;

char *irc_line;
char irc_msgbuf[MSGBUFSIZE];
char *irc_source;
char irc_nick[MAXFIELDLEN];
char *irc_command;
int irc_argc;
char *irc_argv[MAXARGS];

bool mass_command;

char reply_tgt[MAXFIELDLEN];
const char *reply_cmd = NULL;
bool reply_with_nick = false;

int irc_connect(void);
int irc_params(const char*);

/* stolen from ircd-micro */
bool match(char *mask, char *string)
{
	char *m = mask, *s = string;
	char *m_bt = m, *s_bt = s;
	 
	for (;;) {
		switch (*m) {
		case '\0':
			if (*s == '\0')
				return true;
		backtrack:
			if (m_bt == mask)
				return false;
			m = m_bt;
			s = ++s_bt;
			break;
		 
		case '*':
			while (*m == '*')
				m++;
			m_bt = m;
			s_bt = s;
			break;
		 
		default:
			if (!*s)
				return false;
			if (toupper(*m) != toupper(*s)
			    && *m != '?')
				goto backtrack;
			m++;
			s++;
		}
	}
}

void upcase(char *s)
{
	for (; *s; s++)
		*s = toupper(*s);
}

void spaces(char **s)
{
	while (**s && isspace(**s))
		(*s)++;
}

char *tokenize(char **s)
{
	char *p = *s;
	while (**s && !isspace(**s))
		(*s)++;
	if (**s)
		*(*s)++ = '\0';
	spaces(s);
	return p;
}

void ctlmsg(const char *fmt, ...)
{
	char buf[MSGBUFSIZE];
	va_list va;

	va_start(va, fmt);
	vsnprintf(buf, MSGBUFSIZE, fmt, va);
	va_end(va);

	println(irc, "PRIVMSG %s :%s", ctl_chan, buf);
}

void reply(const char *fmt, ...)
{
	char buf[MSGBUFSIZE];
	va_list va;

	if (reply_cmd == NULL)
		return;

	va_start(va, fmt);
	vsnprintf(buf, MSGBUFSIZE, fmt, va);
	va_end(va);

	println(irc, "%s %s :%s%s%s", reply_cmd, reply_tgt,
	        reply_with_nick?irc_nick:"", reply_with_nick?": ":"", buf);
}

void setting(char *opt)
{
	char *val = strchr(opt, '=');
	if (val != NULL)
		*val++ = '\0';

	fprintf(stderr, "SETTING: %s=%s\n", opt, val);

	if (!strcmp(opt, "admin")) {
		if (val != NULL)
			strncpy(ctl_admin, val, MAXFIELDLEN-1);
	} else if (!strcmp(opt, "prefix")) {
		if (val != NULL && *val)
			ctl_prefix = val[0];
	}
}

void settings(char *s)
{
	char buf[4096];
	strncpy(buf, s, 4095);
	s = buf;

	spaces(&s);

	while (*s)
		setting(tokenize(&s));
}

bool is_admin(void)
{
	if (ctl_admin == NULL)
		return false;
	return match(ctl_admin, irc_source);
}

bool access_check(void)
{
	if (!is_admin()) {
		if (!mass_command)
			reply("Insufficient access");
		return false;
	}
	return true;
}

void cmd_access(char *args)
{
	if (mass_command && !is_admin())
		return;

	reply("%s", is_admin() ? "admin" : "none");
}

void cmd_join(char *args)
{
	if (!access_check())
		return;
	println(irc, "JOIN %s", args);
	reply("Dispatched JOIN %s", args);
}

void cmd_part(char *args)
{
	if (!access_check())
		return;
	println(irc, "PART %s", args);
	reply("Dispatched PART %s", args);
}

void cmd_raw(char *args)
{
	if (!access_check())
		return;
	println(irc, "%s", args);
}

void cmd_die(char *args)
{
	if (!access_check())
		return;
	if (args == NULL)
		args = "die";
	println(irc, "QUIT :%s", args);
	linebuf_shutdown(irc);
}

void cmd_nick(char *args)
{
	if (mass_command)
		return;

	if (!is_admin()) {
		reply("Insufficient access");
		return;
	}

	snprintf(ctl_me_want, MAXFIELDLEN, "%s", args);
	println(irc, "NICK %s", ctl_me_want);
}

void cmd_help(char*);

struct ctl_command ctl_commands[] = {
	{ "ACCESS", cmd_access, "ACCESS", "check your privilege" },
	{ "JOIN", cmd_join, "JOIN ...", "send join" },
	{ "PART", cmd_part, "PART ...", "send part" },
	{ "RAW", cmd_raw, "RAW ...", "send raw IRC" },
	{ "DIE", cmd_die, "DIE [<message>]", "disconnect the bot" },
	{ "NICK", cmd_nick, "NICK <nickname>", "change the bot's nickname" },
	{ "HELP", cmd_help, "HELP [<command>]", "get command help" },
	{ "" }
};

void cmd_help(char *args)
{
	struct ctl_command *cmd = ctl_commands;
	char buf[512];
	size_t sz = 0;

	if (mass_command)
		return;

	if (args == NULL) {
		for (; cmd->cb; cmd++) {
			if (sz != 0)
				sz += snprintf(buf+sz, 512-sz, ", ");
			sz += snprintf(buf+sz, 512-sz, "%s", cmd->name);
		}
	} else {
		for (; cmd->cb; cmd++) {
			if (strcasecmp(args, cmd->name))
				continue;
			sz += snprintf(buf+sz, 512-sz, "%c%s -- %s",
			               ctl_prefix, cmd->usage, cmd->help);
			break;
		}
		if (sz == 0)
			sz += snprintf(buf+sz, 512-sz, "no such command");
	}

	reply("%s", buf);
}

void on_001(void)
{
	snprintf(ctl_me, MAXFIELDLEN, "%s", irc_argv[0]);
	println(irc, "JOIN %s", ctl_chan);
}

void on_PING(void)
{
	println(irc, "PONG %s", irc_line + 5);
}

void on_TOPIC(void)
{
	if (irc_argc < 2 || strcmp(irc_argv[0], ctl_chan))
		return;
	settings(irc_argv[1]);
}

void on_332(void)
{
	if (irc_argc < 3 || strcmp(irc_argv[1], ctl_chan))
		return;
	settings(irc_argv[2]);
}

bool parse_command(char *buf, size_t sz)
{
	char *s;

	reply_cmd = NULL;

	if (irc_argv[0][0] == '#') {
		if (irc_argv[1][0] == ctl_prefix) {
			mass_command = true;
			strncpy(buf, irc_argv[1] + 1, sz-1);
		} else if (!strncasecmp(irc_argv[1], ctl_me, strlen(ctl_me))) {
			mass_command = false;
			s = strchr(irc_argv[1], ' ');
			if (s == NULL)
				return false;
			spaces(&s);
			strncpy(buf, s, sz-1);
		} else {
			return false;
		}

		strncpy(reply_tgt, irc_argv[0], MAXFIELDLEN-1);
		reply_cmd = "PRIVMSG";
		reply_with_nick = true;

	} else {
		mass_command = false;
		strncpy(buf, irc_argv[1] + (irc_argv[1][0] == ctl_prefix), sz-1);
		strncpy(reply_tgt, irc_nick, MAXFIELDLEN-1);
		reply_cmd = "NOTICE";
		reply_with_nick = false;
	}

	return true;
}

void on_PRIVMSG(void)
{
	struct ctl_command *cur;
	char cmd[MSGBUFSIZE];
	char *args;

	if (irc_argc < 2)
		return;

	if (!parse_command(cmd, MSGBUFSIZE))
		return;

	args = strchr(cmd, ' ');
	if (args != NULL) {
		*args++ = '\0';
		spaces(&args);
	}

	upcase(cmd);

	for (cur=ctl_commands; cur->cb; cur++) {
		if (!strcmp(cur->name, cmd)) {
			cur->cb(args);
			break;
		}
	}

	reply_cmd = NULL;
}

void on_433(void)
{
	strcat(ctl_me_want, "_");
	println(irc, "NICK %s", ctl_me_want);
}

void on_NICK(void)
{
	if (strcmp(irc_nick, ctl_me))
		return;
	strncpy(ctl_me, irc_argv[0], MAXFIELDLEN-1);
}

void on_432(void)
{
	ctlmsg("%s: Erroneous nickname", irc_argv[1]);
}

struct irc_command irc_commands[] = {
	{ "001",     on_001 },
	{ "PING",    on_PING },
	{ "TOPIC",   on_TOPIC },
	{ "332",     on_332 },
	{ "PRIVMSG", on_PRIVMSG },
	{ "433",     on_433 },
	{ "NICK",    on_NICK },
	{ "432",     on_432 },
	{ "", NULL }
};

void irc_parse(char *s)
{
	int i;
	char *p;

	irc_line = s;
	strncpy(irc_msgbuf, s, MSGBUFSIZE-1);
	s = irc_msgbuf;

	spaces(&s);
	irc_source = NULL;
	irc_nick[0] = '\0';
	if (*s == ':') {
		s++;
		irc_source = tokenize(&s);
		p = strchr(irc_source, '!');
		if (p != NULL) {
			strncpy(irc_nick, irc_source, MAXFIELDLEN-1);
			irc_nick[p - irc_source] = '\0';
		}
	}

	irc_command = tokenize(&s);
	upcase(irc_command);

	for (i=0; i<MAXARGS; i++)
		irc_argv[i] = NULL;

	irc_argc = 0;
	while (*s && irc_argc < MAXARGS) {
		if (*s == ':') {
			irc_argv[irc_argc++] = ++s;
			break;
		}
		irc_argv[irc_argc++] = tokenize(&s);
	}
}

void irc_line_cb(struct linebuf *line, char *s, size_t len)
{
	struct irc_command *cmd;

	irc_parse(s);
	fprintf(stderr, "-> %s$\n", s);

	for (cmd=irc_commands; cmd->cb; cmd++) {
		if (!strcmp(irc_command, cmd->name)) {
			cmd->cb();
			break;
		}
	}
}

void irc_line_event(struct linebuf *line, int event)
{
	switch (event) {
	case LINEBUF_END_OF_STREAM:
		fprintf(stderr, "(end of stream)\n");
		break;

	case LINEBUF_CLEANUP:
		fprintf(stderr, "(cleanup)\n");

		close(line->fd->fd);
		irc = NULL;

		if (irc_reconnect) {
			if (irc_connect() < 0) {
				fprintf(stderr, "(reconnect failed)\n");
				io_poll_break();
			}
		} else {
			io_poll_break();
		}

		break;
	}
}

int do_connect(const char *host, const char *port)
{
	int sock = -1;
	struct addrinfo hints, *ai = NULL;
	const char *at;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	at = "getaddrinfo";
	if (getaddrinfo(host, port, &hints, &ai) < 0)
		goto out;

	at = "socket";
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		goto out;

	at = "connect";
	if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0)
		goto out;

	at = NULL;

out:
	if (at != NULL) {
		perror(at);
		if (sock >= 0)
			close(sock);
		sock = -1;
	}
	if (ai != NULL)
		freeaddrinfo(ai);
	return sock;
}

int irc_connect(void)
{
	int sock;

	if (irc != NULL) {
		fprintf(stderr, "(already connected)\n");
		return -1;
	}

	if ((sock = do_connect(irc_host, irc_port)) < 0)
		return -1;

	irc = linebuf_wrap(sock, irc_line_cb, irc_line_event, NULL);

	println(irc, "NICK %s", ctl_me_want);
	println(irc, "USER foo * 0 :c");

	irc_reconnect = false;

	return 0;
}

int irc_params(const char *opt)
{
	char *s;

	snprintf(irc_host, MAXFIELDLEN, "%s", opt);

	s = strchr(irc_host, ':');
	if (s != NULL) {
		*s++ = '\0';
		snprintf(irc_port, MAXFIELDLEN, "%s", s);
		s = strchr(irc_port, '/');
	} else {
		s = strchr(irc_host, '/');
	}

	if (s != NULL) {
		*s++ = '\0';
		snprintf(ctl_me_want, MAXFIELDLEN, "%s", s);
	}

	return 0;
}

int irc_start(const char *opt)
{
	irc_params(opt);

	if (irc_connect() < 0)
		return -1;

	return 0;
}

