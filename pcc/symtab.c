#include <stdlib.h>
#include <string.h>

struct symtab_n {
	char *k;
	void *v;
	struct symtab_n *next;
};

struct symtab {
	struct symtab_n *head;
	struct symtab *up;
};

struct symtab_n *find(struct symtab_n *n, char *k)
{
	for (; n; n = n->next) {
		if (!strcmp(n->k, k))
			return n;
	}
	return NULL;
}

struct symtab *symtab_new(struct symtab *up)
{
	struct symtab *tab;

	tab = malloc(sizeof(*tab));
	tab->head = NULL;
	tab->up = up;

	return tab;
}

void symtab_set(struct symtab *tab, char *key, void *val)
{
	struct symtab_n *n;

	n = malloc(sizeof(*n));
	n->k = strdup(key);
	n->v = val;

	n->next = tab->head;
	tab->head = n;
}

void *symtab_get(struct symtab *tab, char *key)
{
	for (; tab; tab = tab->up) {
		struct symtab_n *n = find(tab->head, key);
		if (n != NULL)
			return n->v;
	}
}

void symtab_free(struct symtab *tab)
{
	struct symtab_n *n;

	for (n = tab->head; n; n = n->next) {
		free(n->k);
		free(n);
	}

	free(tab);
}
