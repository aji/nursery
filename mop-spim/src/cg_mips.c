/*
 * CSE 440, Project 3
 * Mini Object Pascal MIPS Code Generation
 *
 * Alex Iadicicco
 * shmibs
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "error.h"
#include "cfg.h"
#include "cg.h"

#define TAB "\t"

/* the reg_t type is supposed to be very opaque */

typedef enum {
	REG_ZERO   = 0,
	REG_AT,

	REG_V0, REG_V1,

	REG_A0, REG_A1, REG_A2, REG_A3,

	REG_T0, REG_T1, REG_T2, REG_T3,
	REG_T4, REG_T5, REG_T6, REG_T7,

	REG_S0, REG_S1, REG_S2, REG_S3,
	REG_S4, REG_S5, REG_S6, REG_S7,

	REG_T8, REG_T9,

	REG_K0, REG_K1,

	REG_GP, REG_SP, REG_FP, REG_RA,
} reg_t;

static const unsigned REG_SAVES =
	(1 << REG_S0) | (1 << REG_S1) | (1 << REG_S2) | (1 << REG_S3) |
	(1 << REG_S4) | (1 << REG_S5) | (1 << REG_S6) | (1 << REG_S7);
static const unsigned REG_TEMPS =
	(1 << REG_T0) | (1 << REG_T1) | (1 << REG_T2) | (1 << REG_T3) |
	(1 << REG_T4) | (1 << REG_T5) | (1 << REG_T6) | (1 << REG_T7) |
	(1 << REG_T8) | (1 << REG_T9);
static const unsigned REG_NEVER_SAVE = (1 << REG_ZERO) |
	(1 << REG_AT) | (1 << REG_V0) | (1 << REG_V1) |
	(1 << REG_K0) | (1 << REG_K1) | (1 << REG_GP) | (1 << REG_SP);

static const char *reg_names[] = {
	"$zero",
	"$at",

	"$v0", "$v1",

	"$a0", "$a1", "$a2", "$a3",

	"$t0", "$t1", "$t2", "$t3",
	"$t4", "$t5", "$t6", "$t7",

	"$s0", "$s1", "$s2", "$s3",
	"$s4", "$s5", "$s6", "$s7",

	"$t8", "$t9",

	"$k0", "$k1",

	"$gp", "$sp", "$fp", "$ra",
};

#define _N(reg) (reg_names[(reg)]) /* register to char* name */

static FILE *output = NULL;

#define NUM_EOL 10
static int cur_col = 0;
static char eol_extra[NUM_EOL][512];
static int eol_top = 0;

/* emit line */
static void _E(const char *fmt, ...)
{
	va_list va;

	if (output == NULL)
		output = stdout;

	va_start(va, fmt);
	cur_col = vfprintf(output, fmt, va);
	if (eol_top) {
		eol_top--;
		while (cur_col < 30) {
			putc(' ', output);
			cur_col++;
		}
		fprintf(output, "%s", eol_extra[eol_top]);
	}
	putc('\n', output);
	va_end(va);
}

/* emit comment */
static void _E_C(const char *fmt, ...)
{
	va_list va;

	if (output == NULL)
		output = stdout;

	va_start(va, fmt);
	fprintf(output, "\t# ");
	vfprintf(output, fmt, va);
	fprintf(output, "\n");
	va_end(va);
}

/* set end-of-line */
static void _E_EOL(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vsnprintf(eol_extra[eol_top++], 512, fmt, va);
	va_end(va);
}

/* emit blank line */
#define _E_LF() _E("")

static reg_t number_to_register(int x) { return x; }

