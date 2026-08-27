#include "test_harness.h"

int t_failed;
int t_passed;

void t_check(int cond, const char *file, int line, const char *msg)
{
    if (cond) {
        t_passed++;
    } else {
        t_failed++;
        fprintf(stderr, "FAIL %s:%d: %s\n", file, line, msg);
    }
}

int t_report(void)
{
    printf("%d passed, %d failed\n", t_passed, t_failed);
    return t_failed ? 1 : 0;
}
