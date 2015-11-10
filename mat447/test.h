/* test.h -- definitions required from each test */
/* Copyright (C) 2012 Alex Iadicicco */
/* Please see the README for licensing terms */

#ifndef __INC_TEST_H__
#define __INC_TEST_H__

#include <pari/pari.h>

/* Human-readable name of the test. */
extern const char *test_name;

/* Factors the given integer. If the factorization fails for some reason,
   this should return a negative value. 0 indicates success. */
extern int do_factor(GEN n, long prec);

#endif
