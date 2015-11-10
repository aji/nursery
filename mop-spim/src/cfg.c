/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "error.h"
#include "cfg.h"

static bb_node_t *bb_dummy_new(cfg_context_t *ctx)
{
	bb_node_t *bb = calloc(1, sizeof(*bb));

	bb->is_dummy = true;
	bb->has_condition = false;

	bb->n = list_append(ctx->all_bb, bb);

	return bb;
}

static bb_node_t *bb_new(cfg_context_t *ctx)
{
	bb_node_t *bb = calloc(1, sizeof(*bb));

	bb->is_dummy = false;
	bb->has_condition = false;

	bb->n = list_append(ctx->all_bb, bb);

	return bb;
}

static void promote_bb(bb_node_t *bb)
{
	bb->is_dummy = false;

	if (bb->instructions == NULL)
		bb->instructions = list_new();
}

static inst_t *inst_new(void)
{
	return calloc(1, sizeof(inst_t));
}

static bool is_simple_identifier(expr_t *ex)
{
	if (ex->op != OP_SYMBOL)
		return false;

	/* reference parameters and class fields need explicit memory access */
	return ex->sym->tag != SYM_REF_PARAMETER &&
	       ex->sym->tag != SYM_FIELD;
}

static char *get_identifier(expr_t *ex)
{
	if (ex->op == OP_SYMBOL)
		return ex->sym->name;

	return "(nil)";
}

static char *get_next_temp_id(void)
{
	static int n = 0;
	char buf[32];

	snprintf(buf, 32, "_t%d", n++);
	return strdup(buf);
}

/* this one strings together all consecutive
 * assign statements, expanding them with expr
 * to cfg, and then calls either if_to_cfg 
 * or while_to_cfg and sets the result as the
 * block's *fb on returning */
static bb_node_t* stmt_to_cfg(stmt_t*, cfg_context_t*, bb_node_t *entry);

/* these build a chunk of the CFG from the
 * input statement. they return their exit
 * basic block and append themselves to the
 * provided entry node basic block */
static bb_node_t* if_to_cfg(stmt_t*, cfg_context_t*, bb_node_t *entry);
static bb_node_t* while_to_cfg(stmt_t*, cfg_context_t*, bb_node_t *entry);
static bb_node_t* assign_to_cfg(stmt_t*, cfg_context_t*, bb_node_t *entry);
static bb_node_t* print_to_cfg(stmt_t*, cfg_context_t*, bb_node_t *entry);

/* some helpers */
static bool result_is_reference(inst_t *i);
static inst_op_t *get_result_op(inst_t *i);
static char *load_result(list_t *is, inst_t *tgt);

/* convert an expression to a list of
 * three addr instructions. if var name
 * is null, the result is stored to a new
 * anon_var */
static inst_t *expr_to_tac(cfg_context_t *cfg, list_t *is, expr_t *e, char *vname);
static void expr_op_to_tac(cfg_context_t *cfg, list_t *is, expr_t *ex, inst_op_t *op, bool load);

/* removes dummy blocks */
static void remove_dummy_blocks(cfg_context_t *cfg);

/* labels every basic block with an integer value
 * and creates lists of parent nodes for each */
static void update_metadata(cfg_context_t *cfg);

cfg_context_t* func_to_cfg(func_t *f)
{
	cfg_context_t *cfg;

	cfg = calloc(1, sizeof(*cfg));
	cfg->fn = f;

	cfg->all_bb = list_new();
	cfg->entry = bb_dummy_new(cfg);
	cfg->exit = stmt_to_cfg(f->body, cfg, cfg->entry);

	remove_dummy_blocks(cfg);
	update_metadata(cfg);

	return cfg;
}

