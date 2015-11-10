/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

/* This monstrosity of a source file is where the parse tree is converted
   into a semantic tree. A "semantic tree" is a much more organized and
   straightforward view of the program source, with carefully constructed
   interconnections and indexing to allow efficient traversal. */

/* At the very top level is the function 'build_semantic_tree', with the
   following declaration:

      prog_t* build_semantic_tree(program_t *program)

   This function operates in multiple passes. If errors occur during any pass,
   the pass finishes and traversal stops. The passes are as follows:

      1. Record which classes the user will be defining. Doing this first
         allows all later passes to refer to any class defined anywhere.

      2. Fill out each class's field definitions, and resolve references to
         extended classes.

      At this step, we have all the information we need to perform the fixed
      point type equivalence algorithm, which is run before pass 3.

      3. Create function definitions for all functions in all classes. Note
         that although the function bodies are converted to their semantic
         tree equivalents, they are otherwise left alone to be a tree of
         identifiers and constants.

      4. Iterate over all statements and expressions, resolving references to
         other functions and types, and propagating expression types upwards.

   At this point, the prog_t contains a complex graph describing the semantics
   of the program. Error checking is performed at all steps of the project. */

/* There are several distinct components in this file:

      -  symtab_* functions for manipulating symbol tables.

      -  flatten_* functions turn parse tree structures into dry semantic
         tree equivalents.

      -  resolve_* functions turn dry semantic tree structures into complete,
         usable equivalents, with type, function, and symbol references
         resolved.

   Anything not following these patterns are helper functions */

#include "type.h"
#include "y.tab.h"
#include <ctype.h>

/* used to return NULL if any lower level returns NULL.
 * this should probably variadically include freeing sub
 * trees as well, but whatever for now. */
#define IF_NULL_RET(arg) \
do { \
	if((arg) == NULL) \
		return NULL; \
} while(0)

/* a prog_t* is passed to everything below flatten_program because
 * it contains the type set, error list, and top level symbol table.
 * when passing to inner scope, make p->symbols point to a new child
 * symbol table instead */

/*******************
 *  Symbol Tables  *
 *******************/

symtab_t *symtab_new(symtab_t *parent, bool conceal)
{
	symtab_t *st = calloc(1, sizeof(*st));

	st->parent = parent;
	st->conceal = conceal;
	st->tab = htab_new(0);

	return st;
}

void *symtab_get_here(symtab_t *st, char *k)
{
	void *v = NULL;

	while (st) {
		v = htab_get(st->tab, k);
		if (v != NULL)
			return v;

		if (st->conceal)
			return NULL;

		st = st->parent;
	}

	return NULL;
}

void *symtab_set(symtab_t *st, char *k, void *v)
{
	void *displaced = symtab_get_here(st, k);

	htab_put(st->tab, k, v);

	return displaced;
}

void *symtab_get(symtab_t *st, char *k)
{
	void *v;

	for (v = NULL; !v && st; st = st->parent)
		v = htab_get(st->tab, k);

	return v;
}

/***************
 *  Semantics  *
 ***************/

/* constructors / helpers */

static expr_t *make_expr(expr_op_t op, int line_no)
{
	expr_t *expr = calloc(1, sizeof(*expr));
	expr->op = op;
	expr->line_no = line_no;
	return expr;
}

static expr_t *make_expr_id(expr_op_t op, char *id, int line_no)
{
	expr_t *expr = make_expr(op, line_no);
	expr->id = id;
	return expr;
}

static expr_t *make_expr_uc(expr_op_t op, unsigned uc, int line_no)
{
	expr_t *expr = make_expr(op, line_no);
	expr->uc = uc;
	return expr;
}

static expr_t *make_expr_apply(expr_op_t op, expr_t *fn, list_t *args, int line_no)
{
	expr_t *expr = make_expr(op, line_no);
	expr->apply.fn = fn;
	expr->apply.args = args;
	return expr;
}

static expr_t *make_expr_op(expr_op_t op, expr_t *e0, expr_t *e1, expr_t *e2, int line_no)
{
	expr_t *expr = make_expr(op, line_no);
	expr->ops[0] = e0;
	expr->ops[1] = e1;
	expr->ops[2] = e2;
	return expr;
}

/* fixing up expressions a bit, resolving symbols and types. */

const char *expr_op_to_name[] = {
	[OP_NONE]                = "(invalid)",

	[OP_IDENTIFIER]          = "(identifier)",
	[OP_FUNCTION]            = "(function)",
	[OP_SYMBOL]              = "(symbol)",
	[OP_INTEGER_CONSTANT]    = "(unsigned constant)",
	[OP_BOOLEAN_CONSTANT]    = "(boolean constant)",

	[OP_APPLY]               = "(apply)",
	[OP_INSTANTIATION]       = "(instantiation)",

	[OP_ACTUAL_PARAMETER]    = "(actual parameter)",
	[OP_INDEX]               = "(index)",
	[OP_ATTRIBUTE]           = "(attribute)",

	[OP_NULL]                = "(nil)",
	[OP_NOT]                 = "'NOT'",
	[OP_POS]                 = "'+'",
	[OP_NEG]                 = "'-'",

	[OP_EQ]                  = "'='",
	[OP_NE]                  = "'<>'",
	[OP_LT]                  = "'<'",
	[OP_GT]                  = "'>'",
	[OP_LE]                  = "'<='",
	[OP_GE]                  = "'>='",
	[OP_ADD]                 = "'+'",
	[OP_SUB]                 = "'-'",
	[OP_OR]                  = "'OR'",
	[OP_MUL]                 = "'*'",
	[OP_DIV]                 = "'/'",
	[OP_MOD]                 = "'MOD'",
	[OP_AND]                 = "'AND'",
};

