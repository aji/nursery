/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

struct listener {
	mowgli_vio_t *vio;
	mowgli_vio_sockaddr_t addr;

	int port;
	bool use_ssl;
};

/* client control functions */

/* actions -- abstractions */
static void try_register(struct a_client *cli);
static void send_motd(struct a_client *cli);
static void send_channels(struct a_client *cli);
static void send_burst(struct a_client *cli);

/* misc callbacks for other subsystems */
static void readline_cb(mowgli_linebuf_t *linebuf, char *buf, size_t n, void *cli_arg);
static void shutdown_cb(mowgli_linebuf_t *linebuf, void *cli_arg);
static void cli_disconnected(struct a_client *cli);

/* internal hooks */
static void conf_listen(void *conf_arg, void *priv);

/* IRC hooks */
static int on_PASS(int parc, const char *parv[], void *priv);
static int on_NICK(int parc, const char *parv[], void *priv);
static int on_USER(int parc, const char *parv[], void *priv);
static int on_QUIT(int parc, const char *parv[], void *priv);
static int on_PING(int parc, const char *parv[], void *priv);
static int on_BNC(int parc, const char *parv[], void *priv);

/* user ops */
static void cli_user_out(struct a_user *user, char *fmt, ...);

static struct a_user_ops cli_user_ops = {
	.out = cli_user_out,
};

bool a_client_init(void)
{
	irc_hook_add(alicorn.h_outgoing, "PASS", on_PASS);
	irc_hook_add(alicorn.h_outgoing, "NICK", on_NICK);
	irc_hook_add(alicorn.h_outgoing, "USER", on_USER);
	irc_hook_add(alicorn.h_outgoing, "QUIT", on_QUIT);
	irc_hook_add(alicorn.h_outgoing, "PING", on_PING);
	irc_hook_add(alicorn.h_outgoing, "BNC", on_BNC);

	mowgli_hook_associate("conf.item.listen", conf_listen, NULL);

	return true;
}

void a_client_deinit(void)
{
	irc_hook_del(alicorn.h_outgoing, "PASS", on_PASS);
	irc_hook_del(alicorn.h_outgoing, "NICK", on_NICK);
	irc_hook_del(alicorn.h_outgoing, "USER", on_USER);
	irc_hook_del(alicorn.h_outgoing, "QUIT", on_QUIT);
	irc_hook_del(alicorn.h_outgoing, "PING", on_PING);
	irc_hook_del(alicorn.h_outgoing, "BNC", on_BNC);

	mowgli_hook_dissociate("conf.item.listen", conf_listen);
}

static struct a_client *a_client_create(void)
{
	struct a_client *cli;

	cli = mowgli_alloc(sizeof(*cli));
	memset(cli, 0, sizeof(*cli));

	cli->user = a_user_create(&cli_user_ops, cli);

	cli->state = CLI_REGISTERING;

	cli->linebuf = mowgli_linebuf_create(readline_cb, cli);
	cli->linebuf->shutdown_cb = shutdown_cb;
	cli->linebuf->delim = "\n";

	return cli;
}

static void a_client_destroy(struct a_client *cli)
{
	a_log(LINFO, "Closing client connection to %s", cli->net ? cli->net->name : "(nil)");

	mowgli_hook_call("client.del", cli);

	a_user_destroy(cli->user);
	mowgli_linebuf_destroy(cli->linebuf);
	mowgli_free(cli);
}

static void a_client_listen_read(mowgli_eventloop_t *ev, mowgli_eventloop_io_t *io,
			mowgli_eventloop_io_dir_t dir, void *priv)
{
	struct listener *l = priv;
	struct a_client *cli = a_client_create();

	a_log(LDEBUG, "Connection on port %d", l->port);

	mowgli_vio_accept(l->vio, cli->linebuf->vio);
	mowgli_linebuf_attach_to_eventloop(cli->linebuf, alicorn.eventloop);
}

static mowgli_vio_evops_t a_client_listen_evops = {
	.read_cb = a_client_listen_read,
	.write_cb = NULL,
};

void a_client_printf(struct a_client *cli, char *fmt, ...)
{
	char buf[1024];
	int n;
	va_list va;

	if (!cli)
		return;

	va_start(va, fmt);
	n = vsnprintf(buf, 1024, fmt, va);
	va_end(va);

	mowgli_linebuf_write(cli->linebuf, buf, n);
}

void a_client_out(struct a_client *cli, char *fmt, ...)
{
	char buf[1024];
	va_list va;

	va_start(va, fmt);
	vsnprintf(buf, 1024, fmt, va);
	va_end(va);

	a_client_printf(cli, ":" ALICORN_HOST " NOTICE %s :%s",
			(cli->net == NULL ? "*" : cli->net->nick), buf);
}

