#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int t_failed;
extern int t_passed;

void t_check(int cond, const char *file, int line, const char *msg);
int t_report(void);

#define TCHECK(cond) t_check((cond) ? 1 : 0, __FILE__, __LINE__, #cond)

#endif