const char *expr_op_to_glyph[] = {
	[OP_NOT]                 = "NOT",
	[OP_POS]                 = "+",
	[OP_NEG]                 = "-",

	[OP_EQ]                  = "=",
	[OP_NE]                  = "<>",
	[OP_LT]                  = "<",
	[OP_GT]                  = ">",
	[OP_LE]                  = "<=",
	[OP_GE]                  = ">=",
	[OP_ADD]                 = "+",
	[OP_SUB]                 = "-",
	[OP_OR]                  = "OR",
	[OP_MUL]                 = "*",
	[OP_DIV]                 = "/",
	[OP_MOD]                 = "MOD",
	[OP_AND]                 = "AND",
};

const char *type_to_name(type_t *t)
{
	char buf[2048];

	if (!t) return "(missing)";

	switch (t->tag) {
	case T_INTEGER:  return "integer";
	case T_BOOLEAN:  return "boolean";

	case T_ARRAY:
		/* leaking memory but oh well */
		snprintf(buf, 2048, "array of %d %s",
			t->arr.high - t->arr.low + 1,
			type_to_name(t->arr.of));
		return strdup(buf);

	case T_CLASS:
		return t->cls->name;
	}

	return "(unknown)";
}

static const char *display_name(expr_t *ex)
{
	switch (ex->op) {
	case OP_IDENTIFIER:
	case OP_FUNCTION:
		return ex->id;
	case OP_SYMBOL:
		return ex->sym->name;
	case OP_ACTUAL_PARAMETER:
	case OP_INDEX:
	case OP_NULL:
		return display_name(ex->ops[0]);
	case OP_ATTRIBUTE:
		return display_name(ex->ops[1]);
	default:
		return "(missing)";
	}
}

static char *lowercased(char *s)
{
	static char buf[512];
	snprintf(buf, 512, "%s", s);
	for (s=buf; *s; s++)
		*s = tolower(*s);
	return buf;
}

static symbol_t *resolve_member(prog_t *p, symtab_t *scope, expr_t *ex)
{
	while (ex != NULL) {
		switch (ex->op) {
		case OP_ATTRIBUTE:
			ex = ex->ops[1];
			break;

		case OP_SYMBOL:
			return ex->sym;

		default:
			return NULL;
		}
	}

	return NULL;
}

static bool is_lvalue(expr_t *ex)
{
	switch (ex->op) {
	case OP_IDENTIFIER:
	case OP_SYMBOL:
	case OP_INDEX:
	case OP_ATTRIBUTE:
		return true;

	default:
		return false;
	}
}

static bool resolve_expr(prog_t *p, symtab_t *scope, expr_t *ex);

static bool check_argument_lists(prog_t *p, symtab_t *scope,
                                 list_t *callee, list_t *caller,
                                 int line_no, char *what, char *name)
{
	if (caller->length != callee->length) {
		assert_error(p->errs, line_no,
			"Wrong number of arguments to %s%s: "
			"got %d, expected %d",
			what, name, caller->length, callee->length);
		return false;
	}

	list_node_t *ac = caller->root.next;
	list_node_t *fc = callee->root.next;

	while (ac != &caller->root && fc != &callee->root) {
		expr_t    *expr = ac->v;
		symbol_t  *arg  = fc->v;

		if (!resolve_expr(p, scope, expr))
			return false;

		if (!type_set_equiv(p->type_set, arg->t, expr->t)) {
			assert_error(p->errs, expr->line_no,
				"Cannot pass %s as %s for "
				"argument %s to %s%s",
				type_to_name(expr->t),
				type_to_name(arg->t),
				arg->name, what, name);
			return false;
		}

		if (arg->tag == SYM_REF_PARAMETER && !is_lvalue(expr)) {
			assert_error(p->errs, expr->line_no,
				"For argument %s to %s%s: "
				"expression cannot be passed "
				"by reference",
				arg->name, what, name);
			return false;
		}

		ac = ac->next;
		fc = fc->next;
	}

	return true;
}

