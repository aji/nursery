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

/* The data structures used in this implementation are a little different
   than what might typically be expected.

   The main data type is the 'vnum_t', which represents a value number. A
   vnum_t stores the last identifier it was assigned to, whether it's a
   constant, the value number or constant value, and the expression which
   generated it, if any.

   The main container data type is the 'vnum_ctx_t', which represents all
   value numbers. Some operations are defined on vnum_ctx_t's which allow
   manipulation of these tables. */

#define OP_COMMUTATIVE 1

static unsigned vnum_count = 0;
static unsigned vnum_attr_count = 0;

/* this is a table of properties for different operators */
static unsigned op_flags[] = {
	[OP_INDEX]             = 0,

	[OP_NOT]               = 0,
	[OP_EQ]                = OP_COMMUTATIVE,
	[OP_NE]                = OP_COMMUTATIVE,
	[OP_LT]                = 0,
	[OP_GT]                = 0,
	[OP_LE]                = 0,
	[OP_GE]                = 0,
	[OP_ADD]               = OP_COMMUTATIVE,
	[OP_SUB]               = 0,
	[OP_OR]                = OP_COMMUTATIVE,
	[OP_MUL]               = OP_COMMUTATIVE,
	[OP_DIV]               = 0,
	[OP_MOD]               = 0,
	[OP_AND]               = OP_COMMUTATIVE,
};

/* this table records what type of operator each operator is */
static enum {
	OP_T_UNKNOWN,
	OP_T_UNARY,
	OP_T_BINARY,
	OP_T_IGNORE_ME_I_SUCK,
} op_type[] = {
	[OP_IDENTIFIER]        = OP_T_UNARY,
	[OP_INTEGER_CONSTANT]  = OP_T_UNARY,

	[OP_INDEX]             = OP_T_BINARY,

	[OP_NOT]               = OP_T_IGNORE_ME_I_SUCK,
	[OP_EQ]                = OP_T_BINARY,
	[OP_NE]                = OP_T_BINARY,
	[OP_LT]                = OP_T_BINARY,
	[OP_GT]                = OP_T_BINARY,
	[OP_LE]                = OP_T_BINARY,
	[OP_GE]                = OP_T_BINARY,
	[OP_ADD]               = OP_T_BINARY,
	[OP_SUB]               = OP_T_BINARY,
	[OP_OR]                = OP_T_BINARY,
	[OP_MUL]               = OP_T_BINARY,
	[OP_DIV]               = OP_T_BINARY,
	[OP_MOD]               = OP_T_BINARY,
	[OP_AND]               = OP_T_BINARY,
};

typedef struct _vnum vnum_t;
typedef struct _vnum_ctx vnum_ctx_t;
typedef struct _vnum_match_context vnum_match_context_t;

/* value number reference */
struct _vnum {
	/* a flag indicating whether this value number represents a constant
	   value or a value number */
	bool is_constant;

	/* EITHER the value number OR the constant value, depending on
	   whether is_constant is true */
	int number;

	/* stores the last id that the value number was assigned to. note
	   that this can become invalid if the id is assigned to later! */
	char *last_id;

	/* these members represent the expression used to compute the value
	   number. if op is OP_NONE, then no expression is available. these
	   fields should not be interpreted when is_constant is true. */
	inst_type_t inst;
	expr_op_t op;
	vnum_t *r[2];
};

/* value numbering context */
struct _vnum_ctx {
	vnum_ctx_t *parent;
	
	/* a table mapping symbols to vnum_t. note that different symbols can
	   point to the same vnum_t. */
	htab_t  *vtab;

	/* a table mapping attributes to vnum_t. this is used for value
	   numbering optimizations on attribute accesses. this is a separate
	   value numbering space, and it's up to the programmer to ensure
	   that attr vnums are only compared to other attr vnums. */
	htab_t  *attrtab;

	/* a vector containing all value numbers. this is effectively the value
	   number table */
	vec_t   *nums;
};

struct _vnum_match_context {
	char *id_match;
	vnum_t *vnum_to_match;
};

/* creates a new value numbering context */
static vnum_ctx_t *vnum_ctx_new(vnum_ctx_t *parent)
{
	vnum_ctx_t *ctx = calloc(1, sizeof(*ctx));

	ctx->vtab = htab_new(HTAB_DEFAULT_ORDER);
	ctx->attrtab = htab_new(HTAB_DEFAULT_ORDER);
	ctx->nums = vec_new(10);
	ctx->parent = parent;

	return ctx;
}

