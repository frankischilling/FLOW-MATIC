#include "internal.h"

void fm_word_clear(FmWord *w)
{
    memset(w->pos, 0, FM_WORD_WIDTH);
}

void fm_word_spaces(FmWord *w)
{
    memset(w->pos, ' ', FM_WORD_WIDTH);
}

void fm_word_zeros(FmWord *w)
{
    memset(w->pos, '0', FM_WORD_WIDTH);
}

void fm_word_fill(FmWord *w, unsigned char ch)
{
    memset(w->pos, ch, FM_WORD_WIDTH);
}

bool fm_word_from_text(FmWord *w, const char *text, size_t n, bool *overflow)
{
    fm_word_spaces(w);
    if (overflow) {
        *overflow = n > FM_WORD_WIDTH;
    }
    if (n > FM_WORD_WIDTH) {
        memcpy(w->pos, text, FM_WORD_WIDTH);
        return false;
    }
    memcpy(w->pos, text, n);
    return true;
}

bool fm_word_from_cstr(FmWord *w, const char *text, bool *overflow)
{
    return fm_word_from_text(w, text, strlen(text), overflow);
}

void fm_word_to_cstr(const FmWord *w, char out[FM_WORD_WIDTH + 1])
{
    memcpy(out, w->pos, FM_WORD_WIDTH);
    out[FM_WORD_WIDTH] = '\0';
}

int fm_word_compare_bytes(const FmWord *a, const FmWord *b)
{
    return memcmp(a->pos, b->pos, FM_WORD_WIDTH);
}

bool fm_word_equal(const FmWord *a, const FmWord *b)
{
    return fm_word_compare_bytes(a, b) == 0;
}

bool fm_word_is_spaces(const FmWord *w)
{
    for (int i = 0; i < FM_WORD_WIDTH; i++) {
        if (w->pos[i] != ' ') {
            return false;
        }
    }
    return true;
}

bool fm_word_is_zeros(const FmWord *w)
{
    for (int i = 0; i < FM_WORD_WIDTH; i++) {
        if (w->pos[i] != '0') {
            return false;
        }
    }
    return true;
}

bool fm_word_eq_cstr(const FmWord *w, const char *padded12)
{
    FmWord t;
    bool overflow = false;
    fm_word_from_cstr(&t, padded12, &overflow);
    if (overflow) {
        return false;
    }
    return fm_word_equal(w, &t);
}

size_t fm_word_used_len(const FmWord *w)
{
    size_t n = FM_WORD_WIDTH;
    while (n > 0 && w->pos[n - 1] == ' ') {
        n--;
    }
    return n;
}

int fm_digit_pos_value(unsigned char ch)
{
    if (ch >= '1' && ch <= '9') {
        return (int)(ch - '0');
    }
    if (ch == 'A' || ch == 'a') {
        return 10;
    }
    if (ch == 'B' || ch == 'b') {
        return 11;
    }
    if (ch == 'C' || ch == 'c') {
        return 12;
    }
    return 0;
}

unsigned char fm_digit_pos_char(int value_1_to_12)
{
    if (value_1_to_12 >= 1 && value_1_to_12 <= 9) {
        return (unsigned char)('0' + value_1_to_12);
    }
    if (value_1_to_12 == 10) {
        return 'A';
    }
    if (value_1_to_12 == 11) {
        return 'B';
    }
    if (value_1_to_12 == 12) {
        return 'C';
    }
    return '?';
}

int fm_radix36_value(unsigned char ch)
{
    if (ch >= '1' && ch <= '9') {
        return (int)(ch - '0');
    }
    if (ch >= 'A' && ch <= 'Z') {
        return 10 + (int)(ch - 'A');
    }
    if (ch >= 'a' && ch <= 'z') {
        return 10 + (int)(ch - 'a');
    }
    return 0;
}