void a_client_snotice(struct a_client *cli, char *fmt, ...)
{
	char buf[1024];
	int n;
	va_list va;

	va_start(va, fmt);
	n = vsnprintf(buf, 1024, fmt, va);
	va_end(va);

	a_client_printf(cli, ":" ALICORN_HOST " NOTICE * :*** Notice -- %s", buf);
}

void a_client_numeric(struct a_client *cli, char *num, char *fmt, ...)
{
	char buf[1024];
	int n;
	va_list va;

	va_start(va, fmt);
	n = vsnprintf(buf, 1024, fmt, va);
	va_end(va);

	a_client_printf(cli, ":" ALICORN_HOST " %s %s %s", num, (cli->net == NULL ? "*" : cli->net->nick), buf);
}

void a_client_die(struct a_client *cli, char *fmt, ...)
{
	char buf[1024];
	int n;
	va_list va;

	va_start(va, fmt);
	n = vsnprintf(buf, 1024, fmt, va);
	va_end(va);

	a_client_printf(cli, "ERROR :%s", buf);
	mowgli_linebuf_shut_down(cli->linebuf);
}

static void try_register(struct a_client *cli)
{
	struct a_account *acct;

	if (cli->acct_pass == NULL && cli->acct_user != NULL) {
		a_client_snotice(cli, "You have not sent a password yet. Try /quote PASS <passwd>");
		return;
	}

	if (cli->acct_pass == NULL || cli->acct_user == NULL || cli->acct_net == NULL)
		return;

	a_client_snotice(cli, "Logging you in as %s", cli->acct_user);

	acct = a_account_auth(cli->acct_user, cli->acct_pass);

	if (acct != NULL) {
		cli->net = a_network_by_name(acct, cli->acct_net);

		if (cli->net == NULL) {
			a_client_die(cli, "Account %s has no network %s", cli->acct_user, cli->acct_net);
		} else {
			a_client_snotice(cli, "Connecting you to network %s", cli->net->name);
			a_log(LNOTICE, "Client logging in to %s/%s", acct->name, cli->net->name);
			a_user_set_account(cli->user, acct);
			mowgli_hook_call("client.add", cli);
			send_burst(cli);
		}

	} else {
		a_client_die(cli, "Incorrect authentication information, closing connection");
	}
}

static void send_motd(struct a_client *cli)
{
	char *p, *s;

	p = mowgli_strdup(cli->net->nd.motd->str);

	s = strtok(p, "\n");
	while (s) {
		a_client_numeric(cli, "372", ":%s", s);
		s = strtok(NULL, "\n");
	}

	mowgli_free(p);
}

static void send_channels(struct a_client *cli)
{
	mowgli_patricia_iteration_state_t iter;
	struct a_network *net = cli->net;
	struct a_net_chan *chan;
	char source[512];

	if (net->nick && net->ident && net->host)
		snprintf(source, 512, "%s!%s@%s", net->nick, net->ident, net->host);
	else /* rare, but it could happen */
		snprintf(source, 512, "%s", net->nick);

	mowgli_patricia_foreach_start(net->nd.chans, &iter);
	while ((chan = mowgli_patricia_foreach_cur(net->nd.chans, &iter)) != NULL) {
		a_client_printf(cli, ":%s JOIN :%s", source, chan->name);

		if (chan->topic->pos == 0)
			a_client_numeric(cli, "331", "%s :No topic is set", chan->name);
		else
			a_client_numeric(cli, "332", "%s :%s", chan->name, chan->topic->str);

		mowgli_patricia_foreach_next(net->nd.chans, &iter);
	}
}

static void send_burst(struct a_client *cli)
{
	a_client_numeric(cli, "001", ":Welcome to the Alicorn Internet Relay Chat Bouncer, %s!*@*", cli->net->nick);
	a_client_numeric(cli, "002", ":Your host is %s, running version Alicorn %s", ALICORN_HOST, PACKAGE_VERSION);
	a_client_numeric(cli, "003", ":This server was created at the beginning of the universe");
	a_client_numeric(cli, "004", ":%s alicorn-%s a a", ALICORN_HOST, PACKAGE_VERSION);

	if (cli->net->nd.flags & NET_MOTD_EMPTY) {
		a_client_numeric(cli, "422", ":MOTD file is missing");
	} else {
		a_client_numeric(cli, "375", ":- " ALICORN_HOST " Message of the day");
		send_motd(cli);
		if (cli->net->nd.flags & NET_MOTD_LOCK)
			a_client_numeric(cli, "376", ":End of MOTD command");

		send_channels(cli);
	}
}

static void readline_cb(mowgli_linebuf_t *linebuf, char *buf, size_t n, void *cli_arg)
{
	struct a_client *cli = cli_arg;

	if (buf[n-1] == '\r')
		buf[--n] = '\0';

	if (linebuf->flags & MOWGLI_LINEBUF_LINE_HASNULLCHAR != 0)
		return;
	if (n > 512)
		return;

	/*a_log(LDEBUG, "  %s --> %s", cli->net ? cli->net->name : "(nil)", buf);*/
	a_route(cli_arg, false, buf, n);
}

