#include "test_harness.h"
#include "flowmatic/runtime.h"

int main(void)
{
    unsigned char a[] = "12345";
    unsigned char b[] = "12345";
    TCHECK(fm_dec_compare_numeric(a, 5, 2, b, 5, 2) == 0);
    unsigned char c[] = "12346";
    TCHECK(fm_dec_compare_numeric(a, 5, 2, c, 5, 2) < 0);
    unsigned char d[] = "012345";
    /* 123.45 vs 0123.45 with scale 2 vs 2 and extra integer digit */
    TCHECK(fm_dec_compare_numeric(a, 5, 2, d, 6, 2) == 0);
    return t_report();
}
