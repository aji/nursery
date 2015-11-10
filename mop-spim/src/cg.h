/*
 * CSE 440, Project 3
 * Mini Object Pascal Code Generation
 *
 * Alex Iadicicco
 * shmibs
 */

/* common code generation definitions */

#ifndef __INC_CG_H__
#define __INC_CG_H__

#include "container.h"
#include "cfg.h"

/* these are #defines so that they're easy to grep for in case we need to
   change them for some reason */
#define BYTES_IN_INTEGER 4
#define BYTES_IN_POINTER 4
#define INTEGERS_IN_POINTER 1

/* == cg_common.c == */

/* gets the 'field' size of a type, meaning the size of a function local
   or class variable that holds things with the given type. this is just
   to treat classes differently! */
extern unsigned get_field_size(type_t *t);

/* gets the size of a type */
extern unsigned get_size(type_t *t);

/* allocates space for every symbol in the program, using fields in the
   semantic tree itself */
extern void allocate_symbols(prog_t *p);

/* == cg_mips.c == */

extern void mips_emit_program(prog_t *p);

#endif
