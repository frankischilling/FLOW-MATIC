#include "test_harness.h"
#include "internal.h"
#include "flowmatic/compiler.h"

int main(void)
{
    FmArena a;
    fm_arena_init(&a);
    FmDiagList d;
    fm_diags_init(&d);
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
    FmCompileResult r;
    TCHECK(fm_compile_unit(&a, &p, &o, &d, &r));
    if (d.error_count) {
        fm_diags_print(&d, stderr);
    }
    TCHECK(r.op1 && strstr(r.op1, "OPERATIONS FILE 1"));
    TCHECK(r.op2 && strstr(r.op2, "OPERATIONS FILE 2"));
    TCHECK(r.op3 && strstr(r.op3, "OPERATIONS FILE 3"));
    TCHECK(r.library && strstr(r.library, "GENERATED LIBRARY"));
    TCHECK(r.edited && strstr(r.edited, "EDITED RECORD"));
    TCHECK(strstr(r.edited, "Directory excluded") ||
           strstr(r.edited, "Directory excluded") == NULL);
    TCHECK(strstr(r.edited, "PRODUCT-NO"));
    TCHECK(r.c_source && strstr(r.c_source, "Modern executable backend"));
    TCHECK(strstr(r.c_source, "run->jump[9]"));
    fm_compile_result_free(&r);

    /* sample 3 needs x1 hooks to emit C */
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
    memset(&o, 0, sizeof o);
    o.emit_c = 1;
    fm_diags_init(&d);
    TCHECK(!fm_compile_unit(&a, &p, &o, &d, &r));
    TCHECK(fm_diags_has_error(&d));
    o.x1_hooks = 1;
    fm_diags_init(&d);
    TCHECK(fm_compile_unit(&a, &p, &o, &d, &r));
    TCHECK(r.x1_requires_hooks);
    TCHECK(strstr(r.c_source, "x1_hook"));
    fm_compile_result_free(&r);
    fm_arena_free(&a);
    return t_report();
}
