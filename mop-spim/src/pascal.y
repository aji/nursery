/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

%{
/*
 * grammar.y
 *
 * Pascal grammar in Yacc format, based originally on BNF given
 * in "Standard Pascal -- User Reference Manual", by Doug Cooper.
 * This in turn is the BNF given by the ANSI and ISO Pascal standards,
 * and so, is PUBLIC DOMAIN. The grammar is for ISO Level 0 Pascal.
 * The grammar has been massaged somewhat to make it LALR.
 */

#include <string.h>
#include <stdlib.h>

#include "shared.h"
#include "container.h"

int yylex(void);
void yyerror(const char *error);

extern char *yytext;          /* yacc text variable */
extern int line_number;       /* Holds the current line number; specified
                                 in the lexer */
program_t *program;           /* points to our program */
%}

%token AND ARRAY ASSIGNMENT CLASS COLON COMMA DIGSEQ
%token DO DOT DOTDOT ELSE END EQUAL EXTENDS FUNCTION
%token GE GT IDENTIFIER IF LBRAC LE LPAREN LT MINUS MOD NEW NOT
%token NOTEQUAL OF OR PBEGIN PLUS PRINT PROGRAM RBRAC
%token RPAREN SEMICOLON SLASH STAR THEN
%token VAR WHILE

%type <tden> type_denoter
%type <id> result_type
%type <id> identifier
%type <list> identifier_list
%type <fdes> function_designator
%type <list> actual_parameter_list
%type <list> params
%type <ap> actual_parameter
%type <vd> variable_declaration
%type <list> variable_declaration_list
%type <r> range
%type <un> unsigned_integer
%type <list> formal_parameter_section_list
%type <fps> formal_parameter_section
%type <fps> value_parameter_specification
%type <fps> variable_parameter_specification
%type <va> variable_access
%type <as> assignment_statement
%type <os> object_instantiation
%type <va> print_statement
%type <e> expression
%type <s> statement
%type <list> compound_statement
%type <list> statement_list
%type <s> statement_part
%type <is> if_statement
%type <ws> while_statement
%type <e> boolean_expression
%type <iv> indexed_variable
%type <ad> attribute_designator
%type <md> method_designator
%type <list> index_expression_list
%type <e> index_expression
%type <se> simple_expression
%type <t> term
%type <f> factor
%type <op> sign
%type <p> primary
%type <un> unsigned_constant
%type <un> unsigned_number
%type <at> array_type
%type <cb> class_block
%type <list> variable_declaration_part
%type <list> func_declaration_list
%type <funcd> function_declaration
%type <fb> function_block
%type <fh> function_heading
%type <id> function_identification
%type <list> formal_parameter_list
%type <list> class_list
%type <ci> class_identification
%type <program> program
%type <ph> program_heading
%type <op> relop
%type <op> addop
%type <op> mulop

%union {
	type_denoter_t *tden;
	identifier_t *id;
	function_designator_t *fdes;
	actual_parameter_t *ap;
	variable_declaration_t *vd;
	range_t *r;
	unsigned long un;
	formal_parameter_section_t *fps;
	variable_access_t *va;
	assignment_statement_t *as;
	object_instantiation_t *os;
	expression_t *e;
	statement_t *s;
	if_statement_t *is;
	while_statement_t *ws;
	indexed_variable_t *iv;
	attribute_designator_t *ad;
	method_designator_t *md;
	simple_expression_t *se;
	term_t *t;
	factor_t *f;
	primary_t *p;
	array_type_t *at;
	class_block_t *cb;
	function_declaration_t *funcd;
	function_block_t *fb;
	function_heading_t *fh;
	class_identification_t *ci;
	program_t *program;
	program_heading_t *ph;
	int op;
	list_t *list;
}

%start program

%%

program:
	program_heading semicolon class_list DOT {
		$$ = malloc(sizeof(*$$));
		$$->ph = $1;
		$$->classes = $3;

		program = $$;
	};

program_heading:
	PROGRAM identifier {
		$$ = malloc(sizeof(*$$));
		$$->has_id_list = false;
		$$->id = $2->id;
		$$->id_list = NULL;
		$$->line_no = $2->line_no;
		free($2);
	}
	| PROGRAM identifier LPAREN identifier_list RPAREN {
		$$ = malloc(sizeof(*$$));
		$$->has_id_list = true;
		$$->id = $2->id;
		$$->id_list = $4;
		$$->line_no = $2->line_no;
		free($2);
	};

identifier_list:
	identifier_list comma identifier {
		$$ = $1;
		list_append($$, $3);
        }
	| identifier {
		$$ = list_new();
		list_append($$, $1);
        };

