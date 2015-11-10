#ifndef __INC_SYMTAB_H__
#define __INC_SYMTAB_H__

struct symtab;

extern struct symtab *symtab_new(struct symtab *up);
extern void symtab_set(struct symtab *tab, char *key, void *val);
extern void *symtab_get(struct symtab *tab, char *key);
extern void symtab_free(struct symtab *tab);

#endif
