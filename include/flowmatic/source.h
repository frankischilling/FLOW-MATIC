#ifndef FLOWMATIC_SOURCE_H
#define FLOWMATIC_SOURCE_H

#include <stddef.h>
#include <stdint.h>

typedef struct FmSpan {
    const char *path;
    size_t byte_start;
    size_t byte_end;
    unsigned line_start;
    unsigned col_start;
    unsigned line_end;
    unsigned col_end;
} FmSpan;

typedef struct FmSource {
    const char *path;
    const char *bytes;
    size_t size;
    int owned;
} FmSource;

void fm_span_init(FmSpan *span, const char *path, size_t b0, size_t b1,
                  unsigned l0, unsigned c0, unsigned l1, unsigned c1);
FmSpan fm_span_merge(FmSpan a, FmSpan b);

#endif
