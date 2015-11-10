/*
 * CSE 440, Project 3
 * Mini Object Pascal Code Generation Common
 *
 * Alex Iadicicco
 * shmibs
 */

#include <string.h>
#include "container.h"
#include "cfg.h"
#include "cg.h"

#define DEBUG(X...) fprintf(stderr, X)
#undef DEBUG
#define DEBUG(X...)

/* common code generation actions */

unsigned get_field_size(type_t *type)
{
	if (type->tag == T_CLASS)
		return INTEGERS_IN_POINTER;

	type->size = get_size(type);
	return type->size;
}

unsigned get_size(type_t *type)
{
	unsigned total;
	list_node_t *n;
	symbol_t *sym;

	if (type->size != 0)
		return type->size;

	switch (type->tag) {
	case T_INTEGER:
	case T_BOOLEAN:
		return 1;

	case T_ARRAY:
		total = get_field_size(type->arr.of);
		return (type->arr.high - type->arr.low + 1) * total;

	case T_CLASS:
		total = type->cls->extends ? get_size(type->cls->extends) : 0;
		LIST_EACH(type->cls->fields, n, sym) {
			if (sym->tag != SYM_FIELD)
				continue; /* ICE */

			sym->offset = total;
			total += get_field_size(sym->t);
		}

		DEBUG("    size of %s = %u\n", type->cls->name, total);

		return total;
	}

	/* ICE */
	return 0;
}

static void allocate_fields(void *unused, char *name, void *_type)
{
	type_t *type = _type;

	type->size = get_size(type);
}

/* ok this variable allocation here is very bad because even though we're
   trying to be cross-platform about things, we assume arguments go down the
   stack and variables go up it. therefore, the offsets of arguments do not
   include the size and the offsets of variables do. */

struct local_allocation_ctx {
	int argument_offset;
	int variable_offset;
};

static void allocate_one_local(void *_ctx, char *name, void *_sym)
{
	struct local_allocation_ctx *ctx = _ctx;
	symbol_t *sym = _sym;

	switch (sym->tag) {
	case SYM_THIS:
	case SYM_VAL_PARAMETER:
	case SYM_REF_PARAMETER:
		break;

	case SYM_FUNCTION_LOCAL:
		ctx->variable_offset += get_field_size(sym->t);
		sym->offset = ctx->variable_offset;
		break;

	default:
		return; /* ICE */
	}
}

static void allocate_locals(void *unused, char *name, void *_type)
{
	type_t *type = _type;
	list_node_t *n, *n2;
	func_t *fn;
	symbol_t *arg;
	char buf[2048];

	struct local_allocation_ctx ctx;

	if (type->tag != T_CLASS)
		return; /* ICE? */

	LIST_EACH(type->cls->functions, n, fn) {
		snprintf(buf, 2048, "%s_%s", type->cls->name, fn->name);
		fn->mangled_name = strdup(buf);
		DEBUG("    %s:\n", fn->mangled_name);

		/* implicit 'this' */
		ctx.argument_offset = INTEGERS_IN_POINTER;
		ctx.variable_offset = 0;

		htab_each(fn->local->tab, allocate_one_local, &ctx);

		LIST_EACH(fn->arguments, n2, arg) {
			switch (arg->tag) {
			case SYM_VAL_PARAMETER:
				arg->offset = ctx.argument_offset;
				ctx.argument_offset += get_field_size(arg->t);
				break;

			case SYM_REF_PARAMETER:
				arg->offset = ctx.argument_offset;
				ctx.argument_offset += INTEGERS_IN_POINTER;
				break;

			default:
				break; /* ICE */
			}
		}

		DEBUG("      %u for arguments\n", ctx.argument_offset);
		DEBUG("      %u for locals\n", ctx.variable_offset);
	}
}

void allocate_symbols(prog_t *p)
{
	DEBUG("  allocating class fields\n");
	htab_each(p->types, allocate_fields, NULL);

	DEBUG("  allocating function locals\n");
	htab_each(p->types, allocate_locals, NULL);
}
