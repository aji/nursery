#ifndef __INC_LAMBDA_H__
#define __INC_LAMBDA_H__

#include <stdio.h>
#include <stdlib.h>

typedef struct expr expr_t;

struct expr {
	enum expr_type {
		EX_VARIABLE,
		EX_LAMBDA,
		EX_APPLY,

		EX_ALIAS,
	} type;

	union {
		int var;

		struct {
			int arg;
			expr_t *body;
		} fn;

		struct {
			expr_t *left;
			expr_t *right;
		} app;

		char *name;
	};
};

extern expr_t *ex_clone(expr_t *ex);
extern void ex_delete(expr_t *ex);

extern expr_t *ex_var(int var);
extern expr_t *ex_lambda(int arg, expr_t *body);
extern expr_t *ex_apply(expr_t *left, expr_t *right);
extern expr_t *ex_alias(char *name);

extern expr_t *(*ex_get_alias)(char *name);

/* lexer */

enum lx_token {
	LX_INVALID = 0,
	LX_EOF,
	LX_EOL,

	LX_LAMBDA,
	LX_VARIABLE,
	LX_DOT,
	LX_LPAREN,
	LX_RPAREN,

	LX_ALIAS,
};

extern const char *lx_names[];

extern FILE *lx_file;

extern enum lx_token lx_curtok;
extern char lx_tokstr[];

extern enum lx_token lx_token(void);
extern void lx_untoken(enum lx_token tok);

/* parser */

extern expr_t *p_expr(void);
extern expr_t *p_expr_line(void);

/* manipulation */

extern expr_t *alpha(expr_t *ex, int from, int to);
extern bool equiv(expr_t *e1, expr_t *e2);
extern expr_t *subst(expr_t *ex, int from, expr_t *to);
extern expr_t *reduce(expr_t *ex);

/* data structures */

extern expr_t *cn_encode(int n);
extern int cn_decode(expr_t *ex);

extern expr_t *cb_encode(int b);
extern int cb_decode(expr_t *ex);

#endif
