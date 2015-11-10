/* common things */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "common.h"

static const char *prefix[] =
	{ "-- ", "DEBUG: ", "INFO: ", "WARNING: ", "ERROR: ", "FATAL: " };

void Log(unsigned level, const char *fmt, ...) {
	char buf[512];
	va_list va;

	if (level > FATAL || level == DEFAULT)
		level = INFO;

	va_start(va, fmt);
	vsnprintf(buf, 512, fmt, va);
	va_end(va);

	fprintf(stderr, "%s%s\n", prefix[level], buf);
}

void Abort(const char *msg, ...) {
	char buf[512];
	va_list va;

	va_start(va, msg);
	vsnprintf(buf, 512, msg, va);
	va_end(va);

	Log(FATAL, "%s", buf);
	exit(1);
}
