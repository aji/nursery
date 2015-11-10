/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

#ifndef __INC_DUMP_H__
#define __INC_DUMP_H__

#include "container.h"
#include "shared.h"

typedef struct dump_context {
	int level;
} dump_context_t;

extern void dump_type_denoter(dump_context_t*, type_denoter_t*);
extern void dump_function_designator(dump_context_t*, function_designator_t*);
extern void dump_actual_parameter(dump_context_t*, actual_parameter_t*);
extern void dump_variable_declaration(dump_context_t*, variable_declaration_t*);
extern void dump_formal_parameter_section(dump_context_t*,
                                          formal_parameter_section_t*);
extern void dump_variable_access(dump_context_t*, variable_access_t*);
extern void dump_assignment_statement(dump_context_t*, assignment_statement_t*);
extern void dump_object_instantiation(dump_context_t*, object_instantiation_t*);
extern void dump_expression(dump_context_t*, expression_t*);
extern void dump_statement(dump_context_t*, statement_t*);
extern void dump_if_statement(dump_context_t*, if_statement_t*);
extern void dump_while_statement(dump_context_t*, while_statement_t*);
extern void dump_indexed_variable(dump_context_t*, indexed_variable_t*);
extern void dump_attribute_designator(dump_context_t*, attribute_designator_t*);
extern void dump_method_designator(dump_context_t*, method_designator_t*);
extern void dump_simple_expression(dump_context_t*, simple_expression_t*);
extern void dump_term(dump_context_t*, term_t*);
extern void dump_factor(dump_context_t*, factor_t*);
extern void dump_primary(dump_context_t*, primary_t*);
extern void dump_array_type(dump_context_t*, array_type_t*);
extern void dump_class_block(dump_context_t*, class_block_t*);
extern void dump_function_declaration(dump_context_t*, function_declaration_t*);
extern void dump_function_block(dump_context_t*, function_block_t*);
extern void dump_function_heading(dump_context_t*, function_heading_t*);
extern void dump_class_definition(dump_context_t*, class_definition_t*);
extern void dump_class_identification(dump_context_t*, class_identification_t*);
extern void dump_program(dump_context_t*, program_t*);
extern void dump_program_heading(dump_context_t*, program_heading_t*);

#endif
