/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

#ifndef _ERROR_H_
#define _ERROR_H_

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "container.h"

#define ERRMAX 1024

#define ERR(...) \
	do { \
		fprintf(stderr, "err: %s: ", __func__); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
	} while(0)

#define WARN(...) \
	do { \
		fprintf(stderr, "warning: %s: ", __func__); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
	} while(0)

#define TRY_MALLOC(dest, size) \
do { \
	if( ((dest)=calloc(1, (size))) == NULL) \
		abort(); \
} while(0)

typedef struct {
	int line_no;
	char val[ERRMAX];
} error_t;

void assert_error(list_t *errs, int line_no, const char *format, ...);

extern void ice(const char *fmt, ...) __attribute__((noreturn));

#endif