/* frees all resources associated with a value numbering context */
static void vnum_ctx_release(vnum_ctx_t *ctx)
{
	unsigned i;

	for (i=0; i<ctx->nums->size; i++)
		free(vec_get(ctx->nums, i));

	htab_release(ctx->vtab);
	htab_release(ctx->attrtab);
	vec_release(ctx->nums);

	free(ctx);
}

/* allocates the next vnum_t */
static vnum_t *vnum_next(vnum_ctx_t *ctx)
{
	vnum_t *vnum = calloc(1, sizeof(*vnum));

	vnum->is_constant = false;
	vnum->number = vnum_count;
	vnum_count++;
	vnum->inst = I_ASSIGN;
	vnum->op = OP_NONE;

	vec_append(ctx->nums, vnum);

	return vnum;
}

/* allocates the next attr vnum_t */
static vnum_t *vnum_next_attr(vnum_ctx_t *ctx)
{
	vnum_t *vnum = calloc(1, sizeof(*vnum));

	vnum->is_constant = false;
	vnum->number = vnum_attr_count;
	vnum_attr_count++;
	vnum->inst = I_ATTRIBUTE;
	vnum->op = OP_NONE;

	return vnum;
}

/* compares two vnum_t for equality */
static bool vnum_eq(vnum_t *a, vnum_t *b)
{
	if (a && b && a->is_constant == b->is_constant)
		return a->number == b->number;

	return a == NULL && b == NULL;
}

/* finds a vnum_t for the given id, optionally performing allocation */
static vnum_t *vnum_for_id(vnum_ctx_t *ctx, char *id, bool allow_alloc)
{
	vnum_t *vnum;
	vnum_ctx_t *ctx_cur = ctx;

	/* now bigger, better, and more extended! */
	do {
		vnum = htab_get(ctx_cur->vtab, id);
	} while (
			vnum == NULL && 
			(ctx_cur = ctx_cur->parent) != NULL
		);

	if (vnum == NULL && allow_alloc) {
		vnum = vnum_next(ctx);
		htab_put(ctx->vtab, id, vnum);
	}

	return vnum;
}

/* finds a vnum_t for the given attribute id, optionally performing allocation */
static vnum_t *vnum_for_attribute(vnum_ctx_t *ctx, char *attr, bool allow_alloc)
{
	vnum_t *vnum;
	vnum_ctx_t *ctx_cur = ctx;

	do {
		vnum = htab_get(ctx_cur->attrtab, attr);
	} while (
			vnum == NULL &&
			(ctx_cur = ctx_cur->parent) != NULL
		);

	if (vnum == NULL && allow_alloc) {
		vnum = vnum_next_attr(ctx);
		htab_put(ctx->attrtab, attr, vnum);
	}

	return vnum;
}

/* check if a key value pair has a given vnum and store the associated
 * id in an accessible place */
static void id_for_vnum_check(void *_match_context, char *id, void *_vnum)
{
	vnum_match_context_t *match_context = _match_context;
	
	vnum_t *vnum = _vnum;

	/* match already found */
	if (match_context->id_match != NULL)
		return;
	
	if (vnum->is_constant)
		return;

	if (vnum_eq(vnum, match_context->vnum_to_match))
		match_context->id_match = strdup(id);

	return;
}

/* returns the first id currently associated with the given vnum, or
 * NULL if none is found */
static char *id_for_vnum(vnum_ctx_t *ctx, vnum_t *vnum)
{
	vnum_match_context_t match_context;

	match_context.id_match = NULL;
	match_context.vnum_to_match = vnum;
	
	do {
		htab_each(ctx->vtab, id_for_vnum_check, &match_context);
	} while (
			match_context.id_match == NULL && 
			(ctx = ctx->parent) != NULL
		);

	return match_context.id_match;
}

/* checks if the last_id field of the vnum_t is still valid */
__unused static bool vnum_last_id_valid(vnum_ctx_t *ctx, vnum_t *a)
{
	vnum_t *b;

	if (a->last_id == NULL || a->is_constant)
		return false;

	b = vnum_for_id(ctx, a->last_id, false);

	return vnum_eq(a, b);
}

/* finds a vnum_t for the given inst_op_t, optionally performing allocation */
static vnum_t *vnum_for_inst_op(vnum_ctx_t *ctx, inst_op_t *op, bool allow_alloc)
{
	vnum_t *vnum;

	if (!op->is_val)
		return vnum_for_id(ctx, op->id, allow_alloc);

	if (!allow_alloc)
		return NULL;

	vnum = vnum_next(ctx);
	vnum->is_constant = true;
	vnum->number = op->val;

	return vnum;
}