class_list:
	class_list class_identification PBEGIN class_block END {
		class_definition_t *cd = malloc(sizeof(*cd));
		cd->cid = $2;
		cd->body = $4;

		$$ = $1;
		list_append($$, cd);
	}
	| class_identification PBEGIN class_block END {
		class_definition_t *cd = malloc(sizeof(*cd));
		cd->cid = $1;
		cd->body = $3;

		$$ = list_new();
		list_append($$, cd);
	};

class_identification:
	CLASS identifier {
		$$ = malloc(sizeof(*$$));
		$$->has_extends = false;
		$$->id = $2->id;
		$$->extends = NULL;
		$$->line_no = line_number;
		free($2);
	}
	| CLASS identifier EXTENDS identifier {
		$$ = malloc(sizeof(*$$));
		$$->has_extends = true;
		$$->id = $2->id;
		$$->extends = $4;
		$$->line_no = line_number;
		free($2);
	};

class_block:
	variable_declaration_part func_declaration_list {
		$$ = malloc(sizeof(*$$));
		$$->vd = $1;
		$$->fd = $2;
	};

type_denoter:
	array_type {
		$$ = malloc(sizeof(*$$));
		$$->is_array = true;
		$$->a = $1;
		$$->line_no = line_number;
	}
	| identifier {
		$$ = malloc(sizeof(*$$));
		$$->is_array = false;
		$$->id = $1->id;
		$$->line_no = $1->line_no;
		free($1);
	};

array_type:
	ARRAY LBRAC range RBRAC OF type_denoter {
		$$ = malloc(sizeof(*$$));
		$$->r = $3;
		$$->td = $6;
		$$->line_no = line_number;
	};

range:
	unsigned_integer DOTDOT unsigned_integer {
		$$ = malloc(sizeof(*$$));
		$$->low = $1;
		$$->high = $3;
	};

variable_declaration_part:
	VAR variable_declaration_list semicolon {
		$$ = $2;
	}
	| {
		$$ = list_new();
	};

variable_declaration_list:
	variable_declaration_list semicolon variable_declaration {
		$$ = $1;
		list_append($$, $3);
	}
	| variable_declaration {
		$$ = list_new();
		list_append($$, $1);
	};

variable_declaration:
	identifier_list COLON type_denoter {
		$$ = malloc(sizeof(*$$));
		$$->id_list = $1;
		$$->type = $3;
		$$->line_no = line_number;
	};

func_declaration_list:
	func_declaration_list semicolon function_declaration {
		$$ = $1;
		list_append($$, $3);
	}
	| function_declaration {
		$$ = list_new();
		list_append($$, $1);
	}
	| {
		$$ = list_new();
	};

formal_parameter_list:
	LPAREN formal_parameter_section_list RPAREN {
		$$ = $2;
	};

formal_parameter_section_list:
	formal_parameter_section_list semicolon formal_parameter_section {
		$$ = $1;
		list_append($$, $3);
	}
	| formal_parameter_section {
		$$ = list_new();
		list_append($$, $1);
	};

formal_parameter_section:
	value_parameter_specification
	| variable_parameter_specification
	;

value_parameter_specification:
	identifier_list COLON identifier {
		$$ = malloc(sizeof(*$$));
		$$->is_var = false;
		$$->ids = $1;
		$$->id = $3->id;
		$$->line_no = $3->line_no;
		free($3);
	};

variable_parameter_specification:
	VAR identifier_list COLON identifier {
		$$ = malloc(sizeof(*$$));
		$$->is_var = true;
		$$->ids = $2;
		$$->id = $4->id;
		$$->line_no = $4->line_no;
		free($4);
	};

function_declaration:
	function_identification semicolon function_block {
		$$ = malloc(sizeof(*$$));
		$$->is_heading = false;
		$$->id = $1->id;
		$$->fb = $3;
		$$->line_no = $1->line_no;
		free($1);
	}
	| function_heading semicolon function_block {
		$$ = malloc(sizeof(*$$));
		$$->is_heading = true;
		$$->fh = $1;
		$$->fb = $3;
	};

function_heading:
	FUNCTION identifier COLON result_type {
		$$ = malloc(sizeof(*$$));
		$$->has_fpl = false;
		$$->id = $2->id;
		$$->fpl = list_new();
		$$->result_type = $4->id;
		$$->line_no = $2->line_no;
		free($2);
		free($4);
	}
	| FUNCTION identifier formal_parameter_list COLON result_type {
		$$ = malloc(sizeof(*$$));
		$$->has_fpl = true;
		$$->id = $2->id;
		$$->fpl = $3;
		$$->result_type = $5->id;
		$$->line_no = $2->line_no;
		free($2);
		free($5);
	};

result_type:
	identifier;

function_identification:
	FUNCTION identifier {
		$$ = $2;
	};

function_block:
	variable_declaration_part statement_part {
		$$ = malloc(sizeof(*$$));
		$$->vp = $1;
		$$->sp = $2;
	};