static int hsum(unsigned v)
{
	/* from the famous stanford bithacks page, "The best method for
	   counting bits in a 32-bit integer v is the following: */

	v = v - ((v >> 1) & 0x55555555);
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
	return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

/****************************************************************************
 **  REGISTER ALLOCATOR                                                    **
 ****************************************************************************/

/* these register allocators are per-function */

/* efficient code generation needs lots of information about what's going
   on with the register allocator in the current block of code. while it's
   certainly possible to save all the save registers at the start of a
   function, and all the temp registers at the beginning of every call site,
   it's also a good way to waste memory and cpu cycles.

   however, efficiency can be handled after everything is working.. */

/* $t3 through $t9 are allowed to be used as real locations for
 * loc_t during execution */
#define LOC_REG_FIRST  REG_T3
#define LOC_REG_LAST   REG_T9
#define LOC_REG_COUNT  (LOC_REG_LAST - LOC_REG_FIRST + 1)

typedef enum {
	L_ARGUMENT,
	L_STACK,
	L_REGISTER,
	L_ARRAY,
} loc_type_t;

typedef struct {
	loc_type_t type;
	union {
		reg_t reg;
		unsigned num;
	};
	char *name;
} loc_t;

typedef struct {
	/* a table which maps generic names to actual locations */
	htab_t *loc_tab;
	/* array to show what registers are in use at any given
	 * time */
	bool used_registers[LOC_REG_COUNT];
	unsigned arg_count;
	unsigned stack_count;
} loc_context_t;

static loc_context_t *loc_context_new()
{
	loc_context_t *l;

	l = calloc(1, sizeof(*l));

	l->loc_tab = htab_new(HTAB_DEFAULT_ORDER);

	return l;
}

/* look through the current function's list of locations and return
 * one that matches the given key. return NULL if not found */
static inline loc_t *_L(char *key, loc_context_t *ctx)
{
	loc_t *l = htab_get(ctx->loc_tab, key);
	if (l == NULL)
		ice("no location for %s", key);
	return l;
}

static inline loc_t *_L_unchecked(char *key, loc_context_t *ctx)
{
	return htab_get(ctx->loc_tab, key);
}

static loc_t *loc_add_real(char *key, loc_context_t *ctx, bool is_arg)
{
	loc_t *l;
	int i;

	l = calloc(1, sizeof(*l));
	l->name = key;

	if(is_arg) {
		l->type = L_ARGUMENT;
		l->num = ctx->arg_count;
		ctx->arg_count++;
		htab_put(ctx->loc_tab, key, (void*)l);
		return l;
	} else {
		/* try to put it in a register */
		for(i = 0; i < LOC_REG_COUNT; i++) {
			if(ctx->used_registers[i] == false) {
				ctx->used_registers[i] = true;
				l->type = L_REGISTER;
				l->reg = i+LOC_REG_FIRST;
				htab_put(ctx->loc_tab, key, (void*)l);
				return l;
			}
		}

		/* if that fails, put it on the stack */
		l->type = L_STACK;
		ctx->stack_count++;
		l->num = ctx->stack_count;
		htab_put(ctx->loc_tab, key, (void*)l);
		return l;
	}
}

static loc_t *loc_add_array(char *key, type_t *t, loc_context_t *ctx)
{
	loc_t *l;

	l = calloc(1, sizeof(*l));
	l->name = key;

	l->type = L_ARRAY;
	ctx->stack_count += get_size(t);
	l->num = ctx->stack_count;
	htab_put(ctx->loc_tab, key, l);

	return l;
}

static loc_t *loc_add(char *key, loc_context_t *ctx)
{
	return loc_add_real(key, ctx, false);
}

static unsigned loc_get_used_mask(loc_context_t *ctx)
{
	unsigned mask = 0;
	int i;

	for (i=0; i<LOC_REG_COUNT; i++) {
		if (ctx->used_registers[i])
			mask |= 1 << (i + LOC_REG_FIRST);
	}

	return mask;
}

/* if l's real location is already a register, that register is returned.
 * else, the value is copied to register dest and dest is returned */
static reg_t loc_get(reg_t dest, loc_t *l)
{
	switch(l->type) {
	case L_ARGUMENT:
		_E_EOL("  # argument %s", l->name);
		_E(TAB "lw %s, %u($fp)", _N(dest), 2 * BYTES_IN_POINTER +
				l->num * BYTES_IN_INTEGER);
		return dest;

	case L_STACK:
		_E_EOL("  # stack %s", l->name);
		_E(TAB "lw %s, -%u($fp)", _N(dest), l->num * BYTES_IN_INTEGER);
		return dest;

	case L_ARRAY:
		_E_EOL("  # array %s", l->name);
		_E(TAB "addi %s, $fp, -%u", _N(dest), l->num * BYTES_IN_INTEGER);
		return dest;

	case L_REGISTER:
		return l->reg;

	default:
		/* アイス */
		exit(EXIT_FAILURE);
	}
}

/* subroutine that ensures that the value in register source is copied into
 * the real location associated with l */
static void loc_store(reg_t source, loc_t *l)
{
	switch(l->type) {

	case L_ARGUMENT:
		_E_EOL("  # argument %s", l->name);
		_E(TAB "sw %s, %u($fp)", _N(source), 2 * BYTES_IN_POINTER +
				l->num * BYTES_IN_INTEGER);
		return;

	case L_STACK:
		_E_EOL("  # stack %s", l->name);
		_E(TAB "sw %s, -%u($fp)", _N(source), l->num * BYTES_IN_INTEGER);
		return;

	case L_REGISTER:
		if(source == l->reg)
			return;

		_E_EOL("  # register %s", l->name);
		_E(TAB "add %s, $zero, %s", _N(l->reg), _N(source));
		return;

	case L_ARRAY:
		ice("tried to store into the array itself");

	default:
		/* アイス */
		exit(EXIT_FAILURE);
	}
}

/****************************************************************************
 **  MIPS CODE GENERATION                                                  **
 ****************************************************************************/

static void emit_header(void)
{
	_E_LF();
	_E_LF();
	_E(TAB ".text");
	_E(TAB "j main");
	_E_LF();
	_E("__builtin_print:");
	_E_C("1 = print int syscall");
	_E(TAB "addi $v0, $zero, 1");
	_E(TAB "syscall");
	_E_C("11 = print character syscall");
	_E(TAB "la $a0, 10  # 10 is \\n");
	_E(TAB "addi $v0, $zero, 11");
	_E(TAB "syscall");
	_E(TAB "jr $ra");
}

/* clobbers: a0, v0 */
static void emit_malloc(size_t n, reg_t result)
{
	_E(TAB "addi $a0, $zero, %u", n);
	_E(TAB "addi $v0, $zero, 9"); /* 9 = sbrk syscall */
	_E(TAB "syscall  # allocate %u bytes", n);
	if (result != REG_V0)
		_E(TAB "add %s, $zero, $v0", _N(result));
}

/* clobbers: a0, v0 */
static void emit_print_register(reg_t value)
{
	if (value != REG_A0)
		_E(TAB "add $a0, $zero, %s", _N(value));
	_E(TAB "jal __builtin_print");
}

/* clobbers: a0, v0 */
static void emit_print_immediate(unsigned value)
{
	_E(TAB "addi $a0, $zero, %u", value);
	_E(TAB "jal __builtin_print");
}

/* clobbers everything, since it closes the program */
static void emit_exit(void)
{
	_E_C("exit syscall");
	_E(TAB "addi $v0, $zero, 10"); /* 10 = exit syscall */
	_E(TAB "syscall");
}

static void emit_save_registers(unsigned regmask)
{
	int i, current_offset = 0;

	if (regmask == 0)
		return;

	_E(TAB "addi $sp, $sp, -%u", hsum(regmask) * 4);

	for (i=0; i<32; i++) {
		if (!(regmask & (1 << i)))
			continue;

		_E(TAB "sw %s, %d($sp)",
		   _N(number_to_register(i)),
		   current_offset);
		current_offset += 4;
	}
}

static void emit_restore_registers(unsigned regmask)
{
	int i, current_offset = 0;

	if (regmask == 0)
		return;

	for (i=0; i<32; i++) {
		if (!(regmask & (1 << i)))
			continue;

		_E(TAB "lw %s, %d($sp)",
		   _N(number_to_register(i)),
		   current_offset);
		current_offset += 4;
	}

	_E(TAB "addi $sp, $sp, %u", hsum(regmask) * 4);
}

/* stack setup:

   higher addresses           |
     :   ^             :      | stack direction
     |   ^             |      V
     | arguments       |
     +-----------------+
     | caller fp       |
     +-----------------+
     | current $ra     |
     +-----------------+<-- current fp
     | local variables |
     |   v             |
     :   v             :
   lower addresses

 */

/* $t0, $t1, and $t2 are reserved for use as locations into which values are
 * loaded before calling */
void mips_set(char *key1, char *key2, inst_t *i, loc_context_t *ctx)
{
	loc_t *ldest;
	reg_t r1, r2, rdest;
	bool key1_is_const, key2_is_const;

	if(!strcmp("0", key1) || atoi(key1) != 0) {
		key1_is_const = true;
		r1 = REG_T0;
	} else {
		key1_is_const = false;
		r1 = loc_get(REG_T0, _L(key1, ctx));
	}

	if (i->a.op == OP_NOT) {
		key2_is_const = true;
		r2 = REG_T1;
	} else if (!strcmp("0", key2) || atoi(key2) != 0) {
		if(key1_is_const)
			ice("two constants used in the same set operation");

		key2_is_const = true;
		r2 = REG_T1;
	} else {
		key2_is_const = false;
		r2 = loc_get(REG_T1, _L(key2, ctx));
	}

	/* the destination does not, though */
	ldest = _L_unchecked(i->a.l.id, ctx);

	if(ldest == NULL)
		ldest = loc_add(i->a.l.id, ctx);

	if(ldest->type == L_REGISTER)
		rdest = loc_get(REG_T2, ldest);
	else
		rdest = REG_T2;

	_E_EOL("  # %s = %s %s %s", i->a.l.id, expr_op_to_name[i->a.op],
	       key1, i->a.op == OP_NOT ? "" : key2);

	switch(i->a.op) {
	case OP_INDEX:
		/* we don't use BYTES_IN_POINTER or BYTES_IN_INTEGER (this
		   comment so greps turn it up) because I didn't feel like
		   adding POINTER_SHIFT or INTEGER_SHIFT or anything */

		if (key1_is_const && key2_is_const) {
			_E(TAB "addi %s, $zero, %i", _N(rdest),
			   atoi(key1) + 4 * (atoi(key2) * get_field_size(i->a.t)));
		} else if (key1_is_const) {
			if (get_size(i->a.t) != 1) {
				_E(TAB "addi %s, $zero, %d", _N(rdest),
				   4 * get_field_size(i->a.t));
				_E(TAB "multu %s, %s", _N(rdest), _N(r2));
				_E(TAB "mflo %s", _N(rdest));
			} else {
				_E(TAB "sll %s, %s, 2", _N(rdest), _N(r2));
			}
			_E(TAB "addi %s, %s, %s", _N(rdest), _N(rdest), key1);
		} else if (key2_is_const) {
			_E(TAB "addi %s, %s, %i", _N(rdest), _N(r1),
			   4 * atoi(key2) * get_field_size(i->a.t));
		} else {
			if (get_field_size(i->a.t) != 1) {
				_E(TAB "addi %s, $zero, %d", _N(rdest),
				   4 * get_field_size(i->a.t));
				_E(TAB "multu %s, %s", _N(rdest), _N(r2));
				_E(TAB "mflo %s", _N(rdest));
			} else {
				_E(TAB "sll %s, %s, 2", _N(rdest), _N(r2));
			}
			_E(TAB "add %s, %s, %s", _N(rdest), _N(rdest), _N(r1));
		}
		break;

	case OP_ADD:
		if(key1_is_const)
			_E(TAB "addi %s, %s, %s", _N(rdest), _N(r2), key1);
		else if(key2_is_const)
			_E(TAB "addi %s, %s, %s", _N(rdest), _N(r1), key2);
		else
			_E(TAB "add %s, %s, %s", _N(rdest), _N(r1), _N(r2));
		break;

	case OP_SUB:
		if(key1_is_const)
			_E(TAB "addi %s, %s, -%s", _N(rdest), _N(r2), key1);
		else if(key2_is_const)
			_E(TAB "addi %s, %s, -%s", _N(rdest), _N(r1), key2);
		else
			_E(TAB "sub %s, %s, %s", _N(rdest), _N(r1), _N(r2));
		break;

	case OP_AND:
		if(key1_is_const)
			_E(TAB "andi %s, %s, %s", _N(rdest), _N(r2), key1);
		else if(key2_is_const)
			_E(TAB "andi %s, %s, %s", _N(rdest), _N(r1), key2);
		else
			_E(TAB "and %s, %s, %s", _N(rdest), _N(r1), _N(r2));
		break;

	case OP_NOT:
		if (key1_is_const)
			_E(TAB "addi %s, $zero, %d", _N(rdest), !atoi(key1));
		else
			_E(TAB "xori %s, %s, 1", _N(rdest), _N(r1));
		break;

	case OP_OR:
		if(key1_is_const)
			_E(TAB "ori %s, %s, %s", _N(rdest), _N(r2), key1);
		else if(key2_is_const)
			_E(TAB "ori %s, %s, %s", _N(rdest), _N(r1), key2);
		else
			_E(TAB "or %s, %s, %s", _N(rdest), _N(r1), _N(r2));
		break;

	case OP_MUL:
		if(key1_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r1), key1);
		if(key2_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r2), key2);

		_E(TAB "mult %s, %s", _N(r1), _N(r2));
		_E(TAB "mflo %s", _N(rdest));
		break;

	case OP_DIV:
		if(key1_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r1), key1);
		if(key2_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r2), key2);

		_E(TAB "div %s, %s", _N(r1), _N(r2));
		_E(TAB "mflo %s", _N(rdest));
		break;

	case OP_MOD:
		if(key1_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r1), key1);
		if(key2_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r2), key2);

		_E(TAB "div %s, %s", _N(r1), _N(r2));
		_E(TAB "mfhi %s", _N(rdest));
		break;

	case OP_EQ:
		if(key1_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r1), key1);
		if(key2_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r2), key2);

		_E(TAB "xor %s, %s, %s", _N(rdest), _N(r1), _N(r2));
		_E(TAB "slt %s, $zero, %s", _N(rdest), _N(rdest));
		_E(TAB "xori %s, %s, 1", _N(rdest), _N(rdest));
		break;

	case OP_NE:
		if(key1_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r1), key1);
		if(key2_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r2), key2);

		_E(TAB "xor %s, %s, %s", _N(rdest), _N(r1), _N(r2));
		_E(TAB "slt %s, $zero, %s", _N(rdest), _N(rdest));
		break;

	case OP_LT:
		if(key1_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r1), key1);
		if(key2_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r2), key2);

		_E(TAB "slt %s, %s, %s", _N(rdest), _N(r1), _N(r2));
		break;

	case OP_GT:
		if(key1_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r1), key1);
		if(key2_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r2), key2);

		_E(TAB "slt %s, %s, %s", _N(rdest), _N(r2), _N(r1));
		break;

	case OP_LE:
		if(key1_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r1), key1);
		if(key2_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r2), key2);

		_E(TAB "slt %s, %s, %s", _N(rdest), _N(r2), _N(r1));
		_E(TAB "xori %s, %s, 1", _N(rdest), _N(rdest));
		break;

	case OP_GE:
		if(key1_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r1), key1);
		if(key2_is_const)
			_E(TAB "addi %s, $zero, %s", _N(r2), key2);

		_E(TAB "slt %s, %s, %s", _N(rdest), _N(r1), _N(r2));
		_E(TAB "xori %s, %s, 1", _N(rdest), _N(rdest));
		break;

	default:
		ice("unsupported operation");
		break;
	}

	/* finally, make sure the value gets stored back to the address
	 * associated with ldest */
	loc_store(rdest, ldest);
}

