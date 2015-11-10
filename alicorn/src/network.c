/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

struct dns_priv {
	char *host;
	mowgli_dns_query_t query;
};

/* TODO: reconnect */

void net_user_check_gc(struct a_net_user *nu)
{
	if (nu->visibility > 0 || nu->weak_refs > 0)
		return;

	if (nu->nick)
		mowgli_free(nu->nick);
	if (nu->ident)
		mowgli_free(nu->ident);
	if (nu->host)
		mowgli_free(nu->host);

	mowgli_free(nu);
}

void a_net_user_visible(struct a_net_user *nu) { nu->visibility++; }

void a_net_user_vanish(struct a_net_user *nu)
{
	nu->visibility--;
	net_user_check_gc(nu);
}

void a_net_user_weak_incref(struct a_net_user *nu) { nu->weak_refs++; }

void a_net_user_weak_decref(struct a_net_user *nu)
{
	nu->weak_refs--;
	net_user_check_gc(nu);
}

/* network control functions */
static struct a_network *net_create(const char *name);
static void net_destroy(struct a_network *net);
static int net_add_to_account(struct a_network *net, const char *acct);
static int net_del_from_account(struct a_network *net);
static int net_connect(struct a_network *net, char *h_name, mowgli_vio_sockaddr_t *addr);
static int net_disconnect(struct a_network *net);

/* netdata lowlevel control */
static void netdata_chan_add(struct a_network *net, const char *name);
static void netdata_chan_del(struct a_network *net, const char *name);
static struct a_net_chan *netdata_chan(struct a_network *net, const char *name);
static void netdata_chan_topic(struct a_network *net, const char *name, const char *topic);

/* users and chanusers */
static struct a_net_user *net_user_get(struct a_network *net, const char *nick);

/* actions -- abstractions */
static void net_start_connect(struct a_network *net);
static void net_do_register(struct a_network *net);

/* database hooks */
static int db_net(int parc, const char *parv[], void *priv);
static void db_save_acct(void *net, void *priv);

/* misc. callbacks -- provided to other subsystems */
static void readline_cb(mowgli_linebuf_t *linebuf, char *buf, size_t n, void *net_arg);
static void shutdown_cb(mowgli_linebuf_t *linebuf, void *net_arg);
static void net_disconnected(struct a_network *net);

/* internal hooks */
static void add_client(void *client, void *unused);
static void del_client(void *client, void *unused);

/* IRC hooks */
static int on_PING(int parc, const char *parv[], void *priv);
static int on_JOIN(int parc, const char *parv[], void *priv);
static int on_PART(int parc, const char *parv[], void *priv);
static int on_topic_update(int parc, const char *parv[], void *priv);
static int on_RPL_WELCOME(int parc, const char *parv[], void *priv);
static int on_RPL_ISUPPORT(int parc, const char *parv[], void *priv);
static int on_RPL_MOTDSTART(int parc, const char *parv[], void *priv);
static int on_RPL_MOTD(int parc, const char *parv[], void *priv);
static int on_RPL_ENDOFMOTD(int parc, const char *parv[], void *priv);
static int on_ERR_NOMOTD(int parc, const char *parv[], void *priv);
static int on_RPL_NAMREPLY(int parc, const char *parv[], void *priv);
static int on_RPL_ENDOFNAMES(int parc, const char *parv[], void *priv);
static int on_RPL_WHOISUSER(int parc, const char *parv[], void *priv);


