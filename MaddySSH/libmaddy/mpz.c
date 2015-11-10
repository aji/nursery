/* libmaddy -- mpz.c, arbitrary precision unsigned integer arithmetic */
/* This file is part of MaddySSH */

#include <stdio.h>
#include <maddy/mpz.h>

static mpz_t __mpz_0 = { { 0 } };
static mpz_t __mpz_1 = { { 1 } };
static mpz_t __mpz_2 = { { 2 } };
static mpz_t __mpz_m1 = { { 1 } };

mpz_t *mpz_0 = &__mpz_0;
mpz_t *mpz_1 = &__mpz_1;
mpz_t *mpz_2 = &__mpz_2;
mpz_t *mpz_m1 = &__mpz_m1;

void mpz_init()
{
	mpz_neg(mpz_m1, mpz_m1);
}

static void dump(mpz_t *z)
{
	static int firstrun = 1;
	unsigned int i;
	if (!firstrun)
		putchar('\n');
	firstrun = 0;
	for (i=0; i<MPZ_DIGITS; i++) {
		printf("%04x", z->n[MPZ_DIGITS-i-1]);
		if (i%16 == 15)
			putchar('\n');
	}
}

static void add_digit(r, i, n) mpz_t *r; unsigned int i; unsigned short n;
{
	unsigned int sum;
	for (; n && i<MPZ_DIGITS; i++) {
		sum = r->n[i] + n;
		n = sum >> MPZ_DIGIT_BITS;
		r->n[i] = sum & (MPZ_DIGIT_MAX - 1);
	}
}

static void shift_up(r, op, amt) mpz_t *r, *op; int amt;
{
	int i;
	for (i=MPZ_DIGITS-1; i>=0; i--) {
		r->n[i] = 0;
		if (i-amt >= 0)
			r->n[i] = op->n[i-amt];
	}
}

void mpz_set(r, op) mpz_t *r, *op;
{
	unsigned int i;
	for (i=0; i<MPZ_DIGITS; i++)
		r->n[i] = op->n[i];
}

void mpz_set_ui(r, op) mpz_t *r; unsigned int op;
{
	unsigned int i;
	for (i=2; i<MPZ_DIGITS; i++)
		r->n[i] = 0;
	r->n[0] = op & (MPZ_DIGIT_MAX - 1);
	r->n[1] = op >> (MPZ_DIGIT_BITS);
}

int mpz_cmp(a, b) mpz_t *a, *b;
{
	int i;
	for (i=MPZ_DIGITS-1; i>=0; i--) {
		if (a->n[i] != b->n[i])
			return (a->n[i] < b->n[i]) ? -1 : 1;
	}
	return 0;
}

void mpz_neg(r, op) mpz_t *r, *op;
{
	unsigned int i;
	for (i=0; i<MPZ_DIGITS; i++)
		r->n[i] = ~(op->n[i]);
	add_digit(r, 0, 1);
}

void mpz_add(r, op1, op2) mpz_t *r, *op1, *op2;
{
	unsigned int i, sum, carry;
	carry = 0;
	for (i=0; i<MPZ_DIGITS; i++) {
		sum = op1->n[i] + op2->n[i] + carry;
		carry = sum >> MPZ_DIGIT_BITS;
		r->n[i] = sum & (MPZ_DIGIT_MAX - 1);
	}
}

void mpz_mul(r, op1_a, op2_a) mpz_t *r, *op1_a, *op2_a;
{
	unsigned int i, j, prod, carry;
	mpz_t _op1, _op2, *op1=&_op1, *op2=&_op2;

	mpz_set(op1, op1_a);
	mpz_set(op2, op2_a);
	mpz_set_ui(r, 0);

	for (i=0; i<MPZ_DIGITS; i++) {
		carry = 0;
		for (j=0; j<MPZ_DIGITS; j++) {
			prod = op1->n[i] * op2->n[j] + carry;
			carry = prod >> MPZ_DIGIT_BITS;
			add_digit(r, i+j, prod & (MPZ_DIGIT_MAX - 1));
		}
	}
}

void mpz_shl_ui(r, op1, op2) mpz_t *r, *op1; unsigned int op2;
{
	int i;
	unsigned int sh, mask, overflow, tmp;

	mpz_set(r, op1);
	sh = op2 % MPZ_DIGIT_BITS;

	if (op2 >= MPZ_DIGIT_BITS)
		shift_up(r, r, op2/MPZ_DIGIT_BITS);

	if (sh == 0)
		return;

	mask = ~((1 << (MPZ_DIGIT_BITS - sh)) - 1);
	overflow = 0;

	for (i=0; i<MPZ_DIGITS; i++) {
		tmp = (r->n[i] & mask) >> (MPZ_DIGIT_BITS - sh);
		r->n[i] = (r->n[i] << sh) | overflow;
		overflow = tmp;
	}
}

void mpz_mod(r, op1, mod) mpz_t *r, *op1, *mod;
{
	int i, n, cmp;
	mpz_t n_tmod, tmod;

	mpz_set(r, op1);

	/* computes modulus via repeated subtraction, first by mod<<2047,
	   then by mod<<2046, etc. down to mod<<0 */

	for (i=(MPZ_DIGITS*MPZ_DIGIT_BITS)-1; i>=0; i--) {
		mpz_shl_ui(&tmod, mod, i);

		if (!mpz_cmp(&tmod, mpz_0))
			continue;

		mpz_neg(&n_tmod, &tmod);

		while ((cmp=mpz_cmp(r, &tmod)) >= 0)
			mpz_add(r, r, &n_tmod);
	}
}
