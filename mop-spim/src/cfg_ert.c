/*
 * CSE 440, Project 3
 * Mini Object Pascal Code Generation
 *
 * Alex Iadicicco
 * shmibs
 */

/* The value numbering pass produces many simple assignments of the form
   '_tX <- _tY'. These are wasteful, and removing them by replacing all
   _tX with _tY and deleting the assignment is a relatively straightforward
   optimization opportunity, since the _tX are already in an SSA form due
   to the way they're constructed. */

#include <string.h>
#include "cfg.h"
#include "container.h"

/* The main data structure here is a hash table mapping variable names
   to their replacements. The compiler iterates over all basic blocks
   noting instructions that create aliases. Special provisions are made
   for temporaries carrying a constant value: if the temporary is replaced
   anywhere, a second value numbering pass is needed, as more constant folding
   opportunities may have arisen. However, the value numbering pass SHOULD
   catch all such replacements, and this should never happen. */

typedef struct _ert_context ert_context_t;

struct _ert_context {
	/* char *id -> inst_op_t *replace */
	htab_t *replacements;
	bool replaced_with_constant;
};

static bool is_temporary(inst_op_t *op)
{
	return !op->is_val && op->id[0] == '_' && op->id[1] == 't';
}

static void apply_replacement(ert_context_t *ctx, inst_op_t *op)
{
	inst_op_t *r;

	if (op == NULL || op->is_val)
		return;

	if (!(r = htab_get(ctx->replacements, op->id)))
		return;

	if (r->is_val)
		ctx->replaced_with_constant = true;

	memcpy(op, r, sizeof(*op));
}

static void apply_replacements(ert_context_t *ctx, inst_t *i)
{
	/* candidates for replacement */
	inst_op_t *c0 = NULL;
	inst_op_t *c1 = NULL;

	list_node_t *n;

	switch (i->type) {
	case I_ASSIGN:
		switch (i->a.op) {
		case OP_INDEX:
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
			c1 = &i->a.r[1];
			/* fallthrough */
		case OP_IDENTIFIER:
		case OP_INTEGER_CONSTANT:
		case OP_NOT:
			c0 = &i->a.r[0];
			break;

		default:
			ice("apply_replacements: unknown I_ASSIGN op!");
		}
		break;

	case I_IF:
		c0 = &i->cond;
		break;

	case I_ATTRIBUTE:
		c0 = &i->a.r[0];
		break;

	case I_LOAD:
	case I_STORE:
		c0 = &i->m.src;
		c1 = &i->m.dst;
		break;

	case I_CALL:
		LIST_EACH(i->c.args, n, c0)
			apply_replacement(ctx, c0);
		break;

	case I_ALLOC:
		break;

	case I_PRINT:
		c0 = &i->a.r[0];
		break;
	}

	apply_replacement(ctx, c0);
	apply_replacement(ctx, c1);
}

static void ert_run_bb(ert_context_t *ctx, bb_node_t *bb)
{
	list_node_t *n, *next;

	if (is_dummy(bb))
		return;

	LIST_EACH_NODE_SAFE(bb->instructions, n, next) {
		inst_t *i = n->v;

		apply_replacements(ctx, i);

		if (i->type != I_ASSIGN || !is_temporary(&i->a.l))
			continue;

		switch (i->a.op) {
		case OP_IDENTIFIER:
		case OP_INTEGER_CONSTANT:
			/* we don't have to copy the inst_op_t if we're not
			   actually deleting the associated inst_t! */
			htab_put(ctx->replacements, i->a.l.id, &i->a.r[0]);
			list_delete(bb->instructions, n);
			break;

		default:
			break;
		}
	}
}

bool eliminate_redundant_temporaries(cfg_context_t *cfg)
{
	ert_context_t ctx;
	list_node_t *n;
	bb_node_t *bb;

	ctx.replacements = htab_new(HTAB_DEFAULT_ORDER);
	ctx.replaced_with_constant = false;

	LIST_EACH(cfg->all_bb, n, bb)
		ert_run_bb(&ctx, bb);

	return ctx.replaced_with_constant;
}
