#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "lambda.h"
#include "alias.h"

static expr_t *new_ex(enum expr_type type)
{
	expr_t *ex = malloc(sizeof(expr_t));
	ex->type = type;
	return ex;
}

expr_t *ex_clone(expr_t *ex)
{
	if (!ex)
		return NULL;

	switch (ex->type) {
	case EX_VARIABLE:
		return ex_var(ex->var);

	case EX_LAMBDA:
		return ex_lambda(ex->fn.arg, ex_clone(ex->fn.body));

	case EX_APPLY:
		return ex_apply(ex_clone(ex->app.left),
		                ex_clone(ex->app.right));

	case EX_ALIAS:
		return ex_alias(ex->name);
	}

	return NULL;
}

void ex_delete(expr_t *ex)
{
	if (!ex)
		return;

	switch (ex->type) {
	case EX_VARIABLE:
		free(ex);
		break;

	case EX_LAMBDA:
		ex_delete(ex->fn.body);
		free(ex);
		break;

	case EX_APPLY:
		ex_delete(ex->app.left);
		ex_delete(ex->app.right);
		free(ex);
		break;

	case EX_ALIAS:
		free(ex->name);
		free(ex);
	}
}

expr_t *ex_var(int var)
{
	expr_t *ex = new_ex(EX_VARIABLE);
	ex->var = var;
	return ex;
}

expr_t *ex_lambda(int arg, expr_t *body)
{
	expr_t *ex = new_ex(EX_LAMBDA);
	ex->fn.arg = arg;
	ex->fn.body = body;
	return ex;
}

expr_t *ex_apply(expr_t *left, expr_t *right)
{
	expr_t *ex = new_ex(EX_APPLY);
	ex->app.left = left;
	ex->app.right = right;
	return ex;
}

expr_t *ex_alias(char *name)
{
	expr_t *ex = new_ex(EX_ALIAS);
	ex->name = strdup(name);
	return ex;
}

static expr_t *_ex_get_alias(char *name)
{
	return NULL;
}

expr_t *(*ex_get_alias)(char *name) = _ex_get_alias;

/* lexer */

const char *lx_names[] = {
	[LX_INVALID] = "invalid token",
	[LX_EOF] = "end of file",
	[LX_EOL] = "end of line",

	[LX_LAMBDA] = "lambda",
	[LX_VARIABLE] = "variable",
	[LX_DOT] = "dot",
	[LX_LPAREN] = "left paren",
	[LX_RPAREN] = "right paren",

	[LX_ALIAS] = "alias name",
};

FILE *lx_file;

enum lx_token lx_curtok;
char lx_tokstr[1024];

static int lx_getc(void)
{
	return getc(lx_file);
}

static void lx_ungetc(int c)
{
	ungetc(c, lx_file);
}

static int lx_next_char(void)
{
	int c;

	do {
		c = lx_getc();

		if (c == '#') {
			while (c != '\n' && c != EOF)
				c = lx_getc();
		}
	} while (isspace(c) && c != '\n');

	return c;
}

static inline int isalias(int c)
{
	return c != EOF && !(isspace(c) || strchr("\n\\.()", c));
}

static enum lx_token lx_next_token(void)
{
	char *s;
	int c, d;

	c = lx_next_char();

	lx_tokstr[0] = c;
	lx_tokstr[1] = '\0';

	switch (c) {
	case EOF: return LX_EOF;
	case '\n': return LX_EOL;
	case '\\': return LX_LAMBDA;
	case '.': return LX_DOT;
	case '(': return LX_LPAREN;
	case ')': return LX_RPAREN;
	case '-':
		d = lx_getc();
		if (d == '>')
			return LX_DOT;
		lx_ungetc(d);
		break;
	}

	if (islower(c))
		return LX_VARIABLE;

	if (isalias(c)) {
		s = lx_tokstr;
		do {
			*s++ = c;
			c = lx_getc();
		} while (isalias(c));
		lx_ungetc(c);
		*s = '\0';
		return LX_ALIAS;
	}

	return LX_INVALID;
}

static enum lx_token lx_nexttok = LX_INVALID;

enum lx_token lx_token(void)
{
	if (lx_nexttok != LX_INVALID) {
		lx_curtok = lx_nexttok;
		lx_nexttok = LX_INVALID;
	} else {
		lx_curtok = lx_next_token();
	}

	return lx_curtok;
}

static void lx_error(const char *msg)
{
	fprintf(stderr, "lexer error: %s\n", msg);
	exit(1);
}