/* evaluates whether the two expressions match, under rules of commutativity */
static bool expr_match(inst_type_t iA, expr_op_t opA, vnum_t *A0, vnum_t *A1,
                       inst_type_t iB, expr_op_t opB, vnum_t *B0, vnum_t *B1)
{
	if (iA != iB || opA != opB)
		return false;

	if (vnum_eq(A0, B0) && vnum_eq(A1, B1))
		return true;

	if ((op_flags[opA] & OP_COMMUTATIVE) && vnum_eq(A0, B1) & vnum_eq(A1, B0))
		return true;

	return false;
}

/* finds a vnum matching the given expression */
static vnum_t *vnum_for_expr(vnum_ctx_t *ctx,
                             inst_type_t inst, expr_op_t op, vnum_t *r0, vnum_t *r1)
{
	unsigned i;
	vnum_t *vnum;

	do {
		for (i=0; i<ctx->nums->size; i++) {
			vnum = vec_get(ctx->nums, i);

			if (expr_match(inst, op, r0, r1,
			               vnum->inst, vnum->op,
			               vnum->r[0], vnum->r[1]))
				return vnum;
		}
	} while( (ctx = ctx->parent) != NULL);

	return NULL;
}

/* updates the value number tables to indicate 'id' now holds the constant
   'value' */
static void vnum_put_constant(vnum_ctx_t *ctx, char *id, int value)
{
	vnum_t *vnum = vnum_next(ctx);

	vnum->last_id = id;
	vnum->is_constant = true;
	vnum->number = value;
	vnum->op = OP_NONE;

	htab_put(ctx->vtab, id, vnum);
}

/* updates the value number tables to show that 'id' now holds the same value
   number as 'vnum' */
static void vnum_put_dupe(vnum_ctx_t *ctx, char *id, vnum_t *vnum)
{
	htab_put(ctx->vtab, id, vnum);
	vnum->last_id = id;
}

/* updates the value number tables to show that 'id' now refers to the
   expression 'r0 op r1' */
static void vnum_put_expr(vnum_ctx_t *ctx, char *id,
                          inst_type_t inst, expr_op_t op, vnum_t *r0, vnum_t *r1)
{
	vnum_t *vnum = vnum_next(ctx);

	vnum->last_id = id;

	vnum->inst = inst;
	vnum->op = op;
	vnum->r[0] = r0;
	vnum->r[1] = r1;

	htab_put(ctx->vtab, id, vnum);
}

/* attempts to perform some form of expression reduction. returns true if
   expression reduction was possible, false otherwise. */
static bool try_reduce(vnum_ctx_t *ctx, inst_t *i, vnum_t *r0, vnum_t *r1)
{
	static vnum_t vn_0 = { .is_constant = true, .number = 0 };
	static vnum_t vn_1 = { .is_constant = true, .number = 1 };

	expr_op_t op = i->a.op;

	/* 0 * x, x - x           ->  0 */
	if ((op == OP_MUL && (vnum_eq(r0, &vn_0) || vnum_eq(r1, &vn_0))) ||
	    (op == OP_SUB && vnum_eq(r0, r1))) {
		i->a.op = OP_INTEGER_CONSTANT;
		i->a.r[0].is_val = true;
		i->a.r[0].val = 0;
		return true;
	}

	/* x * 1, x + 0, x - 0    ->  x */
	if ((op == OP_MUL && vnum_eq(r1, &vn_1)) ||
	    (op == OP_ADD && vnum_eq(r1, &vn_0)) ||
	    (op == OP_SUB && vnum_eq(r1, &vn_0))) {
		i->a.op = i->a.r[0].is_val ? OP_INTEGER_CONSTANT : OP_IDENTIFIER;
		return true;
	}

	/* 1 * x, 0 + x           ->  x */
	if ((op == OP_MUL && vnum_eq(r0, &vn_1)) ||
	    (op == OP_ADD && vnum_eq(r0, &vn_0))) {
		i->a.op = i->a.r[1].is_val ? OP_INTEGER_CONSTANT : OP_IDENTIFIER;
		memcpy(&i->a.r[0], &i->a.r[1], sizeof(inst_op_t));
		return true;
	}

	/* x + x                  ->  2 * x */
	if (op == OP_ADD && vnum_eq(r0, r1)) {
		i->a.op = OP_MUL;
		i->a.r[0].is_val = true;
		i->a.r[0].val = 2;
		return true;
	}

	return false;
}