static bb_node_t* stmt_to_cfg(stmt_t* stmt, cfg_context_t *cfg, bb_node_t *entry)
{
	for (; stmt != NULL; stmt = stmt->next) {
		switch (stmt->type) {
		case STMT_IF:
			entry = if_to_cfg(stmt, cfg, entry);
			break;

		case STMT_WHILE:
			entry = while_to_cfg(stmt, cfg, entry);
			break;

		case STMT_ASSIGN:
			entry = assign_to_cfg(stmt, cfg, entry);
			break;

		case STMT_PRINT:
			entry = print_to_cfg(stmt, cfg, entry);
			break;
		}
	}

	return entry;
}

static bb_node_t* if_to_cfg(stmt_t *stmt, cfg_context_t *cfg, bb_node_t *entry)
{
	inst_t *test_i = inst_new();

	test_i->type = I_IF;
	test_i->cond.is_val = false;
	test_i->cond.id = get_next_temp_id();

	entry->has_condition = true;
	promote_bb(entry);
	expr_to_tac(cfg, entry->instructions, stmt->if_s.cond, test_i->cond.id);
	list_append(entry->instructions, test_i);
	entry->tb = bb_new(cfg);
	entry->fb = bb_new(cfg);

	bb_node_t *t_exit = stmt_to_cfg(stmt->if_s.tb, cfg, entry->tb);
	bb_node_t *f_exit = stmt_to_cfg(stmt->if_s.fb, cfg, entry->fb);

	t_exit->tb = bb_dummy_new(cfg);
	f_exit->tb = t_exit->tb;

	return t_exit->tb;
}

static bb_node_t* while_to_cfg(stmt_t *stmt, cfg_context_t *cfg, bb_node_t *entry)
{
	bb_node_t *test = bb_new(cfg);
	inst_t *test_i = inst_new();

	entry->tb = test;

	test_i->type = I_IF;
	test_i->cond.is_val = false;
	test_i->cond.id = get_next_temp_id();

	test->has_condition = true;
	test->instructions = list_new();
	test->tb = bb_new(cfg);

	expr_to_tac(cfg, test->instructions, stmt->while_s.cond, test_i->cond.id);
	list_append(test->instructions, test_i);

	bb_node_t *b_exit = stmt_to_cfg(stmt->while_s.st, cfg, test->tb);
	b_exit->tb = test;

	test->fb = bb_dummy_new(cfg);
	return test->fb;
}

static bb_node_t* assign_to_cfg(stmt_t *stmt, cfg_context_t *cfg, bb_node_t *entry)
{
	char *tl, *tr;
	inst_t *l, *r;
	inst_op_t *op;

	promote_bb(entry);

	if (is_simple_identifier(stmt->assign.lhs)) {
		r = expr_to_tac(cfg, entry->instructions,
			    stmt->assign.rhs, get_identifier(stmt->assign.lhs));

		if (result_is_reference(r) && r->type != I_ALLOC) {
			op = get_result_op(r);

			if (op == NULL)
				ice("get_result_op NULL");

			l = inst_new();
			l->type = I_LOAD;
			l->m.src.is_val = false;
			l->m.src.id = get_next_temp_id();
			l->m.dst.is_val = false;
			l->m.dst.id = op->id;
			op->id = l->m.src.id;
			list_append(entry->instructions, l);
		}

	} else {
		tr = get_next_temp_id();
		r = expr_to_tac(cfg, entry->instructions,
		                stmt->assign.rhs, tr);

		char *p = load_result(entry->instructions, r);
		if (p != NULL)
			tr = p;

		if        (stmt->assign.lhs->op == OP_SYMBOL &&
		           stmt->assign.lhs->sym->tag == SYM_REF_PARAMETER) {
			tl = stmt->assign.lhs->sym->name;

		} else if (stmt->assign.lhs->op == OP_SYMBOL &&
		           stmt->assign.lhs->sym->tag == SYM_FIELD) {
			tl = get_next_temp_id();

			l = inst_new();
			l->type = I_ATTRIBUTE;
			l->a.l.is_val = false;
			l->a.l.id = tl;
			l->a.r[0].is_val = false;
			l->a.r[0].id = "this";
			l->a.r[1].is_val = false;
			l->a.r[1].id = stmt->assign.lhs->sym->name;
			l->a.t = cfg->fn->parent->t;

			list_append(entry->instructions, l);

		} else {
			tl = get_next_temp_id();
			l = expr_to_tac(cfg, entry->instructions,
			                stmt->assign.lhs, tl);

			if (!result_is_reference(l)) {
				ice("assignment impossible: not reference");
				return NULL;
			}
		}

		inst_t *store_i = inst_new();
		store_i->type = I_STORE;
		store_i->m.dst.is_val = false;
		store_i->m.src.is_val = false;
		store_i->m.dst.id = tl;
		store_i->m.src.id = tr;
		list_append(entry->instructions, store_i);
	}

	return entry;
}