/* special case of assignment with one operand. can use $t0 and $t1 */
void mips_set1(char *source, char *dest, loc_context_t *ctx)
{
	loc_t *ldest;
	reg_t rsource, rdest;
	bool source_is_const;

	/* check if source is an immediate val */
	if(!strcmp("0", source) || atoi(source) != 0)
		source_is_const = true;
	else {
		source_is_const = false;
		rsource = loc_get(REG_T0, _L(source, ctx));
	}

	ldest = _L_unchecked(dest, ctx);

	if(ldest == NULL)
		ldest = loc_add(dest, ctx);

	if(ldest->type == L_REGISTER)
		rdest = loc_get(REG_T1, ldest);
	else
		rdest = REG_T1;

	_E_EOL("  # %s = %s", dest, source);

	if(source_is_const)
		_E(TAB "addi %s, $zero, %s", _N(rdest), source);
	else
		_E(TAB "add %s, $zero, %s", _N(rdest), _N(rsource));

	/* finally, sync the reg back to the loc's val */
	loc_store(rdest, ldest);
}

static void emit_bb_body(bb_node_t *bbn, cfg_context_t *cfg, loc_context_t *ctx)
{
	list_node_t *n, *argn;
	void *v;
	inst_t *i;
	loc_t *l;
	reg_t r;
	inst_op_t *arg;
	symbol_t *sym;
	char b[512];
	int idx;

	if (is_dummy(bbn))
		return;

	LIST_EACH(bbn->instructions, n, v) {
		i = (inst_t*)v;

		switch(i->type) {
		case I_ASSIGN:
			if(i->a.op == OP_IDENTIFIER ||
					i->a.op == OP_INTEGER_CONSTANT) {
				if(i->a.r[0].is_val) {
					sprintf(b, "%i", i->a.r[0].val);
					mips_set1(b, i->a.l.id, ctx);
				} else {
					mips_set1(i->a.r[0].id, i->a.l.id, ctx);
				}

			} else {
				if(i->a.r[0].is_val) {
					sprintf(b, "%i", i->a.r[0].val);
					mips_set(b, i->a.r[1].id, i, ctx);
				} else if(i->a.r[1].is_val) {
					sprintf(b, "%i", i->a.r[1].val);
					mips_set(i->a.r[0].id, b, i, ctx);
				} else {
					mips_set(i->a.r[0].id, i->a.r[1].id, i, ctx);
				}
			}
			break;

		case I_IF:
			if(i->cond.is_val) {
				_E(TAB "addi $t0, $zero, %i", i->cond.val);
				r = REG_T0;
				_E_EOL("  # test %i", i->cond.val);
			} else {
				r = loc_get(REG_T0, _L(i->cond.id, ctx));
				_E_EOL("  # test %s", i->cond.id);
			}
			_E(TAB "beq %s, $zero, %s_%i", _N(r), cfg->fn->mangled_name,
				bbn->fb->label);
			break;

		case I_ATTRIBUTE:
			if (i->a.l.is_val || i->a.r[0].is_val || i->a.r[1].is_val)
				ice("I_ATTRIBUTE cannot work with immediates");

			if (!i->a.t || i->a.t->tag != T_CLASS)
				ice("I_ATTRIBUTE needs class type");

			sym = symtab_get(i->a.t->cls->members, i->a.r[1].id);

			if (sym == NULL) {
				ice("attribute %s in I_ATTRIBUTE not found",
				    i->a.r[1].id);
			}

			l = _L_unchecked(i->a.l.id, ctx);
			if (l == NULL)
				l = loc_add(i->a.l.id, ctx);

			if (l->type == L_REGISTER)
				r = loc_get(REG_T1, l);
			else
				r = REG_T1;

			_E_EOL("  # addr. of %s->%s into %s",
			   i->a.r[0].id, i->a.r[1].id, i->a.l.id);
			_E(TAB "addi %s, %s, %i", _N(r),
			   _N(loc_get(REG_T0, _L(i->a.r[0].id, ctx))),
			   sym->offset * BYTES_IN_INTEGER);

			/* finally, sync the reg back to the loc's val */
			loc_store(r, l);

			break;

		case I_LOAD:
			if (i->m.src.is_val || i->m.dst.is_val)
				ice("I_LOAD src and dst cannot be immediate");

			_E_EOL("  # load *%s into %s", i->m.src.id, i->m.dst.id);

			l = _L_unchecked(i->m.dst.id, ctx);
			if (l == NULL)
				l = loc_add(i->m.dst.id, ctx);

			if (l->type == L_REGISTER)
				r = loc_get(REG_T1, l);
			else
				r = REG_T1;

			_E(TAB "lw %s, 0(%s)", _N(r),
			   _N(loc_get(REG_T0, _L(i->m.src.id, ctx))));

			/* finally, sync the reg back to the loc's val */
			loc_store(r, l);

			break;

		case I_STORE:
			if (i->m.dst.is_val)
				ice("I_STORE dst cannot be immediate");

			_E_EOL("  # store into *%s", i->m.dst.id);

			if (i->m.src.is_val) {
				r = REG_T0;
				_E(TAB "addi $t0, $zero, %d", i->m.src.val);
			} else {
				r = loc_get(REG_T0, _L(i->m.src.id, ctx));
			}

			_E(TAB "sw %s, 0(%s)", _N(r),
			   _N(loc_get(REG_T1, _L(i->m.dst.id, ctx))));

			break;

		case I_CALL:
			_E_LF();
			_E_C("call %s in %s", i->c.fn->name, i->c.fn->parent->name);

			/* save registers */
			if (ctx->stack_count) {
				_E(TAB "addi $sp, $sp, -%u",
				   ctx->stack_count * BYTES_IN_INTEGER);
			}
			emit_save_registers(loc_get_used_mask(ctx));

			/* pass arguments */
			_E(TAB "addi $sp, $sp, -%u",
			   i->c.args->length * BYTES_IN_INTEGER);
			idx = 0;
			LIST_EACH(i->c.args, argn, arg) {
				if (arg->is_val) {
					_E(TAB "addi %s, $zero, %d",
					   _N(REG_T0), arg->val);
					r = REG_T0;
				} else {
					r = loc_get(REG_T0, _L(arg->id, ctx));
				}

				_E(TAB "sw %s, %u($sp)", _N(r),
				   idx * BYTES_IN_INTEGER);
				idx++;
			}

			_E(TAB "jal %s", i->c.fn->mangled_name);

			/* discard arguments */
			_E(TAB "addi $sp, $sp, %u",
			   i->c.args->length * BYTES_IN_INTEGER);

			/* restore registers */
			emit_restore_registers(loc_get_used_mask(ctx));
			if (ctx->stack_count) {
				_E(TAB "addi $sp, $sp, %u",
				   ctx->stack_count * BYTES_IN_INTEGER);
			}

			/* copy return address, if you're into that */
			if (!i->c.ret.is_val) {
				l = _L_unchecked(i->c.ret.id, ctx);

				if (l == NULL)
					l = loc_add(i->c.ret.id, ctx);

				if (l->type == L_REGISTER)
					r = loc_get(REG_T1, l);
				else
					r = REG_T1;

				_E_EOL("  # return value into %s", i->c.ret.id);
				_E(TAB "add %s, $zero, $v0", _N(r));

				/* finally, sync the reg back to the loc's val */
				loc_store(r, l);
			}

			_E_LF();

			break;

		case I_ALLOC:
			if (i->alloc.dst.is_val)
				ice("cannot allocate to immediate");

			l = _L_unchecked(i->alloc.dst.id, ctx);

			if (l == NULL)
				l = loc_add(i->alloc.dst.id, ctx);

			if (l->type == L_REGISTER)
				r = loc_get(REG_V0, l);
			else
				r = REG_V0;

			/* ALLOCATE */
			emit_malloc(i->alloc.t->size * BYTES_IN_INTEGER, r);

			/* finally, sync the reg back to the loc's val */
			loc_store(r, l);

			break;

		case I_PRINT:
			if (i->a.r[0].is_val) {
				emit_print_immediate(i->a.r[0].val);
			} else {
				r = loc_get(REG_T0, _L(i->a.r[0].id, ctx));
				emit_print_register(r);
			}
			break;

		default:
			/* アイス */
			ice("unrecognised inst_type_t (though "
					"it probably should be)");
		}
	}
}

