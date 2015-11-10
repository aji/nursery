/*
   Alicorn -- Alex's IRC bouncer
   Copyright (C) 2012 Alex Iadicicco
  
   This file is part of Alicorn and is protected under the terms and
   conditions contained in the COPYING file in the project root
 */

#include <mowgli.h>
#include <unicorn.h>

#include "alicorn.h"

static FILE *log_file = NULL;
static unsigned log_level = LINFO;
static bool log_echo = true;

static char *log_timestamp(void)
{
	static char tsbuf[1024];
	time_t t;

	t = time(NULL);
	strftime(tsbuf, 1024, "%x %X", localtime(&t));

	return tsbuf;
}


bool a_log_init(void)
{
	/* do nothing */
	return true;
}

void a_log_set_echo(bool echo)
{
	log_echo = echo;
}

void a_log_set_level(unsigned level)
{
	log_level = level;
}

int a_log_open(const char *log_fname)
{
	if (log_file != NULL)
		fclose(log_file);

	log_file = fopen(log_fname, "a");

	if (log_file == NULL)
		return -1;

	fprintf(log_file, "\n[%s] Alicorn log opened\n", log_timestamp());
	return 0;
}

void a_log_close(void)
{
	if (log_file != NULL)
		fclose(log_file);
}


void a_log_write(const char *line)
{
	const char *nl;

	nl = "";
	if (line[strlen(line) - 1] != '\n')
		nl = "\n";

	/* ensure newline */
	if (log_file != NULL) {
		fprintf(log_file, "%s%s", line, nl);
		fflush(log_file);
	}

	if (log_echo)
		fprintf(stderr, "%s%s", line, nl);
}


void a_log_real(const char *file, int line, const char *func, unsigned level, const char *fmt, ...)
{
	char buf1[8192];
	char buf2[8192];
	va_list va;

	if (level < log_level)
		return;

	va_start(va, fmt);
	vsnprintf(buf1, 8192, fmt, va);
	va_end(va);

	snprintf(buf2, 8192, "[%s] %s:%d [%s] %s\n", log_timestamp(), file, line, func, buf1);

	a_log_write(buf2);
}

