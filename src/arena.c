#include "internal.h"

void fm_arena_init(FmArena *a)
{
    a->head = NULL;
    a->oom = 0;
}

void fm_arena_free(FmArena *a)
{
    FmArenaChunk *c = a->head;
    while (c) {
        FmArenaChunk *n = c->next;
        free(c);
        c = n;
    }
    a->head = NULL;
    a->oom = 0;
}

static size_t fm_align_up(size_t n, size_t align)
{
    if (align <= 1) {
        return n;
    }
    size_t mask = align - 1u;
    if ((n & mask) == 0) {
        return n;
    }
    if (n > SIZE_MAX - (align - n % align)) {
        return SIZE_MAX;
    }
    return (n + mask) & ~mask;
}

void *fm_arena_alloc(FmArena *a, size_t bytes, size_t align)
{
    if (a->oom) {
        return NULL;
    }
    if (bytes == 0) {
        bytes = 1;
    }
    if (align == 0) {
        align = 1;
    }
    for (FmArenaChunk *c = a->head; c; c = c->next) {
        size_t start = fm_align_up(c->used, align);
        if (start != SIZE_MAX && bytes <= c->cap - start) {
            c->used = start + bytes;
            return c->data + start;
        }
    }
    size_t cap = FM_ARENA_CHUNK;
    if (bytes + 64u > cap) {
        cap = bytes + 64u;
    }
    size_t chunk_bytes = offsetof(FmArenaChunk, data) + cap;
    FmArenaChunk *c = (FmArenaChunk *)malloc(chunk_bytes);
    if (!c) {
        a->oom = 1;
        return NULL;
    }
    c->next = a->head;
    c->cap = cap;
    c->used = 0;
    a->head = c;
    size_t start = fm_align_up(0, align);
    c->used = start + bytes;
    return c->data + start;
}

void *fm_arena_calloc(FmArena *a, size_t n, size_t sz)
{
    if (sz != 0 && n > SIZE_MAX / sz) {
        a->oom = 1;
        return NULL;
    }
    size_t bytes = n * sz;
    void *p = fm_arena_alloc(a, bytes, sz > 8 ? 8 : (sz ? sz : 1));
    if (!p) {
        return NULL;
    }
    memset(p, 0, bytes);
    return p;
}

char *fm_arena_strndup(FmArena *a, const char *s, size_t n)
{
    char *p = (char *)fm_arena_alloc(a, n + 1, 1);
    if (!p) {
        return NULL;
    }
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

char *fm_arena_sprintf(FmArena *a, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);
    if (n < 0) {
        va_end(ap);
        a->oom = 1;
        return NULL;
    }
    char *p = (char *)fm_arena_alloc(a, (size_t)n + 1u, 1);
    if (!p) {
        va_end(ap);
        return NULL;
    }
    vsnprintf(p, (size_t)n + 1u, fmt, ap);
    va_end(ap);
    return p;
}

void fm_buf_init(FmBuf *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->oom = 0;
}

void fm_buf_free(FmBuf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->oom = 0;
}

bool fm_buf_reserve(FmBuf *b, size_t extra)
{
    if (b->oom) {
        return false;
    }
    if (extra > SIZE_MAX - b->len) {
        b->oom = 1;
        return false;
    }
    size_t need = b->len + extra;
    if (need <= b->cap) {
        return true;
    }
    size_t cap = b->cap ? b->cap : 256;
    while (cap < need) {
        if (cap > SIZE_MAX / 2u) {
            cap = need;
            break;
        }
        cap *= 2u;
    }
    char *p = (char *)realloc(b->data, cap);
    if (!p) {
        b->oom = 1;
        return false;
    }
    b->data = p;
    b->cap = cap;
    return true;
}

bool fm_buf_append(FmBuf *b, const char *s, size_t n)
{
    if (!fm_buf_reserve(b, n + 1u)) {
        return false;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

bool fm_buf_puts(FmBuf *b, const char *s)
{
    return fm_buf_append(b, s, strlen(s));
}

bool fm_buf_printf(FmBuf *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);
    if (n < 0) {
        va_end(ap);
        b->oom = 1;
        return false;
    }
    if (!fm_buf_reserve(b, (size_t)n + 1u)) {
        va_end(ap);
        return false;
    }
    vsnprintf(b->data + b->len, (size_t)n + 1u, fmt, ap);
    va_end(ap);
    b->len += (size_t)n;
    return true;
}

bool fm_read_file(const char *path, char **out_bytes, size_t *out_size,
                  FmDiagList *diags)
{
    *out_bytes = NULL;
    *out_size = 0;
    FILE *f = fopen(path, "rb");
    if (!f) {
        FmSpan sp;
        fm_span_init(&sp, path, 0, 0, 1, 1, 1, 1);
        char msg[512];
        snprintf(msg, sizeof msg, "cannot open '%s' for reading", path);
        fm_diag_add(diags, FM_SEV_ERROR, "FM0001", sp, -1, msg,
                    "Check that the path exists and is readable.");
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        FmSpan sp;
        fm_span_init(&sp, path, 0, 0, 1, 1, 1, 1);
        fm_diag_add(diags, FM_SEV_ERROR, "FM0002", sp, -1,
                    "cannot measure the input file", NULL);
        return false;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        FmSpan sp;
        fm_span_init(&sp, path, 0, 0, 1, 1, 1, 1);
        fm_diag_add(diags, FM_SEV_ERROR, "FM0002", sp, -1,
                    "cannot measure the input file", NULL);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    char *buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        FmSpan sp;
        fm_span_init(&sp, path, 0, 0, 1, 1, 1, 1);
        fm_diag_add(diags, FM_SEV_ERROR, "FM0003", sp, -1,
                    "out of memory while reading input", NULL);
        return false;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    *out_bytes = buf;
    *out_size = n;
    return true;
}

unsigned char fm_ascii_ignore(void)
{
    return (unsigned char)'/';
}

bool fm_is_ignore_char(unsigned char ch)
{
    return ch == '/' || ch == 'i' || ch == 'I';
}

bool fm_is_space_char(unsigned char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' ||
           ch == '\f';
}

int fm_parse_uint(const char *s, size_t n, unsigned *out)
{
    if (n == 0) {
        return 0;
    }
    unsigned v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
        unsigned d = (unsigned)(s[i] - '0');
        if (v > (UINT_MAX - d) / 10u) {
            return 0;
        }
        v = v * 10u + d;
    }
    *out = v;
    return 1;
}

int fm_parse_uint_word_right(const FmWord *w, unsigned width, unsigned *out)
{
    if (width == 0 || width > FM_WORD_WIDTH) {
        return 0;
    }
    unsigned start = FM_WORD_WIDTH - width;
    return fm_parse_uint((const char *)w->pos + start, width, out);
}

bool fm_word_is_kw(const FmWord *w, const char *kw)
{
    FmWord tmp;
    bool overflow = false;
    if (!fm_word_from_cstr(&tmp, kw, &overflow) || overflow) {
        return false;
    }
    return fm_word_equal(w, &tmp);
}

bool fm_cstr_eq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

const char *fm_tok_kind_name(FmTokKind k)
{
    switch (k) {
    case FM_TOK_EOF:
        return "end of input";
    case FM_TOK_WORD:
        return "word";
    case FM_TOK_NUMBER:
        return "number";
    case FM_TOK_GROUP:
        return "parenthesized group";
    case FM_TOK_PERIOD:
        return "period";
    case FM_TOK_COMMA:
        return "comma";
    case FM_TOK_SEMI:
        return "semicolon";
    default:
        return "token";
    }
}