static bb_node_t* print_to_cfg(stmt_t *stmt, cfg_context_t *cfg, bb_node_t *entry)
{
	char *to, *p;
	inst_t *print_i, *r;

	promote_bb(entry);

	to = get_next_temp_id();
	r = expr_to_tac(cfg, entry->instructions, stmt->print, to);
	p = load_result(entry->instructions, r);
	if (p != NULL)
		to = p;

	print_i = inst_new();
	print_i->type = I_PRINT;
	print_i->a.r[0].is_val = false;
	print_i->a.r[0].id = to;
	list_append(entry->instructions, print_i);

	return entry;
}

static bool is_constant(expr_t *ex)
{
	switch (ex->op) {
	case OP_INTEGER_CONSTANT:
		return true;

	case OP_NOT:
	case OP_POS:
	case OP_NEG:
		return is_constant(ex->ops[0]);

	case OP_EQ:
	case OP_NE:
	case OP_LT:
	case OP_GT:
	case OP_LE:
	case OP_GE:
	case OP_ADD:
	case OP_SUB:
	case OP_OR:
	case OP_MUL:
	case OP_DIV:
	case OP_MOD:
	case OP_AND:
		return is_constant(ex->ops[0]) && is_constant(ex->ops[1]);

	default:
		return false;
	}
}

static int compute_constant(expr_t *ex)
{
	int op0, op1;

	if (ex == NULL)
		return 0;

	switch (ex->op) {
	case OP_INTEGER_CONSTANT:
		return ex->uc;

	case OP_NOT:   return !compute_constant(ex->ops[0]);
	case OP_POS:   return  compute_constant(ex->ops[0]);
	case OP_NEG:   return -compute_constant(ex->ops[0]);

	/* this is not pretty, but it's Good Enough(tm) */
	case OP_EQ:
	case OP_NE:
	case OP_LT:
	case OP_GT:
	case OP_LE:
	case OP_GE:
	case OP_ADD:
	case OP_SUB:
	case OP_OR:
	case OP_MUL:
	case OP_DIV:
	case OP_MOD:
	case OP_AND:
		op0 = compute_constant(ex->ops[0]);
		op1 = compute_constant(ex->ops[1]);

		return run_op(ex->op, op0, op1);

	default:
		break;
	}

	return 0;
}

static bool result_is_reference(inst_t *i)
{
	switch (i->type) {
	case I_ASSIGN:
		return i->a.op == OP_INDEX;

	case I_ATTRIBUTE:
		return true;

	case I_IF:
	case I_LOAD:
	case I_STORE:
	case I_CALL:
	case I_PRINT:
	case I_ALLOC:
		return false;
	}

	return false;
}

static inst_op_t *get_result_op(inst_t *i)
{
	switch (i->type) {
	case I_ASSIGN:
	case I_ATTRIBUTE:
		return &i->a.l;

	case I_LOAD:
		return &i->m.dst;

	default:
		return NULL;
	}
}

