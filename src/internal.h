#ifndef FM_INTERNAL_H
#define FM_INTERNAL_H

#include "flowmatic/word.h"
#include "flowmatic/source.h"
#include "flowmatic/diagnostic.h"
#include "flowmatic/token.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FM_VERSION_STRING "0.1.0-documented-1958-subset"

enum { FM_ARENA_CHUNK = 65536 };
enum { FM_MAX_FILES = 9 }; /* letters A-I, printed page 106 */
enum { FM_MAX_SERVOS = 10 };
enum { FM_MAX_EXECUTE_DEPTH = 32 };
enum { FM_MAX_MOVE_PAIRS = 32 };
enum { FM_MAX_TEST_VALUES = 16 };
enum { FM_MAX_COMPARE_BRANCHES = 4 };
enum { FM_MAX_SET_PAIRS = 16 };
enum { FM_MAX_FIELDS = 128 };
enum { FM_MAX_SUBITEMS = 32 };
enum { FM_MAX_KEYS = 9 };
enum { FM_MAX_X1_LINES = 512 };
enum { FM_MAX_CONSTANTS_POOL = 256 };

typedef struct FmArenaChunk {
    struct FmArenaChunk *next;
    size_t cap;
    size_t used;
    unsigned char data[1];
} FmArenaChunk;

typedef struct FmArena {
    FmArenaChunk *head;
    int oom;
} FmArena;

void fm_arena_init(FmArena *a);
void fm_arena_free(FmArena *a);
void *fm_arena_alloc(FmArena *a, size_t bytes, size_t align);
void *fm_arena_calloc(FmArena *a, size_t n, size_t sz);
char *fm_arena_strndup(FmArena *a, const char *s, size_t n);
char *fm_arena_sprintf(FmArena *a, const char *fmt, ...);

typedef struct FmBuf {
    char *data;
    size_t len;
    size_t cap;
    int oom;
} FmBuf;

void fm_buf_init(FmBuf *b);
void fm_buf_free(FmBuf *b);
bool fm_buf_reserve(FmBuf *b, size_t extra);
bool fm_buf_append(FmBuf *b, const char *s, size_t n);
bool fm_buf_puts(FmBuf *b, const char *s);
bool fm_buf_printf(FmBuf *b, const char *fmt, ...);

bool fm_read_file(const char *path, char **out_bytes, size_t *out_size,
                  FmDiagList *diags);

unsigned char fm_ascii_ignore(void); /* '/' as ASCII stand-in for Unityper ignore */
bool fm_is_ignore_char(unsigned char ch);
bool fm_is_space_char(unsigned char ch);

int fm_parse_uint(const char *s, size_t n, unsigned *out);
int fm_parse_uint_word_right(const FmWord *w, unsigned width, unsigned *out);

/* Keyword compare against a space-padded word. */
bool fm_word_is_kw(const FmWord *w, const char *kw);
bool fm_cstr_eq(const char *a, const char *b);

const char *fm_tok_kind_name(FmTokKind k);

#endif
