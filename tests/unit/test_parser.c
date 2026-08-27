#include "test_harness.h"
#include "internal.h"
#include "flowmatic/compiler.h"

static int parse_file(const char *path, FmProgram *prog, FmDiagList *d)
{
    FmArena a;
    fm_arena_init(&a);
    char *bytes = NULL;
    size_t sz = 0;
    if (!fm_read_file(path, &bytes, &sz, d)) {
        fm_arena_free(&a);
        return 0;
    }
    char *copy = fm_arena_strndup(&a, bytes, sz);
    free(bytes);
    FmSource src = {path, copy, sz, 0};
    memset(prog, 0, sizeof(*prog));
    int ok = fm_parse_program(&a, &src, d, prog);
    /* leak arena on purpose? must keep tokens... tests exit process */
    (void)ok;
    /* Do not free arena: tokens point into it. Fine for short tests. */
    return !fm_diags_has_error(d);
}

int main(void)
{
    FmProgram prog;
    FmDiagList diags;
    fm_diags_init(&diags);
    TCHECK(parse_file("tests/fixtures/manual/sample1/code.fm", &prog, &diags));
    TCHECK(prog.nops == 18);
    TCHECK(prog.ops[0].kind == FM_OP_INPUT);
    TCHECK(prog.ops[0].u.input.ninputs == 2);
    TCHECK(prog.ops[0].u.input.noutputs == 2);
    TCHECK(prog.ops[0].u.input.nhsp == 1);
    TCHECK(prog.ops[1].kind == FM_OP_COMPARE);
    TCHECK(prog.ops[1].u.compare.nbranches == 3);
    TCHECK(prog.ops[2].kind == FM_OP_TRANSFER);
    TCHECK(prog.ops[6].kind == FM_OP_MOVE);
    TCHECK(prog.ops[8].kind == FM_OP_READ_ITEM);
    TCHECK(prog.ops[8].u.read_item.has_end);
    TCHECK(prog.ops[12].kind == FM_OP_SET);
    TCHECK(prog.ops[12].u.set.npairs == 1);
    TCHECK(prog.ops[14].kind == FM_OP_TEST);
    TCHECK(prog.ops[16].kind == FM_OP_CLOSE_OUT);
    TCHECK(prog.ops[16].u.close_out.nfiles == 2);
    TCHECK(prog.ops[17].kind == FM_OP_STOP);

    fm_diags_init(&diags);
    TCHECK(parse_file("tests/fixtures/manual/sample2/code.fm", &prog, &diags));
    TCHECK(prog.nops == 20);
    TCHECK(prog.ops[8].kind == FM_OP_MOVE);
    TCHECK(prog.ops[8].u.move.pairs[0].src.file_letter == 'A');
    TCHECK(prog.ops[8].u.move.pairs[0].dests[0].field.file_letter == 'W');

    fm_diags_init(&diags);
    TCHECK(parse_file("tests/fixtures/manual/sample3/code.fm", &prog, &diags));
    TCHECK(prog.nops == 28);
    TCHECK(prog.ops[11].kind == FM_OP_X1);
    TCHECK(prog.ops[15].kind == FM_OP_X1);
    TCHECK(prog.ops[23].kind == FM_OP_EXECUTE);
    TCHECK(prog.ops[23].u.execute.from_op == 13);
    TCHECK(prog.ops[23].u.execute.to_op == 17);
    TCHECK(prog.ops[8].u.move.npairs == 2);

    /* Appendix A option forms */
    const char *forms =
        "(0) INPUT INVENTORY FILE-A PRICE FILE-B ; OUTPUT OUT FILE-C .\n"
        "(1) COMPARE F (A) WITH G (B) ; IF EQUAL GO TO OPERATION 2 ; OTHERWISE "
        "GO TO OPERATION 3 .\n"
        "(2) COMPARE F (A) WITH G (B) ; IF GREATER GO TO OPERATION 3 ; "
        "OTHERWISE GO TO OPERATION 4 .\n"
        "(3) COMPARE F (A) WITH G (B) ; IF EQUAL GO TO OPERATION 4 ; IF GREATER "
        "GO TO OPERATION 5 ; OTHERWISE GO TO OPERATION 6 .\n"
        "(4) COMPARE F (A) WITH G (B) ; IF GREATER GO TO OPERATION 5 ; IF EQUAL "
        "GO TO OPERATION 6 ; OTHERWISE GO TO OPERATION 7 .\n"
        "(5) EXECUTE OPERATION 6 .\n"
        "(6) EXECUTE OPERATION 1 THROUGH OPERATION 2 .\n"
        "(7) JUMP TO OPERATION 8 .\n"
        "(8) MOVE F (A) TO G (C) , H (C) ; I (A) TO J (C) .\n"
        "(9) READ-ITEM A ; IF END OF DATA GO TO OPERATION 14 .\n"
        "(10) REWIND A , B .\n"
        "(11) SET OPERATION 7 TO GO TO OPERATION 12 , OPERATION 5 TO GO TO "
        "OPERATION 6 .\n"
        "(12) TEST F (A) AGAINST ABC ; IF GREATER GO TO OPERATION 13 ; IF EQUAL "
        "GO TO OPERATION 13 ; OTHERWISE GO TO OPERATION 13 .\n"
        "(13) TRANSFER A TO C .\n"
        "(14) WRITE-ITEM C .\n"
        "(15) CLOSE-OUT FILE C .\n"
        "(16) STOP . (END)\n";
    FmArena a;
    fm_arena_init(&a);
    FmSource src = {"forms.fm", forms, strlen(forms), 0};
    fm_diags_init(&diags);
    memset(&prog, 0, sizeof prog);
    fm_parse_program(&a, &src, &diags, &prog);
    TCHECK(prog.nops == 17);
    TCHECK(prog.ops[5].kind == FM_OP_EXECUTE);
    TCHECK(prog.ops[5].u.execute.to_op == -1);
    TCHECK(prog.ops[8].u.move.npairs == 2);
    TCHECK(prog.ops[8].u.move.pairs[0].ndests == 2);
    TCHECK(prog.ops[11].u.set.npairs == 2);
    TCHECK(prog.ops[12].kind == FM_OP_TEST);
    TCHECK(prog.ops[15].u.close_out.files_kw == 1);
    if (fm_diags_has_error(&diags)) {
        fm_diags_print(&diags, stderr);
    }
    TCHECK(!fm_diags_has_error(&diags));
    return t_report();
}
