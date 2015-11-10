/*
 * CSE 440, Project 2
 * Mini Object Pascal Value Numbering Optimizer
 *
 * Alex Iadicicco
 * shmibs
 */

#include "type.h"

type_set_t *type_set_new(void)
{
	type_set_t *s;

	s = calloc(1, sizeof(*s));

	s->types = vec_new(8);
	s->equiv = vec_new(8);
	
	return s;
}

#define P_FALSE ((void*) false)
#define P_TRUE  ((void*) true)

static bool* equiv_table_p(type_set_t *ts, type_t *t1, type_t *t2)
{
	if (t1->tabid >= ts->types->size || t2->tabid >= ts->types->size)
		return NULL;

	vec_t *v = (vec_t*) vec_get(ts->equiv, t1->tabid);
	return (bool*) &v->v[t2->tabid];
}

static bool equiv_table(type_set_t *ts, type_t *t1, type_t *t2)
{
	return *equiv_table_p(ts, t1, t2);
}

void type_set_add(type_set_t *ts, type_t *t)
{
	int i;
	vec_t *tt;

	if (t->tag != T_CLASS)
		abort();

	t->tabid = ts->types->size;
	vec_append(ts->types, t);

	tt = vec_new(ts->types->capacity);
	for (i=0; i<ts->equiv->size; i++)
		vec_append(tt, P_TRUE);
	vec_append(ts->equiv, tt);
	for (i=0; i<ts->equiv->size; i++)
		vec_append((vec_t*) vec_get(ts->equiv, i), P_TRUE);
}

/* the "table" argument short-circuits T_CLASS comparison to use the table */
static bool test_equiv(type_set_t *ts, type_t *t1, type_t *t2, bool table)
{
	list_node_t *n1, *n2;

again:
	if (!t1 || !t2 || t1->tag != t2->tag)
		return false;

	switch(t1->tag) {
	case T_ARRAY:
		if(t1->arr.high - t1->arr.low != t2->arr.high - t2->arr.low)
			return false;

		return test_equiv(ts, t1->arr.of, t2->arr.of, false);
		
	case T_CLASS:
		if (table)
			return equiv_table(ts, t1, t2);

		/* first check parent classes */
		if (t1->cls->extends && t2->cls->extends) {
			if (!equiv_table(ts, t1->cls->extends, t2->cls->extends)) {
				t2 = t2->cls->extends;
				goto again;
			}
		}

		/* then check fields of current classes */
		n1 = list_head(t1->cls->fields);
		n2 = list_head(t2->cls->fields);

		while ( /* both nodes are not the end */
			!list_is_nil(t1->cls->fields, n1) &&
			!list_is_nil(t2->cls->fields, n2)
		) {
			symbol_t *s1 = n1->v;
			symbol_t *s2 = n2->v;

			if (s1->tag != SYM_FIELD || s2->tag != SYM_FIELD)
				abort();

			if (!test_equiv(ts, s1->t, s2->t, true)) {
				t2 = t2->cls->extends;
				goto again;
			}

			n1 = n1->next;
			n2 = n2->next;
		}

		if ( /* either node is not the end */
			!list_is_nil(t1->cls->fields, n1) ||
			!list_is_nil(t2->cls->fields, n2)
		) {
			t2 = t2->cls->extends;
			goto again;
		}
		
		return true;
		
	default:
		return true;
	}
}

static bool update_pair(type_set_t *ts, type_t *t1, type_t *t2)
{
	bool was  = test_equiv(ts, t1, t2, true);
	bool is   = test_equiv(ts, t1, t2, false);

	*equiv_table_p(ts, t1, t2) = is ? P_TRUE : P_FALSE;

	return was != is;
}

bool type_set_equiv(type_set_t *ts, type_t *t1, type_t *t2)
{
	return test_equiv(ts, t1, t2, true);
}

void type_set_update_equiv(type_set_t *s)
{
	int i, j;
	bool changed = true;
	
	/* equivalence checking here */
	while (changed) {
		changed = false;

		for(i=0; i < s->types->size; i++) {
			for(j=0; j < s->types->size; j++) {
				changed = update_pair(s,
						vec_get(s->types, i),
						vec_get(s->types, j)
					) || changed;
			}
		}
	}
}
