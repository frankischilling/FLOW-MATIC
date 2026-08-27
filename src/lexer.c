#include "internal.h"
#include "flowmatic/compiler.h"

static int fm_all_digits(const unsigned char *s, size_t n)
{
    if (n == 0) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
    }
    return 1;
}

static int fm_is_group(const unsigned char *s, size_t n)
{
    return n >= 3 && s[0] == '(' && s[n - 1] == ')';
}

bool fm_lex(FmArena *arena, const FmSource *src, FmDiagList *diags,
            FmLexResult *out)
{
    out->tokens = NULL;
    out->ntokens = 0;
    size_t cap = 64;
    FmToken *toks = (FmToken *)fm_arena_calloc(arena, cap, sizeof(FmToken));
    if (!toks) {
        return false;
    }
    size_t ntok = 0;
    const unsigned char *p = (const unsigned char *)src->bytes;
    size_t i = 0;
    unsigned line = 1;
    unsigned col = 1;
    while (i < src->size) {
        unsigned char ch = p[i];
        if (ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f') {
            i++;
            col++;
            continue;
        }
        if (ch == '\r') {
            i++;
            col = 1;
            if (i < src->size && p[i] == '\n') {
                i++;
            }
            line++;
            continue;
        }
        if (ch == '\n') {
            i++;
            line++;
            col = 1;
            continue;
        }
        /* Modern fixture comments. Not FLOW-MATIC syntax. */
        if (ch == '#') {
            while (i < src->size && p[i] != '\n' && p[i] != '\r') {
                i++;
            }
            continue;
        }
        size_t start = i;
        unsigned sl = line;
        unsigned sc = col;
        while (i < src->size) {
            unsigned char c = p[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
                c == '\f') {
                break;
            }
            i++;
            col++;
        }
        size_t n = i - start;
        if (ntok + 1u >= cap) {
            size_t ncap = cap * 2u;
            FmToken *nt =
                (FmToken *)fm_arena_calloc(arena, ncap, sizeof(FmToken));
            if (!nt) {
                return false;
            }
            memcpy(nt, toks, ntok * sizeof(FmToken));
            toks = nt;
            cap = ncap;
        }
        FmToken *t = &toks[ntok];
        memset(t, 0, sizeof(*t));
        fm_span_init(&t->span, src->path, start, i, sl, sc, line, col);
        bool overflow = false;
        fm_word_from_text(&t->lexeme, (const char *)p + start, n, &overflow);
        if (overflow) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "code word has %zu character positions; FLOW-MATIC allows "
                     "at most 12, and none of them may be a space",
                     n);
            fm_diag_add(diags, FM_SEV_ERROR, "FM1101", t->span, -1, msg,
                        "Shorten the name or split it with a hyphen only if it "
                        "must remain a single word of 12 positions or fewer.");
        }
        if (n == 1 && p[start] == '.') {
            t->kind = FM_TOK_PERIOD;
        } else if (n == 1 && p[start] == ',') {
            t->kind = FM_TOK_COMMA;
        } else if (n == 1 && p[start] == ';') {
            t->kind = FM_TOK_SEMI;
        } else if (fm_is_group(p + start, n)) {
            t->kind = FM_TOK_GROUP;
        } else if (fm_all_digits(p + start, n)) {
            t->kind = FM_TOK_NUMBER;
        } else {
            t->kind = FM_TOK_WORD;
        }
        ntok++;
    }
    if (ntok + 1u >= cap) {
        FmToken *nt =
            (FmToken *)fm_arena_calloc(arena, cap + 1u, sizeof(FmToken));
        if (!nt) {
            return false;
        }
        memcpy(nt, toks, ntok * sizeof(FmToken));
        toks = nt;
    }
    FmToken *eof = &toks[ntok];
    memset(eof, 0, sizeof(*eof));
    eof->kind = FM_TOK_EOF;
    fm_span_init(&eof->span, src->path, src->size, src->size, line, col, line,
                 col);
    ntok++;
    out->tokens = toks;
    out->ntokens = ntok;
    return !fm_diags_has_error(diags);
}