bool a_network_init(void)
{
	irc_hook_add(alicorn.h_incoming, "PING", on_PING);
	irc_hook_add(alicorn.h_incoming, "JOIN", on_JOIN);
	irc_hook_add(alicorn.h_incoming, "PART", on_PART);

	irc_hook_add(alicorn.h_incoming, "TOPIC", on_topic_update);
	irc_hook_add(alicorn.h_incoming, "331", on_topic_update);
	irc_hook_add(alicorn.h_incoming, "332", on_topic_update);

	irc_hook_add(alicorn.h_incoming, "001", on_RPL_WELCOME);
	irc_hook_add(alicorn.h_incoming, "005", on_RPL_ISUPPORT);
	irc_hook_add(alicorn.h_incoming, "375", on_RPL_MOTDSTART);
	irc_hook_add(alicorn.h_incoming, "372", on_RPL_MOTD);
	irc_hook_add(alicorn.h_incoming, "376", on_RPL_ENDOFMOTD);
	irc_hook_add(alicorn.h_incoming, "422", on_ERR_NOMOTD);

	irc_hook_add(alicorn.h_incoming, "353", on_RPL_NAMREPLY);
	irc_hook_add(alicorn.h_incoming, "366", on_RPL_ENDOFNAMES);

	irc_hook_add(alicorn.h_incoming, "311", on_RPL_WHOISUSER);

	irc_hook_add(alicorn.h_db, "NET", db_net);

	mowgli_hook_associate("db.save.acct", db_save_acct, NULL);
	mowgli_hook_associate("client.add", add_client, NULL);
	mowgli_hook_associate("client.del", del_client, NULL);

	return true;
}

void a_network_deinit(void)
{
	irc_hook_del(alicorn.h_incoming, "PING", on_PING);
	irc_hook_del(alicorn.h_incoming, "JOIN", on_JOIN);
	irc_hook_del(alicorn.h_incoming, "PART", on_PART);

	irc_hook_del(alicorn.h_incoming, "TOPIC", on_topic_update);
	irc_hook_del(alicorn.h_incoming, "331", on_topic_update);
	irc_hook_del(alicorn.h_incoming, "332", on_topic_update);

	irc_hook_del(alicorn.h_incoming, "001", on_RPL_WELCOME);
	irc_hook_del(alicorn.h_incoming, "005", on_RPL_ISUPPORT);
	irc_hook_del(alicorn.h_incoming, "375", on_RPL_MOTDSTART);
	irc_hook_del(alicorn.h_incoming, "372", on_RPL_MOTD);
	irc_hook_del(alicorn.h_incoming, "376", on_RPL_ENDOFMOTD);
	irc_hook_del(alicorn.h_incoming, "422", on_ERR_NOMOTD);

	irc_hook_del(alicorn.h_incoming, "311", on_RPL_WHOISUSER);

	irc_hook_del(alicorn.h_db, "NET", db_net);

	mowgli_hook_dissociate("db.save.acct", db_save_acct);
	mowgli_hook_dissociate("client.add", add_client);
	mowgli_hook_dissociate("client.del", del_client);
}

static void mowgli_strput(char **p, const char *s)
{
	if (*p)
		mowgli_free(*p);
	*p = mowgli_strdup(s);
}

static struct a_network *net_create(const char *name)
{
	struct a_network *net;

	net = mowgli_alloc(sizeof(*net));
	if (net == NULL)
		return NULL;
	memset(net, 0, sizeof(*net));

	net->name = mowgli_strdup(name);
	net->acct = NULL;

	net->host = NULL;
	net->port = 0;
	net->use_ssl = false;

	/* defaults */
	net->nick = mowgli_strdup(DEFAULT_NICK);
	net->ident = mowgli_strdup(DEFAULT_IDENT);
	net->gecos = mowgli_strdup(DEFAULT_GECOS);

	net->nd.flags = 0;
	net->nd.flags |= NET_MOTD_EMPTY | NET_MOTD_LOCK;
	net->nd.nick = mowgli_strdup(DEFAULT_NICK);
	net->nd.ident = mowgli_strdup(DEFAULT_IDENT);
	net->nd.host = mowgli_strdup("");
	net->nd.gecos = mowgli_strdup(DEFAULT_GECOS);
	net->nd.isupport = irc_isupport_create();
	net->nd.motd = mowgli_string_create();
	net->nd.chans = mowgli_patricia_create(NULL); /* TODO: casemap */
	net->nd.users = mowgli_patricia_create(NULL); /* TODO: casemap */

