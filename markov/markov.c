/* markov.c -- markov chains */
/* Copyright (C) 2015 Alex Iadicicco */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include "markov.h"

#include <stdio.h>

static uint32_t
hash_func(const char *str) {
	const uint8_t *s = (const uint8_t *)str;
	uint32_t hval = 0;

	while (*s) {
		hval *= 0x01000193;
		hval ^= (uint32_t)*s++;
	}

	return hval;
}

/* MARKOV CHAIN DATABASE
   ========================================================================== */

#define DB_MAGIC 0x6d6b4442  /* "mkDB" */
#define DB_VERSION 1

#define DB_BLOCK 1024

#define DB_ALIGN_UP(N) (((N) | (DB_BLOCK - 1)) + 1)
#define DB_ALIGN_DOWN(N) ((N) & ~(DB_BLOCK - 1))

/* The database is a soup of 1 kbyte blocks. There are only 3 kinds of block:
   the super block, B+-tree internal nodes, and data blocks.

   Integers are encoded as little endian fixed size values.

   The super block is a rigidly defined block containing metadata about the
   database itself. It consists of, in order, 4 magic number bytes, a 2 byte
   integer for the version, 2 padding bytes, a 2 byte integer for words per
   edge, a 2 byte integer for the overlap, and an 8 byte integer for the offset
   to the next free data portion.

   B+-tree internal nodes consist of, in order, 63 keys, 64 children, and a
   reference to the next node. These are all encoded as 8 byte integers. A
   value of 0 indicates the field is unused.

   Data blocks are blobs of small and arbitrarily sized data. Database users
   can request up to 1 kbyte of this space. It's used for storing the
   individual bytes for words, the database keys themselves, and various
   counters.

   Words are encoded as C strings, including the null byte. Keys are encoded as
   a sequence of 8 byte references to the words they contain. B+-tree leaf
   nodes are encoded as 8 bytes of 0 and a 4 byte value containing the count
   for the edge or node.

   The B+-tree is used as an index for the counts. Two kinds of key are used.
   The first is a sequence of words_per_edge words, which selects an edge. The
   second is a sequence of (words_per_edge - overlap) / 2 words, which selects
   a node. Keys are ordered using a lexicographic ordering on the words to allow
   efficient iteration over all edges for a word or node. */

typedef uint64_t db_ref;

/* the main database object */
typedef struct DB {
	/* file descriptor for the database, and size. new blocks will be
	   allocated from the size as necessary. */
	int f;
	uint64_t size;

	/* superblock fields: */

	/* magic number and database version */
	uint32_t magic;
	uint16_t version;

	/* words per edge and overlap. words per node can be recovered with
	   (words_per_edge + overlap) / 2. this means that the overlap should
	   have the same parity as words per edge */
	uint16_t words_per_edge;
	uint16_t overlap;

	uint64_t misc_next;
} DB;

/* a single node of the database B tree */
typedef struct db_t_node {
	db_ref keys[63];
	db_ref children[64];
	db_ref next;
} db_t_node;

/* a key in the database. this is always going to be a list of words */
typedef db_ref *db_t_key;

/* a leaf. currently just the weight of the edge or node */
typedef struct db_t_leaf {
	uint64_t count;
} db_t_leaf;

static uint64_t db_parse_64(uint8_t *r) {
	return (r[0] <<  0) | (r[1] <<  8) | (r[2] << 16) | (r[3] << 24) |
	       (r[4] << 32) | (r[5] << 40) | (r[6] << 48) | (r[7] << 56);
}

static uint32_t db_parse_32(uint8_t *r) {
	return (r[0] <<  0) | (r[1] <<  8) | (r[2] << 16) | (r[3] << 24);
}

static uint16_t db_parse_16(uint8_t *r) {
	return (r[0] <<  0) | (r[1] <<  8);
}

static db_ref db_parse_ref(uint8_t *r) {
	return db_parse_64(r);
}

static void db_format_64(uint64_t n, uint8_t *r) {
	r[0] = (n >>  0); r[1] = (n >>  8); r[2] = (n >> 16); r[3] = (n >> 24);
	r[4] = (n >> 32); r[5] = (n >> 40); r[6] = (n >> 48); r[7] = (n >> 56);
}

static void db_format_32(uint32_t n, uint8_t *r) {
	r[0] = (n >>  0); r[1] = (n >>  8); r[2] = (n >> 16); r[3] = (n >> 24);
}

