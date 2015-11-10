#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "symtab.h"
#include "lexer.h"
#include "parse.h"
#include "sem.h"

static void vdie(const char *n, const char *m, va_list va)
{
	char buf[65536];
	vsnprintf(buf, 65536, m, va);
	fprintf(stderr, "%s: %s\n", n, buf);
}

static void ice(const char *m, ...)
{
	va_list va;
	va_start(va, m);
	vdie("internal complier error", m, va);
	va_end(va);
	exit(3);
}

static void err(const char *m, ...)
{
	va_list va;
	va_start(va, m);
	vdie("error", m, va);
	va_end(va);
	exit(4);
}

static struct s_expr *s_expr(struct symtab *tab, struct p_expr *p);

static struct s_apply_args *s_apply_args(struct symtab *tab,
                                         struct p_ex_fn_args *p, int *nargs)
{
	struct s_apply_args *s;

	if (p == NULL)
		return NULL;

	(*nargs)++;

	s = malloc(sizeof(*s));
	s->ex = s_expr(tab, p->ex);
	s->next = s_apply_args(tab, p->next, nargs);

	return s;
}

static struct s_apply *s_apply(struct symtab *tab, struct p_ex_fn *fn)
{
	struct s_apply *app;

	app = malloc(sizeof(*app));
	app->fn = s_expr(tab, fn->fn);
	app->nargs = 0;
	app->args = s_apply_args(tab, fn->args, &app->nargs);

	return app;
}

static struct s_expr *s_expr(struct symtab *tab, struct p_expr *p)
{
	struct s_expr *ex, *s;
	int i, children;

	ex = malloc(sizeof(*ex));
	children = 0;

	switch (p->type) {
	case T_ID:
		s = symtab_get(tab, p->id);
		if (s == NULL) {
			ex->type = S_EXPR_LABEL;
			ex->id = p->id;
		} else {
			free(ex);
			ex = s;
		}
		break;

	case T_INT:
		ex->type = S_EXPR_IMMEDIATE;
		ex->n = p->num;
		break;

	case T_FN:
		ex->type = S_EXPR_APPLICATION;
		ex->app = s_apply(tab, p->fn);
		break;

	case T_LBRACK:
	case T_NEG:
		children = 1;
		break;

	case T_ADD:
	case T_SUB:
	case T_MUL:
	case T_DIV:
	case T_MOD:
	case T_AND:
	case T_OR:
	case T_XOR:
	case T_EQ:
	case T_NE:
	case T_GT:
	case T_LT:
	case T_COMMA:
		children = 2;
		break;

	case T_QUES:
		children = 3;
		break;

	default:
		ice("unknown expression type `%s'", lx_names[p->type]);
	}

	if (children > 0) {
		ex->op = p->type;
		switch (children) {
		case 1: ex->type = S_EXPR_UNARY_OPERATION; break;
		case 2: ex->type = S_EXPR_BINARY_OPERATION; break;
		case 3: ex->type = S_EXPR_CONDITIONAL; break;
		default: ice("can't expression with %d children", children);
		}
		for (i=0; i<children; i++)
			ex->child[i] = s_expr(tab, p->child[i]);
	}

	return ex;
}

static void s_arg_list(struct symtab *tab, char *fn, struct p_arg_list *args)
{
	struct s_expr *ex;
	int n = 0;

	for (; args; args = args->next) {
		if (symtab_get(tab, args->id) != NULL) {
			error("%s declared twice in %s argument list",
			      args->id, fn);
		}

		ex = malloc(sizeof(*ex));
		ex->type = S_EXPR_ARGUMENT;
		ex->n = n++;
		symtab_set(tab, args->id, ex);
	}
}

static struct s_fn_decl *s_fn_decl(struct p_fn_decl *p)
{
	struct s_fn_decl *s;
	struct symtab *tab;

	tab = symtab_new(NULL);

	s = malloc(sizeof(*s));
	s->name = p->name;
	s_arg_list(tab, s->name, p->args);
	s->body = s_expr(tab, p->body);

	symtab_free(tab);

	return s;
}

static struct s_decls *s_decls(struct p_decls *p)
{
	struct s_decls *s;

	if (p == NULL)
		return NULL;

	s = malloc(sizeof(*s));

	switch (p->type) {
	case T_FN:
		s->type = T_FN;
		s->fn = s_fn_decl(p->fn);
		break;

	default:
		ice("unknown declaration type `%s'", lx_names[p->type]);
	}

	s->next = s_decls(p->next);

	return s;
}

struct s_program *s_program(struct p_program *p)
{
	struct s_program *s;
	s = malloc(sizeof(*s));
	s->decls = s_decls(p->decls);
	return s;
}