/* callback for the value numbering table dump */
static void value_numbering_dump_cb(void *_ctx, char *id, void *_vnum)
{
	vnum_t *vnum = _vnum;

	if (vnum->is_constant) {
		fprintf(stderr, "%8s: %d, const.\n", id, vnum->number);
	} else if (vnum->op == OP_NONE) {
		fprintf(stderr, "%8s: #%d\n",
		       id, vnum->number);
	} else if (op_type[vnum->op] == OP_T_UNARY) {
		fprintf(stderr, "%8s: #%d (%s #%d)\n",
		       id, vnum->number,
		       expr_op_to_glyph[vnum->op],
		       vnum->r[0]->number);
	} else if (op_type[vnum->op] == OP_T_UNARY) {
		fprintf(stderr, "%8s: #%d (#%d %s #%d)\n",
		       id, vnum->number,
		       vnum->r[0]->number,
		       expr_op_to_glyph[vnum->op],
		       vnum->r[1]->number);
	} else {
		fprintf(stderr, "%8s: #%d\n",
		       id, vnum->number);
	}
}

__unused static void value_numbering_dump(bb_node_t *bb, vnum_ctx_t *ctx)
{
	fprintf(stderr, "  value numbering pass over #%d\n", bb->label);

	htab_each(ctx->vtab, value_numbering_dump_cb, ctx);
}

/* performs value numbering for the given basic block, using the given
   value numbering context */
static void value_numbering_ctx(bb_node_t *bb, vnum_ctx_t *ctx)
{
	vnum_ctx_t *new_ctx;
	list_node_t *cur;
	inst_t *i;
	vnum_t *r0, *r1, *rhs;
	char *id, *idp;

	if (is_dummy(bb))
		goto skip;

	LIST_EACH(bb->instructions, cur, i) {
		switch (i->type) {
		case I_ATTRIBUTE:
			r0 = vnum_for_inst_op(ctx, &i->a.r[0], true);
			r1 = vnum_for_attribute(ctx, i->a.r[1].id, true);

			rhs = vnum_for_expr(ctx, i->type, OP_ATTRIBUTE, r0, r1);

			if (rhs == NULL || (id = id_for_vnum(ctx, rhs)) == NULL) {
				/* no matching RHS, create new value */
				vnum_put_expr(ctx, i->a.l.id,
				              i->type, OP_ATTRIBUTE, r0, r1);
			} else {
				/* matching RHS, update instruction */
				i->type = I_ASSIGN;
				i->a.op = OP_IDENTIFIER;
				i->a.r[0].is_val = false;
				i->a.r[0].id = id;
				vnum_put_dupe(ctx, i->a.l.id, rhs);
			}

			continue;

		case I_LOAD:
			r0 = vnum_for_inst_op(ctx, &i->m.src, true);
			rhs = vnum_for_expr(ctx, I_LOAD, OP_IDENTIFIER, r0, NULL);

			if (rhs == NULL || (id = id_for_vnum(ctx, rhs)) == NULL) {
				/* no matching RHS, create new value */
				vnum_put_expr(ctx, i->m.dst.id,
				              I_LOAD, OP_IDENTIFIER, r0, NULL);
			} else {
				/* matching RHS, update instruction */
				i->type = I_ASSIGN;
				i->a.op = OP_IDENTIFIER;
				idp = i->m.dst.id;
				i->a.l.is_val = false;
				i->a.l.id = idp;
				i->a.r[0].is_val = false;
				i->a.r[0].id = id;
				vnum_put_dupe(ctx, i->a.l.id, rhs);
			}

			continue;

		case I_ASSIGN:
			/* break to more complex numbering if I_ASSIGN */
			break;

		case I_STORE:
			/* this is an incredible hack. we never actually
			   modify the store, but we DO add an expression for
			   the matching load, so that future unneccessary
			   loads are eliminated */
			/*if (!i->m.src.is_val) {
				r1 = vnum_for_inst_op(ctx, &i->m.dst, true);
				vnum_put_expr(ctx, i->m.src.id,
				              I_LOAD, OP_IDENTIFIER, r1, NULL);
			}*/
			continue;

		default:
			/* skip any other instruction types */
			continue;
		}

		/* we jump to the "repeat" label if we change an instruction
		   enough to warrant a second value numbering pass on it */
	repeat:

		switch (op_type[i->a.op]) {
		case OP_T_IGNORE_ME_I_SUCK:
			break;

		case OP_T_UNARY:
			/* for unary operators, the process is often as simple
			   as copying value numbering properties from the right
			   hand side to the left hand side */

			if (i->a.r[0].is_val) {
				vnum_put_constant(ctx, i->a.l.id, i->a.r[0].val);
				break;
			}

			r0 = vnum_for_id(ctx, i->a.r[0].id, true);

			if (r0->is_constant) {
				vnum_put_constant(ctx, i->a.l.id, r0->number);
				i->a.r[0].is_val = true;
				i->a.r[0].val = r0->number;
			} else {
				vnum_put_dupe(ctx, i->a.l.id, r0);
			}

			break;

		case OP_T_BINARY:
			/* for binary operators, start by getting a value
			   number for the left and right operands, allocating
			   a new number if needed */
			r0 = vnum_for_inst_op(ctx, &i->a.r[0], true);
			r1 = vnum_for_inst_op(ctx, &i->a.r[1], true);

			/* if the expression, given the value numbers, can
			   be somehow reduced to a simpler expression, then
			   try that and redo this round of value numbering
			   for the new instruction */
			if (try_reduce(ctx, i, r0, r1))
				goto repeat;

			/* if both the left and right operands represent
			   known values, then this instruction becomes simple
			   constant assignment */
			if (r0->is_constant && r1->is_constant) {
				int x = run_op(i->a.op, r0->number, r1->number);
				vnum_put_constant(ctx, i->a.l.id, x);
				i->a.op = OP_INTEGER_CONSTANT;
				i->a.r[0].is_val = true;
				i->a.r[0].val = x;
				break;
			}

			/* otherwise, check to see if the right hand side has
			   been computed before. */
			rhs = vnum_for_expr(ctx, i->type, i->a.op, r0, r1);

			if (rhs == NULL || (id = id_for_vnum(ctx, rhs)) == NULL) {
				/* no matching RHS, create new value */
				vnum_put_expr(ctx, i->a.l.id,
				              I_ASSIGN, i->a.op, r0, r1);
			} else {
				/* matching RHS, update instruction */
				i->a.op = OP_IDENTIFIER;
				i->a.r[0].is_val = false;
				i->a.r[0].id = id;
				vnum_put_dupe(ctx, i->a.l.id, rhs);
			}

			break;

		default:
			ice("invalid operand type");
		}
	}

	//value_numbering_dump(bb, ctx);

skip:
	
	/* now do children! if a child node has only one incoming edge,
	 * it is part of the same extended basic block as this one, so
	 * pass the parent */
	if (bb->fb != NULL && bb->fb->visited == 0) {
		if (bb->fb->parent_nodes->length != 1)
			new_ctx = vnum_ctx_new(NULL);
		else
			new_ctx = vnum_ctx_new(ctx);

		bb->fb->visited = 1;
		value_numbering_ctx(bb->fb, new_ctx);
	}
	
	if (bb->tb != NULL && bb->tb->visited == 0) {
		if (bb->tb->parent_nodes->length != 1)
			new_ctx = vnum_ctx_new(NULL);
		else
			new_ctx = vnum_ctx_new(ctx);

		bb->tb->visited = 1;
		value_numbering_ctx(bb->tb, new_ctx);
	}
}

