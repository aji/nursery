/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

#include <stdio.h>
#include <stdarg.h>
#include "error.h"

void assert_error(list_t *errs, int line_no, const char *format, ...)
{
	error_t *e;
	va_list va;

	va_start(va, format);
	
	if((e = malloc(sizeof(*e))) == NULL) {
		ERR("error errored!");
		return;
	}

	e->line_no = line_no;
	
	if(vsnprintf(e->val, ERRMAX, format, va) < 0) {
		ERR("error errored!");
		return;
	}
	
	va_end(va);

	list_append(errs, e);
}

void ice(const char *fmt, ...)
{
	va_list va;
	char buf[65536];

	va_start(va, fmt);
	vsnprintf(buf, sizeof buf, fmt, va);
	va_end(va);

	fprintf(stderr, "internal compiler error: %s\n", buf);
	exit(EXIT_FAILURE);
}

