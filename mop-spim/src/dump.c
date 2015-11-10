/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

#include <stdio.h>
#include <string.h>

#include "shared.h"
#include "container.h"
#include "dump.h"

#include "y.tab.h"

#define dump(CTX, X...) fprintf(stderr, X)

static void indent(dump_context_t *ctx)
{
	dump(ctx, "%*s", ctx->level * 4, "");
}

void dump_id_list(dump_context_t *ctx, list_t *ids)
{
	list_node_t *cur;
	identifier_t *id;

	LIST_EACH(ids, cur, id) {
		dump(ctx, "%s", id->id);

		if (!list_is_last(ids, cur))
			dump(ctx, ", ");
	}
}

void dump_type_denoter(dump_context_t *ctx, type_denoter_t *td)
{
	if (td->is_array) {
		dump_array_type(ctx, td->a);
	} else {
		dump(ctx, "%s", td->id);
	}
}

void dump_function_designator(dump_context_t *ctx, function_designator_t *fd)
{
	list_node_t *cur;
	actual_parameter_t *ap;

	dump(ctx, "%s(", fd->id);

	LIST_EACH(fd->params, cur, ap) {
		dump_actual_parameter(ctx, ap);
		if (!list_is_last(fd->params, cur))
			dump(ctx, ", ");
	}

	dump(ctx, ")");
}

void dump_actual_parameter(dump_context_t *ctx, actual_parameter_t *ap)
{
	dump_expression(ctx, ap->exp1);

	if (ap->exp2 != NULL) {
		dump(ctx, " : ");
		dump_expression(ctx, ap->exp2);

		if (ap->exp3 != NULL) {
			dump(ctx, " : ");
			dump_expression(ctx, ap->exp3);
		}
	}
}

void dump_variable_declaration(dump_context_t *ctx, variable_declaration_t *vd)
{
	dump_type_denoter(ctx, vd->type);
	dump(ctx, " ");
	dump_id_list(ctx, vd->id_list);
}

void dump_formal_parameter_section(
	dump_context_t *ctx,
	formal_parameter_section_t *fps
) {
	list_node_t *cur;
	char *id;

	LIST_EACH(fps->ids, cur, id) {
		dump(ctx, "%s%s %s", fps->id, fps->is_var ? "&" : "", id);

		if (!list_is_last(fps->ids, cur))
			dump(ctx, ", ");
	}
}

void dump_variable_access(dump_context_t *ctx, variable_access_t *va)
{
	switch (va->tag) {
	case VA_IDENTIFIER:
		dump(ctx, "%s", va->id);
		break;

	case VA_INDEXED:
		dump_indexed_variable(ctx, va->iv);
		break;

	case VA_ATTRIBUTE:
		dump_attribute_designator(ctx, va->ad);
		break;

	case VA_METHOD:
		dump_method_designator(ctx, va->md);
		break;
	}
}

void dump_assignment_statement(dump_context_t *ctx, assignment_statement_t *as)
{
	dump_variable_access(ctx, as->va);
	dump(ctx, " = ");

	if (as->is_new) {
		dump_object_instantiation(ctx, as->oi);
	} else {
		dump_expression(ctx, as->e);
	}
}

void dump_object_instantiation(dump_context_t *ctx, object_instantiation_t *oi)
{
	dump(ctx, "new %s(", oi->id);

	if (oi->has_params) {
		list_node_t *cur;
		actual_parameter_t *ap;

		LIST_EACH(oi->params, cur, ap) {
			dump_actual_parameter(ctx, ap);
			if (!list_is_last(oi->params, cur))
				dump(ctx, ", ");
		}
	}

	dump(ctx, ")");
}

void dump_expression(dump_context_t *ctx, expression_t *e)
{
	dump_simple_expression(ctx, e->e1);

	if (!e->is_one) {
		switch (e->relop) {
		case EQUAL:    dump(ctx, " == "); break;
		case NOTEQUAL: dump(ctx, " != "); break;
		case LT:       dump(ctx, " < ");  break;
		case GT:       dump(ctx, " > ");  break;
		case LE:       dump(ctx, " <= "); break;
		case GE:       dump(ctx, " >= "); break;
		default:       dump(ctx, " <#%d> ", e->relop);
		}

		dump_simple_expression(ctx, e->e2);
	}
}