static void shutdown_cb(mowgli_linebuf_t *linebuf, void *cli_arg)
{
	a_client_destroy(cli_arg);
}

static void conf_listen(void *conf_arg, void *priv)
{
	mowgli_config_file_entry_t *conf = conf_arg;
	mowgli_vio_sockaddr_t *addr;
	char *p;
	struct listener *l = NULL;

	if (conf->vardata == NULL || conf->entries != NULL) {
		a_log(LERROR, "Ignoring invalid listen config entry on line %s", conf->varlinenum);
		goto fail;
	}

	p = conf->vardata;
	l = mowgli_alloc(sizeof(*l));
	memset(l, 0, sizeof(*l));

	if (*p == '+') {
		l->use_ssl = true;
		p++;
	}

	l->port = atoi(p);

	if (l->port < 0 || l->port > 65535) {
		a_log(LERROR, "Ignoring invalid listen port (line %s)", conf->varlinenum);
		goto fail;
	}

	l->vio = mowgli_vio_create(l);

	if (l->use_ssl) {
		if (mowgli_vio_openssl_setssl(l->vio, NULL, NULL) != 0) {
			a_log(LERROR, "Failed to set SSL on listener");
			goto fail;
		}
	}

	addr = mowgli_vio_sockaddr_create(&l->addr, AF_INET, "0.0.0.0", l->port);

	if (addr == NULL) {
		a_log(LERROR, "Failed to create sockaddr for port %d", l->port);
		goto fail;
	}

	mowgli_vio_socket(l->vio, AF_INET, SOCK_STREAM, 0);
	mowgli_vio_reuseaddr(l->vio);
	mowgli_vio_bind(l->vio, addr);
	mowgli_vio_listen(l->vio, 5);

	mowgli_vio_eventloop_attach(l->vio, alicorn.eventloop, &a_client_listen_evops);
	MOWGLI_VIO_SETREAD(l->vio)
	MOWGLI_VIO_SETWRITE(l->vio)

	a_log(LINFO, "Listening on port %d", l->port);

	return;

fail:
	if (l != NULL) {
		if(l->vio != NULL)
			mowgli_vio_destroy(l->vio);
		mowgli_free(l);
	}
}

static int on_PASS(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	ctx->eat_message = true;

	if (ctx->cli->state != CLI_REGISTERING) {
		a_client_numeric(ctx->cli, "462", ":You may not reregister");
		return 0;
	}

	ctx->cli->acct_pass = mowgli_strdup(ctx->msg_raw + 5);
	try_register(ctx->cli);

	return 0;
}

static int on_NICK(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	if (ctx->cli->state == CLI_REGISTERING) {
		ctx->eat_message = true;
		return 1;
	}

	return 0;
}

static int on_USER(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	ctx->eat_message = true;

	if (ctx->cli->state != CLI_REGISTERING) {
		a_client_numeric(ctx->cli, "462", ":You may not reregister");
		return 0;
	}

	if (parc > 3) {
		ctx->cli->acct_user = mowgli_strdup(parv[3]);

		ctx->cli->acct_net = strchr(ctx->cli->acct_user, '/');
		if (ctx->cli->acct_net == NULL) {
			mowgli_free(ctx->cli->acct_user);
			ctx->cli->acct_user = NULL;

			a_client_snotice(ctx->cli, "It looks like your client is misconfigured"
						" for Alicorn authentication");
			a_client_snotice(ctx->cli, "To authenticate, set the username to \"account/network\""
						" and the password to your account password");
			a_client_die(ctx->cli, "Client configuration error, exiting");

		} else {
			*ctx->cli->acct_net++ = '\0';
		}
	}

	try_register(ctx->cli);

	return 0;
}

static int on_QUIT(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	/* no reason why a QUIT should make it through */
	ctx->eat_message = true;

	a_client_die(ctx->cli, "Client quit");

	return 0;
}

static int on_PING(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	char *all;

	ctx->eat_message = true;

	all = strstr(ctx->msg_raw, "PING");
	if (all == NULL) /* ... */
		return 0;
	all += 5;

	a_client_printf(ctx->cli, ":%s PONG %s", ALICORN_HOST, all);

	return 0;
}

static int on_BNC(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	char buf[32];
	char *p;

	ctx->eat_message = true;

	a_command_dispatch(ctx->cli->user, NULL, alicorn.cmds, parc-3, parv+3);

	return 0;
}

static void cli_user_out(struct a_user *user, char *fmt, ...)
{
	struct a_client *cli = user->priv;
	va_list va;
	char buf[1024];

	va_start(va, fmt);
	vsnprintf(buf, 1024, fmt, va);
	va_end(va);

	a_client_printf(cli, ":" ALICORN_HOST " NOTICE %s :%s",
			(cli->net == NULL ? "*" : cli->net->nick), buf);
}
