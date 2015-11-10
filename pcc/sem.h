#ifndef __INC_SEM_H__
#define __INC_SEM_H__

/* performs semantic analysis of parse trees, producing a "graph"
   (not a tree, because a lot of structures are reused */

#include "lexer.h"
#include "parse.h"

/* we use lx_token in a few places to keep from re-defining a lot of
   the same thing, i.e. T_ADD, etc. */

struct s_expr;

struct s_apply_args {
	struct s_expr *ex;
	struct s_apply_args *next;
};

struct s_apply {
	struct s_expr *fn;
	int nargs;
	struct s_apply_args *args;
};

struct s_expr {
	enum s_expr_type {
		S_EXPR_IMMEDIATE,
		S_EXPR_ARGUMENT,
		S_EXPR_LABEL,
		S_EXPR_UNARY_OPERATION,
		S_EXPR_BINARY_OPERATION,
		S_EXPR_CONDITIONAL,
		S_EXPR_APPLICATION,
	} type;
	enum lx_token op;
	union {
		struct s_expr *child[3];
		struct s_apply *app;
		char *id;
		int n;
	};
};

struct s_fn_decl {
	char *name;
	struct s_expr *body;
};

struct s_decls {
	enum lx_token type;
	union {
		struct s_fn_decl *fn;
	};
	struct s_decls *next;
};

struct s_program {
	struct s_decls *decls;
};

extern struct s_program *s_program(struct p_program *prog);

#endif
