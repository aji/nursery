/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco

   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

/* BNC hooks */
static void bnc_HELP(struct a_user *user, void*, int parc, const char *parv[]);
static void bnc_DIE(struct a_user *user, void*, int parc, const char *parv[]);
static void bnc_SAVE(struct a_user *user, void*, int parc, const char *parv[]);

static struct a_command_spec cli_commands[] = {
        { "HELP", 0, bnc_HELP, a_command_help_file, "help/core/help", "HELP [<command>]",
          "View help for <command> or a list of commands" },
        { "DIE",  0, bnc_DIE,  a_command_help_file, "help/core/die",  "DIE",
          "Shut down Alicorn immediately" },
	{ "SAVE", 0, bnc_SAVE, a_command_help_file, "help/core/save", "SAVE [<dbfile>]",
	  "Save the database" },
        { 0 },
};

bool a_core_init(void)
{
	a_command_add_all(alicorn.cmds, cli_commands);
	return true;
}

void a_core_deinit(void)
{
	a_command_del_all(alicorn.cmds, cli_commands);
}

static void bnc_HELP(struct a_user *user, void *unused, int parc, const char *parv[])
{
        if (parc > 0) {
                a_command_help(user, alicorn.cmds, parv[0]);
        } else {
                a_commands_help(user, alicorn.cmds);
        }
}

static void bnc_DIE(struct a_user *user, void *unused, int parc, const char *parv[])
{
	a_user_out(user, "Goodbye");
        mowgli_eventloop_break(alicorn.eventloop);
}

static void bnc_SAVE(struct a_user *user, void *unused, int parc, const char *parv[])
{
	const char *file;

	file = (parc > 0 ? parv[0] : "var/alicorn.db");
	a_user_out(user, "Writing database to %s", file);

	a_db_save(file);
}