static char *load_result(list_t *is, inst_t *tgt)
{
	inst_t *i;
	inst_op_t *op;

	if (tgt == NULL || !result_is_reference(tgt))
		return NULL;

	op = get_result_op(tgt);

	if (!op || op->is_val)
		return NULL;

	i = inst_new();

	i->type = I_LOAD;
	i->m.dst.is_val = i->m.src.is_val = false;
	i->m.src.id = op->id;
	i->m.dst.id = get_next_temp_id();

	list_append(is, i);

	return strdup(i->m.dst.id);
}

static void expr_op_to_tac(cfg_context_t *cfg, list_t *is, expr_t *ex, inst_op_t *op, bool load)
{
	inst_t *res;
	char *p;

	if (is_constant(ex)) {
		op->is_val = true;
		op->val = compute_constant(ex);
	} else if (is_simple_identifier(ex)) {
		op->is_val = false;
		op->id = get_identifier(ex);
	} else {
		op->is_val = false;
		op->id = get_next_temp_id();
		res = expr_to_tac(cfg, is, ex, op->id);
		if (load && (p = load_result(is, res)) != NULL)
			op->id = p;
	}
}

static inst_t *expr_to_tac(cfg_context_t *cfg, list_t *is, expr_t *e, char *vname)
{
	inst_t *i2, *i = NULL;
	inst_op_t *op;
	list_node_t *n, *argn;
	expr_t *ex;

	switch(e->op) {
	case OP_SYMBOL:
		i = inst_new();

		if (e->sym->tag == SYM_REF_PARAMETER) {
			i->type = I_LOAD;

			i->m.dst.is_val = false;
			i->m.dst.id = vname;
			i->m.src.is_val = false;
			i->m.src.id = e->sym->name;

		} else if (e->sym->tag == SYM_FIELD) {
			i->type = I_ATTRIBUTE;

			i->a.l.is_val = false;
			i->a.l.id = vname;
			i->a.r[0].is_val = false;
			i->a.r[0].id = "this";
			i->a.r[1].is_val = false;
			i->a.r[1].id = get_identifier(e);
			i->a.t = cfg->fn->parent->t;

		} else {
			i->type = I_ASSIGN;
			i->a.op = OP_IDENTIFIER;

			i->a.l.is_val = false;
			i->a.l.id = vname;
			i->a.r[0].is_val = false;
			i->a.r[0].id = get_identifier(e);
		}

		list_append(is, i);

		break;

	case OP_INTEGER_CONSTANT:
	case OP_BOOLEAN_CONSTANT:
		i = inst_new();

		i->a.r[0].is_val = true;
		i->a.r[0].val = e->uc;

		i->type = I_ASSIGN;
		i->a.op = OP_INTEGER_CONSTANT;
		i->a.l.is_val = false;
		i->a.l.id = vname;

		list_append(is, i);

		break;

	case OP_ATTRIBUTE:
		i = inst_new();

		/* do r[0] */
		expr_op_to_tac(cfg, is, e->ops[0], &i->a.r[0], true);

		/* do r[1] */
		i->a.r[1].is_val = false;
		i->a.r[1].id = get_identifier(e->ops[1]);

		/* do me */
		i->type = I_ATTRIBUTE;
		i->a.op = e->op;
		i->a.l.is_val = false;
		i->a.l.id = vname;
		i->a.t = e->ops[0]->t;

		list_append(is, i);

		break;

	case OP_NULL:
	case OP_POS:
		expr_to_tac(cfg, is, e->ops[0], vname);
		break;

	case OP_NOT:
		i = inst_new();

		expr_op_to_tac(cfg, is, e->ops[0], &i->a.r[0], true);

		i->type = I_ASSIGN;
		i->a.op = OP_NOT;
		i->a.l.is_val = false;
		i->a.l.id = vname;

		list_append(is, i);

		break;

	case OP_NEG:
		/* build 0 - op[0] */
		i = inst_new();

		/* do r[0] */
		i->a.r[0].is_val = true;
		i->a.r[0].val = 0;

		/* do r[1] */
		expr_op_to_tac(cfg, is, e->ops[0], &i->a.r[1], true);

		/* do me */
		i->type = I_ASSIGN;
		i->a.op = OP_SUB;
		i->a.l.is_val = false;
		i->a.l.id = vname;

		list_append(is, i);

		break;

	/* two part expressions are handled by calculating
	 * the lhs and rhs as separate expressions
	 * and then appending the comparison.
	 * having a list_cat operation would be
	 * very useful both here and for trailing
	 * together strings of expr calculations and
	 * assignments in stmt_to_cfg */
	case OP_EQ:
	case OP_NE:
	case OP_LT:
	case OP_GT:
	case OP_LE:
	case OP_GE:
	case OP_ADD:
	case OP_SUB:
	case OP_OR:
	case OP_MUL:
	case OP_DIV:
	case OP_MOD:
	case OP_AND:
	case OP_INDEX:
		i = inst_new();

		if (is_constant(e)) {
			i->a.r[0].is_val = true;
			i->a.r[0].val = compute_constant(e);

			i->type = I_ASSIGN;
			i->a.op = OP_INTEGER_CONSTANT;
			i->a.l.is_val = false;
			i->a.l.id = vname;

			list_append(is, i);

			break;
		}

		/* do r[0] */
		expr_op_to_tac(cfg, is, e->ops[0], &i->a.r[0], e->op != OP_INDEX);

		/* do r[1] */
		expr_op_to_tac(cfg, is, e->ops[1], &i->a.r[1], true);

		/* do me */
		i->type = I_ASSIGN;
		i->a.op = e->op;
		i->a.l.is_val = false;
		i->a.l.id = vname;
		i->a.t = NULL;

		if (e->op == OP_INDEX) {
			i->a.t = e->ops[0]->t;
			if (!i->a.t || i->a.t->tag != T_ARRAY)
				ice("indexing non-array");
			i->a.t = i->a.t->arr.of;
		}

		list_append(is, i);

		break;

	case OP_APPLY:
		i = inst_new();

		/* build instruction */
		i->type = I_CALL;
		i->c.args = list_new();
		i->c.ret.is_val = false;
		i->c.ret.id = vname;

		/* extract 'this' and the function */
		char *this_id = "this";
		symbol_t *sym = NULL;
		char *p;
		inst_t *res;

		if (e->apply.fn->op == OP_SYMBOL) {
			this_id = "this";
			sym = e->apply.fn->sym;

		} else if (e->apply.fn->op == OP_ATTRIBUTE) {
			this_id = get_next_temp_id();
			res = expr_to_tac(cfg, is, e->apply.fn->ops[0], this_id);
			if ((p = load_result(is, res)) != NULL)
				this_id = p;

			if (e->apply.fn->ops[1]->op != OP_SYMBOL) {
				ice("calling non-symbol!");
				return NULL;
			}
			sym = e->apply.fn->ops[1]->sym;
		} else {
			ice("unknown callable!");
			return NULL;
		}

		if (sym == NULL || sym->tag != SYM_FUNCTION) {
			ice("calling non-function!");
			return NULL;
		}
		i->c.fn = sym->fn;

		/* build arguments list */
		op = calloc(1, sizeof(*op));
		op->is_val = false;
		op->id = this_id;
		list_append(i->c.args, op);

		argn = list_head(i->c.fn->arguments);
		LIST_EACH(e->apply.args, n, ex) {
			symbol_t *sym = argn->v;

			op = calloc(1, sizeof(*op));
			expr_op_to_tac(cfg, is, ex, op,
			               sym->tag != SYM_REF_PARAMETER);
			list_append(i->c.args, op);

			argn = argn->next;
		}

		/* append! */
		list_append(is, i);

		break;

	case OP_INSTANTIATION:
		/* build instruction */
		i = inst_new();
		i->type = I_ALLOC;
		i->alloc.t = e->t;
		i->alloc.dst.is_val = false;
		i->alloc.dst.id = vname;

		list_append(is, i);

		/* check if constructor call */
		if (e->t->cls->ctor == NULL) {
			if (e->apply.args && e->apply.args->length)
				ice("constructor call when no constructor");
			return i;
		}

		/* if so, generate the call! */
		i2 = inst_new();
		i2->type = I_CALL;
		i2->c.args = list_new();
		i2->c.ret.is_val = true; /* this means discard return */
		i2->c.ret.val = 0;
		i2->c.fn = e->t->cls->ctor;

		/* build arguments list */
		op = calloc(1, sizeof(*op));
		op->is_val = false;
		op->id = vname;
		list_append(i2->c.args, op);

		argn = list_head(i2->c.fn->arguments);
		LIST_EACH(e->apply.args, n, ex) {
			symbol_t *sym = argn->v;

			op = calloc(1, sizeof(*op));
			expr_op_to_tac(cfg, is, ex, op,
			               sym->tag != SYM_REF_PARAMETER);
			list_append(i2->c.args, op);

			argn = argn->next;
		}

		list_append(is, i2);

		break;

	default:
		/* no other OPs should be seen */
		ice("not yet handled operand");
		break;
	}
	
	return i;
}