static void emit_bb(bb_node_t *bbn, cfg_context_t *cfg, loc_context_t *ctx)
{
	_E_LF();
	_E("%s_%i:", cfg->fn->mangled_name, bbn->label);

	if(!is_dummy(bbn))
		emit_bb_body(bbn, cfg, ctx);

	if(bbn->tb == NULL) {
		if (bbn->label != cfg->all_bb->length - 1)
			_E(TAB "j %s_return", cfg->fn->mangled_name);

		return;
	}

	if (bbn->label != bbn->tb->label - 1) {
		_E(TAB "j %s_%i", cfg->fn->mangled_name,
				bbn->has_condition ? bbn->fb->label
				: bbn->tb->label);
	}
}

static void emit_function_body(func_t *f)
{
	cfg_context_t *cfg;
	list_node_t *n;
	symbol_t *s;
	bb_node_t *bbn;
	int i;

	loc_context_t *lctx;

	_E_LF();
	_E_LF();
	_E_C("");
	_E_C("%s in %s", f->name, f->parent->name);
	_E_C("");

	cfg = func_to_cfg(f);
	do {
		value_numbering_extended(cfg);
	} while (eliminate_redundant_temporaries(cfg));
	fprintf(stderr, "\n\x1b[1;33m%s.%s:\x1b[0m\n", f->parent->name, f->name);
	dump_cfg(cfg);

	/**************
	 *  preamble  *
	 **************/

	_E_LF();
	_E("%s:", f->mangled_name);
	_E_C("save old frame pointer and current return address");
	_E(TAB "addi $sp, $sp, -8");
	_E(TAB "sw $fp, 0($sp)");
	_E(TAB "sw $ra, 4($sp)");
	_E_C("stack pointer becomes frame pointer");
	_E(TAB "add $fp, $zero, $sp");

	lctx = loc_context_new();

	/* return value has same name as function */
	loc_add(f->name, lctx);

	/* add a loc_t to the table for each argument */
	loc_add_real("this", lctx, true);
	LIST_EACH(f->arguments, n, s)
		loc_add_real(s->name, lctx, true);

	/* and for each local variable */
	LIST_EACH(f->vars, n, s) {
		if (s->t->tag == T_ARRAY) {
			loc_add_array(s->name, s->t, lctx);
		} else {
			loc_add(s->name, lctx);
		}
	}

	/***********
	 *  amble  *
	 ***********/

	i = 0;
	LIST_EACH(cfg->all_bb, n, bbn)
		((bb_node_t*)bbn)->label = i++;

	LIST_EACH(cfg->all_bb, n, bbn)
		emit_bb(bbn, cfg, lctx);

	/****************
	 *  post-amble  *
	 ****************/

	reg_t r = loc_get(REG_T0, _L(f->name, lctx));

	_E_LF();
	_E("%s_return:", f->mangled_name);
	_E_C("copy return value into v0");
	_E(TAB "add $v0, $zero, %s", _N(r));
	_E_C("restore frame ptr, return address, and return");
	_E(TAB "lw $fp, 0($sp)");
	_E(TAB "lw $ra, 4($sp)");
	_E(TAB "addi $sp, $sp, 8");
	_E(TAB "jr $ra");
}

