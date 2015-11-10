/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

#ifndef __INC_TYPE_H__
#define __INC_TYPE_H__

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "container.h"
#include "error.h"
#include "shared.h"

/*******************
 *  Symbol Tables  *
 *******************/

/* This structure represents a symbol table, which is just a slightly
   augmented hash table. A symbol table can be derived from another
   symbol table. For typical lookups, all symbol tables in the chain
   are consulted until the first matching entry is found. The 'conceal'
   designation is important for assignment. A symbol table is said to
   'conceal' another if symbols can be inserted into the first having
   the same names as entries in the second. A practical example is a
   function's local variables, which conceal the symbol table of its
   containing class and all its superclasses. */

typedef struct symtab symtab_t;

struct symtab {
	symtab_t *parent;
	bool conceal;
	htab_t *tab;
};

extern symtab_t *symtab_new(symtab_t *parent, bool conceal);
extern void *symtab_set(symtab_t *st, char *k, void *v);
extern void *symtab_get(symtab_t *st, char *k);

/***************
 *  Semantics  *
 ***************/

typedef struct _class class_t;
typedef struct _func func_t;
typedef struct _prog prog_t;
typedef struct _member member_t;
typedef struct _type type_t;
typedef struct _type_set type_set_t;

typedef enum _stmt_type stmt_type_t;
typedef struct _stmt stmt_t;

typedef enum _expr_op expr_op_t;
typedef struct _expr expr_t;

typedef struct _symbol symbol_t;

/* expr_t replaces the following parse tree structs:
     function_designator_t,
     actual_parameter_t,
     variable_access_t,
     object_instantiation_t,
     expression_t,
     indexed_variable_t,
     attribute_designator_t,
     method_designator_t,
     simple_expression_t,
     term_t,
     factor_t,
     primary_t

   stmt_t replaces:
     assign_statement_t,
     statement_t,
     if_statement_t,
     while_statement_t,
     and also covers
     compound and print statements */

struct _class {
	/* This struct represents the semantic information associated
	   with a class. The 'members' symbol table derives from the matching
	   symbol table in the 'extends' class. */

	char *name;
	type_t *t;

	/* must be declared as a type_t containing
	 * a class_t for recursive calls to _test_equiv */
	type_t *extends;

	symtab_t *members; /* by name index */

	list_t *fields;
	list_t *functions;
	func_t *ctor; /* NULL if no ctor */
};

struct _func {
	/* This struct represents the semantic information associated with
	   a function within a class. It should be noted that the symbol table
	   conceals the symbol table in the parent class */

	char *name;
	class_t *parent;
	symtab_t *local;
	list_t *arguments;
	list_t *vars;
	int line_no;
	type_t *result;
	stmt_t *body;

	/* these fields used for code generation */
	char *mangled_name;
};

struct _prog {
	/* This struct represents the semantic information associated with
	   an entire program. */

	char *name;

	htab_t *builtins; /* id => expr_t */
	htab_t *types;
	type_set_t *type_set;

	list_t *errs;
};

struct _type {
	/* This struct represents our semantic understanding of a type. The
	   interpretation of the anonymous union is determined by the value
	   of the 'tag' field. */

	enum {
		T_INTEGER,
		T_BOOLEAN,
		T_ARRAY,
		T_CLASS,
	} tag;

	union {
		struct {
			int low;
			int high;
			type_t *of;
		} arr;

		class_t *cls;
	};

	int tabid;
	int line_no;

	/* these fields used for code generation */
	unsigned size;
};

enum _expr_op {
	OP_NONE, /* not valid externally */

	OP_IDENTIFIER,         /* id is valid */
	OP_FUNCTION,           /* id is valid */
	OP_SYMBOL,             /* sym is valid */
	OP_INTEGER_CONSTANT,   /* uc is valid */
	OP_BOOLEAN_CONSTANT,   /* uc is valid */

	OP_APPLY,              /* fn(args..) */
	OP_INSTANTIATION,      /* new fn (args..) */

	OP_ACTUAL_PARAMETER,   /* e0:e1, or e0:e1:e2 */
	OP_INDEX,              /* e0[e1] */
	OP_ATTRIBUTE,          /* e0.e1 */

	OP_NULL, /* e0 */
	OP_NOT,  /* NOT e0 */
	OP_POS,  /* +e0 */
	OP_NEG,  /* -e0 */

	OP_EQ,   /* e0  =  e1 */
	OP_NE,   /* e0  <> e1 */
	OP_LT,   /* e0  <  e1 */
	OP_GT,   /* e0  >  e1 */
	OP_LE,   /* e0  <= e1 */
	OP_GE,   /* e0  >= e1 */
	OP_ADD,  /* e0  +  e1 */
	OP_SUB,  /* e0  -  e1 */
	OP_OR,   /* e0 OR  e1 */
	OP_MUL,  /* e0  *  e1 */
	OP_DIV,  /* e0  /  e1 */
	OP_MOD,  /* e0 MOD e1 */
	OP_AND,  /* e0 AND e1 */
};

extern const char *expr_op_to_name[];
extern const char *expr_op_to_glyph[];

struct _expr {
	/* This struct represents a semantic 'expression'. It should
	   be noted that expr_t is used for many things outside of
	   just simple expressions, such as parameters, indexing, and
	   attributes. */

	expr_op_t op;

	type_t *t;

	union {
		char *id;
		symbol_t *sym;
		unsigned uc;
		struct {
			expr_t *fn;
			list_t *args;
		} apply;
		expr_t *ops[3];
	};

	int line_no;
};

/* compound statements are handled as a linked list */
enum _stmt_type {
	STMT_IF,
	STMT_WHILE,
	STMT_ASSIGN,
	STMT_PRINT,
};

struct _stmt {
	/* This struct represents a semantic statement. Statements are much
	   more straightforward and static, compared to expressions. It should
	   be noted that 'compound statements' are here implemented as a
	   linked list of the statements themselves. */

	stmt_type_t type;

	union {
		struct {
			expr_t *cond;
			stmt_t *st;
		} while_s;

		struct {
			expr_t *cond;
			stmt_t *tb;
			stmt_t *fb;
		} if_s;

		struct {
			expr_t *lhs;
			expr_t *rhs;
		} assign;

		expr_t *print;
	};

	stmt_t *next;
};

struct _symbol {
	/* This struct represents a symbol table entry. Every symbol table
	   entry has a name, type, and line of declaration. */

	enum {
		SYM_FIELD,          /* t = field type */
		SYM_FUNCTION,       /* t = return type */
		SYM_VAL_PARAMETER,  /* t = parameter type */
		SYM_REF_PARAMETER,  /* t = parameter type */
		SYM_FUNCTION_LOCAL, /* t = variable type */
		SYM_THIS,           /* t = this type */
		SYM_RETURN,         /* t = variable type */
	} tag;

	char *name;
	type_t *t;

	func_t *fn;

	int line_declared;

	/* these fields used for code generation */
	int offset;
};

/* flatten the program tree and add symbols to the
 * symbol table. return a prog_t, or NULL on err */
extern prog_t* build_semantic_tree(program_t *program);

extern const char *type_to_name(type_t*);

struct _type_set {
	vec_t *types; /* vector of type_t */
	vec_t *equiv; /* vector of vectors of booleans */
};

extern type_set_t *type_set_new(void);
extern void type_set_add(type_set_t *ts, type_t *t);
extern bool type_set_equiv(type_set_t *ts, type_t *t1, type_t *t2);
extern void type_set_update_equiv(type_set_t *s);

#endif
