#include "test_harness.h"
#include "internal.h"
#include "flowmatic/compiler.h"

static int has_code(const FmDiagList *d, const char *code)
{
    for (size_t i = 0; i < d->len; i++) {
        if (strcmp(d->items[i].code, code) == 0) {
            return 1;
        }
    }
    return 0;
}

static void parse_only(const char *s, FmDiagList *d)
{
    FmArena a;
    fm_arena_init(&a);
    FmSource src = {"neg.fm", s, strlen(s), 0};
    FmProgram p;
    memset(&p, 0, sizeof p);
    fm_parse_program(&a, &src, d, &p);
}

int main(void)
{
    FmDiagList d;
    fm_diags_init(&d);
    parse_only("(0) INPUT X FILE-A Y FILE-B ; OUTPUT O FILE-C P FILE-D\n"
               "(1) STOP . (END)\n",
               &d);
    TCHECK(has_code(&d, "FM2120")); /* missing period on INPUT */

    fm_diags_init(&d);
    parse_only("(0) INPUT THIRTEENCHARS FILE-A Y FILE-B ; OUTPUT O FILE-C P "
               "FILE-D .\n(1) STOP . (END)\n",
               &d);
    TCHECK(has_code(&d, "FM1101") || fm_diags_has_error(&d));

    fm_diags_init(&d);
    {
        char buf[4000];
        strcpy(buf, "(0) INPUT X FILE-A Y FILE-B ; OUTPUT O FILE-C P FILE-D .\n"
                    "(1) MOVE ");
        for (int i = 0; i < 40; i++) {
            strcat(buf, "F (A) TO G (C) ; ");
        }
        strcat(buf, "F (A) TO G (C) .\n(2) STOP . (END)\n");
        parse_only(buf, &d);
        TCHECK(has_code(&d, "FM2121"));
    }

    const char *dup =
        "(0) INPUT X FILE-A Y FILE-B ; OUTPUT O FILE-C P FILE-D .\n"
        "(1) JUMP TO OPERATION 2 .\n"
        "(1) JUMP TO OPERATION 2 .\n"
        "(2) READ-ITEM A ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(3) READ-ITEM B ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(4) STOP . (END)\n";
    FmArena a;
    fm_arena_init(&a);
    FmSource src = {"d.fm", dup, strlen(dup), 0};
    FmProgram p;
    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    fm_parse_program(&a, &src, &d, &p);
    fm_analyze(&a, &p, &d);
    TCHECK(has_code(&d, "FM4005"));

    const char *gap =
        "(0) INPUT X FILE-A Y FILE-B ; OUTPUT O FILE-C P FILE-D .\n"
        "(2) STOP . (END)\n";
    src.bytes = gap;
    src.size = strlen(gap);
    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    fm_parse_program(&a, &src, &d, &p);
    fm_analyze(&a, &p, &d);
    TCHECK(has_code(&d, "FM4004"));

    const char *noin =
        "(1) JUMP TO OPERATION 2 .\n(2) STOP . (END)\n";
    src.bytes = noin;
    src.size = strlen(noin);
    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    fm_parse_program(&a, &src, &d, &p);
    fm_analyze(&a, &p, &d);
    TCHECK(has_code(&d, "FM4002"));

    const char *nostop =
        "(0) INPUT X FILE-A Y FILE-B ; OUTPUT O FILE-C P FILE-D .\n"
        "(1) JUMP TO OPERATION 0 .\n";
    src.bytes = nostop;
    src.size = strlen(nostop);
    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    fm_parse_program(&a, &src, &d, &p);
    fm_analyze(&a, &p, &d);
    TCHECK(has_code(&d, "FM4003"));

    const char *undef =
        "(0) INPUT X FILE-A Y FILE-B ; OUTPUT O FILE-C P FILE-D .\n"
        "(1) JUMP TO OPERATION 9 .\n"
        "(2) READ-ITEM A ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(3) READ-ITEM B ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(4) STOP . (END)\n";
    src.bytes = undef;
    src.size = strlen(undef);
    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    fm_parse_program(&a, &src, &d, &p);
    fm_analyze(&a, &p, &d);
    TCHECK(has_code(&d, "FM1204"));

    /* Remaining required negatives use Sample Problem 1 Data Designs. */
    const char *ds1[4] = {
        "tests/fixtures/manual/sample1/inventory.dd",
        "tests/fixtures/manual/sample1/price.dd",
        "tests/fixtures/manual/sample1/priced-inv.dd",
        "tests/fixtures/manual/sample1/unpriced-inv.dd"};

    const char *unknown_field =
        "(0) INPUT INVENTORY FILE-A PRICE FILE-B ; OUTPUT PRICED-INV FILE-C "
        "UNPRICED-INV FILE-D ; HSP D .\n"
        "(1) COMPARE NOSUCH (A) WITH PRODUCT-NO (B) ; IF EQUAL GO TO OPERATION "
        "4 ; OTHERWISE GO TO OPERATION 4 .\n"
        "(2) READ-ITEM A ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(3) READ-ITEM B ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(4) STOP . (END)\n";
    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    {
        FmUnitPaths up;
        memset(&up, 0, sizeof up);
        up.designs = ds1;
        up.ndesigns = 4;
        TCHECK(fm_load_unit(&a, &up, &d, &p));
        FmSource cs = {"uf.fm", unknown_field, strlen(unknown_field), 0};
        fm_parse_program(&a, &cs, &d, &p);
        fm_analyze(&a, &p, &d);
        TCHECK(has_code(&d, "FM4106"));
    }

    const char *unknown_file =
        "(0) INPUT INVENTORY FILE-A PRICE FILE-B ; OUTPUT PRICED-INV FILE-C "
        "UNPRICED-INV FILE-D ; HSP D .\n"
        "(1) COMPARE PRODUCT-NO (A) WITH PRODUCT-NO (E) ; IF EQUAL GO TO "
        "OPERATION 4 ; OTHERWISE GO TO OPERATION 4 .\n"
        "(2) READ-ITEM A ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(3) READ-ITEM B ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(4) STOP . (END)\n";
    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    {
        FmUnitPaths up;
        memset(&up, 0, sizeof up);
        up.designs = ds1;
        up.ndesigns = 4;
        TCHECK(fm_load_unit(&a, &up, &d, &p));
        FmSource cs = {"ue.fm", unknown_file, strlen(unknown_file), 0};
        fm_parse_program(&a, &cs, &d, &p);
        fm_analyze(&a, &p, &d);
        TCHECK(has_code(&d, "FM4105"));
    }

    const char *xfer =
        "(0) INPUT INVENTORY FILE-A PRICE FILE-B ; OUTPUT PRICED-INV FILE-C "
        "UNPRICED-INV FILE-D ; HSP D .\n"
        "(1) TRANSFER A TO B .\n"
        "(2) READ-ITEM A ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(3) READ-ITEM B ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(4) STOP . (END)\n";
    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    {
        FmUnitPaths up;
        memset(&up, 0, sizeof up);
        up.designs = ds1;
        up.ndesigns = 4;
        TCHECK(fm_load_unit(&a, &up, &d, &p));
        FmSource cs = {"xf.fm", xfer, strlen(xfer), 0};
        fm_parse_program(&a, &cs, &d, &p);
        fm_analyze(&a, &p, &d);
        TCHECK(has_code(&d, "FM4041"));
    }

    const char *no_end =
        "(0) INPUT INVENTORY FILE-A PRICE FILE-B ; OUTPUT PRICED-INV FILE-C "
        "UNPRICED-INV FILE-D ; HSP D .\n"
        "(1) READ-ITEM A .\n"
        "(2) READ-ITEM B ; IF END OF DATA GO TO OPERATION 3 .\n"
        "(3) STOP . (END)\n";
    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    {
        FmUnitPaths up;
        memset(&up, 0, sizeof up);
        up.designs = ds1;
        up.ndesigns = 4;
        TCHECK(fm_load_unit(&a, &up, &d, &p));
        FmSource cs = {"ne.fm", no_end, strlen(no_end), 0};
        fm_parse_program(&a, &cs, &d, &p);
        fm_analyze(&a, &p, &d);
        TCHECK(has_code(&d, "FM4051"));
    }

    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    {
        const char *ds2w[5] = {
            "tests/fixtures/manual/sample1/inventory.dd",
            "tests/fixtures/manual/sample1/price.dd",
            "tests/fixtures/manual/sample1/priced-inv.dd",
            "tests/fixtures/manual/sample1/unpriced-inv.dd",
            "tests/fixtures/manual/sample2/error.dd"};
        FmUnitPaths up;
        memset(&up, 0, sizeof up);
        up.designs = ds2w;
        up.ndesigns = 5;
        up.w_storage = "tests/fixtures/manual/sample2/wstorage.dd";
        up.code = "tests/fixtures/manual/sample2/code.fm";
        TCHECK(fm_load_unit(&a, &up, &d, &p));
        fm_analyze(&a, &p, &d);
        TCHECK(has_code(&d, "FM4006"));
    }

    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    {
        const char *ds2[5] = {
            "tests/fixtures/manual/sample1/inventory.dd",
            "tests/fixtures/manual/sample1/price.dd",
            "tests/fixtures/manual/sample1/priced-inv.dd",
            "tests/fixtures/manual/sample1/unpriced-inv.dd",
            "tests/fixtures/manual/sample2/error.dd"};
        FmUnitPaths up;
        memset(&up, 0, sizeof up);
        up.designs = ds2;
        up.ndesigns = 5;
        up.directory = "tests/fixtures/manual/sample2/directory.dir";
        up.code = "tests/fixtures/manual/sample1/code.fm";
        TCHECK(fm_load_unit(&a, &up, &d, &p));
        fm_analyze(&a, &p, &d);
        TCHECK(has_code(&d, "FM4007"));
    }

    memset(&p, 0, sizeof p);
    fm_diags_init(&d);
    {
        FmUnitPaths up;
        memset(&up, 0, sizeof up);
        up.combined = "tests/fixtures/negative/directory-after-code.unit";
        fm_load_unit(&a, &up, &d, &p);
        TCHECK(has_code(&d, "FM3303"));
    }

    fm_arena_free(&a);
    return t_report();
}