	net->state = NET_DISCONNECTED;
	net->linebuf = NULL;
	net->clients = mowgli_list_create();

	return net;
}

static void net_destroy(struct a_network *net)
{
	mowgli_free(net->name);
	if (net->acct)
		net_del_from_account(net);

	if (net->host)
		mowgli_free(net->host);

	mowgli_free(net->nick);
	mowgli_free(net->ident);
	mowgli_free(net->gecos);

	mowgli_free(net->nd.nick);
	mowgli_free(net->nd.ident);
	mowgli_free(net->nd.host);
	mowgli_free(net->nd.gecos);
	irc_isupport_destroy(net->nd.isupport);
	mowgli_string_destroy(net->nd.motd);

	if (net->linebuf)
		mowgli_linebuf_destroy(net->linebuf);
	mowgli_list_free(net->clients); /* XXX */

	mowgli_free(net);
}

static void parse_server(struct a_network *net, const char *spec)
{
	char hostspec[1024];
	char *portspec;

	/* snag the hostname */
	snprintf(hostspec, 1024, "%s", spec);

	portspec = strchr(hostspec, '/');
	if (portspec != NULL)
		*portspec = '\0';

	net->host = mowgli_strdup(hostspec);

	/* snag the port */
	portspec = strchr(spec, '/');

	if (portspec == NULL)
		portspec = "/6667";
	portspec++;

	net->use_ssl = false;
	if (portspec[0] == '+') {
		net->use_ssl = true;
		portspec++;
	}

	net->port = atoi(portspec);
	if (net->port < 1 || net->port > 65535) {
		a_log(LERROR, "Invalid port number %d, defaulting to 6667", net->port);
		net->port = 6667;
	}
}

static int net_add_to_account(struct a_network *net, const char *acct_name)
{
	struct a_account *acct;

	acct = a_account_find(acct_name);
	if (acct == NULL) {
		a_log(LERROR, "Could not find account %s", acct_name);
		return -1;
	}

	if (mowgli_patricia_retrieve(acct->networks, net->name) != NULL) {
		a_log(LERROR, "Network %s for %s already exists, ignoring duplicate configuration",
					net->name, acct->name);
		return -1;
	}

	a_log(LINFO, "Adding network %s to account %s", net->name, acct->name);

	mowgli_patricia_add(acct->networks, net->name, net);
	net->acct = acct;

	return 0;
}

static int net_del_from_account(struct a_network *net)
{
	a_log(LINFO, "Deleting network %s from account %s", net->name, net->acct->name);

	mowgli_patricia_delete(net->acct->networks, net->name);

	return 0;
}