void lx_untoken(enum lx_token tok)
{
	if (lx_nexttok != LX_INVALID) {
		lx_error("Tried to untoken more than once");
		return;
	}

	if (tok == LX_INVALID) {
		lx_error("Tried to untoken LX_INVALID");
		return;
	}

	lx_nexttok = tok;
}

/* parser */

static void p_error(const char *msg, ...)
{
	char buf[512];
	va_list va;
	va_start(va, msg);
	vsnprintf(buf, 512, msg, va);
	va_end(va);
	fprintf(stderr, "parser error: %s\n", buf);
	exit(2);
}

static void p_expect(enum lx_token te)
{
	if (lx_token() != te) {
		p_error("expected %s", lx_names[te]);
		return;
	}
}

static expr_t *do_lambdas(char *var, expr_t *body)
{
	if (*var == '\0')
		return body;

	return ex_lambda(*var, do_lambdas(var+1, body));
}

static expr_t *p_lambda(void)
{
	char *s, vars[300];
	enum lx_token t;
	expr_t *body;

	s = vars;
	for (;;) {
		t = lx_token();
		if (t != LX_VARIABLE) {
			lx_untoken(t);
			break;
		}
		*s++ = lx_tokstr[0];
	}
	*s = '\0';

	if (!vars[0]) {
		p_error("expected %s", lx_names[LX_VARIABLE]);
		return NULL;
	}

	p_expect(LX_DOT);

	if (!(body = p_expr())) {
		p_error("expected expression");
		return NULL;
	}

	return do_lambdas(vars, body);
}

expr_t *p_expr(void)
{
	expr_t *ex, *exes[300];
	size_t i, nex;
	enum lx_token t;

	nex = 0;
	exes[0] = NULL;

	for (;;) {
		t = lx_token();

		switch (t) {
		case LX_LPAREN:
			ex = p_expr();
			p_expect(LX_RPAREN);
			break;

		case LX_VARIABLE:
			ex = ex_var(lx_tokstr[0]);
			break;

		case LX_ALIAS:
			ex = ex_alias(lx_tokstr);
			break;

		case LX_LAMBDA:
			ex = p_lambda();
			break;

		default:
			lx_untoken(t);
			ex = NULL;
		}

		if (ex == NULL)
			break;

		exes[nex++] = ex;
	}

	ex = exes[0];
	for (i=1; i<nex; i++)
		ex = ex_apply(ex, exes[i]);

	return ex;
}

expr_t *p_expr_line(void)
{
	expr_t *ex;
	enum lx_token t;

	t = lx_token();
	if (t == LX_EOF)
		return NULL;
	lx_untoken(t);

	ex = p_expr();
	p_expect(LX_EOL);

	return ex;
}

/* manipulation */

static int alpha_pool = 256;
static int alpha_next(void)
{
	return alpha_pool++;
}

expr_t *alpha(expr_t *ex, int from, int to)
{
	expr_t *nex;

	if (!ex)
		return NULL;

	switch (ex->type) {
	case EX_VARIABLE:
		nex = ex_var(ex->var);
		if (nex->var == from)
			nex->var = to;
		return nex;

	case EX_APPLY:
		return ex_apply(alpha(ex->app.left,  from, to),
		                alpha(ex->app.right, from, to));

	case EX_ALIAS:
		return ex_clone(ex);

	case EX_LAMBDA:
		if (ex->fn.arg == from)
			return ex_clone(ex);

		return ex_lambda(ex->fn.arg,
		                 alpha(ex->fn.body, from, to));
	}

	return NULL;
}

bool equiv(expr_t *e1, expr_t *e2) /* some arbitrary equivalence? */
{
	bool eq;
	int to;
	expr_t *ne1, *ne2;

	if (!e1 || !e2)
		return !e1 && !e2;

	if (e1->type != e2->type)
		return false;

	switch (e1->type) {
	case EX_VARIABLE:
		return e1->var == e2->var;

	case EX_APPLY:
		return equiv(e1->app.left,  e2->app.left) &&
		       equiv(e1->app.right, e2->app.right);

	case EX_ALIAS:
		return !strcmp(e1->name, e2->name);

	case EX_LAMBDA:
		/* TODO: wrap x? */
		to = alpha_next();
		ne1 = alpha(e1->fn.body, e1->fn.arg, to);
		ne2 = alpha(e2->fn.body, e2->fn.arg, to);
		eq = equiv(ne1, ne2);
		ex_delete(ne1);
		ex_delete(ne2);
		return eq;
	}

	return false;
}

