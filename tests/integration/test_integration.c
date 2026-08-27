#include "test_harness.h"
#include "internal.h"
#include "flowmatic/compiler.h"
#include "flowmatic/runtime.h"

static int compile_ok(FmUnitPaths *p, FmCompileResult *r, FmDiagList *d)
{
    FmArena a;
    fm_arena_init(&a);
    FmCompileOptions o;
    memset(&o, 0, sizeof o);
    return fm_compile_unit(&a, p, &o, d, r);
}

int main(void)
{
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
    FmCompileResult r;
    FmDiagList d;
    fm_diags_init(&d);
    TCHECK(compile_ok(&p, &r, &d));
    if (d.error_count) {
        fm_diags_print(&d, stderr);
    }

    FmRun run;
    fm_bind_runtime(&r.program, &run);
    TCHECK(fm_rt_open_input(&run, 'A',
                            "tests/fixtures/runtime/sample1_inventory.txt"));
    TCHECK(fm_rt_open_input(&run, 'B',
                            "tests/fixtures/runtime/sample1_price.txt"));
    TCHECK(fm_rt_open_output(&run, 'C', "tests/fixtures/runtime/out_priced.txt"));
    TCHECK(fm_rt_open_output(&run, 'D',
                             "tests/fixtures/runtime/out_unpriced.txt"));
    int rc = fm_exec_program(&r.program, &run);
    TCHECK(rc == 0);
    fm_rt_close(&run);

    FILE *fc = fopen("tests/fixtures/runtime/out_priced.txt", "rb");
    TCHECK(fc != NULL);
    char line[256];
    int np = 0;
    int saw1 = 0, saw3 = 0;
    while (fc && fgets(line, sizeof line, fc)) {
        np++;
        if (strncmp(line, "P00000000001", 12) == 0) {
            saw1 = 1;
        }
        if (strncmp(line, "P00000000003", 12) == 0) {
            saw3 = 1;
        }
    }
    if (fc) {
        fclose(fc);
    }
    TCHECK(saw1);
    TCHECK(saw3);
    TCHECK(np == 2);

    FILE *fu = fopen("tests/fixtures/runtime/out_unpriced.txt", "rb");
    TCHECK(fu != NULL);
    int nu = 0, saw4 = 0;
    while (fu && fgets(line, sizeof line, fu)) {
        nu++;
        if (strncmp(line, "P00000000004", 12) == 0) {
            saw4 = 1;
        }
    }
    if (fu) {
        fclose(fu);
    }
    TCHECK(saw4);
    TCHECK(nu == 1);

    /* sample 2: W-storage + duplicate */
    const char *ds2[5] = {
        "tests/fixtures/manual/sample1/inventory.dd",
        "tests/fixtures/manual/sample1/price.dd",
        "tests/fixtures/manual/sample1/priced-inv.dd",
        "tests/fixtures/manual/sample1/unpriced-inv.dd",
        "tests/fixtures/manual/sample2/error.dd"};
    memset(&p, 0, sizeof p);
    p.designs = ds2;
    p.ndesigns = 5;
    p.w_storage = "tests/fixtures/manual/sample2/wstorage.dd";
    p.directory = "tests/fixtures/manual/sample2/directory.dir";
    p.code = "tests/fixtures/manual/sample2/code.fm";
    fm_diags_init(&d);
    TCHECK(compile_ok(&p, &r, &d));
    if (d.error_count) {
        fm_diags_print(&d, stderr);
    }
    fm_bind_runtime(&r.program, &run);
    TCHECK(fm_rt_open_input(&run, 'A',
                            "tests/fixtures/runtime/sample2_inventory.txt"));
    TCHECK(fm_rt_open_input(&run, 'B',
                            "tests/fixtures/runtime/sample1_price.txt"));
    TCHECK(fm_rt_open_output(&run, 'C', "tests/fixtures/runtime/out2_priced.txt"));
    TCHECK(fm_rt_open_output(&run, 'D',
                             "tests/fixtures/runtime/out2_unpriced.txt"));
    TCHECK(fm_rt_open_output(&run, 'E', "tests/fixtures/runtime/out2_error.txt"));
    rc = fm_exec_program(&r.program, &run);
    TCHECK(rc == 0);
    fm_rt_close(&run);
    FILE *fe = fopen("tests/fixtures/runtime/out2_error.txt", "rb");
    TCHECK(fe != NULL);
    int ne = 0;
    while (fe && fgets(line, sizeof line, fe)) {
        ne++;
    }
    if (fe) {
        fclose(fe);
    }
    TCHECK(ne >= 1);

    /* sample 3 without hooks must fail at exec */
    const char *ds3[4] = {
        "tests/fixtures/manual/sample1/inventory.dd",
        "tests/fixtures/manual/sample1/price.dd",
        "tests/fixtures/manual/sample1/priced-inv.dd",
        "tests/fixtures/manual/sample1/unpriced-inv.dd"};
    memset(&p, 0, sizeof p);
    p.designs = ds3;
    p.ndesigns = 4;
    p.w_storage = "tests/fixtures/manual/sample3/wstorage.dd";
    p.directory = "tests/fixtures/manual/sample3/directory.dir";
    p.code = "tests/fixtures/manual/sample3/code.fm";
    p.x1 = "tests/fixtures/manual/sample3/x1.sec";
    fm_diags_init(&d);
    TCHECK(compile_ok(&p, &r, &d));
    fm_bind_runtime(&r.program, &run);
    TCHECK(fm_rt_open_input(&run, 'A',
                            "tests/fixtures/runtime/sample1_inventory.txt"));
    TCHECK(fm_rt_open_input(&run, 'B',
                            "tests/fixtures/runtime/sample1_price.txt"));
    TCHECK(fm_rt_open_output(&run, 'C', "tests/fixtures/runtime/out3_c.txt"));
    TCHECK(fm_rt_open_output(&run, 'D', "tests/fixtures/runtime/out3_d.txt"));
    rc = fm_exec_program(&r.program, &run);
    TCHECK(rc == 2);
    fm_rt_close(&run);

    return t_report();
}
