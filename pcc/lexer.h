/* stateful lexer */

#ifndef __INC_LEXER_H__
#define __INC_LEXER_H__

enum lx_token
{
	T_INVALID,
	T_EOF,
	T_ID,
	T_INT,
	T_LPAR,
	T_RPAR,
	T_LBRACK,
	T_RBRACK,
	T_ADD,
	T_SUB,
	T_MUL,
	T_DIV,
	T_MOD,
	T_LSH,
	T_RSH,
	T_AND,
	T_OR,
	T_XOR,
	T_EQ,
	T_NE,
	T_GT,
	T_LT,
	T_GE,
	T_LE,
	T_LAND,
	T_LOR,
	T_NEG,
	T_QUES,
	T_COLON,
	T_COMMA,
	T_ASSIGN,
	T_FN,
};

extern const char *lx_names[];
extern char lx_tokstr[];

extern enum lx_token lx_token(void);
extern void lx_unget(enum lx_token);

extern void lx_init(void);

#endif
