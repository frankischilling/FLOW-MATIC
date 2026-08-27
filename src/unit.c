#include "internal.h"
#include "flowmatic/compiler.h"

static int fm_load_text(FmArena *arena, const char *path, FmSource *src,
                        FmDiagList *diags)
{
    char *bytes = NULL;
    size_t sz = 0;
    if (!fm_read_file(path, &bytes, &sz, diags)) {
        return 0;
    }
    /* Keep file bytes in the arena so later phases can still read spans. */
    char *copy = fm_arena_strndup(arena, bytes, sz);
    free(bytes);
    if (!copy) {
        return 0;
    }
    src->path = path;
    src->bytes = copy;
    src->size = sz;
    src->owned = 0;
    return 1;
}

static int fm_slice_source(FmArena *arena, const FmSource *parent,
                           const char *label, size_t b0, size_t b1,
                           FmSource *out)
{
    if (b1 < b0) {
        b1 = b0;
    }
    out->path = parent->path;
    out->bytes = parent->bytes + b0;
    out->size = b1 - b0;
    out->owned = 0;
    (void)arena;
    (void)label;
    return 1;
}

static const char *fm_find_marker(const char *s, size_t n, const char *m)
{
    size_t ml = strlen(m);
    if (ml > n) {
        return NULL;
    }
    for (size_t i = 0; i + ml <= n; i++) {
        if (memcmp(s + i, m, ml) == 0) {
            return s + i;
        }
    }
    return NULL;
}

bool fm_load_unit(FmArena *arena, const FmUnitPaths *paths, FmDiagList *diags,
                  FmProgram *prog)
{
    memset(prog, 0, sizeof(*prog));
    for (size_t i = 0; i < paths->ndesigns; i++) {
        FmSource src;
        if (!fm_load_text(arena, paths->designs[i], &src, diags)) {
            return false;
        }
        if (prog->ndesigns >= 12) {
            return false;
        }
        fm_parse_data_design_text(arena, &src, diags,
                                  &prog->designs[prog->ndesigns]);
        prog->ndesigns++;
    }
    if (paths->w_storage) {
        FmSource src;
        if (!fm_load_text(arena, paths->w_storage, &src, diags)) {
            return false;
        }
        if (prog->ndesigns >= 12) {
            return false;
        }
        fm_parse_data_design_text(arena, &src, diags,
                                  &prog->designs[prog->ndesigns]);
        prog->designs[prog->ndesigns].is_wstorage = 1;
        prog->designs[prog->ndesigns].letter = 'W';
        prog->ndesigns++;
    }
    if (paths->directory) {
        FmSource src;
        if (!fm_load_text(arena, paths->directory, &src, diags)) {
            return false;
        }
        fm_parse_directory_text(arena, &src, diags, &prog->directory);
    }
    if (paths->code) {
        FmSource src;
        if (!fm_load_text(arena, paths->code, &src, diags)) {
            return false;
        }
        fm_parse_program(arena, &src, diags, prog);
    }
    if (paths->x1) {
        FmSource src;
        if (!fm_load_text(arena, paths->x1, &src, diags)) {
            return false;
        }
        if (!fm_program_reserve_x1(arena, prog, 64)) {
            return false;
        }
        fm_parse_x1_text(arena, &src, diags, prog->x1, &prog->nx1, prog->x1_cap);
    }
    if (paths->combined) {
        FmSource src;
        if (!fm_load_text(arena, paths->combined, &src, diags)) {
            return false;
        }
        /* Modern transport markers, not FLOW-MATIC syntax:
           @section data-design / w-storage / directory / code / x1 */
        const char *markers[] = {
            "@section data-design", "@section w-storage", "@section directory",
            "@section code", "@section x1"};
        const char *pos[5];
        int any = 0;
        for (int m = 0; m < 5; m++) {
            pos[m] = fm_find_marker(src.bytes, src.size, markers[m]);
            if (pos[m]) {
                any = 1;
            }
        }
        if (!any) {
            /* Treat as FLOW-MATIC code. */
            fm_parse_program(arena, &src, diags, prog);
            return !fm_diags_has_error(diags);
        }
        /* Documented tape order: Data Designs, W-storage, Directory, code, X-1.
           Marker order in a combined file is checked against that sequence. */
        {
            FmSpan sp;
            fm_span_init(&sp, src.path, 0, 0, 1, 1, 1, 1);
            if (pos[2] && pos[0] && pos[2] < pos[0]) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM3303", sp, -1,
                            "the Directory appears before the File Data Designs",
                            "Printed page 69: the Directory follows all File "
                            "and W-storage Data Designs and precedes the code.");
            }
            if (pos[2] && pos[1] && pos[2] < pos[1]) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM3303", sp, -1,
                            "the Directory appears before the W-storage Data "
                            "Design",
                            "Printed page 69: the Directory follows all File "
                            "and W-storage Data Designs.");
            }
            if (pos[2] && pos[3] && pos[2] > pos[3]) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM3303", sp, -1,
                            "the Directory appears after the FLOW-MATIC code",
                            "Printed page 69: the Directory precedes the "
                            "FLOW-MATIC code.");
            }
        }
        for (int m = 0; m < 5; m++) {
            if (!pos[m]) {
                continue;
            }
            const char *start = pos[m];
            while (start < src.bytes + src.size && *start != '\n') {
                start++;
            }
            if (start < src.bytes + src.size) {
                start++;
            }
            size_t b0 = (size_t)(start - src.bytes);
            size_t b1 = src.size;
            for (int k = 0; k < 5; k++) {
                if (pos[k] && pos[k] > pos[m]) {
                    size_t cand = (size_t)(pos[k] - src.bytes);
                    if (cand < b1) {
                        b1 = cand;
                    }
                }
            }
            FmSource slice;
            fm_slice_source(arena, &src, markers[m], b0, b1, &slice);
            if (m == 0 || m == 1) {
                if (prog->ndesigns < 12) {
                    fm_parse_data_design_text(arena, &slice, diags,
                                              &prog->designs[prog->ndesigns]);
                    if (m == 1) {
                        prog->designs[prog->ndesigns].is_wstorage = 1;
                        prog->designs[prog->ndesigns].letter = 'W';
                    }
                    prog->ndesigns++;
                }
            } else if (m == 2) {
                fm_parse_directory_text(arena, &slice, diags, &prog->directory);
            } else if (m == 3) {
                fm_parse_program(arena, &slice, diags, prog);
            } else if (m == 4) {
                if (!fm_program_reserve_x1(arena, prog, 64)) {
                    return false;
                }
                fm_parse_x1_text(arena, &slice, diags, prog->x1, &prog->nx1,
                                 prog->x1_cap);
            }
        }
    }
    return !fm_diags_has_error(diags);
}