static bool resolve_expr(prog_t *p, symtab_t *scope, expr_t *ex)
{
	symbol_t *sym;
	//list_node_t *cur;
	expr_t *curex;
	type_t *t;

	switch (ex->op) {
	case OP_NONE:
		abort();
		break;

	case OP_IDENTIFIER:
		curex = htab_get(p->builtins, lowercased(ex->id));
		if (curex != NULL) {
			memcpy(ex, curex, sizeof(*ex));
			return true;
		}

		sym = symtab_get(scope, ex->id);

		if (sym == NULL) {
			assert_error(p->errs, ex->line_no,
				"Variable \"%s\" not declared",
				ex->id);
			return false;
		}

		ex->op = OP_SYMBOL;
		ex->sym = sym;
		ex->t = sym->t;

		break;

	case OP_FUNCTION:
		sym = symtab_get(scope, ex->id);

		if (sym == NULL) {
			assert_error(p->errs, ex->line_no,
				"Function \"%s\" not declared",
				ex->id);
			return false;
		}

		if (sym->tag != SYM_FUNCTION) {
			assert_error(p->errs, ex->line_no,
				"Cannot call %s \"%s\"",
				type_to_name(sym->t),
				sym->name);
			return false;
		}

		ex->op = OP_SYMBOL;
		ex->sym = sym;
		ex->t = sym->t;

		break;

	case OP_SYMBOL:
		break;

	case OP_INTEGER_CONSTANT:
		ex->t = htab_get(p->types, "integer");
		break;
	case OP_BOOLEAN_CONSTANT:
		ex->t = htab_get(p->types, "boolean");
		break;

	case OP_APPLY:
		if (!resolve_expr(p, scope, ex->apply.fn))
			return false;

		sym = resolve_member(p, scope, ex->apply.fn);

		if (!sym) {
			assert_error(p->errs, ex->line_no, "ICE: symbol missing!");
			return false;
		}

		if (sym->tag != SYM_FUNCTION) {
			assert_error(p->errs, ex->line_no,
				"Cannot call %s \"%s\"",
				type_to_name(sym->t),
				sym->name);
			return false;
		}

		if (!check_argument_lists(p, scope,
		                          sym->fn->arguments,
		                          ex->apply.args,
		                          ex->line_no, "", sym->name))
			return false;

		ex->t = ex->apply.fn->t;

		break;

	case OP_INSTANTIATION:
		if (!ex->apply.fn || ex->apply.fn->op != OP_IDENTIFIER)
			abort();

		ex->t = htab_get(p->types, ex->apply.fn->id);

		if (ex->t == NULL) {
			assert_error(p->errs, ex->line_no,
				"Data type \"%s\" is not defined",
				ex->apply.fn->id);
			return false;
		}

		if (ex->t->tag != T_CLASS) {
			assert_error(p->errs, ex->line_no,
				"Cannot instantiate instances of %s",
				type_to_name(ex->t));
			return false;
		}

		if (ex->apply.args->length && !ex->t->cls->ctor) {
			assert_error(p->errs, ex->line_no,
				"%s has no constructor",
				type_to_name(ex->t));
			return false;
		}

		if (!ex->t->cls->ctor)
			return true;

		if (!check_argument_lists(p, scope,
		                          ex->t->cls->ctor->arguments,
		                          ex->apply.args,
		                          ex->line_no, ex->t->cls->name,
		                          " constructor"))
			return false;

		break;

	case OP_ACTUAL_PARAMETER:
		abort();
		break;

	case OP_INDEX:
		if (!resolve_expr(p, scope, ex->ops[0]))
			return false;
		if (!resolve_expr(p, scope, ex->ops[1]))
			return false;

		if (ex->ops[0]->t->tag != T_ARRAY) {
			assert_error(p->errs, ex->line_no,
				"Indexed variable is %s, not array",
				type_to_name(ex->ops[0]->t)
				);
			return false;
		}

		if (ex->ops[1]->t->tag != T_INTEGER) {
			assert_error(p->errs, ex->line_no,
				"Array index not an integer for variable \"%s\"",
				display_name(ex->ops[0])
				);
			return false;
		}

		t = ex->ops[0]->t;

		if (t->arr.low != 0) {
			/* automatically re-index array ON THE FLY to be
			   zero-based. this is probably about the evilest
			   thing in this codebase. */

			ex->ops[1] = make_expr_op(OP_SUB,
			                 ex->ops[1],
			                 make_expr_uc(OP_INTEGER_CONSTANT,
			                     t->arr.low,
			                     ex->line_no),
			                 NULL, ex->line_no);
		}

		ex->t = t->arr.of;

		break;

	case OP_ATTRIBUTE:
		if (!resolve_expr(p, scope, ex->ops[0]))
			return false;

		if (ex->ops[0]->t->tag != T_CLASS) {
			assert_error(p->errs, ex->line_no,
				"%s has no attributes",
				type_to_name(ex->ops[0]->t)
				);
			return false;
		}

		if (!resolve_expr(p, ex->ops[0]->t->cls->members, ex->ops[1]))
			return false;

		if (ex->ops[1]->op != OP_SYMBOL) {
			assert_error(p->errs, ex->line_no,
				"Attributes must be symbols");
			return false;
		}

		ex->t = ex->ops[1]->t;

		break;

	case OP_NULL:
		if (!resolve_expr(p, scope, ex->ops[0]))
			return false;
		ex->t = ex->ops[0]->t;
		break;

	case OP_NOT:
		if (!resolve_expr(p, scope, ex->ops[0]))
			return false;
		if (ex->ops[0]->t->tag != T_BOOLEAN)
			goto unary_type_error;
		ex->t = ex->ops[0]->t;
		break;

	case OP_POS:
	case OP_NEG:
		if (!resolve_expr(p, scope, ex->ops[0]))
			return false;
		if (ex->ops[0]->t->tag != T_INTEGER)
			goto unary_type_error;
		ex->t = ex->ops[0]->t;
		break;

	case OP_EQ:
	case OP_NE:
	case OP_LT:
	case OP_GT:
	case OP_LE:
	case OP_GE:
		if (!resolve_expr(p, scope, ex->ops[0]))
			return false;
		if (!resolve_expr(p, scope, ex->ops[1]))
			return false;
		if (ex->ops[0]->t->tag != T_INTEGER ||
		    ex->ops[1]->t->tag != T_INTEGER) {
			goto binary_type_error;
		}
		ex->t = htab_get(p->types, "boolean");
		break;

	case OP_ADD:
	case OP_SUB:
	case OP_MUL:
	case OP_DIV:
	case OP_MOD:
		if (!resolve_expr(p, scope, ex->ops[0]))
			return false;
		if (!resolve_expr(p, scope, ex->ops[1]))
			return false;
		if (ex->ops[0]->t->tag != T_INTEGER ||
		    ex->ops[1]->t->tag != T_INTEGER) {
			goto binary_type_error;
		}
		ex->t = htab_get(p->types, "integer");
		break;

	case OP_OR:
	case OP_AND:
		if (!resolve_expr(p, scope, ex->ops[0]))
			return false;
		if (!resolve_expr(p, scope, ex->ops[1]))
			return false;
		if (ex->ops[0]->t->tag != T_BOOLEAN ||
		    ex->ops[1]->t->tag != T_BOOLEAN) {
			goto binary_type_error;
		}
		ex->t = htab_get(p->types, "boolean");
		break;
	}

	return true;

unary_type_error:
	assert_error(p->errs, ex->line_no,
		"Cannot apply %s to %s",
		expr_op_to_name[ex->op],
		type_to_name(ex->ops[0]->t)
		);
	return false;

binary_type_error:
	assert_error(p->errs, ex->line_no,
		"Cannot apply %s to %s and %s",
		expr_op_to_name[ex->op],
		type_to_name(ex->ops[0]->t),
		type_to_name(ex->ops[1]->t)
		);
	return false;
}

