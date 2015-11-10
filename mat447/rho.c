#include <pari/pari.h>
#include "test.h"

const char *test_name = "Pollard's Rho";

long more;

GEN f(GEN n)
{
	return gaddgs(gsqr(n), more);
}

int do_factor(GEN n, long prec)
{
	GEN x, y, d;
	pari_sp ltop;

	more = 1;

	ltop = avma;

	x = gmodulsg(2, n);
	y = gmodulsg(2, n);

	for (;;) {
		x = f(x);
		y = f(f(y));

		d = gel(gsub(x, y), 2);
		d = ggcd(gabs(d, prec), n);

		if (gcmp(d, gen_1) != 0) {
			if (gcmp(d, n) == 0) {
				printf("x^2 + %d failed\n", more);
				more++; /* cruddy way to pick a new polynomial... */
			} else {
				x = gdiv(n, d);
				pari_printf("%Ps = %Ps * %Ps\n", n, d, x);
				return 0;
			}
		}

		gerepileall(ltop, 2, &x, &y);
	}
}
