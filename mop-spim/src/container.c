/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

/* assorted containers */

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "container.h"

/* algorithm support */

static int nibble(char *s, int nibnum)
{
	if (nibnum & 1) {
		return ((unsigned) s[nibnum >> 1]) & 0xf;
	} else {
		return ((unsigned) s[nibnum >> 1]) >> 4;
	}
}

typedef unsigned long hash_t;
typedef unsigned char octet_t;

/* we have to implement as follows so that FNV works on both 32 and 64 bit
   systems. not that bad, it just means that the multiply by prime step is
   a little more complicated than it needs to be.

   all 8 octet vectors are big-endian representations of their 64 bit
   counterparts

   i don't know if this implementation of FNV-1a is exactly to spec or not,
   but it seems to act like a good hash function so i'm allowing it */

static const octet_t FNV_OFFSET_BASIS[8] =
	{ 0xcb, 0xf2, 0x9c, 0xe4, 0x84, 0x22, 0x23, 0x25 };

static hash_t fnv1a(void *_s, unsigned len)
{
	octet_t hash[8], product[8];
	octet_t *s = (octet_t*)_s;
	unsigned carry;
	int i;

	for (i=0; i<8; i++)
		hash[i] = FNV_OFFSET_BASIS[i];

	while (len--) {
		/* xor next octet in. easy */
		hash[7] ^= *s;

		/* multiply by the prime. not so easy */

		/* the prime p is 0x100000001b3, which equals (1<<40) +
		   (1<<8) + (0xb3). so to compute h * p, we distribute h
		   across the summands and add those */

		for (i=1; i<8; i++) /* 1<<8 */
			product[i - 1] = hash[i];
		product[7] = 0x00;

		for (i=5; i<8; i++) /* 1<<40 */
			product[i - 5] += hash[i];

		carry = 0;
		for (i=8; --i>0; ) { /* 0xb3 */
			carry = hash[i] * 0xb3 + carry;
			product[i] += carry & 0xff;
			carry >>= 8;
		}

		for (i=0; i<8; i++)
			hash[i] = product[i];
	}

	/*
	for (i=0; i<8; i++)
		printf("%02x", hash[i]);
	printf("\n");
	*/

	return  0
#if 0 /* maybe we can ifdef this to something 64 bit? */
		+ (hash[0] << 56) + (hash[1] << 48)
		+ (hash[2] << 40) + (hash[3] << 32)
#endif
		+ (hash[4] << 24) + (hash[5] << 16)
		+ (hash[6] <<  8) + (hash[7] <<  0);
}

/*
 * linked lists
 */

#define ROOT(p) (&(p)->root)

static list_t *list_alloc(void)
{
	return calloc(1, sizeof(list_t));
}

static list_node_t *list_node_alloc(void *v)
{
	list_node_t *n = calloc(1, sizeof(list_node_t));
	n->v = v;

	return n;
}

static void list_node_free(list_node_t *n)
{
	free(n);
}

list_t *list_new(void)
{
	list_t *ll = list_alloc();

	ROOT(ll)->next = ROOT(ll);
	ROOT(ll)->prev = ROOT(ll);

	return ll;
}

void list_release(list_t *ll)
{
	list_node_t *n, *nn;

	LIST_EACH_NODE_SAFE(ll, n, nn)
		list_node_free(n);

	free(ll);
}

void list_release_callback(list_t *ll, void (*free_node)(const void*) )
{
	list_node_t *n, *nn;

	LIST_EACH_NODE_SAFE(ll, n, nn) {
		(*free_node)(n->v);
		list_node_free(n);
	}

	free(ll);
}

static void __ll_put_after(list_node_t *aft, list_node_t *n)
{
	n->next = aft->next;
	n->prev = aft;

	n->next->prev = n;
	n->prev->next = n;
}

list_node_t *list_append(list_t *ll, void *v)
{
	list_node_t *n = list_node_alloc(v);

	__ll_put_after(ROOT(ll)->prev, n);
	ll->length += 1;

	return n;
}

list_node_t *list_prepend(list_t *ll, void *v)
{
	list_node_t *n = list_node_alloc(v);

	__ll_put_after(ROOT(ll), n);
	ll->length += 1;

	return n;
}

list_node_t *list_add_before(list_t *ll, list_node_t *n, void *v)
{
	list_node_t *n2 = list_node_alloc(v);

	__ll_put_after(n->prev, n2);
	ll->length += 1;

	return n2;
}

