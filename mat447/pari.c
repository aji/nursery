#include <pari/pari.h>
#include "test.h"

const char *test_name = "null";

int do_factor(GEN n, long prec)
{
	GEN M = factor(n);
	pari_printf("%Ps = %Ps * %Ps\n", n, gcoeff(M, 1, 1), gcoeff(M, 2, 1));
	return 0;
}
