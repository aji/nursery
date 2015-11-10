#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h>

#include "lambda.h"
#include "alias.h"

#define VAR_REPR_TABLE_SIZE 26

static int var_repr_table[VAR_REPR_TABLE_SIZE];
static const char *var_reprs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static char var_repr(int n)
{
	int j, i = n % VAR_REPR_TABLE_SIZE;

	if (n < 256)
		return n;

	j = 0;
	while (j<VAR_REPR_TABLE_SIZE && var_repr_table[i] > 0
	       && var_repr_table[i] != n) {
		j++;
		i = (i + 1) % VAR_REPR_TABLE_SIZE;
	}

	if (j == VAR_REPR_TABLE_SIZE)
		return '@';

	var_repr_table[i] = n;
	return var_reprs[i];
}

static void var_repr_reset(void)
{
	int i;

	for (i=0; i<VAR_REPR_TABLE_SIZE; i++)
		var_repr_table[i] = -1;
}

static void *find_replacement(const char *name, expr_t *ex, void *nex)
{
	if (equiv(ex, (expr_t*) nex))
		return (void*) name;
	return NULL;
}

static expr_t *try_replace(expr_t *ex)
{
	/* don't bother with this for now
	char *alias;

	alias = (char*) aliases_each(find_replacement, ex);
	if (alias) {
		ex_delete(ex);
		ex = ex_alias(alias);
	}
	*/

	return ex;
}

/* wow, much inefficient */
static expr_t *replace_aliases(expr_t *ex)
{
	expr_t *nex;
	char buf[512];
	int n;

	if (!ex)
		return NULL;

	n = cn_decode(ex);
	if (n > 0) {
		snprintf(buf, 512, "%d", n);
		return ex_alias(buf);
	}

	n = cb_decode(ex);
	if (n != -1) {
		snprintf(buf, 512, "%c", n ? 'T' : 'F');
		return ex_alias(buf);
	}

	nex = try_replace(ex_clone(ex));
	ex = NULL;

	switch (nex->type) {
	case EX_VARIABLE:
		ex = NULL;
		break;

	case EX_ALIAS: /* don't even bother */
		ex = NULL;
		break;

	case EX_LAMBDA:
		ex = ex_lambda(nex->fn.arg,
		                replace_aliases(nex->fn.body));
		break;

	case EX_APPLY:
		ex = ex_apply(replace_aliases(nex->app.left),
		              replace_aliases(nex->app.right));
		break;
	}

	if (ex != NULL) {
		ex_delete(nex);
		nex = ex;
	}

	return try_replace(nex);
}

static void print(expr_t *ex, int depth)
{
	int c;

	if (!ex) {
		printf("()"); /* ??? */
		return;
	}

	switch (ex->type) {
	case EX_VARIABLE:
		putchar(var_repr(ex->var));
		break;

	case EX_ALIAS:
		printf("%s", ex->name);
		break;

	case EX_LAMBDA:
		if (depth)
			putchar('(');
		c = '\\';
		do {
			putchar(c);
			c = ' ';
			putchar(var_repr(ex->fn.arg));
			ex = ex->fn.body;
		} while (ex->type == EX_LAMBDA);
		printf(". ");
		print(ex, depth+1);
		if (depth)
			putchar(')');
		break;

	case EX_APPLY:
		print(ex->app.left, depth+1);
		putchar(' ');
		if (ex->app.right->type == EX_APPLY)
			putchar('(');
		print(ex->app.right, depth+1);
		if (ex->app.right->type == EX_APPLY)
			putchar(')');
		break;
	}
}

static void println(expr_t *ex, bool replace)
{
	expr_t *nex;
	var_repr_reset();
	nex = replace ? replace_aliases(ex) : ex;
	print(nex, 0);
	if (replace) ex_delete(nex);
	putchar('\n');
}

static void *list_alias(const char *name, expr_t *ex, void *name_find)
{
	if (name_find) {
		if (!strcmp(name, name_find)) {
			printf("%s := ", name);
			print(ex, 0);
			return name_find;
		}
	} else {
		printf(" %s", name);
	}

	return NULL;
}

static void list_aliases(char *name_find)
{
	if (!name_find)
		printf("aliases:");
	if (!aliases_each(list_alias, name_find) && name_find)
		printf("no such alias %s", name_find);
	putchar('\n');
}

static void do_alias(void)
{
	expr_t *ex;
	char *s, name[256];
	int c;

	do {
		c = getc(lx_file);
	} while (c != '\n' && isspace(c));

	if (c == '\n') {
		list_aliases(NULL);
		return;
	}

	ungetc(c, lx_file);

	s = name;
	for (;;) {
		c = getc(lx_file);
		if (isspace(c))
			break;
		*s++ = c;
	}
	*s = '\0';
	if (!name[0]) {
		fprintf(stderr, "alias names cannot be empty\n");
		exit(1);
	}

	ex = p_expr_line();
	if (ex != NULL) {
		alias_set(name, ex);
		aliases_save("aliases.db");
		printf("added alias %s\n", name);
	} else {
		list_aliases(name);
	}
}

static expr_t *get_alias(char *name)
{
	expr_t *ex;
	char *s;

	ex = alias_get(name);

	s = name;

	/* the 1 char aliases */
	if (s[0] && !s[1]) switch (s[0]) {
	case 'T': return cb_encode(1);
	case 'F': return cb_encode(0);
	}

	for (; *s; s++) {
		if (!isdigit(*s))
			return ex;
	}

	return cn_encode(atoi(name));
}

int main(int argc, char *argv[])
{
	expr_t *ex, *nex;
	bool replace = true;
	int i, c;

	bool verbose = false;

	while ((c = getopt(argc, argv, "v")) != -1) switch (c) {
	case 'v': verbose = true; break;
	}

	stderr = stdout;

	aliases_load("aliases.db");
	ex_get_alias = get_alias;

	lx_file = stdin;

	c = getc(lx_file);
	if (c == '=') {
		do_alias();
		return 0;
	} else if (c == '\'') {
		replace = false;
	} else {
		ungetc(c, lx_file);
	}

	ex = p_expr_line();

	for (i=0; i<2000; i++) {
		if (verbose)
			println(ex, false);
		nex = reduce(ex);
		if (equiv(ex, nex))
			break;
		ex_delete(ex);
		ex = nex;
	}

	println(ex, replace);

	return 0;
}