static int dummy_fix(cfg_context_t *cfg, bb_node_t **bb_slot)
{
	bb_node_t *bb = *bb_slot;

	/* if not dummy or permitted exit dummy node, be done */
	if (!bb || !is_dummy(bb) || bb->tb == NULL)
		return 0;

	/* point slot past dummy node, decrement the reference count on the
	   dummy node and increment the reference count on the node being
	   pointed to. */
	*bb_slot = bb->tb;
	bb->tb->label++;
	bb->label--;

	return 1;
}

/* remove dummy blocks */
static void remove_dummy_blocks(cfg_context_t *cfg)
{
	list_node_t *cur, *ncur;
	bb_node_t *bb;
	int changed;

	/* initialize reference counts */

	LIST_EACH(cfg->all_bb, cur, bb)
		bb->label = 0;

	LIST_EACH(cfg->all_bb, cur, bb) {
		if (bb->tb != NULL)
			bb->tb->label++;
		if (bb->fb != NULL)
			bb->fb->label++;
	}

	/* hold one reference to entry node */

	if (cfg->entry != NULL)
		cfg->entry->label ++;

	/* until no changes are made, continue to remove blocks */

	do {
		changed = 0;

		LIST_EACH(cfg->all_bb, cur, bb) {
			changed += dummy_fix(cfg, &bb->tb);
			changed += dummy_fix(cfg, &bb->fb);
		}
	} while (changed);

	LIST_EACH_NODE_SAFE(cfg->all_bb, cur, ncur) {
		bb = cur->v;

		/* if all references gone, free the node */
		if (bb->label <= 0) {
			list_delete(cfg->all_bb, bb->n);
			free(bb);
		}
	}
}