static bool resolve_stmt(prog_t *p, symtab_t *scope, stmt_t *st)
{
	bool works;

	if (st == NULL)
		return true;

	switch (st->type) {
	case STMT_IF:
		works = resolve_expr(p, scope, st->if_s.cond) &&
			resolve_stmt(p, scope, st->if_s.tb) &&
			resolve_stmt(p, scope, st->if_s.fb);
		break;

	case STMT_WHILE:
		works = resolve_expr(p, scope, st->while_s.cond) &&
			resolve_stmt(p, scope, st->while_s.st);
		break;

	case STMT_ASSIGN:
		works = resolve_expr(p, scope, st->assign.lhs) &&
			resolve_expr(p, scope, st->assign.rhs);

		if (!works)
			return false;

		switch (st->assign.lhs->op) {
		case OP_IDENTIFIER:
		case OP_INDEX:
		case OP_ATTRIBUTE:
			break;

		case OP_SYMBOL:
			if (st->assign.lhs->sym->tag == SYM_FUNCTION) {
				/* helpful errors, just like clang c: */
				assert_error(p->errs, st->assign.lhs->line_no,
					"Cannot assign to function (did you "
					"try to return in a function without "
					"a return value?)"
					);
				return false;
			}
			break;

		default:
			assert_error(p->errs, st->assign.lhs->line_no,
				"Left hand side of assignment not assignable"
				);
			return false;
		}

		if (!type_set_equiv(p->type_set,
				st->assign.lhs->t,
				st->assign.rhs->t)
		) {
			assert_error(p->errs, st->assign.lhs->line_no,
				"Type mismatch between \"%s\" and \"%s\"",
				type_to_name(st->assign.lhs->t),
				type_to_name(st->assign.rhs->t)
				);
			return false;
		}
		break;

	case STMT_PRINT:
		works = resolve_expr(p, scope, st->print);
		break;
	}

	return resolve_stmt(p, scope, st->next);
}

/* declarations */

static type_t* flatten_type_denoter(prog_t *p, type_denoter_t*);
static expr_t* flatten_function_designator(prog_t *p, function_designator_t*);
static expr_t* flatten_actual_parameter(prog_t *p, actual_parameter_t*);
static expr_t* flatten_variable_access(prog_t *p, variable_access_t*);
static stmt_t* flatten_assignment_statement(prog_t *p, symtab_t*, assignment_statement_t*);
static expr_t* flatten_object_instantiation(prog_t *p, object_instantiation_t*);
static expr_t* flatten_expression(prog_t *p, expression_t*);
static stmt_t* flatten_statement(prog_t *p, symtab_t*, statement_t*);
static stmt_t* flatten_if_statement(prog_t *p, symtab_t*, if_statement_t*);
static stmt_t* flatten_while_statement(prog_t *p, symtab_t*, while_statement_t*);
static expr_t* flatten_indexed_variable(prog_t *p, indexed_variable_t*);
static expr_t* flatten_attribute_designator(prog_t *p, attribute_designator_t*);
static expr_t* flatten_method_designator(prog_t *p, method_designator_t*);
static expr_t* flatten_simple_expression(prog_t *p, simple_expression_t*);
static expr_t* flatten_term(prog_t *p, term_t*);
static expr_t* flatten_factor(prog_t *p, factor_t*);
static expr_t* flatten_primary(prog_t *p, primary_t*);
static type_t* flatten_array_type(prog_t *p, array_type_t*);
static func_t* flatten_function_declaration(prog_t *p, class_t*,
                                             function_declaration_t*);
static prog_t* flatten_program(program_t*);

/* definitions */

