#ifndef __INC_ALIAS_H__
#define __INC_ALIAS_H__

#include "lambda.h"

extern expr_t *alias_get(const char *name);
extern void alias_set(const char *name, expr_t *ex);

extern void aliases_save(const char *fname);
extern void aliases_load(const char *fname);

/* returns first non-NULL result */
extern void *aliases_each(void *(*cb)(const char*, expr_t*, void*), void*);

#endif