static void db_format_16(uint32_t n, uint8_t *r) {
	r[0] = (n >>  0); r[1] = (n >>  8);
}

static void db_format_ref(db_ref n, uint8_t *r) {
	db_format_64(n, r);
}

static ssize_t db_load(DB *db, db_ref wr, uint8_t *buf) {
	off_t at = wr;
	off_t end = DB_ALIGN_UP(wr);
	ssize_t sz;

	while (at < end) {
		sz = pread(db->f, buf + (at - wr), end - at, at);
		if (sz <= 0)
			return -1;
		at += sz;
	}

	return at - wr;
}

static ssize_t db_store(DB *db, db_ref wr, uint8_t *buf, size_t size) {
	off_t at = wr;
	off_t end = wr + size;
	ssize_t sz;

	while (at < end) {
		sz = pwrite(db->f, buf + (at - wr), end - at, at);
		if (sz <= 0)
			return -1;
		at += sz;
	}

	return at - wr;
}

static db_ref db_new_block(DB *db) {
	uint8_t zero[DB_BLOCK];
	memset(zero, 0, DB_BLOCK);

	db_store(db, db->size, zero, DB_BLOCK);
	db->size += DB_BLOCK;

	return db->size - DB_BLOCK;
}

static db_ref db_store_data(DB *db, uint8_t *buf, size_t size) {
	db_ref at;

	if (size > DB_BLOCK)
		return 0;

	if (DB_ALIGN_DOWN(db->misc_next) != DB_ALIGN_DOWN(db->misc_next + size))
		db->misc_next = db_new_block(db);

	at = db->misc_next;
	db_store(db, at, buf, size);

	return at;
}

static int db_load_node(DB *db, db_ref r, db_t_node *n) {
	uint8_t block[DB_BLOCK];
	int i;
	ptrdiff_t at = 0;

	if (db_load(db, r, block) != DB_BLOCK)
		return -1;

	for (i=0; i<63; i++, at+=8)
		n->keys[i] = db_parse_ref(block + at);

	for (i=0; i<64; i++, at+=8)
		n->children[i] = db_parse_ref(block + at);

	n->next = db_parse_ref(block + at);

	return 0;
}

static int db_load_key_n(DB *db, db_ref r, db_t_key k, int n) {
	uint8_t block[DB_BLOCK];
	int i;
	ptrdiff_t at = 0;

	if (n > db->words_per_edge || n < 0)
		n = db->words_per_edge;

	if (db_load(db, r, block) < n * 8)
		return -1;

	for (i=0; i<n; i++, at+=8)
		k[i] = db_parse_ref(block + at);

	return 0;
}

static int db_load_key(DB *db, db_ref r, db_t_key k) {
	return db_load_key_n(db, r, k, db->words_per_edge);
}

static int db_key_cmp_n(DB *db, db_t_key k1, db_t_key k2, int n) {
	int i;

	if (n > db->words_per_edge || n < 0)
		n = db->words_per_edge;

	for (i=0; i<n; i++) {
		if (k1[i] < k2[i])
			return -1;
		if (k1[i] > k2[i])
			return 1;
	}

	return 0;
}

static int db_key_cmp(DB *db, db_t_key k1, db_t_key k2) {
	return db_key_cmp_n(db, k1, k2, db->words_per_edge);
}