statement_part:
	compound_statement {
		$$ = malloc(sizeof(*$$));
		$$->tag = ST_COMPOUND;
		$$->cs = $1;
		$$->line_no = line_number;
	};

compound_statement:
	PBEGIN statement_list END {
		$$ = $2;
	};

statement_list:
	statement {
		$$ = list_new();
		list_append($$, $1);
	}
	| statement_list semicolon statement {
		$$ = $1;
		list_append($$, $3);
	};

statement:
	assignment_statement {
		$$ = malloc(sizeof(*$$));
		$$->tag = ST_ASSIGNMENT;
		$$->as = $1;
		$$->line_no = line_number;
	}
	| compound_statement {
		$$ = malloc(sizeof(*$$));
		$$->tag = ST_COMPOUND;
		$$->cs = $1;
		$$->line_no = line_number;
	}
	| if_statement {
		$$ = malloc(sizeof(*$$));
		$$->tag = ST_IF;
		$$->is = $1;
		$$->line_no = line_number;
	}
	| while_statement {
		$$ = malloc(sizeof(*$$));
		$$->tag = ST_WHILE;
		$$->ws = $1;
		$$->line_no = line_number;
	}
	| print_statement {
		$$ = malloc(sizeof(*$$));
		$$->tag = ST_PRINT;
		$$->va = $1;
		$$->line_no = line_number;
        };

while_statement:
	WHILE boolean_expression DO statement {
		$$ = malloc(sizeof(*$$));
		$$->condition = $2;
		$$->st = $4;
	};

if_statement:
	IF boolean_expression THEN statement ELSE statement {
		$$ = malloc(sizeof(*$$));
		$$->condition = $2;
		$$->tb = $4;
		$$->eb = $6;
	};

assignment_statement:
	variable_access ASSIGNMENT expression {
		$$ = malloc(sizeof(*$$));
		$$->is_new = false;
		$$->va = $1;
		$$->e = $3;
	}
	| variable_access ASSIGNMENT object_instantiation {
		$$ = malloc(sizeof(*$$));
		$$->is_new = true;
		$$->va = $1;
		$$->oi = $3;
	};

object_instantiation:
	NEW identifier {
		$$ = malloc(sizeof(*$$));
		$$->has_params = false;
		$$->id = $2->id;
		$$->params = list_new();
		$$->line_no = $2->line_no;
		free($2);
	}
	| NEW identifier params {
		$$ = malloc(sizeof(*$$));
		$$->has_params = true;
		$$->id = $2->id;
		$$->params = $3;
		$$->line_no = $2->line_no;
		free($2);
	};

print_statement:
	PRINT variable_access { $$ = $2; };

variable_access:
	identifier {
		$$ = malloc(sizeof(*$$));
		$$->tag = VA_IDENTIFIER;
		$$->id = $1->id;
		$$->line_no = $1->line_no;
		free($1);
	}
	| indexed_variable {
		$$ = malloc(sizeof(*$$));
		$$->tag = VA_INDEXED;
		$$->iv = $1;
		$$->line_no = line_number;
	}
	| attribute_designator {
		$$ = malloc(sizeof(*$$));
		$$->tag = VA_ATTRIBUTE;
		$$->ad = $1;
		$$->line_no = line_number;
	}
	| method_designator {
		$$ = malloc(sizeof(*$$));
		$$->tag = VA_METHOD;
		$$->md = $1;
		$$->line_no = line_number;
	};

indexed_variable:
	variable_access LBRAC index_expression_list RBRAC {
		$$ = malloc(sizeof(*$$));
		$$->va = $1;
		$$->idxs = $3;
		$$->line_no = line_number;
	};

index_expression_list:
	index_expression_list comma index_expression {
		$$ = $1;
		list_append($$, $3);
	}
	| index_expression {
		$$ = list_new();
		list_append($$, $1);
	};

index_expression:
	expression;

attribute_designator:
	variable_access DOT identifier {
		$$ = malloc(sizeof(*$$));
		$$->va = $1;
		$$->id = $3->id;
		$$->line_no = $3->line_no;
		free($3);
	};

method_designator:
	variable_access DOT function_designator {
		$$ = malloc(sizeof(*$$));
		$$->va = $1;
		$$->fd = $3;
		$$->line_no = line_number;
	};


params:
	LPAREN actual_parameter_list RPAREN { $$ = $2; };

actual_parameter_list:
	actual_parameter_list comma actual_parameter {
		$$ = $1;
		list_append($$, $3);
	}
	| actual_parameter {
		$$ = list_new();
		list_append($$, $1);
	};