static type_t* flatten_type_name(prog_t *p, char *id, int line_no)
{
	type_t *t = htab_get(p->types, id);

	if (t == NULL) {
		assert_error(p->errs, line_no,
			"Data type \"%s\" is not defined",
			id);
		return NULL;
	}

	return t;
}

static type_t* flatten_type_denoter(prog_t *p, type_denoter_t *td)
{
	if (td->is_array)
		return flatten_array_type(p, td->a);

	return flatten_type_name(p, td->id, td->line_no);
}

static expr_t* flatten_function_designator(prog_t *p, function_designator_t *fd)
{
	list_node_t *cur;
	actual_parameter_t *ap;

	expr_t *ex = make_expr_apply(OP_APPLY,
		make_expr_id(OP_FUNCTION, fd->id, fd->line_no),
		list_new(),
		fd->line_no
		);

	LIST_EACH(fd->params, cur, ap)
		list_append(ex->apply.args, flatten_actual_parameter(p, ap));

	return ex;
}

static expr_t* flatten_actual_parameter(prog_t *p, actual_parameter_t *ap)
{
	if (ap->exp2 != NULL || ap->exp3 != NULL) {
		assert_error(p->errs, ap->line_no,
			"x:y and x:y:z parameter forms not supported!");
		/* don't fail right away, I guess... */
	}

	return flatten_expression(p, ap->exp1);
}

static expr_t* flatten_variable_access(prog_t *p, variable_access_t *va)
{
	switch (va->tag) {
	case VA_INDEXED:    return flatten_indexed_variable(p, va->iv);
	case VA_ATTRIBUTE:  return flatten_attribute_designator(p, va->ad);
	case VA_METHOD:     return flatten_method_designator(p, va->md);
	case VA_IDENTIFIER: return make_expr_id(OP_IDENTIFIER, va->id, va->line_no);
	default: abort();
	}
}

static stmt_t* flatten_assignment_statement(prog_t *p, symtab_t *scope, assignment_statement_t *as)
{
	stmt_t *st = calloc(1, sizeof(*st));

	st->type = STMT_ASSIGN;
	st->assign.lhs = flatten_variable_access(p, as->va);

	if (as->is_new) {
		st->assign.rhs = flatten_object_instantiation(p, as->oi);
	} else {
		st->assign.rhs = flatten_expression(p, as->e);
	}

	return st;
}

static expr_t* flatten_object_instantiation(prog_t *p, object_instantiation_t *oi)
{
	expr_t *ex;
	list_node_t *cur;
	actual_parameter_t *ap;

	ex = make_expr_apply(OP_INSTANTIATION,
		make_expr_id(OP_IDENTIFIER, oi->id, oi->line_no),
		list_new(),
		oi->line_no
		);

	LIST_EACH(oi->params, cur, ap)
		list_append(ex->apply.args, flatten_actual_parameter(p, ap));

	return ex;
}

static expr_t* flatten_expression(prog_t *p, expression_t *ein)
{
	expr_t *e1, *eout;
	
	IF_NULL_RET(e1 = flatten_simple_expression(p, ein->e1));

	if (ein->is_one)
		return e1;
	
	TRY_MALLOC(eout, sizeof(*eout));

	eout->ops[0] = e1;
	IF_NULL_RET(eout->ops[1]=flatten_simple_expression(p, ein->e2));

	switch (ein->relop) {
	case EQUAL:     eout->op = OP_EQ; break;
	case NOTEQUAL:  eout->op = OP_NE; break;
	case LT:        eout->op = OP_LT; break;
	case GT:        eout->op = OP_GT; break;
	case LE:        eout->op = OP_LE; break;
	case GE:        eout->op = OP_GE; break;
	default:        abort();
	}

	eout->line_no = ein->line_no;

	return eout;
}

static stmt_t* flatten_statement(prog_t *p, symtab_t *scope, statement_t *sin)
{
	list_node_t *curin;
	stmt_t *sout, *curout;
	
	switch(sin->tag) {
	case ST_ASSIGNMENT:
		sout = flatten_assignment_statement(p, scope, sin->as);
		return sout;

	case ST_COMPOUND:
		sout = NULL;
		curout = NULL;
		LIST_EACH_NODE(sin->cs, curin) {
			if(sout == NULL) {
				sout=flatten_statement(p, scope, curin->v);
				if (sout) curout=sout;
			} else {
				curout->next=flatten_statement(p, scope, curin->v);
				if (curout->next) curout=curout->next;
			}
		}
		if (curout) curout->next = NULL;
		return sout;
	
	case ST_IF:
		sout = flatten_if_statement(p, scope, sin->is);
		return sout;

	case ST_WHILE:
		sout = flatten_while_statement(p, scope, sin->ws);
		return sout;

	case ST_PRINT:
		TRY_MALLOC(sout, sizeof(*sout));
		sout->type=STMT_PRINT;
		sout->print=flatten_variable_access(p, sin->va);
		sout->next=NULL;
		return sout;
	}

	abort();
}

static stmt_t* flatten_if_statement(prog_t *p, symtab_t *scope, if_statement_t *is)
{
	stmt_t *s;
	TRY_MALLOC(s, sizeof(*s));

	s->type=STMT_IF;

	s->if_s.cond=flatten_expression(p, is->condition);
	IF_NULL_RET(s->if_s.cond);

	s->if_s.tb=flatten_statement(p, scope, is->tb);
	IF_NULL_RET(s->if_s.tb);
	s->if_s.fb=flatten_statement(p, scope, is->eb);
	IF_NULL_RET(s->if_s.fb);

	return s;
}