static void update_metadata(cfg_context_t *cfg)
{
	list_node_t *cur;
	bb_node_t *bb;
	int i = 0;

	LIST_EACH(cfg->all_bb, cur, bb) {
		bb->visited = 0;
		bb->label = i++;
		bb->parent_nodes = list_new();
	}

	LIST_EACH(cfg->all_bb, cur, bb) {
		if (bb->fb != NULL)
			list_append(bb->fb->parent_nodes, bb);

		if (bb->tb != NULL)
			list_append(bb->tb->parent_nodes, bb);
	}
}

static void dump_inst_op_to_str(inst_op_t *op, char *buf, int n)
{
	if (op->is_val) {
		snprintf(buf, n, "\x1b[1;35m%d\x1b[0m", op->val);
	} else if (op->id[0] == '_') {
		snprintf(buf, n, "\x1b[34m%s\x1b[0m", op->id);
	} else {
		snprintf(buf, n, "\x1b[1;36m%s\x1b[0m", op->id);
	}
}

static void dump_inst(bb_node_t *bb, inst_t *i)
{
	char b0[512], b1[512], b2[512];
	list_node_t *n;
	inst_op_t *arg;
	bool comma;

	switch (i->type) {
	case I_ASSIGN:
		dump_inst_op_to_str(&i->a.l,     b0, 512);
		dump_inst_op_to_str(&i->a.r[0],  b1, 512);
		switch (i->a.op) {
		case OP_IDENTIFIER:
		case OP_INTEGER_CONSTANT:
			fprintf(stderr, "    %s <- %s\n", b0, b1);
			break;

		case OP_NOT:
			fprintf(stderr, "    %s <- NOT %s\n", b0, b1);
			break;

		case OP_INDEX:
			dump_inst_op_to_str(&i->a.r[1],  b2, 512);
			fprintf(stderr, "    %s <- &%s[%s] (%s)\n",
			        b0, b1, b2, type_to_name(i->a.t));
			break;

		default:
			dump_inst_op_to_str(&i->a.r[1],  b2, 512);
			fprintf(stderr, "    %s <- %s %2s %s\n", b0, b1,
			       expr_op_to_glyph[i->a.op], b2);
			break;
		}
		break;

	case I_IF:
		dump_inst_op_to_str(&i->cond, b1, 512);
		fprintf(stderr, "    test %s\n", b1);
		break;

	case I_ATTRIBUTE:
		dump_inst_op_to_str(&i->a.l,     b0, 512);
		dump_inst_op_to_str(&i->a.r[0],  b1, 512);
		dump_inst_op_to_str(&i->a.r[1],  b2, 512);
		fprintf(stderr, "    %s <- &%s->%s\n", b0, b1, b2);
		break;

	case I_LOAD:
		dump_inst_op_to_str(&i->m.dst, b0, 512);
		dump_inst_op_to_str(&i->m.src, b1, 512);
		fprintf(stderr, "    %s <- *%s\n", b0, b1);
		break;

	case I_STORE:
		dump_inst_op_to_str(&i->m.dst, b0, 512);
		dump_inst_op_to_str(&i->m.src, b1, 512);
		fprintf(stderr, "    %s -> *%s\n", b1, b0);
		break;

	case I_CALL:
		dump_inst_op_to_str(&i->c.ret, b0, 512);
		fprintf(stderr, "    %s <- \x1b[1;32m%s\x1b[0m(",
		        b0, i->c.fn->name);
		comma = false;
		LIST_EACH(i->c.args, n, arg) {
			dump_inst_op_to_str(arg, b0, 512);
			fprintf(stderr, "%s%s", comma ? ", " : "", b0);
			comma = true;
		}
		fprintf(stderr, ")\n");
		break;

	case I_ALLOC:
		dump_inst_op_to_str(&i->alloc.dst, b0, 512);
		fprintf(stderr, "    %s <- \x1b[1;32mnew %s\x1b[0m\n",
		        b0, type_to_name(i->alloc.t));
		break;

	case I_PRINT:
		dump_inst_op_to_str(&i->a.r[0], b0, 512);
		fprintf(stderr, "    print %s\n", b0);
		break;
	}
}