list_node_t *list_add_after(list_t *ll, list_node_t *n, void *v)
{
	list_node_t *n2 = list_node_alloc(v);

	__ll_put_after(n, n2);
	ll->length += 1;

	return n2;
}

void list_delete(list_t *ll, list_node_t *n)
{
	if (n == NULL)
		return;

	assert(ll->length > 0);
	assert(n != ROOT(ll));

	n->next->prev = n->prev;
	n->prev->next = n->next;

	ll->length -= 1;

	list_node_free(n);
}

list_node_t *list_find(list_t *ll, void *p)
{
	list_node_t *cur;

	LIST_EACH_NODE(ll, cur) {
		if (cur->v == p)
			return cur;
	}

	return NULL;
}

void list_delete_val(list_t *ll, void *p)
{
	list_delete(ll, list_find(ll, p));
}

/*
 * vectors
 */

#define VEC_MIN_CAPACITY 8

vec_t *vec_new(unsigned capacity)
{
	vec_t *vec = malloc(sizeof(*vec));

	vec->size = 0;
	vec->capacity = capacity;

	if (vec->capacity < VEC_MIN_CAPACITY)
		vec->capacity = VEC_MIN_CAPACITY;

	vec->v = calloc(vec->capacity, sizeof(*vec->v));

	return vec;
}

void vec_release(vec_t *vec)
{
	free(vec->v);
	free(vec);
}

void vec_set(vec_t *vec, unsigned idx, void *v)
{
	if (idx >= vec->size) {
		unsigned cap = vec->capacity;

		while (idx >= cap)
			cap = cap + (cap >> 1);

		if (cap != vec->capacity) {
			vec->capacity = cap;
			vec->v = realloc(vec->v, cap * sizeof(*vec->v));
		}

		while (idx >= vec->size)
			vec->v[vec->size++] = NULL;
	}

	vec->v[idx] = v;
}

void vec_append(vec_t *vec, void *v)
{
	vec_set(vec, vec->size, v);
}

/*
 * hash tables
 */

#define MIN_ORDER  4 /* 2^4 = 16, kinda cramped but not too bad. */
#define MAX_ORDER 12 /* 2^12 = 4096, which i think is plenty big. */

struct htab_cell {
	char *k;
	hash_t hash;
	void *v;
	htab_cell_t *next;
};

htab_t *htab_new(unsigned initial_order)
{
	htab_t *ht = calloc(1, sizeof(*ht));

	if (initial_order == 0)
		initial_order = HTAB_DEFAULT_ORDER;
	if (initial_order < MIN_ORDER)
		initial_order = MIN_ORDER;
	if (initial_order > MAX_ORDER)
		initial_order = MAX_ORDER;

	ht->order = initial_order;
	ht->load = 0;

	ht->tab = calloc(1 << ht->order, sizeof(htab_cell_t*));

	return ht;
}

void htab_release(htab_t *ht)
{
	htab_cell_t *cur, *next;
	int i;

	for (i=0; i<1<<ht->order; i++) {
		cur = ht->tab[i];

		while (cur) {
			next = cur->next;
			free(cur->k);
			free(cur);
			cur = next;
		}
	}

	free(ht->tab);
	free(ht);
}

void htab_put(htab_t *ht, char *k, void *v)
{
	hash_t hash;
	htab_cell_t *cur;
	unsigned idx;

	hash = fnv1a(k, strlen(k));
	idx = hash & ((1 << ht->order) - 1);
	cur = ht->tab[idx];

	while (cur) {
		if (cur->hash == hash && !strcmp(cur->k, k)) {
			cur->v = v;
			return;
		}
		cur = cur->next;
	}

	ht->load++;

	/* TODO: resize if load above some threshold */

	cur = malloc(sizeof(*cur));
	cur->k = strdup(k);
	cur->v = v;
	cur->hash = hash;
	cur->next = ht->tab[idx];

	ht->tab[idx] = cur;
}

void *htab_get(htab_t *ht, char *k)
{
	hash_t hash;
	htab_cell_t *cur;
	unsigned idx;

	hash = fnv1a(k, strlen(k));
	idx = hash & ((1 << ht->order) - 1);
	cur = ht->tab[idx];

	while (cur) {
		if (cur->hash == hash && !strcmp(cur->k, k))
			return cur->v;

		cur = cur->next;
	}

	return NULL;
}

