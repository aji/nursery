/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

#ifndef __CFG_H__
#define __CFG_H__

#include <stdbool.h>

#include "container.h"
#include "type.h"

typedef enum _inst_type inst_type_t;
typedef struct _bb_node bb_node_t;
typedef struct _inst inst_t;
typedef struct _inst_op inst_op_t;
typedef struct _cfg_context cfg_context_t;

enum _inst_type {
	I_ASSIGN,
	I_IF,
	I_ATTRIBUTE,
	I_LOAD,
	I_STORE,
	I_CALL,
	I_ALLOC,
	I_PRINT,
};

struct _bb_node {
	bool is_dummy;
	bool has_condition;
	list_node_t *n; /* node in cfg's all_bb */
	list_t *instructions;

	bb_node_t *tb;
	bb_node_t *fb;

	/* only used during dump/traversal: */
	int visited;
	int label;
	bool is_while;

	/* a list of bb_node_t* */
	list_t *parent_nodes;
};

struct _inst_op { /* instruction operand */
	bool is_val;
	union {
		char *id;
		int val;
	};

	/* only used as a temporary annotation: */
	int value_number;
};

struct _inst {
	inst_type_t type;

	union {
		/* assignment between operands */
		struct {
			/* can be anything from OP_NULL to OP_MOD,
			   not including boolean ops */
			expr_op_t op;
			inst_op_t l, r[2];
			type_t *t; /* for OP_INDEX only */
		} a;

		inst_op_t cond;

		/* explicit memory access */
		struct {
			/* source value, dest address if the source is
			   also an address, it should be 'converted'
			   to a value using an I_LOAD */
			inst_op_t dst, src;
		} m;

		/* function call */
		struct {
			/* the actual function for any call can be
			   resolved at compile time, so we store
			   that explicitly here. the implicit 'this'
			   parameter when making a call is passed as the
			   first element of 'args'. 'args' is a list of
			   inst_op_t* */
			func_t *fn;
			list_t *args;
			/* if ret.is_val == true, then the return value
			   is to be discarded */
			inst_op_t ret;
		} c;

		struct {
			inst_op_t dst;
			type_t *t;
		} alloc;
	};
};

struct _cfg_context {
	/* the containing function */
	func_t *fn;

	/* basic blocks */
	bb_node_t *entry, *exit;
	list_t *all_bb;

	/* used only during dump: */
	int next_label;
};

/* == cfg.c == */

extern cfg_context_t* func_to_cfg(func_t *f);

/* dumps cfg to stdout, very lazily */
extern void dump_cfg(cfg_context_t *cfg);

/* performs the given operation on the constant operands */
extern int run_op(expr_op_t op, int a, int b);

/* determines if the given block is a dummy block */
extern bool is_dummy(bb_node_t *bb);

/* == cfg_vnum.c == */

/* performs simple value numbering on a basic block */
extern void value_numbering_basic(cfg_context_t *cfg);

/* performs extended value numbering on a basic block */
extern void value_numbering_extended(cfg_context_t *cfg);

/* == cfg_ert.c == */

/* deletes temporary variables which are only aliases for others */
extern bool eliminate_redundant_temporaries(cfg_context_t *cfg);

#endif