static bool expr_uses(expr_t *ex, int var)
{
	if (!ex)
		return 0;

	switch (ex->type) {
	case EX_VARIABLE:
		if (ex->var == var)
			return 1;
		return 0;

	case EX_ALIAS:
		return 0;

	case EX_APPLY:
		return expr_uses(ex->app.left,  var) ||
		       expr_uses(ex->app.right, var);

	case EX_LAMBDA:
		if (ex->fn.arg == var)
			return 0;

		return expr_uses(ex->fn.body, var);
	}

	return 0;
}

expr_t *subst(expr_t *ex, int from, expr_t *to)
{
	int ato;
	expr_t *nex, *mex;

	if (!ex)
		return NULL;

	switch (ex->type) {
	case EX_VARIABLE:
		if (ex->var == from)
			return ex_clone(to);
		return ex_clone(ex);

	case EX_ALIAS:
		return ex_clone(ex);

	case EX_APPLY:
		return ex_apply(subst(ex->app.left,  from, to),
		                subst(ex->app.right, from, to));

	case EX_LAMBDA:
		if (ex->fn.arg == from)
			return ex_clone(ex);

		if (expr_uses(to, ex->fn.arg)) {
			ato = alpha_next();
			nex = alpha(ex->fn.body, ex->fn.arg, ato);
		} else {
			ato = ex->fn.arg;
			nex = ex_clone(ex->fn.body);
		}

		mex = subst(nex, from, to);
		ex_delete(nex);
		return ex_lambda(ato, mex);
	}

	return NULL;
}

/* performs a single step of reductions */
expr_t *try_reduce(expr_t *ex, bool *reducible)
{
	expr_t *lex, *rex;
	bool lhs;

	if (!ex)
		return NULL;

	switch (ex->type) {
	case EX_VARIABLE:
		*reducible = false;
		return ex_clone(ex);

	case EX_LAMBDA:
		return ex_lambda(ex->fn.arg, try_reduce(ex->fn.body, reducible));

	case EX_ALIAS:
		lex = ex_get_alias(ex->name);
		*reducible = true;
		if (!lex) {
			lex = ex;
			*reducible = false;
		}
		return ex_clone(lex);

	case EX_APPLY:
		switch (ex->app.left->type) {
		case EX_VARIABLE:
		case EX_ALIAS:
		case EX_APPLY:
			lex = try_reduce(ex->app.left, &lhs);
			if (!lhs) {
				rex = try_reduce(ex->app.right, reducible);
			} else {
				*reducible = true;
				rex = ex_clone(ex->app.right);
			}
			return ex_apply(lex, rex);

		case EX_LAMBDA:
			break;
		}

		*reducible = true;
		return subst(ex->app.left->fn.body,
		             ex->app.left->fn.arg,
		             ex->app.right);
	}

	return NULL;
}

expr_t *reduce(expr_t *ex)
{
	bool x;
	return try_reduce(ex, &x);
}

/* data structures */

expr_t *cn_encode_body(int n, int s, int z)
{
	expr_t *ex = ex_var(z);

	while (n --> 0)
		ex = ex_apply(ex_var(s), ex);

	return ex;
}

expr_t *cn_encode(int n)
{
	return ex_lambda('s', ex_lambda('z', cn_encode_body(n, 's', 'z')));
}

int cn_decode(expr_t *ex)
{
	int n, s, z;

	if (ex->type != EX_LAMBDA ||
	    ex->fn.body->type != EX_LAMBDA)
		return -1;

	s = ex->fn.arg;
	z = ex->fn.body->fn.arg;
	n = 0;

	ex = ex->fn.body->fn.body;

	while (ex->type != EX_VARIABLE) {
		if (ex->type != EX_APPLY ||
		    ex->app.left->type != EX_VARIABLE ||
		    ex->app.left->var != s)
			return -1;
		ex = ex->app.right;
		n++;
	}

	if (ex->var != z)
		return -1;

	return n;
}

expr_t *cb_encode(int b)
{
	return ex_lambda('t', ex_lambda('f', ex_var(b ? 't' : 'f')));
}

int cb_decode(expr_t *ex)
{
	if (ex->type != EX_LAMBDA ||
	    ex->fn.body->type != EX_LAMBDA ||
	    ex->fn.body->fn.body->type != EX_VARIABLE)
		return -1;

	if (ex->fn.body->fn.body->var == ex->fn.arg)
		return 1;
	if (ex->fn.body->fn.body->var == ex->fn.body->fn.arg)
		return 0;

	return -1;
}