/* performs basic value numbering for the given cfg */
void value_numbering_basic(cfg_context_t *cfg)
{
	vnum_ctx_t *ctx = vnum_ctx_new(NULL);
	list_node_t *cur;
	bb_node_t *bb;

	/* set visited flag for all BBs. this is a really bad hack to prevent
	   value_numbering_ctx from trying to recurse */
	LIST_EACH(cfg->all_bb, cur, bb)
		bb->visited = 1;

	LIST_EACH(cfg->all_bb, cur, bb) {
		ctx = vnum_ctx_new(NULL);
		value_numbering_ctx(bb, ctx);
		vnum_ctx_release(ctx);
	}
}

/* performs extended value numbering for the given cfg */
void value_numbering_extended(cfg_context_t *cfg)
{
	vnum_ctx_t *ctx = vnum_ctx_new(NULL);
	list_node_t *cur;
	bb_node_t *bb;

	/* reset visited flag for all basic blocks */
	LIST_EACH(cfg->all_bb, cur, bb)
		bb->visited = 0;

	/* as far as I know, this should only land on the entry node, but
	   you can never be too careful! */
	LIST_EACH(cfg->all_bb, cur, bb) {
		/* in addition to skipping visited nodes, we also skip nodes
		   with exactly one parent node, as they are definitely part
		   of another extended basic block, and so not the root of
		   any extended value numbering pass. */
		if (bb->visited || bb->parent_nodes->length == 1)
			continue;

		ctx = vnum_ctx_new(NULL);
		value_numbering_ctx(bb, ctx);
		vnum_ctx_release(ctx);
	}
}
