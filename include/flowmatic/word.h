#ifndef FLOWMATIC_WORD_H
#define FLOWMATIC_WORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { FM_WORD_WIDTH = 12 };
enum { FM_BLOCK_WORDS = 60 };
enum { FM_BLOCKETTE_WORDS = 10 };
enum { FM_MAX_OPERATIONS = 999 };
enum { FM_MAX_STMT_WORDS = 60 };
enum { FM_MAX_X1_CONSTANTS = 59 };

/* Canonical UNIVAC word: twelve character positions, not a C string. */
typedef struct FmWord {
    unsigned char pos[FM_WORD_WIDTH];
} FmWord;

typedef struct FmBlockette {
    FmWord words[FM_BLOCKETTE_WORDS];
} FmBlockette;

typedef struct FmBlock {
    FmWord words[FM_BLOCK_WORDS];
} FmBlock;

void fm_word_clear(FmWord *w);
void fm_word_spaces(FmWord *w);
void fm_word_zeros(FmWord *w);
void fm_word_fill(FmWord *w, unsigned char ch);
bool fm_word_from_text(FmWord *w, const char *text, size_t n, bool *overflow);
bool fm_word_from_cstr(FmWord *w, const char *text, bool *overflow);
void fm_word_to_cstr(const FmWord *w, char out[FM_WORD_WIDTH + 1]);
int fm_word_compare_bytes(const FmWord *a, const FmWord *b);
bool fm_word_equal(const FmWord *a, const FmWord *b);
bool fm_word_is_spaces(const FmWord *w);
bool fm_word_is_zeros(const FmWord *w);
bool fm_word_eq_cstr(const FmWord *w, const char *padded12);
size_t fm_word_used_len(const FmWord *w);

/* Digit position labels 1-9, A, B, C as used in Field Design. */
int fm_digit_pos_value(unsigned char ch); /* 1..12, or 0 if invalid */
unsigned char fm_digit_pos_char(int value_1_to_12);

/* n in decimal-point codes 1-9, A-Z where A=10 ... Z=35. */
int fm_radix36_value(unsigned char ch); /* 1..35, or 0 if invalid */

#endif
