/* markov.h -- fast UTF-8 markov chain library */
/* Copyright (C) 2015 Alex Iadicicco */

#ifndef __INC_MARKOV_H__
#define __INC_MARKOV_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct mk_chain mk_chain_t;

extern mk_chain_t *mk_chain_new(unsigned words_per_node, unsigned overlap);
extern void mk_chain_associate(mk_chain_t*, const char **snippet);
extern void mk_chain_generate(mk_chain_t*, bool (*f)(void*, const char*), void*);

#endif
