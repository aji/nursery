#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "lexer.h"
#include "parse.h"
#include "sem.h"

static void ice(const char *m, ...)
{
	char buf[65536];
	va_list va;
	va_start(va, m);
	vsnprintf(buf, 65536, m, va);
	va_end(va);
	fprintf(stderr, "internal compiler error: %s\n", buf);
	exit(5);
}

static void emit(const char *m, ...)
{
	char buf[65536];
	va_list va;
	va_start(va, m);
	vsnprintf(buf, 65536, m, va);
	va_end(va);
	printf("%s\n", buf);
}

static void emit_expr(struct s_expr *ex);

static void emit_apply_args(struct s_apply_args *app)
{
	if (app == NULL)
		return;

	emit_apply_args(app->next);

	emit_expr(app->ex);
	emit("\tpushl\t%%eax");
}

static void emit_apply(struct s_apply *app)
{
	emit_apply_args(app->args);

	switch (app->fn->type) {
	case S_EXPR_LABEL:
		emit("\tcall\t%s", app->fn->id);
		break;
	default:
		emit_expr(app->fn);
		emit("\tcall\t%%eax");
		break;
	}

	if (app->nargs > 0)
		emit("\taddl\t$%d, %%esp", app->nargs << 2);
}

static void emit_expr(struct s_expr *ex)
{
	static int L = 1;
	const char *cc = NULL;
	int no, out; /* for cond. */

	switch (ex->type) {
	case S_EXPR_IMMEDIATE:
		emit("\tmovl\t$%d, %%eax", ex->n);
		break;

	case S_EXPR_ARGUMENT:
		emit("\tmovl\t%d(%%ebp), %%eax", 8 + (ex->n << 2));
		break;

	case S_EXPR_LABEL:
		emit("\tmovl\t$%s, %%eax", ex->n);
		break;

	case S_EXPR_UNARY_OPERATION:
		emit_expr(ex->child[0]);
		switch (ex->op) {
		case T_LBRACK:
			emit("\tmovl\t0(%%eax), %%eax");
			break;
		case T_NEG:
			ice("T_NEG unimplemented");
			break;
		}
		break;

	case S_EXPR_BINARY_OPERATION:
		emit_expr(ex->child[0]);
		emit("\tpushl\t%%eax");
		emit_expr(ex->child[1]);
		emit("\tmovl\t%%eax, %%ebx");
		emit("\tpopl\t%%eax");

		switch (ex->op) {
		case T_ADD:
			emit("\taddl\t%%ebx, %%eax");
			break;
		case T_SUB:
			emit("\tsubl\t%%ebx, %%eax");
			break;
		case T_EQ: cc = cc ? cc : "z";
		case T_NE: cc = cc ? cc : "nz";
		case T_GT: cc = cc ? cc : "g";
		case T_LT: cc = cc ? cc : "l";
			emit("\tcmpl\t%%ebx, %%eax");
			emit("\tset%s\t%%al", cc);
			emit("\tmovzbl\t%%al, %%eax");
			break;
		case T_COMMA:
			emit("\tmovl\t%%ebx, %%eax");
			break;
		default:
			ice("`%s' unimplemented", lx_names[ex->op]);
			break;
		}
		break;

	case S_EXPR_CONDITIONAL:
		no = L++;
		out = L++;

		emit_expr(ex->child[0]);
		emit("\tcmpl\t$0, %%eax");
		emit("\tje\t.L%d", no);
		emit_expr(ex->child[1]);
		emit("\tjmp\t.L%d", out);
		emit(".L%d:", no);
		emit_expr(ex->child[2]);
		emit(".L%d:", out);
		break;

	case S_EXPR_APPLICATION:
		emit_apply(ex->app);
		break;

	default:
		ice("unexpected expression type");
	}
}

static void emit_fn_decl(struct s_fn_decl *fn)
{
	emit("\n\t.globl %s", fn->name);
	emit("%s:", fn->name);

	emit("\tpushl\t%%ebp");
	emit("\tmovl\t%%esp, %%ebp");
	emit("");

	emit_expr(fn->body);

	emit("");
	emit("\tmovl\t%%ebp, %%esp");
	emit("\tpopl\t%%ebp");
	emit("\tret");
}

static void emit_decls(struct s_decls *decl)
{
	emit("\t.text");
	for (; decl; decl = decl->next) switch (decl->type) {
	case T_FN:
		emit_fn_decl(decl->fn);
		break;

	default:
		ice("unexpected declaration type");
	}
}

static void emit_program(struct s_program *prog)
{
	emit_decls(prog->decls);
}

int main(int argc, char *argv[])
{
	struct p_program *p_prog;
	struct s_program *s_prog;

	lx_init();

	p_prog = p_program();
	s_prog = s_program(p_prog);

	emit_program(s_prog);
}