actual_parameter:
	expression {
		$$ = malloc(sizeof(*$$));
		$$->exp1 = $1;
		$$->exp2 = NULL;
		$$->exp3 = NULL;
		$$->line_no = line_number;
	}
	| expression COLON expression {
		$$ = malloc(sizeof(*$$));
		$$->exp1 = $1;
		$$->exp2 = $3;
		$$->exp3 = NULL;
		$$->line_no = line_number;
	}
	| expression COLON expression COLON expression {
		$$ = malloc(sizeof(*$$));
		$$->exp1 = $1;
		$$->exp2 = $3;
		$$->exp3 = $5;
		$$->line_no = line_number;
	};

boolean_expression:
	expression;

expression:
	simple_expression {
		$$ = malloc(sizeof(*$$));
		$$->is_one = true;
		$$->e1 = $1;
		$$->e2 = NULL;
		$$->relop = -1;
		$$->line_no = line_number;
	}
	| simple_expression relop simple_expression {
		$$ = malloc(sizeof(*$$));
		$$->is_one = false;
		$$->e1 = $1;
		$$->e2 = $3;
		$$->relop = $2;
		$$->line_no = line_number;
	};

simple_expression:
	term {
		$$ = malloc(sizeof(*$$));
		$$->is_term = true;
		$$->t = $1;
		$$->se = NULL;
		$$->addop = -1;
		$$->line_no = line_number;
	}
	| simple_expression addop term {
		$$ = malloc(sizeof(*$$));
		$$->is_term = false;
		$$->t = $3;
		$$->se = $1;
		$$->addop = $2;
		$$->line_no = line_number;
	};

term:
	factor {
		$$ = malloc(sizeof(*$$));
		$$->is_factor = true;
		$$->f = $1;
		$$->t = NULL;
		$$->mulop = -1;
		$$->line_no = line_number;
	}
	| term mulop factor {
		$$ = malloc(sizeof(*$$));
		$$->is_factor = false;
		$$->f = $3;
		$$->t = $1;
		$$->mulop = $2;
		$$->line_no = line_number;
	};

factor:
	sign factor {
		$$ = malloc(sizeof(*$$));
		$$->is_sign = true;
		$$->sign = $1;
		$$->f = $2;
		$$->line_no = line_number;
	}
	| primary {
		$$ = malloc(sizeof(*$$));
		$$->is_sign = false;
		$$->p = $1;
		$$->line_no = line_number;
	};

primary:
	variable_access {
		$$ = malloc(sizeof(*$$));
		$$->tag = PR_VARIABLE_ACCESS;
		$$->va = $1;
		$$->line_no = $1->line_no;
	}
	| unsigned_constant {
		$$ = malloc(sizeof(*$$));
		$$->tag = PR_UNSIGNED_CONSTANT;
		$$->uc = $1;
		$$->line_no = line_number;
	}
	| function_designator {
		$$ = malloc(sizeof(*$$));
		$$->tag = PR_FUNCTION_DESIGNATOR;
		$$->fd = $1;
		$$->line_no = $1->line_no;
	}
	| LPAREN expression RPAREN {
		$$ = malloc(sizeof(*$$));
		$$->tag = PR_EXPRESSION;
		$$->e = $2;
		$$->line_no = $2->line_no;
	}
	| NOT primary {
		$$ = malloc(sizeof(*$$));
		$$->tag = PR_NOT_PRIMARY;
		$$->p = $2;
		$$->line_no = $2->line_no;
	};

unsigned_constant:
	unsigned_number;

unsigned_number:
	unsigned_integer;

unsigned_integer:
	DIGSEQ {
		$$ = atoi(yytext);
	};

/* functions with no params will be handled by plain identifier */
function_designator:
	identifier params {
		$$ = malloc(sizeof(*$$));
		$$->id = $1->id;
		$$->params = $2;
		$$->line_no = $1->line_no;
		free($1);
	};

sign:
  PLUS       { $$ = PLUS;                                                 } |
  MINUS      { $$ = MINUS;                                                } ;

addop:
  PLUS       { $$ = PLUS;                                                 } |
  MINUS      { $$ = MINUS;                                                } |
  OR         { $$ = OR;                                                   } ;

mulop:
  STAR       { $$ = STAR;                                                 } |
  SLASH      { $$ = SLASH;                                                } |
  MOD        { $$ = MOD;                                                  } |
  AND        { $$ = AND;                                                  } ;

relop:
  EQUAL      { $$ = EQUAL;                                                } |
  NOTEQUAL   { $$ = NOTEQUAL;                                             } |
  LT         { $$ = LT;                                                   } |
  GT         { $$ = GT;                                                   } |
  LE         { $$ = LE;                                                   } |
  GE         { $$ = GE;                                                   } ;

identifier:
	IDENTIFIER {
		$$ = malloc(sizeof(*$$));
		$$->id = strdup(yytext);
		$$->line_no = line_number;
	};

semicolon:
	SEMICOLON;

comma:
	COMMA;

%%