static int net_connect(struct a_network *net, char *h_name, mowgli_vio_sockaddr_t *addr)
{
	char buf[65536];

	net->linebuf = mowgli_linebuf_create(readline_cb, net);
	net->linebuf->shutdown_cb = shutdown_cb;
	net->linebuf->delim = "\n";

	if (net->state == NET_CONNECTED) {
		a_log(LERROR, "Attempt to connect already-connected network");
		return;
	}

	if (addr->addr.ss_family == AF_INET) {
		struct sockaddr_in *addr4 = (void*)&addr->addr;

		addr4->sin_port = htons(net->port);
		addr->addrlen = sizeof(struct sockaddr_in);

		inet_ntop(AF_INET, &addr4->sin_addr, buf, 65536);
		a_log(LDEBUG, "Connecting IPv4 %s/%d", buf, net->port);

	} else if (addr->addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *addr6 = (void*)&addr->addr;

		addr6->sin6_port = net->port;
		addr->addrlen = sizeof(struct sockaddr_in);

		inet_ntop(AF_INET6, &addr6->sin6_addr, buf, 65536);
		a_log(LDEBUG, "Connecting IPv6 %s/%d", buf, net->port);

	} else {
		a_log(LERROR, "Unknown address family %d for %s (%s)", addr->addr.ss_family,
					h_name, net->name);
		return -1;
	}

	a_log(LNOTICE, "Connecting to %s (%s)", h_name, net->name);

	if (net->use_ssl) {
		if (mowgli_vio_openssl_setssl(net->linebuf->vio, NULL, NULL) != 0) {
			a_log(LERROR, "Failed to set SSL on %s", net->name);
			return -1;
		}
	}

	if (mowgli_vio_socket(net->linebuf->vio, AF_INET, SOCK_STREAM, 0) != 0) {
		a_log(LERROR, "Failed to create socket for %s", net->name);
		return -1;
	}

	mowgli_linebuf_attach_to_eventloop(net->linebuf, alicorn.eventloop);

	if (mowgli_vio_connect(net->linebuf->vio, addr) != 0) {
		a_log(LERROR, "Failed to connect to %s (%s)", h_name, net->name);
		return -1;
	}

	net->state = NET_CONNECTED;

	net_do_register(net);

	return 0;
}

static int net_disconnect(struct a_network *net)
{
	if (net->state == NET_DISCONNECTED) {
		a_log(LERROR, "Attempt to disconnect disconnected network");
		return;
	}

	if (net->linebuf) {
		mowgli_linebuf_destroy(net->linebuf);
		net->linebuf = NULL;
	}
	return 0;
}

static mowgli_patricia_t *create_users_patricia(struct a_network *net)
{
	return mowgli_patricia_create(
		irc_casemap_fn(irc_isupport_get_casemapping(net->nd.isupport))
		);
}

static void netdata_chan_add(struct a_network *net, const char *name)
{
	struct a_net_chan *chan;

	if (!net || !name || mowgli_patricia_retrieve(net->nd.chans, name))
		return;

	chan = mowgli_alloc(sizeof(*chan));
	chan->name = mowgli_strdup(name);
	chan->topic = mowgli_string_create();
	chan->users = create_users_patricia(net);

	mowgli_patricia_add(net->nd.chans, name, chan);
}

static void netdata_chan_del(struct a_network *net, const char *name)
{
	struct a_net_chan *chan;

	chan = netdata_chan(net, name);
	if (chan == NULL) {
		a_log(LERROR, "Channel %s does not exist!", name);
		return;
	}

	mowgli_patricia_delete(net->nd.chans, name);

	mowgli_string_destroy(chan->topic);
	mowgli_free(chan->name);
	mowgli_free(chan);
}

static struct a_net_chan *netdata_chan(struct a_network *net, const char *name)
{
	if (net == NULL || name == NULL)
		return;

	return mowgli_patricia_retrieve(net->nd.chans, name);
}

static void netdata_chan_topic(struct a_network *net, const char *name, const char *topic)
{
	struct a_net_chan *chan;

	chan = netdata_chan(net, name);
	if (chan == NULL) {
		a_log(LERROR, "Channel %s does not exist!", name);
		return;
	}

	mowgli_string_reset(chan->topic);
	if (topic != NULL)
		mowgli_string_append(chan->topic, topic, strlen(topic));
}

static struct a_net_user *net_user_get(struct a_network *net, const char *nick)
{
	struct a_net_user *nu;

	if ((nu = mowgli_patricia_fetch(net->nd.users, nick)) != NULL)
		return nu;

	nu = mowgli_alloc(sizeof(*nu));
	nu->nick = strdup(nick);
	nu->ident = NULL;
	nu->host = NULL;

	nu->visibility = 0;
	nu->weak_refs = 0;

	return nu;
}

