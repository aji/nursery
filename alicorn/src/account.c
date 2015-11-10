/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

static mowgli_patricia_t *accounts;

static struct a_account *acct_find(const char *name);
static struct a_account *acct_add(const char *name, const char *pass);
static int acct_del(struct a_account *acct);

static int db_acct(int parc, const char *parv[], void *priv);
static void db_save(void *db, void *unused);

static void add_client(void *client, void *unused);
static void del_client(void *client, void *unused);

static void bnc_ACCT(struct a_user *user, void*, int parc, const char *parv[]);
static void bnc_ACCT_HELP(struct a_user *user, void*, int parc, const char *parv[]);
static void bnc_ACCT_ADD(struct a_user *user, void*, int parc, const char *parv[]);
static void bnc_ACCT_DEL(struct a_user *user, void*, int parc, const char *parv[]);
static void bnc_ACCT_SET(struct a_user *user, void*, int parc, const char *parv[]);
static void bnc_ACCT_LIST(struct a_user *user, void*, int parc, const char *parv[]);
static void bnc_ACCT_SET_PASSWORD(struct a_user *user, void*, int parc, const char *parv[]);

static struct a_command_spec acct_command_specs[] = {
	{ "ACCT", 0, bnc_ACCT, a_command_help_file, "help/acct/acct", "ACCT <command...>",
	  "Control user accounts" },
	{ 0 },
};
static struct a_command_spec acct_subcommand_specs[] = {
	{ "HELP", 0, bnc_ACCT_HELP, a_command_help_file, "help/acct/help", "ACCT HELP [<command>]",
	  "View help for <command>, or list commands" },
	{ "ADD",  2, bnc_ACCT_ADD,  a_command_help_file, "help/acct/add",  "ACCT ADD <name> <password>",
	  "Add a new user account" },
	{ "DEL",  1, bnc_ACCT_DEL,  a_command_help_file, "help/acct/del",  "ACCT DEL <name>",
	  "Delete a user account. The account must not be in use" },
	{ "SET",  0, bnc_ACCT_SET,  a_command_help_file, "help/acct/set",  "ACCT SET <name> <property...>",
	  "Change settings about a user account" },
	{ "LIST", 0, bnc_ACCT_LIST, NULL, NULL, "ACCT LIST",
	  "Lists accounts" },
	{ 0 },
};
static struct a_command_spec acct_set_subcommand_specs[] = {
	{ "PASSWORD", 1, bnc_ACCT_SET_PASSWORD, a_command_help_file, "help/acct/set_password",
	  "ACCT SET <name> PASSWORD <password>", "Change the password for an account" },
	{ 0 },
};

static struct a_commands *acct_subcommands;
static struct a_commands *acct_set_subcommands;

bool a_account_init(void)
{
	accounts = mowgli_patricia_create(NULL);

	irc_hook_add(alicorn.h_db, "ACCT", db_acct);
	mowgli_hook_associate("db.save", db_save, NULL);

	mowgli_hook_associate("client.add", add_client, NULL);
	mowgli_hook_associate("client.del", del_client, NULL);

	acct_subcommands = a_commands_create();
	acct_set_subcommands = a_commands_create();

	a_command_add_all(alicorn.cmds, acct_command_specs);
	a_command_add_all(acct_subcommands, acct_subcommand_specs);
	a_command_add_all(acct_set_subcommands, acct_set_subcommand_specs);

	return true;
}

void a_account_deinit_destroy_cb(const char *key, void *data, void *unused)
{
	/* TODO: destroy user objects */
}
void a_account_deinit(void)
{
	mowgli_patricia_destroy(accounts, a_account_deinit_destroy_cb, NULL);

	irc_hook_del(alicorn.h_db, "ACCT", db_acct);
	mowgli_hook_dissociate("db.save", db_save);

	mowgli_hook_dissociate("client.add", add_client);
	mowgli_hook_dissociate("client.del", del_client);

	a_command_del_all(alicorn.cmds, acct_command_specs);
	a_command_del_all(acct_subcommands, acct_subcommand_specs);
	a_command_del_all(acct_set_subcommands, acct_set_subcommand_specs);

	a_commands_destroy(acct_subcommands);
	a_commands_destroy(acct_set_subcommands);
}

static struct a_account *acct_find(const char *name)
{
	if (name == NULL)
		return NULL;

	if (accounts == NULL) {
		a_log(LERROR, "Attempt to find account before account subsystem initialized");
		return NULL;
	}

	return mowgli_patricia_retrieve(accounts, name);
}

static struct a_account *acct_add(const char *name, const char *password)
{
	struct a_account *acct;

	acct = acct_find(name);
	if (acct != NULL) {
		a_log(LINFO, "Refusing to duplicate account %s", name);
		return NULL;
	}

	acct = mowgli_alloc(sizeof(*acct));

	acct->name = mowgli_strdup(name);
	acct->pass = mowgli_strdup(password);
	acct->clients = mowgli_list_create();
	acct->networks = mowgli_patricia_create(NULL);
	acct->attr = mowgli_patricia_create(NULL);

	mowgli_patricia_add(accounts, name, acct);

	mowgli_hook_call("account.add", acct);

	return acct;
}

