/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

/* assorted containers */
/* Copyright (c) 2014 Alex Iadicicco */

#pragma once

#include <sys/types.h>
#include <stdlib.h>

#ifdef __GNUC__
#define __unused __attribute__((unused))
#else
#define __unused
#endif

/* linked lists */

typedef struct list_node list_node_t;
typedef struct list list_t;

struct list_node {
	void *v;
	list_node_t *next, *prev;
};

struct list {
	list_node_t root;
	size_t length;
};

#define LIST_EACH_NODE(LL, CUR) \
	for ((CUR)=(LL)->root.next; \
	     (CUR)!=&((LL)->root); \
	     (CUR)=(CUR)->next)
#define LIST_EACH_NODE_SAFE(LL, CUR, NCUR) \
	for ((CUR)=(LL)->root.next; \
	     (NCUR)=(CUR)->next, (CUR)!=&((LL)->root); \
	     (CUR)=(NCUR))
#define LIST_EACH(LL, CUR, VAL) \
	for ((CUR)=(LL)->root.next; \
	     (VAL)=(CUR)->v, (CUR)!=&((LL)->root); \
	     (CUR)=(CUR)->next)

extern list_t *list_new(void);
extern void list_release(list_t*);
extern list_node_t *list_append(list_t*, void*);                    /* O(1) */
extern list_node_t *list_prepend(list_t*, void*);                   /* O(1) */
extern list_node_t *list_add_before(list_t*, list_node_t*, void*);  /* O(1) */
extern list_node_t *list_add_after(list_t*, list_node_t*, void*);   /* O(1) */
extern void list_delete(list_t*, list_node_t*);                     /* O(1) */
extern list_node_t *list_find(list_t*, void*);                      /* O(n) */
extern void list_delete_val(list_t*, void*);                        /* O(n) */

static inline int list_is_last(list_t *ll, list_node_t *n)
{
	return n->next == &ll->root;
}

static inline int list_is_first(list_t *ll, list_node_t *n)
{
	return n->prev == &ll->root;
}

static inline int list_is_nil(list_t *ll, list_node_t *n)
{
	return !n || n == &ll->root;
}

static inline list_node_t *list_head(list_t *ll)
{
	return ll->root.next == &ll->root ? NULL : ll->root.next;
}

static inline list_node_t *list_tail(list_t *ll)
{
	return ll->root.prev == &ll->root ? NULL : ll->root.prev;
}

/* vectors */

typedef struct vec vec_t;

struct vec {
	unsigned size, capacity;
	void **v;
};

extern vec_t *vec_new(unsigned capacity);
extern void vec_release(vec_t *vec);
extern void vec_set(vec_t*, unsigned idx, void*);
extern void vec_append(vec_t*, void*);

static inline void *vec_get(vec_t *vec, unsigned idx)
{
	if (idx >= vec->size)
		return NULL;

	return vec->v[idx];
}

/* hash tables */

typedef struct htab htab_t;
typedef struct htab_cell htab_cell_t; /* defined internally */

#define HTAB_DEFAULT_ORDER 5

struct htab {
	unsigned order; /* size = 2^order */
	unsigned load;
	htab_cell_t **tab;
};

extern htab_t *htab_new(unsigned initial_order); /* size = 2^order */
extern void htab_release(htab_t*);
extern void htab_put(htab_t*, char *k, void *v);
extern void *htab_get(htab_t*, char *k);
extern void *htab_delete(htab_t*, char *k);

extern void htab_each(htab_t*, void (*cb)(void*, char *k, void *v), void*);

/* aa trees (coming soon!) */

/* radix tries */

typedef struct trie trie_t;
typedef void (trie_canonize_t)(char*);

extern trie_t *trie_new(trie_canonize_t *canonize);
extern void trie_release(trie_t*);
extern void trie_put(trie_t*, char*, void*);
extern void *trie_get(trie_t*, char*);
extern void *trie_delete(trie_t*, char*);