static void a_network_resolve_cb(mowgli_dns_reply_t *reply, int reason, void *priv_arg)
{
	struct a_network *net = priv_arg;
	const char *reason_str;

	/* assume something will go wrong for now */
	net->state = NET_CANNOT_CONTINUE;

	if (reply == NULL) {
		switch (reason) {
		case MOWGLI_DNS_RES_NXDOMAIN:
			reason_str = "Nonexistent domain, cannot continue";
			break;
		case MOWGLI_DNS_RES_INVALID:
			reason_str = "Invalid domain, cannot continue";
			break;
		case MOWGLI_DNS_RES_TIMEOUT:
			reason_str = "Request timed out, trying again";
			break;
		}

		a_log(LERROR, "Resolution of %s (%s) failed: %s", net->host, net->name, reason_str);

		if (reason == MOWGLI_DNS_RES_TIMEOUT) {
			net->state = NET_DISCONNECTED;
			net_start_connect(net);
		}

		return;
	}

	net_connect(net, reply->h_name, &reply->addr);
}

struct a_network *a_network_by_name(struct a_account *acct, char *name)
{
	mowgli_patricia_t *networks;

	if (acct == NULL || name == NULL)
		return NULL;

	return mowgli_patricia_retrieve(acct->networks, name);
}

void a_network_disconnect(struct a_network *net)
{
	net_disconnect(net); /* hurr */
}

void a_network_printf(struct a_network *net, char *fmt, ...)
{
	char buf[65536];
	int n;
	va_list va;

	if (!net)
		return;

	va_start(va, fmt);
	n = vsnprintf(buf, 65536, fmt, va);
	va_end(va);

	mowgli_linebuf_write(net->linebuf, buf, n);
}


/* actions */

static void net_start_connect(struct a_network *net)
{
	struct sockaddr_in *addr4;
	struct sockaddr_in6 *addr6;
	mowgli_dns_query_t *query;
	mowgli_vio_sockaddr_t addr;

	if (net == NULL)
		return;
	if (net->state != NET_DISCONNECTED)
		return;
	if (net->host == NULL || net->port < 1 || net->port > 65535)
		return;

	query = &net->query;

	query->ptr = net;
	query->callback = a_network_resolve_cb;

	addr4 = (void*)&addr.addr;
	addr6 = (void*)&addr.addr;

	if (inet_pton(AF_INET, net->host, &addr4->sin_addr) > 0) {
		addr.addr.ss_family = AF_INET;
		net_connect(net, net->host, &addr);

	} else if (inet_pton(AF_INET6, net->host, &addr6->sin6_addr) > 0) {
		addr.addr.ss_family = AF_INET6;
		net_connect(net, net->host, &addr);

	} else {
		a_log(LDEBUG, "Resolving %s", net->host);
		net->state = NET_RESOLVING;
		mowgli_dns_gethost_byname(alicorn.dns, net->host, query, MOWGLI_DNS_T_A);
	}
}

static void net_do_register(struct a_network *net)
{
	if (net == NULL || net->state != NET_CONNECTED)
		return;

	a_log(LDEBUG, "Registering to %s", net->name);

	mowgli_hook_call("network.register", net);

	a_network_printf(net, "NICK %s", net->nick);
	a_network_printf(net, "USER %s * 8 :%s", net->ident, net->gecos);

	mowgli_strput(&net->nd.nick, net->nick);
	mowgli_strput(&net->nd.ident, net->ident);
	mowgli_strput(&net->nd.gecos, net->gecos);
}

/* db callbacks */

/* :<acct> NET <name> <address> [<nick> [<user> [:<gecos>]]] */
static int db_net(int parc, const char *parv[], void *priv)
{
	struct a_network *net;
	int line = (int)priv;

	if (parc < 3 || parc > 6) {
		a_log(LERROR, "Bad NET line on %d", line);
		return 0;
	}

	net = net_create(parv[1]);
	if (net == NULL) {
		a_log(LERROR, "Failed to allocate network!");
		return 0;
	}

	mowgli_strput(&net->nick, parv[0]);
	parse_server(net, parv[2]);
	if (parc > 3)
		mowgli_strput(&net->nick, parv[3]);
	if (parc > 4)
		mowgli_strput(&net->ident, parv[4]);
	if (parc > 5)
		mowgli_strput(&net->gecos, parv[5]);

	if (net_add_to_account(net, parv[0]) < 0) {
		a_log(LERROR, "Failed to add network %s to account %s", net->name, parv[0]);
		goto fail;
	}

	net_start_connect(net);

	return 0;

fail:
	net_destroy(net);
	return 0;
}