static int acct_del(struct a_account *acct)
{
	/* TODO: respond to this hook in other places */
	mowgli_hook_call("account.del", acct);

	mowgli_patricia_delete(accounts, acct->name);

	mowgli_free(acct->name);
	mowgli_free(acct->pass);
	mowgli_list_free(acct->clients);
	mowgli_patricia_destroy(acct->networks, NULL, NULL);
	mowgli_patricia_destroy(acct->attr, NULL, NULL);

	mowgli_free(acct);

	return 0;
}

/* ACCT <name> :<password> */
static int db_acct(int parc, const char *parv[], void *priv)
{
	struct a_account *acct;
	int line = (int)priv;

	if (parc != 3) {
		a_log(LERROR, "Bad ACCT line on %d", line);
		return 0;
	}

	acct = acct_add(parv[1], parv[2]);

	if (acct == NULL)
		a_log(LERROR, "Duplicate ACCT line on %d", line);

	return 0;
}

static void db_save(void *unused1, void *unused2)
{
	mowgli_patricia_iteration_state_t iter;
	struct a_account *cur;

	mowgli_patricia_foreach_start(accounts, &iter);
	while ((cur = mowgli_patricia_foreach_cur(accounts, &iter)) != NULL) {
		a_db_printf("ACCT %s :%s", cur->name, cur->pass);
		mowgli_hook_call("db.save.acct", cur);
		mowgli_patricia_foreach_next(accounts, &iter);
	}
}

static void add_client(void *client, void *priv)
{
	struct a_client *cli = client;

	if (cli->user->acct == NULL)
		return;

	mowgli_node_add(cli, mowgli_node_create(), cli->user->acct->clients);
}

static void del_client(void *client, void *priv)
{
	struct a_client *cli = client;
	mowgli_node_t *n;

	if (cli->user->acct == NULL)
		return;

	n = mowgli_node_find(cli, cli->user->acct->clients);
	if (n == NULL)
		return;

	mowgli_node_delete(n, cli->user->acct->clients);
}

struct a_account *a_account_find(const char *name)
{
	return acct_find(name);
}

struct a_account *a_account_auth(const char *name, const char *pass)
{
	struct a_account *acct;

	if (name == NULL || pass == NULL)
		return NULL;

	acct = a_account_find(name);
	if (acct == NULL)
		return NULL;

	/* hooray for plaintext auth! */
	if (!strcmp(acct->pass, pass))
		return acct;

	return NULL;
}

static void bnc_ACCT(struct a_user *user, void *unused, int parc, const char *parv[])
{
	if (parc < 1) {
		a_user_out(user, "No command given to ACCT. Try ACCT HELP");
		return;
	}

	a_command_dispatch(user, NULL, acct_subcommands, parc, parv);
}

static void bnc_ACCT_HELP(struct a_user *user, void *unused, int parc, const char *parv[])
{
	/* TODO: bounce SET help */
	if (parc > 0) {
		a_command_help(user, acct_subcommands, parv[0]);
	} else {
		a_commands_help(user, acct_subcommands);
	}
}

static void bnc_ACCT_ADD(struct a_user *user, void *unused, int parc, const char *parv[])
{
	struct a_account *acct;

	acct = acct_add(parv[0], parv[1]);

	if (acct == NULL) {
		a_user_out(user, "Failed to add \2%s\2. Does it already exist?", parv[0]);
	} else {
		a_user_out(user, "Added account \2%s\2.", parv[0]);
	}
}

static void bnc_ACCT_DEL(struct a_user *user, void *unused, int parc, const char *parv[])
{
	struct a_account *acct;
	const char *name = parv[0];

	acct = acct_find(name);

	if (acct == NULL) {
		a_user_out(user, "No such account \2%s\2.", name);
		return;
	}

	if (acct->clients->count != 0) {
		a_user_out(user, "Account \2%s\2 still in use.", name);
		return;
	}

	if (acct_del(acct) < 0) {
		a_user_out(user, "Failed to delete account \2%s\2.", name);
	} else {
		a_user_out(user, "Deleted account \2%s\2.", name);
	}
}

static void bnc_ACCT_SET(struct a_user *user, void *unused, int parc, const char *parv[])
{
	struct a_account *acct = user->acct;

	/* TODO: determine different between, for example, /bnc ACCT
	   SET PASSWORD and /bnc ACCT SET account PASSWORD */

	if (parc < 1) {
		a_user_out(user, "No account given to ACCT SET!");
		return;
	}

	if (parc < 2) {
		a_user_out(user, "No option given to ACCT SET!");
		return;
	}

	acct = a_account_find(parv[0]);

	if (acct == NULL) {
		a_user_out(user, "Account \2%s\2 does not exist", parv[0]);
		return;
	}

	a_command_dispatch(user, acct, acct_set_subcommands, parc-1, parv+1);
}

static void bnc_ACCT_LIST(struct a_user *user, void *unused, int parc, const char *parv[])
{
	mowgli_patricia_iteration_state_t iter;
	struct a_account *acct;
	int i = 1;

	a_user_out(user, "Account list:");

	mowgli_patricia_foreach_start(accounts, &iter);
	while ((acct = mowgli_patricia_foreach_cur(accounts, &iter)) != NULL) {
		a_user_out(user, "%3d. %s", i++, acct->name);
		mowgli_patricia_foreach_next(accounts, &iter);
	}

	a_user_out(user, "End of account list");
}

static void bnc_ACCT_SET_PASSWORD(struct a_user *user, void *acct_arg, int parc, const char *parv[])
{
	struct a_account *acct = acct_arg;
	a_user_out(user, "Pretending to set %s password to %s!", acct->name, parv[0]);
}
