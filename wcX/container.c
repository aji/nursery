/* containers.h */

#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "container.h"

void Vec_Push(Vector *v, void *elt) {
	if (v->len + 1 >= v->capacity) {
		if (v->base == NULL || v->capacity < VEC_INITIAL_CAPACITY) {
			v->capacity   = VEC_INITIAL_CAPACITY;
		} else {
			v->capacity  += (v->capacity >> 1);
		}

		v->base = realloc(v->base, v->capacity * sizeof(*v->base));
	}

	v->base[v->len++] = elt;
}
