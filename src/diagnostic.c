#include "internal.h"

void fm_diags_init(FmDiagList *list)
{
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
    list->error_count = 0;
    list->warning_count = 0;
}

void fm_diags_free(FmDiagList *list)
{
    for (size_t i = 0; i < list->len; i++) {
        free(list->items[i].message);
        free(list->items[i].correction);
        if (list->items[i].related) {
            for (size_t r = 0; r < list->items[i].related_len; r++) {
                free((void *)list->items[i].related[r].label);
            }
            free(list->items[i].related);
        }
    }
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
    list->error_count = 0;
    list->warning_count = 0;
}

static char *fm_dup_str(const char *s)
{
    if (!s) {
        return NULL;
    }
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1u);
    if (!p) {
        return NULL;
    }
    memcpy(p, s, n + 1u);
    return p;
}

bool fm_diag_add(FmDiagList *list, FmSeverity sev, const char *code,
                 FmSpan span, int operation, const char *message,
                 const char *correction)
{
    if (list->len == list->cap) {
        size_t cap = list->cap ? list->cap * 2u : 8u;
        FmDiagnostic *p =
            (FmDiagnostic *)realloc(list->items, cap * sizeof(*p));
        if (!p) {
            return false;
        }
        list->items = p;
        list->cap = cap;
    }
    FmDiagnostic *d = &list->items[list->len];
    memset(d, 0, sizeof(*d));
    d->severity = sev;
    d->code = code;
    d->span = span;
    d->operation = operation;
    d->message = fm_dup_str(message ? message : "");
    d->correction = fm_dup_str(correction);
    if (!d->message) {
        return false;
    }
    list->len++;
    if (sev == FM_SEV_ERROR) {
        list->error_count++;
    } else if (sev == FM_SEV_WARNING) {
        list->warning_count++;
    }
    return true;
}

bool fm_diag_add_related(FmDiagList *list, size_t index, FmSpan span,
                         const char *label)
{
    if (index >= list->len) {
        return false;
    }
    FmDiagnostic *d = &list->items[index];
    FmRelatedLoc *p = (FmRelatedLoc *)realloc(
        d->related, (d->related_len + 1u) * sizeof(*p));
    if (!p) {
        return false;
    }
    d->related = p;
    d->related[d->related_len].span = span;
    d->related[d->related_len].label = fm_dup_str(label ? label : "");
    d->related_len++;
    return true;
}

static const char *fm_sev_name(FmSeverity s)
{
    switch (s) {
    case FM_SEV_NOTE:
        return "note";
    case FM_SEV_WARNING:
        return "warning";
    case FM_SEV_ERROR:
        return "error";
    default:
        return "message";
    }
}

void fm_diag_print(const FmDiagnostic *d, void *file)
{
    FILE *fp = file ? (FILE *)file : stderr;
    const char *path = d->span.path ? d->span.path : "<input>";
    fprintf(fp, "%s:%u:%u: %s %s:\n", path, d->span.line_start,
            d->span.col_start, fm_sev_name(d->severity), d->code);
    fprintf(fp, "%s\n", d->message ? d->message : "");
    if (d->correction && d->correction[0]) {
        fprintf(fp, "    %s\n", d->correction);
    }
    for (size_t i = 0; i < d->related_len; i++) {
        const FmRelatedLoc *r = &d->related[i];
        const char *rp = r->span.path ? r->span.path : path;
        fprintf(fp, "    %s:%u:%u: %s\n", rp, r->span.line_start,
                r->span.col_start, r->label ? r->label : "");
    }
}

void fm_diags_print(const FmDiagList *list, void *file)
{
    for (size_t i = 0; i < list->len; i++) {
        fm_diag_print(&list->items[i], file);
        if (i + 1u < list->len) {
            fputc('\n', file ? (FILE *)file : stderr);
        }
    }
}

bool fm_diags_has_error(const FmDiagList *list)
{
    return list->error_count > 0;
}
