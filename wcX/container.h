/* containers */

#ifndef __INC_CONTAINER_H__
#define __INC_CONTAINER_H__

typedef struct Vector Vector;

#define VEC_INITIAL_CAPACITY 5
#define VEC_EACH(V, CUR) \
	for (       (CUR)  =  (void*)((V)->base)            ; \
	     (void*)(CUR)  <  (void*)((V)->base + (V)->len)   \
	                           && (V)->base             ; \
	            (CUR)  =  (void*)((void**)(CUR) + 1)    )

struct Vector {
	unsigned    capacity;
	unsigned    len;

	void      **base;
};

extern void Vec_Push(Vector*, void*);

#endif