static db_ref db_search_n(DB *db, db_t_key k, int n) {
	db_t_node node;

	if (n > db->words_per_edge || n < 0)
		n = db->words_per_edge;

	db_ref keys[63][n];
	db_ref next_node;
	int i, nkeys;

	next_node = DB_BLOCK; /* root is always block 1 */

	/* I can justify using a goto because a) there is only 1 label, b) one
	   goto happens in a loop, and the flow to do that in an infinite loop,
	   the "right" way, would be more confusing, and c) we get one less
	   level of indentation for free this way. */
search_in_node:
	if ((next_node % DB_BLOCK) != 0) {
		/* nodes are always on a block boundary, so if the next node
		   is not on a block boundary, it must be a leaf! */
		return next_node;
	}

	db_load_node(db, next_node, &node);
	if (node.keys[0] == 0) {
		/* if a leaf happens to occur on a block boundary, it always
		   starts with 8 bytes of 0, which will become node.keys[0].
		   however, this could be the root node, so we ignore that case
		   */
		if (next_node != DB_BLOCK)
			return next_node;
	}

	/* load and count all the keys. we lose the speed advantage of
	   a B-tree for this by not having all the keys in the block,
	   but this can be sped up with a cache. */
	for (nkeys=0; nkeys<63; nkeys++) {
		if (node.keys[nkeys] == 0)
			break;
		db_load_key_n(db, node.keys[nkeys], keys[nkeys], n);
	}

	if (nkeys == 0) /* only possible at the root */
		return 0;

	/* if the search key is before the first key, then we go to the first
	   child */
	if (db_key_cmp_n(db, k, keys[0], n) < 0) {
		next_node = node.children[0];
		goto search_in_node;
	}

	/* then we search the remaining keys checking if the search key is
	   between any of them, descending to the corresponding child */
	for (i=1; i<nkeys; i++) {
		if (db_key_cmp_n(db, k, keys[i-1], n) >= 0 &&
		    db_key_cmp_n(db, k, keys[i  ], n) <= 0) {
			next_node = node.children[i];
			goto search_in_node;
		}
	}

	/* otherwise, we verify that the search key is after the last key,
	   and descend to the last child if so. otherwise, the database is
	   inconsistent! */
	if (db_key_cmp_n(db, k, keys[nkeys-1], n) >= 0) {
		next_node = node.children[nkeys];
		goto search_in_node;
	} else {
		/* this is an error condition! */
		return 0;
	}

	return 0;
}

static db_ref db_search(DB *db, db_t_key k) {
	return db_search_n(db, k, db->words_per_edge);
}

static int db_insert_n(DB *db, db_t_key k, db_ref ref, int n) {
	db_t_node node;

	if (n > db->words_per_edge || n < 0)
		n = db->words_per_edge;

	db_ref keys[63][n];
	db_ref next_node;
	int i, nkeys;

	next_node = DB_BLOCK; /* root is always block 1 */

	/* I can justify using a goto because a) there is only 1 label, b) one
	   goto happens in a loop, and the flow to do that in an infinite loop,
	   the "right" way, would be more confusing, and c) we get one less
	   level of indentation for free this way. */
search_in_node:
	if ((next_node % DB_BLOCK) != 0) {
		/* nodes are always on a block boundary, so if the next node
		   is not on a block boundary, it must be a leaf! */
		return next_node;
	}

	db_load_node(db, next_node, &node);
	if (node.keys[0] == 0) {
		/* if a leaf happens to occur on a block boundary, it always
		   starts with 8 bytes of 0, which will become node.keys[0].
		   however, this could be the root node, so we ignore that case
		   */
		if (next_node != DB_BLOCK)
			return next_node;
	}

	/* load and count all the keys. we lose the speed advantage of
	   a B-tree for this by not having all the keys in the block,
	   but this can be sped up with a cache. */
	for (nkeys=0; nkeys<63; nkeys++) {
		if (node.keys[nkeys] == 0)
			break;
		db_load_key_n(db, node.keys[nkeys], keys[nkeys], n);
	}

	if (nkeys == 0) /* only possible at the root */
		return 0;

	/* if the search key is before the first key, then we go to the first
	   child */
	if (db_key_cmp_n(db, k, keys[0], n) < 0) {
		next_node = node.children[0];
		goto search_in_node;
	}

	/* then we search the remaining keys checking if the search key is
	   between any of them, descending to the corresponding child */
	for (i=1; i<nkeys; i++) {
		if (db_key_cmp_n(db, k, keys[i-1], n) >= 0 &&
		    db_key_cmp_n(db, k, keys[i  ], n) <= 0) {
			next_node = node.children[i];
			goto search_in_node;
		}
	}

	/* otherwise, we verify that the search key is after the last key,
	   and descend to the last child if so. otherwise, the database is
	   inconsistent! */
	if (db_key_cmp_n(db, k, keys[nkeys-1], n) >= 0) {
		next_node = node.children[nkeys];
		goto search_in_node;
	} else {
		/* this is an error condition! */
		return 0;
	}

	return 0;
}

static int db_insert(DB *db, db_t_key k, db_ref ref) {
	return db_insert_n(db, k, ref, db->words_per_edge);
}

/* MARKOV CHAINERY (public API)
   ========================================================================== */

mk_chain_t *
mk_chain_new(unsigned words_per_node, unsigned overlap) {
	return NULL;
}

void
mk_chain_associate(mk_chain_t *mk, const char **snippet) {
}

void
mk_chain_generate(mk_chain_t *mk, bool (*f)(void*, const char*), void *user) {
}
