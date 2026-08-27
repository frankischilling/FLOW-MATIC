#ifndef FLOWMATIC_DIAGNOSTIC_H
#define FLOWMATIC_DIAGNOSTIC_H

#include "flowmatic/source.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum FmSeverity {
    FM_SEV_NOTE = 0,
    FM_SEV_WARNING = 1,
    FM_SEV_ERROR = 2
} FmSeverity;

typedef struct FmRelatedLoc {
    FmSpan span;
    const char *label;
} FmRelatedLoc;

typedef struct FmDiagnostic {
    FmSeverity severity;
    const char *code; /* stable id, e.g. FM1204 */
    FmSpan span;
    int operation; /* -1 if not applicable */
    char *message;
    char *correction;
    FmRelatedLoc *related;
    size_t related_len;
} FmDiagnostic;

typedef struct FmDiagList {
    FmDiagnostic *items;
    size_t len;
    size_t cap;
    int error_count;
    int warning_count;
} FmDiagList;

void fm_diags_init(FmDiagList *list);
void fm_diags_free(FmDiagList *list);
bool fm_diag_add(FmDiagList *list, FmSeverity sev, const char *code,
                 FmSpan span, int operation, const char *message,
                 const char *correction);
bool fm_diag_add_related(FmDiagList *list, size_t index, FmSpan span,
                         const char *label);
void fm_diag_print(const FmDiagnostic *d, void *file); /* FILE* */
void fm_diags_print(const FmDiagList *list, void *file);
bool fm_diags_has_error(const FmDiagList *list);

#endif
