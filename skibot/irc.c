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
#include "child.h"
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

char ctl_me[MAXFIELDLEN] = "SKI|GEN01";
char *ctl_chan = "#ski";
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
	} else if (!strcmp(opt, "status")) {
		if (val != NULL && *val)
			strncpy(ctl_status_default, val, MAXFIELDLEN-1);
	} else if (!strcmp(opt, "shell")) {
		ctl_sh_enabled = 1;
	} else if (!strcmp(opt, "noshell")) {
		ctl_sh_enabled = 0;
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

void cmd_access(char *args)
{
	if (mass_command && !is_admin())
		return;

	reply("%s", is_admin() ? "admin" : "none");
}

enum stat_decorate { STAT_NORMAL, STAT_BOLD, STAT_DULL, STAT_COLOR };

size_t stat_admin(char *buf, size_t sz, enum stat_decorate deco)
{
	switch (deco) {
	case STAT_NORMAL:
	case STAT_DULL:
		return snprintf(buf, sz, "admins match %s", ctl_admin);
	case STAT_BOLD:
	case STAT_COLOR:
		return snprintf(buf, sz, "admins match \2%s\2", ctl_admin);
	}
	return 0;
}

size_t stat_irc(char *buf, size_t sz, enum stat_decorate deco)
{
	return snprintf(buf, sz, "%s:%s/%s", irc_host, irc_port, ctl_me+4);
}

size_t stat_uptime(char *buf, size_t sz, enum stat_decorate deco)
{
	char tbuf[128];
	time_t days = time(NULL) - started;
	unsigned hr, min, sec;

	sec = days % 60; days /= 60;
	min = days % 60; days /= 60;
	hr = days % 24; days /= 24;

	snprintf(tbuf, 128, "%3d days %02d:%02d:%02d", (int)days, hr, min, sec);
	switch (deco) {
	case STAT_NORMAL:
		return snprintf(buf, sz, "up %s", tbuf);
	case STAT_COLOR:
		return snprintf(buf, sz, "up \00308%s\xf", tbuf);
	case STAT_BOLD:
		return snprintf(buf, sz, "up \2%s\2", tbuf);
	case STAT_DULL:
		return snprintf(buf, sz, "\0031up\xf %s", tbuf);
	}
	return 0;
}

size_t stat_version_real(char *buf, size_t sz, enum stat_decorate deco, bool brief)
{
	char tbuf[128] = "";

	switch (deco) {
	case STAT_NORMAL:
	case STAT_DULL:
		snprintf(tbuf, 128, "%s-%s", pkg_name, pkg_version);
		break;
	case STAT_BOLD:
		snprintf(tbuf, 128, "\2%s-%s\2", pkg_name, pkg_version);
		break;
	case STAT_COLOR:
		snprintf(tbuf, 128, "\00310%s-%s\xf", pkg_name, pkg_version);
		break;
	}

	if (brief)
		return snprintf(buf, sz, "%s", tbuf);
	return snprintf(buf, sz, "%s built %s", tbuf, pkg_built);
}

size_t stat_version(char *buf, size_t sz, enum stat_decorate deco)
{
	return stat_version_real(buf, sz, deco, false);
}

size_t stat_whoami(char *buf, size_t sz, enum stat_decorate deco)
{
	switch (deco) {
	case STAT_NORMAL:
		return snprintf(buf, sz, "you are %s", irc_source);
	case STAT_COLOR:
	case STAT_BOLD:
		return snprintf(buf, sz, "you are \2%s\2", irc_source);
	case STAT_DULL:
		return snprintf(buf, sz, "\0031you are\xf %s", irc_source);
	}
	return 0;
}

size_t stat_irc_brief(char *buf, size_t sz, enum stat_decorate deco)
{
	switch (deco) {
	case STAT_NORMAL:
		return snprintf(buf, sz, "%s", ctl_me+4);
	case STAT_DULL:
		return snprintf(buf, sz, "\0031%s\xf", ctl_me+4);
	case STAT_COLOR:
	case STAT_BOLD:
		return snprintf(buf, sz, "\2%s\2", ctl_me+4);
	}
	return 0;
}

size_t stat_uptime_brief(char *buf, size_t sz, enum stat_decorate deco)
{
	int diff = time(NULL) - started;
	switch (deco) {
	case STAT_NORMAL:
		return snprintf(buf, sz, "[%08d]", diff);
	case STAT_DULL:
		return snprintf(buf, sz, "\0031[%08d]\xf", diff);
	case STAT_COLOR:
		return snprintf(buf, sz, "[\00308%08d\xf]", diff);
	case STAT_BOLD:
		return snprintf(buf, sz, "[\2%08d\2]", diff);
	}
	return 0;
}

size_t stat_version_brief(char *buf, size_t sz, enum stat_decorate deco)
{
	return stat_version_real(buf, sz, deco, true);
}

size_t (*stat_cb[256])(char*,size_t,enum stat_decorate) = {
	['a'] = stat_admin,
	['i'] = stat_irc,
	['u'] = stat_uptime,
	['v'] = stat_version,
	['w'] = stat_whoami,

	['I'] = stat_irc_brief,
	['U'] = stat_uptime_brief,
	['V'] = stat_version_brief,
};
void cmd_status(char *args)
{
	char buf[MSGBUFSIZE];
	size_t sz = 0;
	enum stat_decorate deco = STAT_NORMAL;

	if (mass_command && !is_admin())
		return;

	if (args == NULL)
		args = ctl_status_default;

	for (; *args; args++) {
		switch (*args) {
		case '!':
			deco = STAT_BOLD;
			break;
		case '.':
			deco = STAT_DULL;
			break;
		case '$':
			deco = STAT_COLOR;
			break;

		default:
			if (stat_cb[(unsigned char)*args]) {
				if (sz != 0)
					sz += snprintf(buf+sz, MSGBUFSIZE-sz, ", ");
				sz += stat_cb[(unsigned char)*args](buf+sz,
						MSGBUFSIZE-sz, deco);
			}
			deco = STAT_NORMAL;
		}
	}

	if (sz > 0)
		reply("%s", buf);
}

void cmd_die(char *args)
{
	if (!is_admin())
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

	if (!args || strlen(args) != 5) {
		reply("Nickname should be 5 chars long");
		return;
	}

	upcase(args);

	println(irc, "NICK SKI|%s", args);
}

void cmd_hop(char *args)
{
	if (!is_admin()) {
		if (!mass_command)
			reply("Insufficient access");
		return;
	}

	if (!args) {
		if (!mass_command)
			reply("Usage: %cHOP HOST[:PORT][/NICK]", ctl_prefix);
		return;
	}

	println(irc, "QUIT :hopping");
	linebuf_shutdown(irc);

	irc_reconnect = true;
	irc_params(args);
}

void cmd_update(char *args)
{
	char buf[MSGBUFSIZE];

	if (!is_admin()) {
		if (!mass_command)
			reply("Insufficient access");
		return;
	}

	reply("Updating...");
	update(buf, MSGBUFSIZE);
	reply("Update failed: %s", buf);
}

struct child *sh_child = NULL;
int sh_lines = 0;

void sh_line(struct child *ch, char *line)
{
	if (ch != sh_child || sh_lines > 5)
		return;

	ctlmsg("| %s", line);

	sh_lines++;
	if (sh_lines > 5)
		ctlmsg("(output truncated)");
}

void sh_event(struct child *ch, int event, int data)
{
	if (sh_child == NULL)
		return;

	fprintf(stderr, "sh_event: %d, %d\n", event, data);

	switch (event) {
	case CHILD_EXITED:
		if (data != 0)
			ctlmsg("(exited with status %d)", data);
		sh_child = NULL;
		return;

	case CHILD_SIGNALED:
		ctlmsg("(received signal %d)", data);
		sh_child = NULL;
		return;
	}
}

void cmd_sh(char *args)
{
	char *argv[] = { "sh", "-c", args, NULL };

	if (!is_admin()) {
		if (!mass_command)
			reply("Insufficient access");
		return;
	}

	if (!ctl_sh_enabled) {
		if (!mass_command)
			reply("This command is disabled");
		return;
	}

	if (!args || !*args) {
		if (!mass_command)
			reply("Usage: %cSH <command>", ctl_prefix);
		return;
	}

	if (sh_child != NULL) {
		reply("Command already in progress. Sending SIGINT");
		child_kill(sh_child, SIGINT);
		return;
	}

	sh_child = child_spawn("/bin/sh", argv, sh_line, sh_event, NULL);
	sh_lines = 0;

	if (sh_child == NULL) {
		reply("Failed to spawn child process");
		return;
	}
}

void cmd_help(char*);

struct ctl_command ctl_commands[] = {
	{ "ACCESS", cmd_access, "ACCESS", "check your access to the bot" },
	{ "STATUS", cmd_status, "STATUS [<stats>]",
	  "get stats about the bot. a=admins iI=irc uU=uptime w=whoami (upper "
	  "case versions are brief). !=bold, .=dull, $=color (not supported "
	  "by all stats)" },
	{ "DIE", cmd_die, "DIE [<message>]", "kill the bot" },
	{ "NICK", cmd_nick, "NICK [<nickname>]", "change the bot's nickname" },
	{ "HOP", cmd_hop, "HOP <host>[:<port>][/<nick>]", "switch servers" },
	{ "UPDATE", cmd_update, "UPDATE", "self-exec without disconnecting" },
	{ "SH", cmd_sh, "SH <command>", "execute command with sh -c" },
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
	println(irc, "JOIN %s", ctl_chan);
}

void on_PING(void)
{
	println(irc, "PONG %s", irc_line + 5);
}

void on_TOPIC(void)
{
	if (irc_argc < 2)
		return;
	settings(irc_argv[1]);
}

void on_332(void)
{
	if (irc_argc < 3)
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
	strcat(ctl_me, "_");
	println(irc, "NICK %s", ctl_me);
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

	println(irc, "NICK %s", ctl_me);
	println(irc, "USER skibot * 0 :https://github.com/aji/skibot");

	irc_reconnect = false;

	return 0;
}

int irc_params(const char *opt)
{
	char *s;

	strncpy(irc_host, opt, MAXFIELDLEN-1);

	s = strchr(irc_host, ':');
	if (s != NULL) {
		*s++ = '\0';
		strncpy(irc_port, s, MAXFIELDLEN-1);
		s = strchr(irc_port, '/');
	} else {
		s = strchr(irc_host, '/');
	}

	if (s != NULL) {
		*s++ = '\0';
		upcase(s);
		snprintf(ctl_me, MAXFIELDLEN, "SKI|%s", s);
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


void int_put(FILE *save, int n)
{
	fwrite(&n, sizeof(n), 1, save);
}

int int_get(FILE *save)
{
	int n;
	fread(&n, sizeof(n), 1, save);
	return n;
}

void str_put(FILE *save, const char *s, int sz)
{
	if (sz < 0)
		sz = strlen(s);
	int_put(save, sz);
	fwrite(s, 1, sz, save);
}

void str_get(FILE *save, char *s, int *sz)
{
	int n;
	if (sz == NULL)
		sz = &n;
	*sz = int_get(save);
	fread(s, 1, *sz, save);
	s[*sz] = '\0';
}

int irc_save(FILE *save)
{
	int_put(save, irc->fd->fd);

	str_put(save, irc->inbuf, irc->inlen);
	str_put(save, irc->outbuf, irc->outlen);

	str_put(save, ctl_me, -1);
	str_put(save, ctl_admin, -1);
	int_put(save, ctl_prefix);
	str_put(save, ctl_status_default, -1);

	str_put(save, irc_host, -1);
	str_put(save, irc_port, -1);

	int_put(save, ctl_sh_enabled);

	return 0;
}

int irc_restore(FILE *save)
{
	int fd;

	fd = int_get(save);
	irc = linebuf_wrap(fd, irc_line_cb, irc_line_event, NULL);

	str_get(save, irc->inbuf, &irc->inlen);
	str_get(save, irc->outbuf, &irc->outlen);

	str_get(save, ctl_me, NULL);
	str_get(save, ctl_admin, NULL);
	ctl_prefix = int_get(save);
	str_get(save, ctl_status_default, NULL);

	str_get(save, irc_host, NULL);
	str_get(save, irc_port, NULL);

	ctl_sh_enabled = int_get(save);

	/* hax... */
	irc->fd->post(irc->fd);

	ctlmsg("Update successful");

	return 0;
}
