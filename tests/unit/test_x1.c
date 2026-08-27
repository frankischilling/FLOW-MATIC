#include "test_harness.h"
#include "internal.h"
#include "flowmatic/compiler.h"

int main(void)
{
    FmArena a;
    fm_arena_init(&a);
    FmDiagList d;
    fm_diags_init(&d);
    char *bytes = NULL;
    size_t sz = 0;
    TCHECK(fm_read_file("tests/fixtures/manual/appendix_c/fig44.x1", &bytes, &sz,
                        &d));
    FmSource src = {"fig44.x1", bytes, sz, 1};
    FmX1Section secs[8];
    size_t n = 0;
    TCHECK(fm_parse_x1_text(&a, &src, &d, secs, &n, 8));
    TCHECK(n == 1);
    TCHECK(secs[0].op_number == 12);
    TCHECK(secs[0].has_end);
    TCHECK(secs[0].const_count == 3);
    TCHECK(secs[0].code_const_count == 2);
    int saw_j = 0;
    for (size_t i = 0; i < secs[0].nlines; i++) {
        if (secs[0].lines[i].kind == FM_X1_J_OPLINE) {
            saw_j = 1;
        }
    }
    TCHECK(saw_j);
    free(bytes);

    fm_diags_init(&d);
    TCHECK(fm_read_file("tests/fixtures/manual/sample3/x1.sec", &bytes, &sz, &d));
    src.bytes = bytes;
    src.size = sz;
    src.path = "x1.sec";
    n = 0;
    TCHECK(fm_parse_x1_text(&a, &src, &d, secs, &n, 8));
    TCHECK(n == 2);
    TCHECK(secs[0].op_number == 11);
    TCHECK(secs[1].op_number == 15);
    free(bytes);

    /* missing section */
    const char *code =
        "(0) INPUT INVENTORY FILE-A PRICE FILE-B ; OUTPUT X FILE-C Y FILE-D .\n"
        "(1) X-1 ADD STUFF .\n"
        "(2) READ-ITEM A ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(3) READ-ITEM B ; IF END OF DATA GO TO OPERATION 4 .\n"
        "(4) CLOSE-OUT FILES C , D .\n"
        "(5) STOP . (END)\n";
    FmSource cs = {"x.fm", code, strlen(code), 0};
    FmProgram prog;
    memset(&prog, 0, sizeof prog);
    fm_diags_init(&d);
    fm_parse_program(&a, &cs, &d, &prog);
    /* attach dummy designs letters by skipping analyze file names... */
    fm_x1_validate(&prog, &d);
    int missing = 0;
    for (size_t i = 0; i < d.len; i++) {
        if (strcmp(d.items[i].code, "FM6202") == 0) {
            missing = 1;
        }
    }
    TCHECK(missing);

    /* bad letter */
    const char *bad =
        "X-1      001\nQ0Z001\nEND SUBROUTN\nEND SUBROUTN\nEND SUBROUTN\n"
        "END SUBROUTN\nEND SUBROUTN\nEND SUBROUTN\nEND SUBROUTN\n"
        "END SUBROUTN\nEND SUBROUTN\n";
    src.bytes = bad;
    src.size = strlen(bad);
    src.path = "bad.x1";
    n = 0;
    fm_diags_init(&d);
    fm_parse_x1_text(&a, &src, &d, secs, &n, 8);
    memset(&prog, 0, sizeof prog);
    if (!fm_program_reserve_x1(&a, &prog, n > 0 ? n : 1)) {
        return t_report();
    }
    prog.x1[0] = secs[0];
    prog.nx1 = n;
    fm_x1_validate(&prog, &d);
    int badaddr = 0;
    for (size_t i = 0; i < d.len; i++) {
        if (strcmp(d.items[i].code, "FM6203") == 0) {
            badaddr = 1;
        }
    }
    TCHECK(badaddr);

    fm_arena_free(&a);
    return t_report();
}
