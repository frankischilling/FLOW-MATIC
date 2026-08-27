#ifndef FLOWMATIC_TOKEN_H
#define FLOWMATIC_TOKEN_H

#include "flowmatic/source.h"
#include "flowmatic/word.h"

typedef enum FmTokKind {
    FM_TOK_EOF = 0,
    FM_TOK_WORD,
    FM_TOK_NUMBER,
    FM_TOK_GROUP, /* (A), (0), (END), (14) */
    FM_TOK_PERIOD,
    FM_TOK_COMMA,
    FM_TOK_SEMI
} FmTokKind;

typedef struct FmToken {
    FmTokKind kind;
    FmSpan span;
    FmWord lexeme; /* twelve positions; unused tail is spaces */
    size_t word_index; /* index in statement word count, 0-based */
} FmToken;

#endif
