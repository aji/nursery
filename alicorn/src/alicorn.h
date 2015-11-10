/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#ifndef __INC_ALICORN_H__
#define __INC_ALICORN_H__

#include <mowgli.h>
#include <unicorn.h>

#include "autoconf.h"


#define DEFAULT_NICK "alicorn"
#define DEFAULT_IDENT "alicorn"
#define DEFAULT_GECOS PACKAGE_NAME " " PACKAGE_VERSION

#define ALICORN_HOST "alicorn.bnc"


/* user accounts */

struct a_account {
	char *name;
	char *pass;

	mowgli_list_t *clients;
	mowgli_patricia_t *networks;
	mowgli_patricia_t *attr;
};

extern bool a_account_init(void);
extern void a_account_deinit(void);

extern struct a_account *a_account_find(const char *name);
extern struct a_account *a_account_auth(const char *name, const char *pass);


/* users */

struct a_user;

struct a_user_ops {
	/* This function should be prepared to deal with IRC-style \02's
	   indicating bold text */
	void (*out)(struct a_user *user, char *fmt, ...);
};

struct a_user {
	/* this may be null. */
	struct a_account *acct;

	struct a_user_ops *ops;
	void *priv;
};

extern struct a_user *a_user_create(struct a_user_ops *ops, void *priv);
extern void a_user_destroy(struct a_user *user);

extern void a_user_set_account(struct a_user *user, struct a_account *acct);

#define a_user_out(user, f...) ((user)->ops->out((user), f))


/* networks */

#define NET_MOTD_EMPTY 0x00000001
#define NET_MOTD_LOCK  0x00000002

#define NET_CHAN_COLLECTING_NAMES 0x00000001

struct a_net_user {
	char *nick;
	char *ident;
	char *host;

	/* since we're just a user, we can't actually see all the users
	   all the time, so this number keeps track of how many channels we
	   can currently see the given user in. this is something like a
	   reference count, but there are potentially reasons we'd want to
	   keep a a_net_user around after they're no longer visible, which
	   is why there is a separate weak_refs count as well. the object
	   is only deleted when both of these counts drop to 0 */
	int visibility;
	int weak_refs;
};

extern void a_net_user_visible(struct a_net_user*);
extern void a_net_user_vanish(struct a_net_user*);
extern void a_net_user_weak_incref(struct a_net_user*);
extern void a_net_user_weak_decref(struct a_net_user*);

struct a_net_chanuser {
	struct a_net_user *u;
	struct a_net_chan *c;

	unsigned long status;
};

struct a_net_chan {
	char *name;
	mowgli_string_t *topic;
	mowgli_patricia_t *users;
	unsigned flags;
};

struct a_network {
	char *name;
	struct a_account *acct;

	char *host;
	int port;
	bool use_ssl;

	char *nick;
	char *ident;
	char *gecos;

	struct {
		unsigned long flags;
		char *nick;
		char *ident;
		char *host;
		char *gecos;
		irc_isupport_t *isupport;
		mowgli_string_t *motd;
		mowgli_patricia_t *chans;
		mowgli_patricia_t *users;
	} nd;

	enum {
		NET_DISCONNECTED,
		NET_RESOLVING,
		NET_CONNECTED,
		NET_CANNOT_CONTINUE,
	} state;
	mowgli_linebuf_t *linebuf;
	mowgli_dns_query_t query;
	mowgli_list_t *clients;
};

extern bool a_network_init(void);
extern void a_network_deinit(void);

extern struct a_network *a_network_by_name(struct a_account *acct, char *name);

extern void a_network_disconnect(struct a_network *net);
extern void a_network_printf(struct a_network *net, char *fmt, ...);


/* clients and listening */

struct a_client {
	struct a_user *user;
	struct a_network *net;

	char *acct_user;
	char *acct_net;
	char *acct_pass;

	enum {
		CLI_REGISTERING,
		CLI_CONNECTED,
	} state;
	mowgli_linebuf_t *linebuf;
};

