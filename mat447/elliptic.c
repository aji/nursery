#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <pari/pari.h>
#include "test.h"

const char *test_name = "Elliptic Curve Factorization";

#define ROWS 4
#define COLS 4
#define NCURVES ROWS*COLS

/* A point is represented as a 3-element t_VEC. P[1] and P[2] are the x
   and y coords of the point. P[3] is either gen_0 or gen_1, indicating
   whether P is the point at infinity */

#define point_is_infty(P)    (gcmp(gel(P, 3), gen_1) == 0)
#define point_set_infty(P)   (gel(P, 3) = gen_1)

/* Factorizer state is represented with a t_VEC as follows:
     [ n, b, c, m, X ]
   n is the current modulus,
   b and c are the coefficients of the curve y^2 = x^3 + bx + c
   m is the current factorial on P
   X is a stored point to assist in calculating m!P */

#define curve_n(E) gel(E, 1)
#define curve_b(E) gel(E, 2)
#define curve_c(E) gel(E, 3)
#define curve_m(E) gel(E, 4)
#define curve_X(E) gel(E, 5)
#define curve_Y(E) gel(E, 6)

static GEN curves; /* t_VEC of all curves */
static pari_sp big_ltop;

/* Computes C = A + B. Return status indicates if addition was
   successful. If return status is false, C contains a non-trivial factor
   of E->n. The coords of A and B should be t_INTMODs */
bool point_add(GEN E, GEN A, GEN B, GEN *C)
{
	pari_sp ltop = avma;
	GEN m, p1, p2;

	GEN Ax, Ay, Bx, By;

	if (point_is_infty(B)) {
		*C = gcopy(A);
		return true;
	} else if (point_is_infty(A)) {
		*C = gcopy(B);
		return true;
	}

	Ax = gel(A,1); Ay = gel(A,2);
	Bx = gel(B,1); By = gel(B,2);

	/* this if statement sets p1 and p2 to the numerator and
	   denominator of the slope, such that m=p1/p2 */
	if (gcmp(gel(Ax,2), gel(Bx,2)) == 0) {
		if (gcmp(gel(Ay,2), gel(By,2)) != 0 || gcmp(gel(Ay,2), gen_0) == 0) {
			p1 = cgetg(4, t_VEC);
			point_set_infty(p1);
			return p1;
		}

		/* A == B, use (3Ax^2 + b) / 2Ay */

		p1 = gadd(gmulgs(gsqr(Ax), 3), curve_b(E));
		p2 = gmulgs(Ay, 2);
	} else {
		/* A != B, use (By - Ay) / (Bx - Ax) */
		p1 = gsub(By, Ay);
		p2 = gsub(Bx, Ax);
	}

	*C = ggcd(p2, curve_n(E));
	if (gcmp(gel(*C,2), gen_1) != 0) {
		/* p1 is not invertible, *C is a non-trivial
		   factor of n */
		*C = gerepileupto(ltop, *C);
		return false;
	}

	/* p1 is invertible, compute m = p1/p2 */
	m = gmul(p1, ginv(p2));

	/* Cx = m^2 - Ax - Bx */
	/* Cy = m(Ax - Cx) - Ay */
	*C = cgetg(4, t_VEC);
	gel(*C,1) = gsub(gsub(gsqr(m), Ax), Bx);
	gel(*C,2) = gsub(gmul(m, gsub(Ax, gel(*C,1))), Ay);
	gel(*C,3) = gen_0;

	*C = gerepileupto(ltop, *C);

	return true;
}

/* iterator pointer */
static bool (*iter)(void);
static union {
	long s;
	GEN g;
} iter_data;

bool iter_long(void)
{
	if (iter_data.s <= 0) {
		iter_data.s = 0;
		return false;
	}
	iter_data.s--;
	return true;
}

bool iter_gen(void)
{
	pari_sp ltop = avma;
	if (gcmp(iter_data.g, gen_0) <= 0) {
		gunclone(iter_data.g);
		return false;
	}
	gaffect(gsub(iter_data.g, gen_1), iter_data.g);
	avma = ltop;
	return true;
}

void iter_start(GEN E)
{
	pari_sp ltop = avma;
	if (gcmpgs(curve_m(E), LONG_MAX) < 0) {
		iter = iter_long;
		iter_data.s = gtolong(curve_m(E));
	} else {
		iter = iter_gen;
		iter_data.g = gclone(curve_m(E));
	}
	avma = ltop;
}

/* returns false when cannot continue adding P */
bool step(GEN E)
{
	pari_sp ltop;
	long i = 0;
	GEN Y;

	ltop = avma;
	Y = curve_Y(E);

	//for (iter_start(E); iter(); ) {
	for (i=0; i<10; i++) {
		if (!point_add(E, curve_X(E), Y, &Y)) {
			pari_printf("%Ps has factor %Ps\n", curve_n(E), Y);
			avma = ltop;
			return false;
		}

		Y = gerepileupto(ltop, gcopy(Y));
	}

	curve_Y(E) = Y;
	curve_m(E) = gadd(curve_m(E), gen_1);

	return true;
}

GEN curve_init(GEN n, int i)
{
	pari_sp ltop = avma;
	GEN E, b, Px, Py;

	E = cgetg(7, t_VEC);
	curve_X(E) = cgetg(4, t_VEC);
	curve_Y(E) = cgetg(4, t_VEC);

	gel(curve_Y(E), 1) = gen_0;
	gel(curve_Y(E), 2) = gen_0;
	gel(curve_Y(E), 3) = gen_1;

	/* Copy n */
	curve_n(E) = n;

	/* Choose P and b randomly */
	curve_b(E) = b = gmodulsg(rand(), n);
	gel(curve_X(E), 1) = Px = gmodulsg(rand(), n);
	gel(curve_X(E), 2) = Py = gmodulsg(rand(), n);
	gel(curve_X(E), 3) = gen_0;

	/* Calc c = (y^2 - x^3 - bx) % n */
	curve_c(E) = gsub(gsub(gsqr(Py), gpowgs(Px, 3)), gmul(b, Px));

	/* start with 1!P */
	curve_m(E) = gen_1;

	return gerepileupto(ltop, E);
}

int do_factor(GEN n, long prec)
{
	pari_sp ltop;
	GEN E;
	int i;

	big_ltop = avma;
	curves = cgetg(NCURVES+1, t_VEC);
	for (i=0; i<NCURVES; i++)
		gel(curves, i+1) = curve_init(n, i);

	ltop = avma;

	for (;;) {
		for (i=0; i<NCURVES; i++) {
			E = gcopy(gel(curves, i+1));

			if (!step(E))
				return 0;

			gel(curves, i+1) = gerepileupto(ltop, E);
		}
	}

	return 0;
}