void dump_statement(dump_context_t *ctx, statement_t *s)
{
	list_node_t *cur;
	statement_t *ss;

	switch (s->tag) {
	case ST_ASSIGNMENT:
		indent(ctx);
		dump_assignment_statement(ctx, s->as);
		dump(ctx, "\n");
		break;

	case ST_COMPOUND:
		LIST_EACH(s->cs, cur, ss)
			dump_statement(ctx, ss);
		if (s->cs->length == 1)
			dump(ctx, "(empty)");
		break;

	case ST_IF:
		dump_if_statement(ctx, s->is);
		break;

	case ST_WHILE:
		dump_while_statement(ctx, s->ws);
		break;

	case ST_PRINT:
		indent(ctx);
		dump(ctx, "print ");
		dump_variable_access(ctx, s->va);
		dump(ctx, "\n");
		break;
	}
}

void dump_if_statement(dump_context_t *ctx, if_statement_t *is)
{
	indent(ctx);
	dump(ctx, "if (");
	dump_expression(ctx, is->condition);
	dump(ctx, "):\n");

	ctx->level += 1;
	dump_statement(ctx, is->tb);
	ctx->level -= 1;

	indent(ctx);
	dump(ctx, "else:\n");

	ctx->level += 1;
	dump_statement(ctx, is->eb);
	ctx->level -= 1;
}

void dump_while_statement(dump_context_t *ctx, while_statement_t *ws)
{
	indent(ctx);

	dump(ctx, "while (");
	dump_expression(ctx, ws->condition);
	dump(ctx, "):\n");

	ctx->level += 1;
	dump_statement(ctx, ws->st);
	ctx->level -= 1;
}

void dump_indexed_variable(dump_context_t *ctx, indexed_variable_t *iv)
{
	list_node_t *cur;
	expression_t *e;

	dump_variable_access(ctx, iv->va);

	dump(ctx, "[");
	LIST_EACH(iv->idxs, cur, e) {
		dump_expression(ctx, e);
		if (!list_is_last(iv->idxs, cur))
			dump(ctx, ", ");
	}
	dump(ctx, "]");
}

void dump_attribute_designator(dump_context_t *ctx, attribute_designator_t *ad)
{
	dump_variable_access(ctx, ad->va);
	dump(ctx, ".%s", ad->id);
}

void dump_method_designator(dump_context_t *ctx, method_designator_t *md)
{
	dump_variable_access(ctx, md->va);
	dump(ctx, ".");
	dump_function_designator(ctx, md->fd);
}

void dump_simple_expression(dump_context_t *ctx, simple_expression_t *se)
{
	if (se->is_term) {
		dump_term(ctx, se->t);
	} else {
		dump_simple_expression(ctx, se->se);
		switch (se->addop) {
		case PLUS:   dump(ctx, " + "); break;
		case MINUS:  dump(ctx, " - "); break;
		case OR:     dump(ctx, " | "); break;
		default:     dump(ctx, " <#%d> ", se->addop);
		}
		dump_term(ctx, se->t);
	}
}

void dump_term(dump_context_t *ctx, term_t *t)
{
	if (t->is_factor) {
		dump_factor(ctx, t->f);
	} else {
		dump_term(ctx, t->t);
		switch (t->mulop) {
		case STAR:   dump(ctx, " * "); break;
		case SLASH:  dump(ctx, " / "); break;
		case MOD:    dump(ctx, " %% "); break;
		case AND:    dump(ctx, " & "); break;
		default:     dump(ctx, " <#%d> ", t->mulop);
		}
		dump_factor(ctx, t->f);
	}
}

void dump_factor(dump_context_t *ctx, factor_t *f)
{
	if (f->is_sign) {
		switch (f->sign) {
		case PLUS:   dump(ctx, "+"); break;
		case MINUS:  dump(ctx, "-"); break;
		default:     dump(ctx, "#%d", f->sign);
		}
		dump_factor(ctx, f->f);
	} else {
		dump_primary(ctx, f->p);
	}
}

