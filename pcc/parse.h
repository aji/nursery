#ifndef __INC_PARSE_H__
#define __INC_PARSE_H__

#include "lexer.h"

struct p_expr;

struct p_ex_fn_args {
	struct p_expr *ex;
	struct p_ex_fn_args *next;
};

struct p_ex_fn {
	struct p_expr *fn;
	struct p_ex_fn_args *args;
};

struct p_expr {
	enum lx_token type;
	union {
		struct p_expr *child[3];
		struct p_ex_fn *fn;
		char *id;
		int num;
	};
};

struct p_arg_list {
	char *id;
	struct p_arg_list *next;
};

struct p_fn_decl {
	char *name;
	struct p_arg_list *args;
	struct p_expr *body;
};

struct p_decls {
	enum lx_token type;
	union {
		struct p_fn_decl *fn;
	};
	struct p_decls *next;
};

struct p_program {
	struct p_decls *decls;
};

extern void p_program_dump(struct p_program*);

extern struct p_expr *p_ex_p(void);
extern struct p_ex_fn_args *p_ex_fn_args(void);
extern struct p_expr *p_ex_fn(void);
extern struct p_expr *p_ex_mul(void);
extern struct p_expr *p_ex_add(void);
extern struct p_expr *p_ex_bin(void);
extern struct p_expr *p_ex_cmp(void);
extern struct p_expr *p_ex_tern(void);
extern struct p_expr *p_ex_comma(void);
extern struct p_expr *p_expr(void);

extern struct p_fn_decl *p_fn_decl(void);
extern void p_fn_name(struct p_fn_decl*);
extern void p_fn_def(struct p_fn_decl*);
extern struct p_arg_list *p_arg_list(void);
extern struct p_arg_list *p_arg_list2(void);

extern struct p_decls *p_decls(void);

extern struct p_program *p_program(void);

#endif
