/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco

   This file is part of Alicorn and is protected under the terms contained
   in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

static FILE *db_file = NULL;

void a_db_load(const char *db_fname)
{
	irc_message_t *msg;
	FILE *f = fopen(db_fname, "r");
	char buf[1024];
	char *p;
	int line;

	if (f == NULL) {
		a_log(LERROR, "Failed to open %s for read", db_fname);
		return;
	}

	msg = irc_message_create();
	line = 0;

	while (fgets(buf, 1024, f)) {
		p = buf + strlen(buf) - 1;
		if (*p == '\n')
			*p = '\0';
		line++;

		if (irc_message_parse(msg, buf) < 0) {
			a_log(LERROR, "Parse error on line %d", line);
		}

		a_log(LDEBUG, "  %s", msg->command);
		irc_hook_simple_dispatch(alicorn.h_db, msg, (void*)line);
	}

	a_log(LINFO, "Loaded database from %s", db_fname);

	irc_message_destroy(msg);
	fclose(f);
}

void a_db_save(const char *db_fname)
{
	char buf[1024];

	snprintf(buf, 1024, "%s.new", db_fname);
	db_file = fopen(buf, "w");

	if (db_file == NULL) {
		a_log(LERROR, "Failed to open %s for write", buf);
		return;
	}

	mowgli_hook_call("db.save", NULL);

	fclose(db_file);
	rename(buf, db_fname);
	db_file = NULL;

	a_log(LINFO, "Saved database to %s", db_fname);
}

void a_db_printf(const char *fmt, ...)
{
	va_list va;
	char buf[1024];

	if (db_file == NULL) {
		a_log(LERROR, "No database open for write!");
		return;
	}

	va_start(va, fmt);
	vsnprintf(buf, 1024, fmt, va);
	va_end(va);

	fprintf(db_file, "%s\n", buf);
}