static void dump_add_var(list_t *vs, inst_op_t *op)
{
	list_node_t *cur;
	char *id;
	int c;

	if (op->is_val)
		return;

	if (vs->length == 0) {
		list_append(vs, op->id);
		return;
	}

	/* this combination insertion AND search also keeps the list sorted! */
	LIST_EACH(vs, cur, id) {
		c = strcmp(op->id, id);

		if (c <= 0) {
			if (c < 0)
				list_add_before(vs, cur, op->id);
			return;
		}
	}
	list_append(vs, op->id);
}

static void dump_add_variables(bb_node_t *bb, list_t *vs)
{
	list_node_t *cur, *n;
	inst_t *i;
	inst_op_t *arg;

	if (is_dummy(bb))
		return;

	LIST_EACH(bb->instructions, cur, i) {
		switch (i->type) {
		case I_ATTRIBUTE:
			dump_add_var(vs, &i->a.l);
			dump_add_var(vs, &i->a.r[0]);
			break;

		case I_ASSIGN:
			dump_add_var(vs, &i->a.l);

			switch (i->a.op) {
			case OP_IDENTIFIER:
			case OP_INTEGER_CONSTANT:
			case OP_NOT:
				dump_add_var(vs, &i->a.r[0]);
				break;

			default:
				dump_add_var(vs, &i->a.r[0]);
				dump_add_var(vs, &i->a.r[1]);
				break;
			}
			break;

		case I_LOAD:
		case I_STORE:
			dump_add_var(vs, &i->m.dst);
			dump_add_var(vs, &i->m.src);
			break;

		case I_IF:
			dump_add_var(vs, &i->cond);
			break;

		case I_CALL:
			dump_add_var(vs, &i->c.ret);
			LIST_EACH(i->c.args, n, arg)
				dump_add_var(vs, arg);
			break;

		case I_ALLOC:
			dump_add_var(vs, &i->alloc.dst);
			break;

		case I_PRINT:
			dump_add_var(vs, &i->a.r[0]);
			break;
		}
	}
}