static stmt_t* flatten_while_statement(prog_t *p, symtab_t *scope, while_statement_t *ws)
{
	stmt_t *s;
	TRY_MALLOC(s, sizeof(*s));

	s->type=STMT_WHILE;

	s->while_s.cond=flatten_expression(p, ws->condition);
	IF_NULL_RET(s->while_s.cond);

	s->while_s.st=flatten_statement(p, scope, ws->st);
	IF_NULL_RET(s->while_s.st);

	return s;
}

static expr_t* flatten_indexed_variable(prog_t *p, indexed_variable_t *iv)
{
	list_node_t *cur;
	expression_t *exin;
	expr_t *ex = NULL;

	LIST_EACH(iv->idxs, cur, exin) {
		if (ex == NULL)
			ex = flatten_variable_access(p, iv->va);

		ex = make_expr_op(OP_INDEX,
			ex, flatten_expression(p, exin), NULL,
			iv->line_no);
	}

	return ex;
}

static expr_t* flatten_attribute_designator(prog_t *p, attribute_designator_t *ad)
{
	expr_t *ex = make_expr_op(OP_ATTRIBUTE,
		flatten_variable_access(p, ad->va),
		make_expr_id(OP_IDENTIFIER, ad->id, ad->line_no),
		NULL,
		ad->line_no
		);

	return ex;
}

static expr_t* flatten_method_designator(prog_t *p, method_designator_t *md)
{
	list_node_t *cur;
	actual_parameter_t *ap;

	/* we do not recursively call flatten_function_designator because,
	   although our shared.h structs look like this:

		method_desginator {
			variable_access { ... }
			function_designator {
				identifier,
				arguments
			}
		}

	   it should really look like this:

		method_designator {
			variable_access {
				variable_access { ... }
				identifier
			}
			arguments
		}

	   since, in a sense, the method being called is itself a kind of
	   variable access, in the form of an OP_ATTRIBUTE on the variable
	   access in the original struct. */

	expr_t *ex = make_expr_apply(OP_APPLY,
		make_expr_op(OP_ATTRIBUTE,
			flatten_variable_access(p, md->va),
			make_expr_id(OP_FUNCTION, md->fd->id, md->fd->line_no),
			NULL,
			md->fd->line_no
			),
		list_new(),
		md->fd->line_no
		);

	LIST_EACH(md->fd->params, cur, ap)
		list_append(ex->apply.args, flatten_actual_parameter(p, ap));

	ex->line_no = md->line_no;

	return ex;
}

static expr_t* flatten_simple_expression(prog_t *p, simple_expression_t *se)
{
	expr_t *ex = flatten_term(p, se->t);
	expr_op_t op;

	if (se->is_term)
		return ex;

	switch (se->addop) {
	case PLUS:   op = OP_ADD; break;
	case MINUS:  op = OP_SUB; break;
	case OR:     op = OP_OR; break;
	default:     abort();
	}

	return make_expr_op(op,
		flatten_simple_expression(p, se->se), ex, NULL,
		se->line_no);
}

static expr_t* flatten_term(prog_t *p, term_t *t)
{
	expr_t *ex = flatten_factor(p, t->f);
	expr_op_t op;

	if (t->is_factor)
		return ex;

	switch (t->mulop) {
	case STAR:   op = OP_MUL; break;
	case SLASH:  op = OP_DIV; break;
	case MOD:    op = OP_MOD; break;
	case AND:    op = OP_AND; break;
	default:     abort();
	}

	return make_expr_op(op, flatten_term(p, t->t), ex, NULL, t->line_no);
}

static expr_t* flatten_factor(prog_t *p, factor_t *f)
{
	if (!f->is_sign)
		return flatten_primary(p, f->p);

	expr_t *ex = flatten_factor(p, f->f);

	switch (f->sign) {
	case MINUS:
		if (ex->op == OP_NEG) {
			assert_error(p->errs, ex->line_no,
				"Too many signs");
			return ex->ops[0];
		}

		ex = make_expr_op(OP_NEG, ex, NULL, NULL, f->line_no);
		break;

	case PLUS:  break; /* quick little "optimization" */
	default:    abort();
	}

	ex->line_no = f->line_no;

	return ex;
}

static expr_t* flatten_primary(prog_t *p, primary_t *pr)
{
	switch (pr->tag) {
	case PR_VARIABLE_ACCESS:
		return flatten_variable_access(p, pr->va);
	case PR_UNSIGNED_CONSTANT:
		return make_expr_uc(OP_INTEGER_CONSTANT, pr->uc, pr->line_no);
	case PR_FUNCTION_DESIGNATOR:
		return flatten_function_designator(p, pr->fd);
	case PR_EXPRESSION:
		return flatten_expression(p, pr->e);
	case PR_NOT_PRIMARY:
		return make_expr_op(OP_NOT,
			flatten_primary(p, pr->p), NULL, NULL,
			pr->line_no);
	}

	abort();
}

