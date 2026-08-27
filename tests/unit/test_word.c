#include "test_harness.h"
#include "flowmatic/word.h"

int main(void)
{
    FmWord w;
    bool overflow = false;
    TCHECK(fm_word_from_cstr(&w, "PRODUCT-NO", &overflow));
    TCHECK(!overflow);
    TCHECK(fm_word_used_len(&w) == 10);
    TCHECK(fm_word_eq_cstr(&w, "PRODUCT-NO"));
    TCHECK(w.pos[10] == ' ');
    TCHECK(w.pos[11] == ' ');
    overflow = false;
    TCHECK(!fm_word_from_cstr(&w, "THIRTEENCHARS", &overflow));
    TCHECK(overflow);
    TCHECK(fm_digit_pos_value('C') == 12);
    TCHECK(fm_digit_pos_value('A') == 10);
    TCHECK(fm_radix36_value('Z') == 35);
    TCHECK(fm_radix36_value('A') == 10);
    FmWord z;
    fm_word_zeros(&z);
    TCHECK(fm_word_is_zeros(&z));
    return t_report();
}
