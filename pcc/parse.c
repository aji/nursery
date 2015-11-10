#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "lexer.h"
#include "parse.h"

#ifdef DEBUG
# define p_trace_in() fprintf(stderr, ">>> %s\n", __func__)
# define p_trace_out() fprintf(stderr, "<<< %s\n", __func__)
#else
# define p_trace_in() do{}while(0)
# define p_trace_out() do{}while(0)
#endif

static void p_expr_dump(struct p_expr *ex, int depth)
{
	int i, children = 0;
	struct p_ex_fn_args *arg;

	switch (ex->type) {
	case T_ID:
		fprintf(stderr, " %s", ex->id);
		return;
	case T_INT:
		fprintf(stderr, " %d", ex->num);
		return;

	case T_FN:
		i = 0;
		for (arg = ex->fn->args; arg; arg = arg->next) {
			p_expr_dump(arg->ex, depth+1);
			i++;
		}
		p_expr_dump(ex->fn->fn, depth+1);
		fprintf(stderr, " (%d)", i);
		return;

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
		fprintf(stderr, " `%s'?", lx_names[ex->type]);
		return;
	}

	for (i=0; i<children; i++)
		p_expr_dump(ex->child[i], depth + 1);
	fprintf(stderr, " %s", lx_names[ex->type]);
}

static void p_arg_list_dump(struct p_arg_list *args)
{
	for (; args != NULL; args = args->next)
		fprintf(stderr, " %s", args->id);
	putchar('\n');
}

static void p_fn_decl_dump(struct p_fn_decl *fn)
{
	fprintf(stderr, "fn decl: %s\n", fn->name);
	fprintf(stderr, "  args:");
	p_arg_list_dump(fn->args);
	fprintf(stderr, "  body:");
	p_expr_dump(fn->body, 0);
	fprintf(stderr, "\n");
}

static void p_decls_dump(struct p_decls *decls)
{
	for (; decls != NULL; decls = decls->next) switch (decls->type) {
	case T_FN:
		p_fn_decl_dump(decls->fn);
		break;
	default:
		fprintf(stderr, "invalid decl type `%s'\n", lx_names[decls->type]);
		break;
	}
}

void p_program_dump(struct p_program *prog)
{
	p_decls_dump(prog->decls);
}

void p_error(const char *m, ...)
{
	char buf[65536];
	va_list va;

	va_start(va, m);
	vsnprintf(buf, 65536, m, va);
	va_end(va);

	fprintf(stderr, "parse error: %s\n", buf);
	exit(1);
}

void expect(enum lx_token tok)
{
	enum lx_token t = lx_token();
	if (t != tok) {
		p_error("unexpected %s; expected %s",
		        lx_names[t], lx_names[tok]);
	}
}

struct p_expr *p_ex_p(void)
{
	enum lx_token t;
	struct p_expr *ex;

	ex = malloc(sizeof(*ex));

	p_trace_in();

	t = lx_token();
	switch (t) {
	case T_ID:
		ex->type = T_ID;
		ex->id = strdup(lx_tokstr);
		break;
	case T_INT:
		ex->type = T_INT;
		ex->num = atoi(lx_tokstr);
		break;
	case T_LPAR:
		free(ex);
		ex = p_expr();
		expect(T_RPAR);
		break;
	case T_LBRACK:
		ex->type = T_LBRACK;
		ex->child[0] = p_expr();
		expect(T_RBRACK);
		break;

	default:
		p_error("unexpected %s; expected %s, %s, %s, or %s",
			lx_names[t], lx_names[T_ID], lx_names[T_INT],
			lx_names[T_LPAR], lx_names[T_LBRACK]);
	}

	p_trace_out();

	return ex;
}

struct p_ex_fn_args *p_ex_fn_args(void)
{
	enum lx_token t;
	struct p_ex_fn_args *args;

	t = lx_token();

	switch (t) {
	case T_RPAR:
		args = NULL;
		break;

	default:
		lx_unget(t);
		args = malloc(sizeof(*args));
		args->ex = p_expr();
		args->next = p_ex_fn_args();
		break;
	}

	return args;
}

struct p_expr *p_ex_fn(void)
{
	enum lx_token t;
	struct p_expr *ex, *fnex;
	struct p_ex_fn *fn;

	ex = p_ex_p();

	t = lx_token();
	switch (t) {
	case T_LPAR:
		fn = malloc(sizeof(*fn));
		fn->fn = ex;
		fn->args = p_ex_fn_args();

		ex = malloc(sizeof(*fnex));
		ex->type = T_FN;
		ex->fn = fn;
		break;

	default:
		lx_unget(t);
	}

	return ex;
}

struct p_expr *p_ex_mul(void)
{
	enum lx_token t;
	struct p_expr *ex, *left;

	p_trace_in();

	left = p_ex_fn();

	t = lx_token();
	switch (t) {
	case T_MUL:
	case T_DIV:
	case T_MOD:
		ex = malloc(sizeof(*ex));
		ex->type = t;
		ex->child[0] = left;
		ex->child[1] = p_ex_mul();
		break;

	default:
		ex = left;
		lx_unget(t);
		break;
	}

	p_trace_out();

	return ex;
}