static type_t* flatten_array_type(prog_t *p, array_type_t *atin)
{
	type_t *atout;
	
	TRY_MALLOC(atout, sizeof(*atout));
	atout->tag=T_ARRAY;
	atout->arr.low=atin->r->low;
	atout->arr.high=atin->r->high;
	atout->arr.of=flatten_type_denoter(p, atin->td);

	if (atout->arr.low > atout->arr.high) {
		assert_error(p->errs, atin->line_no,
			"Invalid array range (%d,%d)",
			atout->arr.low,
			atout->arr.high);
		/* continue anyway */
	}

	return atout;
}

static void add_formal_parameters(prog_t *p, func_t *f,
                                  formal_parameter_section_t *fps)
{
	symbol_t *sym, *old;
	identifier_t *id;
	list_node_t *cur;

	type_t *t = flatten_type_name(p, fps->id, fps->line_no);

	if (t == NULL)
		return;

	LIST_EACH(fps->ids, cur, id) {
		sym = calloc(1, sizeof(*sym));

		sym->tag = fps->is_var ?  SYM_REF_PARAMETER : SYM_VAL_PARAMETER;
		sym->name = id->id;
		sym->t = t;
		sym->line_declared = id->line_no;

		if ((old = symtab_set(f->local, sym->name, sym)) != NULL) {
			assert_error(p->errs, sym->line_declared,
				"Parameter \"%s\" already declared at line %d",
				old->name, old->line_declared);
			free(sym);
			continue;
		}

		list_append(f->arguments, sym);
	}
}

static void add_local_variables(prog_t *p, func_t *f,
                                variable_declaration_t *vd)
{
	symbol_t *sym, *old;
	identifier_t *id;
	list_node_t *cur;

	type_t *t = flatten_type_denoter(p, vd->type);

	if (t == NULL)
		return;

	LIST_EACH(vd->id_list, cur, id) {
		sym = calloc(1, sizeof(*sym));

		sym->tag = SYM_FUNCTION_LOCAL;
		sym->name = id->id;
		sym->t = t;
		sym->line_declared = id->line_no;

		if ((old = symtab_set(f->local, sym->name, sym)) != NULL) {
			if (old->tag == SYM_RETURN) {
				assert_error(p->errs, sym->line_declared,
					"Variable name \"%s\" is reserved for "
					"use as a function return value",
					old->name);
			} else {
				assert_error(p->errs, sym->line_declared,
					"Local \"%s\" already declared at line %d",
					old->name, old->line_declared);
			}
			continue;
		}

		list_append(f->vars, sym);
	}
}

static func_t* flatten_function_declaration(prog_t *p, class_t *parent,
                                             function_declaration_t *fd)
{
	func_t *f = calloc(1, sizeof(*f));
	list_node_t *cur;
	variable_declaration_t *vd;
	symbol_t *tmp;

	f->local = symtab_new(parent->members, true);
	f->parent = parent;
	f->arguments = list_new();
	f->vars = list_new();
	f->line_no = fd->line_no;

	if (fd->is_heading) {
		f->name = fd->fh->id;
		f->result = htab_get(p->types, fd->fh->result_type);

		if (fd->fh->has_fpl) {
			list_node_t *cur;
			formal_parameter_section_t *fps;

			LIST_EACH(fd->fh->fpl, cur, fps)
				add_formal_parameters(p, f, fps);
		}
	} else {
		f->name = fd->id;
		f->result = NULL;
	}

	tmp = calloc(1, sizeof(*tmp));
	tmp->tag = SYM_THIS;
	tmp->name = "this";
	tmp->t = htab_get(p->types, parent->name);
	tmp->line_declared = fd->line_no;
	symtab_set(f->local, tmp->name, tmp);

	if (f->result != NULL) {
		tmp = calloc(1, sizeof(*tmp));
		tmp->tag = SYM_RETURN;
		tmp->name = f->name;
		tmp->t = f->result;
		tmp->line_declared = fd->line_no;
		symtab_set(f->local, tmp->name, tmp);
	}

	LIST_EACH(fd->fb->vp, cur, vd)
		add_local_variables(p, f, vd);

	IF_NULL_RET(f->body = flatten_statement(p, f->local, fd->fb->sp));

	return f;
}

static void insert_class_fields(prog_t *p, class_t *c, variable_declaration_t *vd)
{
	list_node_t *cur;
	identifier_t *id;
	type_t *t;
	symbol_t *sym, *old;

	t = flatten_type_denoter(p, vd->type);

	if (t == NULL)
		return;

	LIST_EACH(vd->id_list, cur, id) {
		sym = calloc(1, sizeof(*sym));

		sym->tag = SYM_FIELD;
		sym->name = id->id;
		sym->t = t;
		sym->line_declared = id->line_no;

		if ((old = symtab_set(c->members, sym->name, sym)) != NULL) {
			assert_error(p->errs, sym->line_declared,
				"Variable \"%s\" already declared at line %d",
				old->name, old->line_declared);
			free(sym);
			continue;
		}

		list_append(c->fields, sym);
	}
}

