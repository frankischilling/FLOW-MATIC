#include "test_harness.h"
#include "internal.h"
#include "flowmatic/compiler.h"

int main(void)
{
    FmArena a;
    fm_arena_init(&a);
    char *bytes = NULL;
    size_t sz = 0;
    FmDiagList d;
    fm_diags_init(&d);
    TCHECK(fm_read_file("tests/fixtures/manual/sample1/inventory.dd", &bytes,
                        &sz, &d));
    FmSource src = {"inventory.dd", bytes, sz, 1};
    FmDataDesign dd;
    TCHECK(fm_parse_data_design_text(&a, &src, &d, &dd));
    TCHECK(fm_word_is_kw(&dd.file_name, "INVENTORY"));
    TCHECK(dd.file.multi_reel == 1);
    TCHECK(dd.item.item_size == 10);
    TCHECK(dd.item.nkeys == 1);
    TCHECK(dd.nfields == 2);
    TCHECK(dd.fields[0].type == FM_FTYPE_ALPHANUM);
    TCHECK(dd.fields[0].full_word == 1);
    TCHECK(dd.fields[1].type == FM_FTYPE_NUMERIC);
    TCHECK(dd.fields[1].word_loc == 1);
    TCHECK(dd.fields[1].left_pos == 7);
    TCHECK(dd.fields[1].length == 6);
    free(bytes);

    fm_diags_init(&d);
    TCHECK(fm_read_file("tests/fixtures/manual/sample1/price.dd", &bytes, &sz,
                        &d));
    src.bytes = bytes;
    src.size = sz;
    src.path = "price.dd";
    TCHECK(fm_parse_data_design_text(&a, &src, &d, &dd));
    TCHECK(dd.item.item_size == 2);
    TCHECK(dd.file.multi_reel == 0);
    TCHECK(dd.nfields == 2);
    TCHECK(dd.fields[1].dec_dir == FM_DEC_RIGHT);
    TCHECK(dd.fields[1].dec_n == 3);
    TCHECK(!dd.fields[1].full_word);
    free(bytes);

    /* overlapping packed field */
    const char *ov =
        "NAME OF FILE\nX\nFIELD DESIGN\n\nPRODUCT-NO\n000000000000\n"
        "000002///1C0\n000000000000\nTYPE-NUMBER\n000000000000\n"
        "000003///A30\n000000000111\nEND FILE DES\n";
    src.bytes = ov;
    src.size = strlen(ov);
    src.path = "ov.dd";
    fm_diags_init(&d);
    TCHECK(fm_parse_data_design_text(&a, &src, &d, &dd));
    TCHECK(dd.nfields == 2);
    TCHECK(dd.fields[1].left_pos == 10);
    TCHECK(dd.fields[1].length == 3);

    /* sub-item */
    const char *si =
        "NAME OF FILE\nX\nITEM DESIGN\n\nITEM SIZE\n000000000010\nNO OF KEYS\n"
        "000000000000\nKEY 1\n            \nADDRESS\n000000000005\nFIELD DESIGN\n\n"
        "END FILE DES\n";
    src.bytes = si;
    src.size = strlen(si);
    src.path = "si.dd";
    fm_diags_init(&d);
    TCHECK(fm_parse_data_design_text(&a, &src, &d, &dd));
    TCHECK(dd.item.nsubitems == 1);
    TCHECK(dd.item.subitems[0].start_word == 0);
    TCHECK(dd.item.subitems[0].end_word == 5);

    fm_diags_init(&d);
    TCHECK(fm_read_file("tests/fixtures/manual/sample2/directory.dir", &bytes,
                        &sz, &d));
    src.bytes = bytes;
    src.size = sz;
    src.path = "directory.dir";
    FmDirectory dir;
    TCHECK(fm_parse_directory_text(&a, &src, &d, &dir));
    TCHECK(dir.present);
    TCHECK(dir.w_high == 0);
    free(bytes);

    fm_diags_init(&d);
    TCHECK(fm_read_file("tests/fixtures/manual/sample3/directory.dir", &bytes,
                        &sz, &d));
    src.bytes = bytes;
    src.size = sz;
    TCHECK(fm_parse_directory_text(&a, &src, &d, &dir));
    TCHECK(dir.w_high == 1);
    free(bytes);

    fm_arena_free(&a);
    return t_report();
}