__unused static void dump_variables_pretty(cfg_context_t *cfg)
{
	list_node_t *cur;
	list_t *variables;
	bb_node_t *bb;
	char *id;
	int i, nth;

	variables = list_new();

	LIST_EACH(cfg->all_bb, cur, bb)
		dump_add_variables(bb, variables);

	fprintf(stderr, "  \x1b[1mvariables:\x1b[0m");
	i = -1;
	nth = 0;
	LIST_EACH(variables, cur, id) {
		if (nth != 0)
			fprintf(stderr, ", ");
		if (i < 0 || i + strlen(id) > 40) {
			fprintf(stderr, "\n    ");
			i = 0;
		}
		fprintf(stderr, "%s%s\x1b[0m",
		        id[0] == '_' ? "\x1b[34m" : "\x1b[1;36m", id);
		i += strlen(id) + (i == 0 ? 0 : 2);
		nth++;
	}
	fprintf(stderr, "\n");
}

static void dump_bb(cfg_context_t *cfg, bb_node_t *bb)
{
	list_node_t *cur;
	inst_t *i;

	fprintf(stderr, "  \x1b[1mblock #%d:\x1b[0m\n", bb->label);

	if (is_dummy(bb)) {
		fprintf(stderr, "    (dummy)\n");
	} else {
		LIST_EACH(bb->instructions, cur, i)
			dump_inst(bb, i);
	}

	if (bb->fb)
		fprintf(stderr, "    goto #%d, if false\n", bb->fb->label);

	if (bb->tb)
		fprintf(stderr, "    goto #%d\n", bb->tb->label);
}

void dump_cfg(cfg_context_t *cfg)
{
	list_node_t *cur;
	bb_node_t *bb;
	int i;

	i = 0;
	LIST_EACH(cfg->all_bb, cur, bb) {
		bb->visited = 0;
		bb->label = i++;
	}

	//dump_variables_pretty(cfg);

	LIST_EACH(cfg->all_bb, cur, bb)
		dump_bb(cfg, bb);
}

int run_op(expr_op_t op, int a, int b)
{
	switch (op) {
	case OP_EQ:   return a == b;
	case OP_NE:   return a != b;
	case OP_LT:   return a <  b;
	case OP_GT:   return a >  b;
	case OP_LE:   return a <= b;
	case OP_GE:   return a >= b;
	case OP_ADD:  return a +  b;
	case OP_SUB:  return a -  b;
	case OP_OR:   return a || b;
	case OP_MUL:  return a *  b;
	case OP_DIV:  return a /  b;
	case OP_MOD:  return a %  b;
	case OP_AND:  return a && b;
	default:      ice("invalid constant operator %d\n", op);
	}
}

bool is_dummy(bb_node_t *bb)
{
	return bb && (bb->is_dummy || bb->instructions == NULL);
}