static void db_save_acct(void *acct_arg, void *priv)
{
	mowgli_patricia_iteration_state_t iter;
	struct a_account *acct = acct_arg;
	struct a_network *net;

	mowgli_patricia_foreach_start(acct->networks, &iter);
	while ((net = mowgli_patricia_foreach_cur(acct->networks, &iter)) != NULL) {
		a_db_printf(":%s NET %s %s/%s%d %s %s :%s", acct->name, net->name,
				net->host, net->use_ssl ? "+" : "", net->port,
				net->nick, net->ident, net->gecos);
		mowgli_patricia_foreach_next(acct->networks, &iter);
	}
}

/* misc callbacks */

static void readline_cb(mowgli_linebuf_t *linebuf, char *buf, size_t n, void *net_arg)
{
	struct a_network *net = net_arg;

	if (buf[n-1] == '\r')
		buf[--n] = '\0';

	if (linebuf->flags & MOWGLI_LINEBUF_LINE_HASNULLCHAR != 0)
		return; /* we don't put up with null bytes B) */
	if (n > 512)
		return; /* we're too cool for long lines B) */

	/*a_log(LDEBUG, "  %s <-- %s", net->name, buf);*/
	a_route(net, true, buf, n);
}

static void shutdown_cb(mowgli_linebuf_t *linebuf, void *net_arg)
{
	struct a_network *net = net_arg;
	net_disconnected(net);
}

static void net_disconnected(struct a_network *net)
{
	/* TODO: timed reconnect */
	a_log(LINFO, "Disconnected from %s, reconnecting...", net->name);
	net->state = NET_DISCONNECTED;
	net_start_connect(net);
}


/* internal hooks */

static void add_client(void *client, void *unused)
{
	struct a_client *cli = client;

	if (cli->net == NULL)
		return;

	mowgli_node_add(cli, mowgli_node_create(), cli->net->clients);
}

static void del_client(void *client, void *unused)
{
	struct a_client *cli = client;
	mowgli_node_t *n;

	if (cli->net == NULL)
		return;

	n = mowgli_node_find(cli, cli->net->clients);
	if (n == NULL)
		return;

	mowgli_node_delete(n, cli->net->clients);
}


/* IRC hooks */

static int on_PING(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	char *p;

	ctx->eat_message = true;

	p = strstr(ctx->msg_raw, "PING ");
	if (p == NULL) /* wat */
		return 0; /* ... I guess we just ping out, then ... */

	a_network_printf(ctx->net, "PONG %s", p + 5);

	return 0;
}

static int on_JOIN(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	int map = irc_isupport_get_casemapping(ctx->net->nd.isupport);

	if (parc < 4)
		return 0;

	if (irc_casecmp(map, parv[0], ctx->net->nd.nick) == 0)
		netdata_chan_add(ctx->net, parv[3]);
	return 0;
}

static int on_PART(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	int map = irc_isupport_get_casemapping(ctx->net->nd.isupport);

	if (parc < 4)
		return 0;

	if (irc_casecmp(map, parv[0], ctx->net->nd.nick) == 0)
		netdata_chan_del(ctx->net, parv[3]);
	return 0;
}

