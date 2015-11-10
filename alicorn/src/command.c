/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

struct command {
	char name[32];
	int nargs;
	void (*cb)(struct a_user *user, void *ctx, int parc, const char *parv[]);
	void (*help)(struct a_user *user, void *priv);
	void *help_priv;
	char usage[512];
	char description[512];
};

struct a_commands {
	mowgli_patricia_t *list;
};

static struct command *cmd_create(struct a_command_spec *spec);
static void cmd_destroy(struct command *cmd);
static void cmd_add(struct a_commands *cmds, const char *name, struct command *cmd);
static void cmd_del(struct a_commands *cmds, const char *name);
static struct command *cmd_find(struct a_commands *cmds, const char *name);


static void casemap(char *p)
{
	do { *p = toupper(*p); } while (*++p);
}

struct a_commands *a_commands_create(void)
{
	struct a_commands *cmds;

	cmds = mowgli_alloc(sizeof(*cmds));
	cmds->list = mowgli_patricia_create(casemap);

	return cmds;
}

static void a_commands_destroy_cb(const char *key, void *data, void *priv)
{
	struct command *cmd = data;
	cmd_destroy(cmd);
}
void a_commands_destroy(struct a_commands *cmds)
{
	mowgli_patricia_destroy(cmds->list, a_commands_destroy_cb, NULL);
	mowgli_free(cmds);
}

void a_command_add_all(struct a_commands *cmds, struct a_command_spec *spec)
{
	while (spec->name) {
		cmd_add(cmds, spec->name, cmd_create(spec));
		spec++;
	}
}

void a_command_del_all(struct a_commands *cmds, struct a_command_spec *spec)
{
	while (spec->name) {
		cmd_del(cmds, spec->name);
		spec++;
	}
}

void a_command_dispatch(struct a_user *user, void *ctx, struct a_commands *cmds, int parc, const char *parv[])
{
	struct command *cmd;
	int cmd_parc, i;
	const char **cmd_parv;
	char buf[512];
	char *p;

	if (parc < 1) {
		a_user_out(user, "No command given");
		return;
	}

	/* purely cosmetic casemap */
	snprintf(buf, 512, "%s", parv[0]);
	casemap(buf);

	cmd = cmd_find(cmds, buf);

	if (cmd == NULL) {
		a_user_out(user, "No such command \2%s\2", buf);
		return;
	}

	/* advance to command arguments */
	parc--; parv++;

	if (parc < cmd->nargs) {
		a_user_out(user, "Not enough arguments for \2%s\2. Usage: %s", buf, cmd->usage);
		return;
	}

	a_log(LDEBUG, "Command %s issued", buf);

	if (cmd->nargs < 1) {
		cmd->cb(user, NULL, parc, parv);
	} else {
		cmd_parc = cmd->nargs;
		cmd_parv = mowgli_alloc(sizeof(char*) * cmd_parc);

		for (i=0; i<cmd_parc-1; i++)
			cmd_parv[i] = parv[i];

		cmd_parv[i] = p = buf;
		for (; i<parc; i++) {
			/* I added irc_message_eol to libunicorn and
			   didn't end up using it, heh... */
			strcpy(p, parv[i]);
			p += strlen(p);
			if (i + 1 < parc)
				*p++ = ' ';
		}
		*p = '\0';

		cmd->cb(user, ctx, cmd_parc, cmd_parv);

		mowgli_free(cmd_parv);
	}

	return;
}

void a_commands_help(struct a_user *user, struct a_commands *cmds)
{
	mowgli_patricia_iteration_state_t iter;
	struct command *cmd;
	char *p;

	a_user_out(user, "***** \2Commands Help\2 *****");

	a_user_out(user, "The following commands are available.");
	a_user_out(user, "Try \2HELP <command>\2 for more information");

	mowgli_patricia_foreach_start(cmds->list, &iter);
	while ((cmd = mowgli_patricia_foreach_cur(cmds->list, &iter)) != NULL) {
		p = cmd->description ? cmd->description : (cmd->usage ? cmd->usage : "No help available");
		a_user_out(user, "\2%-13s\2 %s", cmd->name, p);
		mowgli_patricia_foreach_next(cmds->list, &iter);
	}

	a_user_out(user, "***** \2End of Help\2 *****");
}

void a_command_help(struct a_user *user, struct a_commands *cmds, const char *cmdname)
{
	struct command *cmd;
	char buf[512];

	/* purely cosmetic casemap */
	snprintf(buf, 512, "%s", cmdname);
	casemap(buf);

	cmd = cmd_find(cmds, cmdname);

	if (cmd == NULL) {
		a_user_out(user, "No such command \2%s\2", buf);
		return;
	}

	if (cmd->help) {
		cmd->help(user, cmd->help_priv);
	} else if (cmd->usage && cmd->description) {
		a_user_out(user, "\2%s\2: %s. Usage: %s", buf, cmd->description, cmd->usage);
	} else if (cmd->usage) {
		a_user_out(user, "\2%s\2 usage: %s", buf, cmd->usage);
	} else if (cmd->description) {
		a_user_out(user, "\2%s\2: %s", buf, cmd->description);
	} else {
		a_user_out(user, "No help for \2%s\2", buf);
	}
}

void a_command_help_file(struct a_user *user, void *fname_arg)
{
	/* TODO: caching? */
	const char *fname = fname_arg;
	FILE *f;
	char buf[1024];
	char *p;

	f = fopen(fname, "r");

	if (f == NULL) {
		a_user_out(user, "Could not open help file");
		return;
	}

	p = strrchr(fname, '/');
	if (p == NULL)
		p = (char*)fname;
	else
		p++;
	snprintf(buf, 1024, "%s", p);
	casemap(buf);

	a_user_out(user, "***** \2Help for %s\2 *****", buf);

	while (fgets(buf, 1024, f)) {
		p = buf + strlen(buf) - 1;
		if (*p == '\n')
			*p = '\0';

		a_user_out(user, "%s", buf);
	}

	a_user_out(user, "***** \2End of help\2 *****");

	fclose(f);
}

static struct command *cmd_create(struct a_command_spec *spec)
{
	struct command *cmd;

	cmd = mowgli_alloc(sizeof(*cmd));
	snprintf(cmd->name, 32, "%s", spec->name);
	cmd->nargs = spec->nargs;
	cmd->cb = spec->cb;
	cmd->help = spec->help;
	cmd->help_priv = spec->help_priv;
	snprintf(cmd->usage, 512, "%s", spec->usage);
	snprintf(cmd->description, 512, "%s", spec->description);

	return cmd;
}

static void cmd_destroy(struct command *cmd)
{
	if (cmd == NULL)
		return;
	mowgli_free(cmd);
}

static void cmd_add(struct a_commands *cmds, const char *name, struct command *cmd)
{
	struct command *old;

	cmd_destroy(mowgli_patricia_delete(cmds->list, name));
	mowgli_patricia_add(cmds->list, name, cmd);
}

static void cmd_del(struct a_commands *cmds, const char *name)
{
	cmd_destroy(mowgli_patricia_delete(cmds->list, name));
}

static struct command *cmd_find(struct a_commands *cmds, const char *name)
{
	return mowgli_patricia_retrieve(cmds->list, name);
}