struct p_expr *p_ex_add(void)
{
	enum lx_token t;
	struct p_expr *ex, *left;

	p_trace_in();

	left = p_ex_mul();

	t = lx_token();
	switch (t) {
	case T_ADD:
	case T_SUB:
		ex = malloc(sizeof(*ex));
		ex->type = t;
		ex->child[0] = left;
		ex->child[1] = p_ex_add();
		break;

	default:
		ex = left;
		lx_unget(t);
		break;
	}

	p_trace_out();

	return ex;
}

struct p_expr *p_ex_bin(void)
{
	enum lx_token t;
	struct p_expr *ex, *left;

	p_trace_in();

	left = p_ex_add();

	t = lx_token();
	switch (t) {
	case T_AND:
	case T_OR:
	case T_XOR:
		ex = malloc(sizeof(*ex));
		ex->type = t;
		ex->child[0] = left;
		ex->child[1] = p_ex_bin();
		break;

	default:
		ex = left;
		lx_unget(t);
		break;
	}

	p_trace_out();

	return ex;
}

struct p_expr *p_ex_cmp(void)
{
	enum lx_token t;
	struct p_expr *ex, *left;

	p_trace_in();

	left = p_ex_bin();

	t = lx_token();
	switch (t) {
	case T_EQ:
	case T_NE:
	case T_GT:
	case T_LT:
		ex = malloc(sizeof(*ex));
		ex->type = t;
		ex->child[0] = left;
		/* we use p_ex_bin here, because the comparison operators
		   are not "associative" */
		ex->child[1] = p_ex_bin();
		break;

	default:
		ex = left;
		lx_unget(t);
		break;
	}

	p_trace_out();

	return ex;
}

struct p_expr *p_ex_tern(void)
{
	enum lx_token t;
	struct p_expr *ex, *left;

	p_trace_in();

	left = p_ex_cmp();

	t = lx_token();
	switch (t) {
	case T_QUES:
		ex = malloc(sizeof(*ex));
		ex->type = T_QUES;
		ex->child[0] = left;
		/* we use p_ex_cmp here because I haven't yet decided how
		   I want ?: to bind */
		ex->child[1] = p_ex_cmp();
		expect(T_COLON);
		ex->child[2] = p_ex_cmp();
		break;

	default:
		ex = left;
		lx_unget(t);
		break;
	}

	p_trace_out();

	return ex;
}

struct p_expr *p_ex_comma(void)
{
	enum lx_token t;
	struct p_expr *ex, *left;

	p_trace_in();

	left = p_ex_tern();

	t = lx_token();
	switch (t) {
	case T_COMMA:
		ex = malloc(sizeof(*ex));
		ex->type = T_COMMA;
		ex->child[0] = left;
		ex->child[1] = p_ex_comma();
		break;

	default:
		ex = left;
		lx_unget(t);
		break;
	}

	p_trace_out();

	return ex;
}

struct p_expr *p_expr(void)
{
	return p_ex_comma();
}

struct p_fn_decl *p_fn_decl(void)
{
	struct p_fn_decl *fn;

	fn = malloc(sizeof(*fn));

	expect(T_FN);
	p_fn_name(fn);
	p_fn_def(fn);

	return fn;
}

void p_fn_name(struct p_fn_decl *fn)
{
	static int n = 0;
	char buf[48];
	enum lx_token t;

	snprintf(buf, 48, "<fn-%d>", ++n);

	t = lx_token();
	fn->name = lx_tokstr;
	if (t != T_ID) {
		fn->name = buf;
		lx_unget(t);
	}

	fn->name = strdup(fn->name);
}

void p_fn_def(struct p_fn_decl *fn)
{
	expect(T_LPAR);
	fn->args = p_arg_list();
	expect(T_RPAR);
	fn->body = p_expr();
}

struct p_arg_list *p_arg_list(void)
{
	enum lx_token t;
	struct p_arg_list *args = NULL;

	t = lx_token();
	if (t == T_ID) {
		args = malloc(sizeof(*args));
		args->id = strdup(lx_tokstr);
		args->next = p_arg_list(); /* not ..._list2 */
	} else {
		lx_unget(t);
	}

	return args;
}

struct p_arg_list *p_arg_list2(void)
{
	enum lx_token t;
	struct p_arg_list *args = NULL;

	t = lx_token();
	if (t != T_COMMA) {
		lx_unget(t);
		return NULL;
	}

	expect(T_ID);
	args = malloc(sizeof(*args));
	args->id = strdup(lx_tokstr);
	args->next = p_arg_list2();

	return args;
}

struct p_decls *p_decls(void)
{
	struct p_decls *decls;
	enum lx_token t;

	decls = malloc(sizeof(*decls));

	t = lx_token();
	lx_unget(t);

	switch (t) {
	case T_FN:
		decls->type = T_FN;
		decls->fn = p_fn_decl();
		break;
	default:
		free(decls);
		return NULL;
	}

	/* we only make it this far if not default */
	decls->next = p_decls();
	return decls;
}

struct p_program *p_program(void)
{
	struct p_program *prog;
	prog = malloc(sizeof(*prog));
	prog->decls = p_decls();
	expect(T_EOF);
	return prog;
}