static int on_topic_update(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	const char *chan = NULL;
	const char *topic = NULL;

	if (!strcmp(ctx->msg.command, "TOPIC")) {
		if (parc < 5)
			return 0;
		chan = parv[3];
		if (strlen(parv[4]) != 0)
			topic = parv[4];
	} else if (!strcmp(ctx->msg.command, "332")) {
		if (parc < 6)
			return 0;
		chan = parv[4];
		topic = parv[5];
	} else if (!strcmp(ctx->msg.command, "331")) {
		if (parc < 6)
			return 0;
		chan = parv[4];
	}

	if (chan == NULL)
		return 0;

	a_log(LINFO, "%s topic %s%s", chan,
		topic == NULL ? "unset" : "changed to ",
		topic == NULL ? "" : topic);
	netdata_chan_topic(ctx->net, chan, topic);

	return 0;
}

static int on_RPL_WELCOME(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	a_log(LINFO, "Connected to %s!", ctx->net->name);

	mowgli_strput(&ctx->net->nd.nick, parv[3]);

	a_network_printf(ctx->net, "WHOIS %s", ctx->net->nick);

	irc_isupport_reset(ctx->net->nd.isupport);
}

static int on_RPL_ISUPPORT(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	irc_isupport_parse(ctx->net->nd.isupport, &ctx->msg);
}

static int on_RPL_MOTDSTART(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	ctx->net->nd.flags &= ~NET_MOTD_EMPTY;
	ctx->net->nd.flags &= ~NET_MOTD_LOCK;
	mowgli_string_reset(ctx->net->nd.motd);

	return 0;
}

static int on_RPL_MOTD(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	if (ctx->net->nd.flags & NET_MOTD_LOCK)
		return 0;

	mowgli_string_append(ctx->net->nd.motd, parv[parc-1], strlen(parv[parc-1]));
	mowgli_string_append_char(ctx->net->nd.motd, '\n');

	return 0;
}

static int on_RPL_ENDOFMOTD(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	ctx->net->nd.flags |= NET_MOTD_LOCK;

	return 0;
}

static int on_ERR_NOMOTD(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;

	ctx->net->nd.flags |= NET_MOTD_LOCK;
	ctx->net->nd.flags |= NET_MOTD_EMPTY;

	return 0;
}

static void on_RPL_NAMREPLY_cb(const char *key, void *data, void *priv)
{
	struct a_net_chanuser *cu = data;
	a_net_user_vanish(cu->u);
	mowgli_free(cu);
}

static int on_RPL_NAMREPLY(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	struct a_net_chan *nc;

	if (parc < 2)
		return 0;
	if (!(nc = netdata_chan(ctx->net, parv[0])))
		return 0;

	if (!(nc->flags & NET_CHAN_COLLECTING_NAMES)) {
		nc->flags |= NET_CHAN_COLLECTING_NAMES;
		mowgli_patricia_destroy(nc->users, on_RPL_NAMREPLY_cb, NULL);
		nc->users = create_users_patricia(ctx->net);
	}
}

static int on_RPL_ENDOFNAMES(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	struct a_net_chan *nc;

	if (parc < 2)
		return 0;
	if (!(nc = netdata_chan(ctx->net, parv[0])))
		return 0;

	nc->flags &= ~NET_CHAN_COLLECTING_NAMES;
}

static int on_RPL_WHOISUSER(int parc, const char *parv[], void *priv)
{
	struct a_route_ctx *ctx = priv;
	int map = irc_isupport_get_casemapping(ctx->net->nd.isupport);

	if (parc < 9)
		return 0;

	if (irc_casecmp(map, parv[4], ctx->net->nd.nick))
		return 0;

	mowgli_strput(&ctx->net->nd.ident, parv[5]);
	mowgli_strput(&ctx->net->nd.host, parv[6]);
	mowgli_strput(&ctx->net->nd.gecos, parv[8]);

	a_log(LINFO, "I am %s!%s@%s :%s", ctx->net->nd.nick, ctx->net->nd.ident,
			ctx->net->nd.host, ctx->net->nd.gecos);

	return 0;
}
