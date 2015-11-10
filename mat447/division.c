#include <pari/pari.h>
#include "test.h"

const char *test_name = "Division Algorithm";

int do_factor(GEN n, long prec)
{
	pari_sp ltop;
	GEN sq = gfloor(gsqrt(n, prec));
	GEN q = stoi(2);

	ltop = avma;
	for (;;) {
		if (cmpii(q, sq) > 0)
			return -1;

		if (equalsi(0, gmod(n, q))) {
			pari_printf("%Ps = %Ps * %Ps\n", n, q, gdiv(n, q));
			return 0;
		}
		gaddz(gen_1, q, q);
		avma = ltop;
	}
}
