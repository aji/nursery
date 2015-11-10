/* libmaddy -- mpz.h, arbitrary precision unsigned integer arithmetic */
/* This file is part of MaddySSH */

#ifndef __INC_MPZ_H__
#define __INC_MPZ_H__

#define MPZ_DIGITS     (128)

/* *NOT* USHORT_MAX. It's possible that USHORT_MAX is larger than 2^16,
   which is not what we want. What we *do* want is UINT_MAX that can
   fit two unsigned shorts */
#define MPZ_DIGIT_MAX  (65536U)
#define MPZ_DIGIT_BITS (16)

struct mpz {
	/* Digits are stored in little-endian order */
	unsigned short n[MPZ_DIGITS];
};

typedef struct mpz mpz_t;

/* some useful constants: 0, 1, 2, and -1 */
extern mpz_t *mpz_0, *mpz_1, *mpz_2, *mpz_m1;

extern void mpz_init(/* void */);

extern void mpz_set(/* mpz_t *r, *op */);
extern void mpz_set_ui(/* mpz_t *r, unsigned int op */);
extern int mpz_cmp(/* mpz_t *a, *b */); /* unsigned comparison!! */

extern void mpz_neg(/* mpz_t *r, *op */);
extern void mpz_add(/* mpz_t *r, *op1, *op2 */);
extern void mpz_mul(/* mpz_t *r, *op1, *op2 */);
extern void mpz_shl_ui(/* mpz_t *r, *op1, unsigned int op2 */);
extern void mpz_mod(/* mpz_t *r, *op1, *op2 */);

#endif