extern bool a_client_init(void);
extern void a_client_deinit(void);

extern void a_client_printf(struct a_client *cli, char *fmt, ...);
extern void a_client_out(struct a_client *cli, char *fmt, ...);
extern void a_client_snotice(struct a_client *cli, char *fmt, ...);
extern void a_client_numeric(struct a_client *cli, char *num, char *fmt, ...);
extern void a_client_die(struct a_client *cli, char *fmt, ...);


/* commands */

struct a_command_spec {
	const char *name;
	int nargs;
	void (*cb)(struct a_user *user, void *ctx, int parc, const char *parv[]);
	void (*help)(struct a_user *user, void *priv);
	void *help_priv;
	const char *usage;
	const char *description;
};

extern struct a_commands *a_commands_create(void);
extern void a_commands_destroy(struct a_commands *cmds);

extern void a_command_add_all(struct a_commands *cmds, struct a_command_spec *cmd);
extern void a_command_del_all(struct a_commands *cmds, struct a_command_spec *cmd);

/* The parc and parv to this function should be just the command and
   its following arguments */
extern void a_command_dispatch(struct a_user *user, void *priv,
		struct a_commands *cmds, int parc, const char *parv[]);

extern void a_commands_help(struct a_user *user, struct a_commands *cmds);
extern void a_command_help(struct a_user *user, struct a_commands *cmds, const char *cmd);
extern void a_command_help_file(struct a_user *user, void *fname_arg);


/* routing */

struct a_route_ctx {
	/* the message currently being processed */
	char msg_raw[513];
	irc_message_t msg;

	/* the source of the message in question */
	union {
		void *source;
		struct a_network *net;
		struct a_client *cli;
	};

	/* do we stop the message here? defaults to false */
	bool eat_message;

};

extern void a_route(void *source, bool incoming, char *msg, size_t n);


/* modules */

struct a_module {
	char *name;

	mowgli_module_t syms;

	bool (*load)(void);
	void (*unload)(void);

	bool keep;
};

extern bool a_module_init(void);
extern void a_module_deinit(void);

extern struct a_module *a_module_load(char *name);
extern void a_module_unload(char *name);

extern void a_module_conf_start(void *unused, void *priv);
extern void a_module_conf_module(void *name, void *priv);
extern void a_module_conf_end(void *unused, void *priv);


/* configuration files */

extern int a_config_hash(const char *conf_fname);


/* database files */

extern void a_db_load(const char *db_fname);
extern void a_db_save(const char *db_fname);
extern void a_db_printf(const char *fmt, ...);


/* capabilities */

extern bool a_cap_init(void);
extern void a_cap_deinit(void);


/* core functionality */

extern bool a_core_init(void);
extern void a_core_deinit(void);


/* command line bouncer control */

extern bool a_cmdsock_init(void);
extern void a_cmdsock_deinit(void);
/* this function is used to send commands over a socket. parc and parv
   should represent only the command, as with a_command_dispatch */
extern int a_cmdsock_send(int parc, char *parv[]);


/* logger */

#define LDEBUG 0
#define LINFO 1
#define LNOTICE 2
#define LWARN 3
#define LERROR 4
#define LFATAL 5

extern bool a_log_init(void);

extern void a_log_set_echo(bool echo);
extern void a_log_set_level(unsigned level);
extern int a_log_open(const char *log_fname);
extern void a_log_close(void);
extern void a_log_write(const char *line);

#define a_log(args...) a_log_real(__FILE__, __LINE__, __func__, args)
extern void a_log_real(const char *file, int line, const char *func, unsigned level, const char *fmt, ...);


/* global program data */

struct alicorn {
	/* the main event loop */
	mowgli_eventloop_t *eventloop;
	/* resolver */
	mowgli_dns_t *dns;

	/* various hook tables */
	irc_hook_table_t *h_outgoing;
	irc_hook_table_t *h_incoming;
	irc_hook_table_t *h_db;

	/* global command list */
	struct a_commands *cmds;
};

extern struct alicorn alicorn;


#endif