static void emit_class(void *_p, char *name, void *_t)
{
	type_t *t = _t;
	list_node_t *n;
	func_t *fn;

	if (t->tag != T_CLASS)
		return;

	LIST_EACH(t->cls->functions, n, fn)
		emit_function_body(fn);
}

static bool locate_main(prog_t *p, type_t **m_cls, func_t **m_fn)
{
	symbol_t *main_fn_sym;

	if ((*m_cls = htab_get(p->types, p->name)) == NULL)
		return false;

	if ((*m_cls)->tag != T_CLASS)
		return false;

	if ((main_fn_sym = symtab_get((*m_cls)->cls->members, p->name)) == NULL)
		return false;

	if (main_fn_sym->tag != SYM_FUNCTION)
		return false;

	*m_fn = main_fn_sym->fn;
	return true;
}

static void emit_footer(prog_t *p)
{
	type_t *t;
	func_t *f;

	if (!locate_main(p, &t, &f))
		ice("no main method defined");

	_E_LF();
	_E_LF();
	_E_C("entry point");
	_E("main:");
	emit_malloc(t->size * BYTES_IN_INTEGER, REG_V0);
	_E(TAB "addi $sp, $sp, -4");
	_E(TAB "sw $v0, 0($sp)");
	_E(TAB "jal %s", f->mangled_name);

	emit_exit();
}

void mips_emit_program(prog_t *p)
{
	emit_header();

	htab_each(p->types, emit_class, p);

	emit_footer(p);
}
