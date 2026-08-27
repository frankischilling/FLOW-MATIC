#include "internal.h"

void fm_span_init(FmSpan *span, const char *path, size_t b0, size_t b1,
                  unsigned l0, unsigned c0, unsigned l1, unsigned c1)
{
    span->path = path;
    span->byte_start = b0;
    span->byte_end = b1;
    span->line_start = l0;
    span->col_start = c0;
    span->line_end = l1;
    span->col_end = c1;
}

FmSpan fm_span_merge(FmSpan a, FmSpan b)
{
    FmSpan r = a;
    if (b.byte_start < r.byte_start) {
        r.byte_start = b.byte_start;
        r.line_start = b.line_start;
        r.col_start = b.col_start;
    }
    if (b.byte_end > r.byte_end) {
        r.byte_end = b.byte_end;
        r.line_end = b.line_end;
        r.col_end = b.col_end;
    }
    return r;
}
