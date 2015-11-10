/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

#include <stdio.h>

#include "shared.h"
#include "dump.h"
#include "type.h"
#include "error.h"
#include "y.tab.h"
#include "cg.h"

extern program_t *program;

int main(int argc, char *argv[])
{
	if (yyparse() != 0) {
		fprintf(stderr, "Errors detected. Exiting.\n");
		return 1;
	}

	prog_t *p = build_semantic_tree(program);

	if (p->errs->length != 0) {
		error_t *err;
		list_node_t *n;

		LIST_EACH(p->errs, n, err) {
			if (err->line_no) {
				fprintf(stderr, "ERROR:%d:%s\n", err->line_no,
				        err->val);
			} else {
				fprintf(stderr, "ERROR:%s\n", err->val);
			}
		}

		fprintf(stderr, "Errors detected. Exiting.\n");
		return 2;
	}

	allocate_symbols(p);
	mips_emit_program(p);
	fprintf(stderr, "\n");

	return 0;
}
