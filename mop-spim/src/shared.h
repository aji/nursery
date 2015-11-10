/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

#ifndef __INC_SHARED_H__
#define __INC_SHARED_H__

#include <stdbool.h>

#include "container.h"

typedef struct identifier identifier_t;
typedef struct type_denoter type_denoter_t;
typedef struct function_designator function_designator_t;
typedef struct actual_parameter actual_parameter_t;
typedef struct variable_declaration variable_declaration_t;
typedef struct range range_t;
typedef struct formal_parameter_section formal_parameter_section_t;
typedef struct variable_access variable_access_t;
typedef struct assignment_statement assignment_statement_t;
typedef struct object_instantiation object_instantiation_t;
typedef struct expression expression_t;
typedef struct statement statement_t;
typedef struct if_statement if_statement_t;
typedef struct while_statement while_statement_t;
typedef struct indexed_variable indexed_variable_t;
typedef struct attribute_designator attribute_designator_t;
typedef struct method_designator method_designator_t;
typedef struct simple_expression simple_expression_t;
typedef struct term term_t;
typedef struct factor factor_t;
typedef struct primary primary_t;
typedef struct array_type array_type_t;
typedef struct class_block class_block_t;
typedef struct function_declaration function_declaration_t;
typedef struct function_block function_block_t;
typedef struct function_heading function_heading_t;
typedef struct class_definition class_definition_t;
typedef struct class_identification class_identification_t;
typedef struct program program_t;
typedef struct program_heading program_heading_t;

struct identifier {
	char *id;
	int line_no;
};

struct type_denoter {
	bool is_array;
	union {
		array_type_t *a;
		char *id;
	};
	int line_no;
};

struct function_designator {
	char *id;
	list_t *params;
	int line_no;
};

struct actual_parameter {
	expression_t *exp1;
	expression_t *exp2;
	expression_t *exp3;
	int line_no;
};

struct variable_declaration {
	list_t *id_list;
	type_denoter_t *type;
	int line_no;
};

struct range {
	int low;
	int high;
};

struct formal_parameter_section {
	bool is_var;
	list_t *ids;
	char *id;
	int line_no;
};

struct variable_access {
	enum {
		VA_IDENTIFIER,
		VA_INDEXED,
		VA_ATTRIBUTE,
		VA_METHOD
	} tag;
	union {
		char *id;
		indexed_variable_t *iv;
		attribute_designator_t *ad;
		method_designator_t *md;
	};
	int line_no;
};

struct assignment_statement {
	bool is_new;
	variable_access_t *va;
	union {
		expression_t *e;
		object_instantiation_t *oi;
	};
};

struct object_instantiation {
	bool has_params;
	char *id;
	list_t *params;
	int line_no;
};

struct expression {
	bool is_one;
	simple_expression_t *e1;
	simple_expression_t *e2;
	int relop;
	int line_no;
};

struct statement {
	enum {
		ST_ASSIGNMENT,
		ST_COMPOUND,
		ST_IF,
		ST_WHILE,
		ST_PRINT
	} tag;
	union {
		assignment_statement_t *as;
		list_t *cs;
		if_statement_t *is;
		while_statement_t *ws;
		variable_access_t *va;
	};
	int line_no;
};

struct if_statement {
	expression_t *condition;
	statement_t *tb;
	statement_t *eb;
};

struct while_statement {
	expression_t *condition;
	statement_t *st;
};

struct indexed_variable {
	variable_access_t *va;
	list_t *idxs;
	int line_no;
};

struct attribute_designator {
	variable_access_t *va;
	char *id;
	int line_no;
};

struct method_designator {
	variable_access_t *va;
	function_designator_t *fd;
	int line_no;
};

struct simple_expression {
	bool is_term;
	term_t *t;
	simple_expression_t *se;
	int addop;
	int line_no;
};

struct term {
	bool is_factor;
	factor_t *f;
	term_t *t;
	int mulop;
	int line_no;
};

struct factor {
	bool is_sign;
	int sign;
	factor_t *f;
	primary_t *p;
	int line_no;
};

struct primary {
	enum {
		PR_VARIABLE_ACCESS,
		PR_UNSIGNED_CONSTANT,
		PR_FUNCTION_DESIGNATOR,
		PR_EXPRESSION,
		PR_NOT_PRIMARY
	} tag;
	union {
		variable_access_t *va;
		int uc;
		function_designator_t *fd;
		expression_t *e;
		primary_t *p;
	};
	int line_no;
};

struct array_type {
	range_t *r;
	type_denoter_t *td;
	int line_no;
};

struct class_block {
	list_t *vd;
	list_t *fd;
};

struct function_declaration {
	bool is_heading;
	union {
		char *id;
		function_heading_t *fh;
	};
	function_block_t *fb;
	int line_no;
};

struct function_block {
	list_t *vp;
	statement_t *sp;
};

struct function_heading {
	bool has_fpl;
	char *id;
	list_t *fpl;
	char *result_type;
	int line_no;
};

struct class_definition {
	class_identification_t *cid;
	class_block_t *body;
};

struct class_identification {
	bool has_extends;
	char *id;
	identifier_t *extends;
	int line_no;
};

struct program {
	program_heading_t *ph;
	list_t *classes;
};

struct program_heading {
	bool has_id_list;
	char *id;
	list_t *id_list;
	int line_no;
};

#endif