void dump_primary(dump_context_t *ctx, primary_t *p)
{
	switch (p->tag) {
	case PR_VARIABLE_ACCESS:
		dump_variable_access(ctx, p->va);
		break;

	case PR_UNSIGNED_CONSTANT:
		dump(ctx, "%d", p->uc);
		break;

	case PR_FUNCTION_DESIGNATOR:
		dump_function_designator(ctx, p->fd);
		break;

	case PR_EXPRESSION:
		dump(ctx, "(");
		dump_expression(ctx, p->e);
		dump(ctx, ")");
		break;

	case PR_NOT_PRIMARY:
		dump(ctx, "!");
		dump_primary(ctx, p->p);
		break;
	}
}

void dump_array_type(dump_context_t *ctx, array_type_t *at)
{
	dump(ctx, "[%d..%d]", at->r->low, at->r->high);
	dump_type_denoter(ctx, at->td);
}

void dump_class_block(dump_context_t *ctx, class_block_t *cb)
{
	list_node_t *cur;

	variable_declaration_t *vd;
	function_declaration_t *fd;

	LIST_EACH(cb->vd, cur, vd) {
		indent(ctx);
		dump_variable_declaration(ctx, vd);
		dump(ctx, "\n");
	}

	LIST_EACH(cb->fd, cur, fd)
		dump_function_declaration(ctx, fd);
}

void dump_function_declaration(dump_context_t *ctx, function_declaration_t *fd)
{
	indent(ctx);
	if (fd->is_heading) {
		dump_function_heading(ctx, fd->fh);
	} else {
		dump("proc %s", fd->id);
	}
	dump(ctx, " {\n");

	ctx->level += 1;
	dump_function_block(ctx, fd->fb);
	ctx->level -= 1;

	indent(ctx);
	dump(ctx, "}\n");
}

void dump_function_block(dump_context_t *ctx, function_block_t *fb)
{
	list_node_t *cur;

	variable_declaration_t *vd;
	statement_t *s;

	LIST_EACH(fb->vp, cur, vd) {
		indent(ctx);
		dump_variable_declaration(ctx, vd);
		dump(ctx, "\n");
	}

	LIST_EACH(fb->sp->cs, cur, s)
		dump_statement(ctx, s);
}

void dump_function_heading(dump_context_t *ctx, function_heading_t *fh)
{
	dump(ctx, "fn %s(", fh->id);

	if (fh->has_fpl) {
		list_node_t *cur;
		formal_parameter_section_t *fps;

		LIST_EACH(fh->fpl, cur, fps) {
			dump_formal_parameter_section(ctx, fps);
			if (!list_is_last(fh->fpl, cur))
				dump(ctx, ", ");
		}
	}

	dump(ctx, ") -> %s", fh->result_type);
}

void dump_class_definition(dump_context_t *ctx, class_definition_t *cd)
{
	indent(ctx);

	dump_class_identification(ctx, cd->cid);
	dump(ctx, " {\n");

	ctx->level += 1;
	dump_class_block(ctx, cd->body);
	ctx->level -= 1;

	dump(ctx, "}\n");
}

void dump_class_identification(dump_context_t *ctx, class_identification_t *ci)
{
	if (ci->has_extends) {
		dump(ctx, "class(%s, extends %s)", ci->id, ci->extends->id);
	} else {
		dump(ctx, "class(%s)", ci->id);
	}
}

void dump_program(dump_context_t *ctx, program_t *p)
{
	dump_context_t default_context = { .level = 0 };

	if (ctx == NULL)
		ctx = &default_context;

	dump_program_heading(ctx, p->ph);
	dump(ctx, "\n");

	list_node_t *cur;
	class_definition_t *cd;

	LIST_EACH(p->classes, cur, cd)
		dump_class_definition(ctx, cd);
}

void dump_program_heading(dump_context_t *ctx, program_heading_t *ph)
{
	if (ph->has_id_list) {
		dump(ctx, "program(%s, ", ph->id);
		dump_id_list(ctx, ph->id_list);
		dump(ctx, ")");
	} else {
		dump(ctx, "program(%s)", ph->id);
	}
}