static void insert_class_function(prog_t *p, class_t *c, function_declaration_t *fd)
{
	func_t *f;
	symbol_t *sym, *old;

	if ((f = flatten_function_declaration(p, c, fd)) == NULL)
		return;

	sym = calloc(1, sizeof(*sym));
	sym->tag = SYM_FUNCTION;
	sym->name = f->name;
	sym->t = f->result;
	sym->fn = f;
	sym->line_declared = f->line_no;

	if ((old = symtab_set(c->members, sym->name, sym)) != NULL) {
		assert_error(p->errs, f->line_no,
			"Function \"%s\" already declared at line %d",
			f->name, old->line_declared);
		return;
	}
	list_append(c->functions, f);

	/* assign constructor */
	if (!strcmp(sym->name, c->name))
		c->ctor = sym->fn;
}

static prog_t* flatten_program(program_t *pin)
{
	prog_t *pout;
	list_node_t *cur, *cur2;
	class_definition_t *cd;
	type_t *ct;

	TRY_MALLOC(pout, sizeof(*pout));

	pout->name = pin->ph->id;
	pout->types = htab_new(HTAB_DEFAULT_ORDER);
	pout->builtins = htab_new(HTAB_DEFAULT_ORDER);
	pout->type_set = type_set_new();
	pout->errs = list_new();

	/* initialization: fill type table with builtin types */

	type_t *t_builtin = calloc(2, sizeof(*t_builtin));
	t_builtin[0].tag = T_INTEGER;
	t_builtin[1].tag = T_BOOLEAN;
	htab_put(pout->types, "integer", t_builtin + 0);
	htab_put(pout->types, "boolean", t_builtin + 1);

	expr_t *e_builtin = calloc(2, sizeof(*e_builtin));
	e_builtin[0].op = OP_BOOLEAN_CONSTANT;
	e_builtin[1].op = OP_BOOLEAN_CONSTANT;
	e_builtin[0].t = t_builtin + 1; /* boolean */
	e_builtin[1].t = t_builtin + 1; /* boolean */
	e_builtin[0].uc = 0; /* false */
	e_builtin[1].uc = 1; /* true */
	e_builtin[0].line_no = -1;
	e_builtin[1].line_no = -1;
	htab_put(pout->builtins, "false", e_builtin + 0);
	htab_put(pout->builtins, "true", e_builtin + 1);

	/* first pass: allocate class_t for every class */

	LIST_EACH(pin->classes, cur, cd) {
		class_t *c;

		ct = htab_get(pout->types, cd->cid->id);

		if (ct != NULL) {
			assert_error(pout->errs, cd->cid->line_no,
				"Class \"%s\" already declared at line %d",
				cd->cid->id, ct->line_no);
			continue;
		}

		c = calloc(1, sizeof(*c));
		c->name = cd->cid->id;
		c->fields = list_new();
		c->functions = list_new();

		ct = calloc(1, sizeof(*ct));
		ct->tag = T_CLASS;
		ct->cls = c;
		ct->line_no = cd->cid->line_no;

		c->t = ct;

		htab_put(pout->types, c->name, ct);
	}

	if (pout->errs->length > 0)
		return pout;

	/* second pass: fill out classes type-relevant features */

	LIST_EACH(pin->classes, cur, cd) {
		ct = htab_get(pout->types, cd->cid->id);
		class_t *c;
		variable_declaration_t *vd;

		if (ct == NULL || ct->tag != T_CLASS)
			abort();

		c = ct->cls;

		if (cd->cid->has_extends) {
			c->extends = htab_get(pout->types, cd->cid->extends->id);
			if (c->extends == NULL) {
				assert_error(pout->errs,
					cd->cid->extends->line_no,
					"Class \"%s\" attempted to extend "
					"class \"%s\" which does not exist",
					c->name, cd->cid->extends->id);
				continue;
			}
			if (c->extends->tag != T_CLASS) {
				assert_error(pout->errs,
					cd->cid->extends->line_no,
					"Class \"%s\" attempted to extend "
					"\"%s\" which is not a class",
					c->name, cd->cid->extends->id);
				continue;
			}
			c->members = symtab_new(c->extends->cls->members, false);
		} else {
			c->extends = NULL;
			c->members = symtab_new(NULL, false);
		}

		LIST_EACH(cd->body->vd, cur2, vd)
			insert_class_fields(pout, c, vd);

		type_set_add(pout->type_set, ct);
	}

	if (pout->errs->length > 0)
		return pout;

	/* interlude: run type equivalence algorithm */

	type_set_update_equiv(pout->type_set);

	/* third pass: build class functions */

	LIST_EACH(pin->classes, cur, cd) {
		ct = htab_get(pout->types, cd->cid->id);
		class_t *c;
		function_declaration_t *fd;

		if (ct == NULL || ct->tag != T_CLASS)
			abort();

		c = ct->cls;

		LIST_EACH(cd->body->fd, cur2, fd)
			insert_class_function(pout, c, fd);
	}

	/* fourth pass: resolve class bodies */

	LIST_EACH(pin->classes, cur, cd) {
		ct = htab_get(pout->types, cd->cid->id);
		func_t *f;

		if (ct == NULL || ct->tag != T_CLASS)
			abort();

		LIST_EACH(ct->cls->functions, cur2, f) {
			if (!resolve_stmt(pout, f->local, f->body))
				break;
		}
	}

	/* last checks! */

	ct = htab_get(pout->types, pout->name);
	if (ct == NULL || ct->tag != T_CLASS) {
		assert_error(pout->errs, 0, "Missing program class");
	}

	return pout;
}

prog_t* build_semantic_tree(program_t *program)
{
	return flatten_program(program);
}
