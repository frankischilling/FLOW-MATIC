#include "test_harness.h"
#include "internal.h"
#include "flowmatic/compiler.h"

static int compile_sample1(FmCompileResult *r, FmDiagList *d)
{
    FmArena a;
    fm_arena_init(&a);
    const char *ds[4] = {
        "tests/fixtures/manual/sample1/inventory.dd",
        "tests/fixtures/manual/sample1/price.dd",
        "tests/fixtures/manual/sample1/priced-inv.dd",
        "tests/fixtures/manual/sample1/unpriced-inv.dd"};
    FmUnitPaths p;
    memset(&p, 0, sizeof p);
    p.designs = ds;
    p.ndesigns = 4;
    p.code = "tests/fixtures/manual/sample1/code.fm";
    FmCompileOptions o;
    memset(&o, 0, sizeof o);
    o.check_only = 1;
    int ok = fm_compile_unit(&a, &p, &o, d, r);
    return ok;
}

int main(void)
{
    FmCompileResult r;
    FmDiagList d;
    fm_diags_init(&d);
    TCHECK(compile_sample1(&r, &d));
    if (d.error_count) {
        fm_diags_print(&d, stderr);
    }
    TCHECK(r.program.nops == 18);
    TCHECK(r.program.designs[0].letter == 'A');

    /* numbering gap */
    const char *gap =
        "(0) INPUT INVENTORY FILE-A PRICE FILE-B ; OUTPUT X FILE-C Y FILE-D .\n"
        "(2) STOP . (END)\n";
    FmArena a;
    fm_arena_init(&a);
    FmSource src = {"gap.fm", gap, strlen(gap), 0};
    FmProgram prog;
    memset(&prog, 0, sizeof prog);
    fm_diags_init(&d);
    fm_parse_program(&a, &src, &d, &prog);
    fm_analyze(&a, &prog, &d);
    TCHECK(fm_diags_has_error(&d));

    return t_report();
}
