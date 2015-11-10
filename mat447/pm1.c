#include <pari/pari.h>
#include "test.h"

const char *test_name = "Pollard's p-1";

static GEN a;
static GEN B, Bi;

void repile_globals(pari_sp ltop)
{
	gerepileall(ltop, 3, &a, &B, &Bi);
}

void B_next(void)
{
	Bi = gaddgs(Bi, 1);
	B = gmul(B, Bi);
}

int factorable(GEN n, long prec)
{
	GEN M;

	M = ggcd(gsubgs(gel(powgi(gmodulo(a, n), B), 2), 1), n);
	
	if (gcmp(gen_1, M) < 0 && gcmp(M, n) < 0) {
		pari_printf("%Ps, %Ps, %Ps\n", Bi, n, M);
		return 1;
	} else if (gcmp(M, n) == 0) {
		return 1;
	}

	return 0;
}

int pm1(GEN n, long prec)
{
	pari_sp ltop = avma;

	a = stoi(2);
	B = stoi(2);
	Bi = stoi(2);

	for (;;) {
		repile_globals(ltop);

		if (factorable(n, prec))
			return 0;

		B_next();
	}

}

int is_sqrt(GEN n, long prec)
{
	pari_sp ltop = avma;
	GEN p;
	int ret;

	p = gfloor(gsqrt(n, prec));
	p = gmul(p, p);

	ret = (gcmp(p, n) == 0);

	avma = ltop;

	return ret;
}

int do_factor(GEN n, long prec)
{
	pari_sp ltop;
	int ret;

	if (is_sqrt(n, prec))
		return 0;

	ltop = avma;
	ret = pm1(n, prec);
	avma = ltop;
	
	return ret;
}
