#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "lambda.h"
#include "alias.h"

static void dump_ex(FILE *f, expr_t *ex)
{
	switch (ex->type) {
	case EX_VARIABLE:
		fprintf(f, "V%d\n", ex->var);
		break;

	case EX_LAMBDA:
		fprintf(f, "L%d\n", ex->fn.arg);
		dump_ex(f, ex->fn.body);
		break;

	case EX_APPLY:
		fprintf(f, "A\n");
		dump_ex(f, ex->app.left);
		dump_ex(f, ex->app.right);
		break;

	case EX_ALIAS:
		fprintf(f, "S%s\n", ex->name);
		break;
	}
}

static expr_t *read_ex(FILE *f)
{
	expr_t *left, *right;
	char *s, buf[256];

	fgets(buf, 256, f);
	s = strchr(buf, '\n');
	if (s) *s = '\0';

	switch (buf[0]) {
	case 'V':
		return ex_var(atoi(buf+1));
	case 'L':
		right = read_ex(f);
		return ex_lambda(atoi(buf+1), right);

	case 'A':
		left = read_ex(f);
		right = read_ex(f);
		return ex_apply(left, right);

	case 'S':
		return ex_alias(buf+1);
	}

	return NULL;
}

struct alias_n {
	char *name;
	expr_t *ex;

	struct alias_n *child[2];
};

static struct alias_n _aliases_root = { "", NULL, { NULL, NULL } };
static struct alias_n *aliases = &_aliases_root;

static struct alias_n *find(const char *name)
{
	struct alias_n *cur;
	int c;

	cur = aliases;
	while (cur) {
		c = strcmp(name, cur->name);
		if (c == 0)
			break;
		cur = cur->child[c < 0 ? 0 : 1];
	}

	return cur;
}

static void insert(struct alias_n *n)
{
	struct alias_n *cur;
	int c, dir;

	cur = aliases;
	while (cur) {
		c = strcmp(n->name, cur->name);

		if (c == 0) {
			free(n->name);
			ex_delete(cur->ex);
			cur->ex = n->ex;
			free(n);
			return;
		}

		dir = (c < 0 ? 0 : 1);

		if (cur->child[dir] == NULL) {
			cur->child[dir] = n;
			return;
		}

		cur = cur->child[dir];
	}
}

expr_t *alias_get(const char *name)
{
	struct alias_n *n = find(name);
	return n ? n->ex : NULL;
}

void alias_set(const char *name, expr_t *ex)
{
	struct alias_n *n;

	n = malloc(sizeof(*n));
	n->name = strdup(name);
	n->ex = ex_clone(ex);

	insert(n);
}

static void _save(struct alias_n *n, FILE *f)
{
	if (!n)
		return;

	if (n->name && *n->name && n->ex) {
		fprintf(f, "=%s\n", n->name);
		dump_ex(f, n->ex);
		putc('\n', f);
	}

	_save(n->child[0], f);
	_save(n->child[1], f);
}

void aliases_save(const char *fname)
{
	char name[PATH_MAX];
	FILE *f;

	snprintf(name, PATH_MAX, "%s.next", fname);

	f = fopen(name, "w");
	_save(aliases, f);
	rename(name, fname);
}

void aliases_load(const char *fname)
{
	char *s, name[256];
	FILE *f;

	f = fopen(fname, "r");
	if (!f)
		return;

	while (!feof(f)) {
		fgets(name, 256, f);
		s = strchr(name, '\n');
		if (s) *s = '\0'; else continue;
		if (name[0] != '=') continue;
		alias_set(name+1, read_ex(f));
	}
}

static void *_each(struct alias_n *cur,
                   void *(*cb)(const char*, expr_t*, void*), void *data)
{
	void *r;

	if (!cur)
		return NULL;

	if ((r = _each(cur->child[0], cb, data)) != NULL)
		return r;

	if (cur->name && *cur->name && cur->ex) {
		if ((r = cb(cur->name, cur->ex, data)) != NULL)
			return r;
	}

	if ((r = _each(cur->child[1], cb, data)) != NULL)
		return r;

	return NULL;
}

void *aliases_each(void *(*cb)(const char*, expr_t*, void*), void *data)
{
	return _each(aliases, cb, data);
}