void *htab_delete(htab_t *ht, char *k)
{
	hash_t hash;
	htab_cell_t **cur;
	unsigned idx;

	/* this time, cur points to the pointer we followed to get to the
	   node being inspected so that when we eventually delete the node
	   we can adjust the pointer */

	hash = fnv1a(k, strlen(k));
	idx = hash & ((1 << ht->order) - 1);

	cur = &ht->tab[idx];

	while (*cur) {
		if ((*cur)->hash == hash && !strcmp((*cur)->k, k)) {
			void *nuked = (*cur)->v;
			htab_cell_t *next = (*cur)->next;

			free((*cur)->k);
			free(*cur);
			*cur = next;

			ht->load--;

			return nuked;
		}

		cur = &(*cur)->next;
	}

	return NULL;
}

void htab_each(htab_t *ht, void (*cb)(void*, char *k, void *v), void *priv)
{
	htab_cell_t *cur;
	unsigned idx;

	for (idx = 0; idx < 1 << ht->order; idx++) {
		for (cur = ht->tab[idx]; cur; cur = cur->next)
			cb(priv, cur->k, cur->v);
	}
}

/*
 * radix tries
 */

typedef struct trie_node trie_node_t;

struct trie_node {
	char *key;
	void *value;
	trie_node_t *down[16];
};

struct trie {
	trie_node_t *root;
	trie_canonize_t *canonize;
	size_t size;
};

static void __null_canonize(char *s) { }

trie_t *trie_new(trie_canonize_t *canonize)
{
	trie_t *tt = calloc(1, sizeof(*tt));
	tt->root = calloc(1, sizeof(*tt->root));
	tt->canonize = canonize ? canonize : __null_canonize;

	return tt;
}

static trie_node_t *trie_node_alloc(void)
{
	return calloc(1, sizeof(trie_node_t));
}

static void trie_release_node(trie_node_t *n)
{
	int i;

	if (n == NULL)
		return;
	if (n->key != NULL)
		free(n->key);
	for (i=0; i<16; i++)
		trie_release_node(n->down[i]);

	free(n);
}

void trie_release(trie_t *tt)
{
	trie_release_node(tt->root);
	free(tt);
}

void trie_put(trie_t *tt, char *k, void *v)
{
	char buf[strlen(k) + 1];
	int nib, i = 0;
	trie_node_t *n = tt->root;

	strcpy(buf, k);
	tt->canonize(buf);

	for (i=0; buf[i >> 1]; i++) {
		nib = nibble(buf, i);

		if (n->down[nib] == NULL)
			n->down[nib] = trie_node_alloc();

		n = n->down[nib];
	}

	if (n->key == NULL)
		n->key = strdup(k);

	n->value = v;
}

void *trie_get(trie_t *tt, char *k)
{
	char buf[strlen(k) + 1];
	int nib, i = 0;
	trie_node_t *n = tt->root;

	strcpy(buf, k);
	tt->canonize(buf);

	for (i=0; buf[i >> 1]; i++) {
		nib = nibble(buf, i);

		if (n->down[nib] == NULL)
			return NULL;

		n = n->down[nib];
	}

	return n->value;
}

static int __trie_num_children(trie_node_t *n)
{
	int i, cnt;

	if (n == NULL)
		return 0;

	cnt = 0;

	for (i=0; i<16; i++) {
		if (n->down[i] != NULL)
			cnt++;
	}

	if (n->key != NULL)
		cnt++;

	return cnt;
}

static void *__trie_delete(trie_node_t **np, int root, int i, char *s)
{
	trie_node_t *n = *np;
	int nib = nibble(s, i);
	void *v;

	if (n == NULL)
		return NULL;

	if (*s == '\0') {
		v = n->value;

		if (n->key != NULL)
			free(n->key);
		n->value = NULL;
		n->key = NULL;

	} else {
		v = __trie_delete(&n->down[nib], 0, i?0:1, s+i);
	}

	if (__trie_num_children(n) == 0 && !root) {
		*np = NULL;
		free(n);
	}

	return v;
}

void *trie_delete(trie_t *tt, char *k)
{
	char buf[strlen(k) + 1];

	strcpy(buf, k);
	tt->canonize(buf);

	return __trie_delete(&tt->root, 1, 0, buf);
}
