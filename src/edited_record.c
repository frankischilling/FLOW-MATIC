#include "internal.h"
#include "ir.h"

void fm_write_edited(const FmProgram *prog, const FmIr *ir, int machine,
                     FmBuf *out)
{
    fm_buf_puts(out, "EDITED RECORD OF COMPILATION\n");
    fm_buf_puts(out, "backend: modern-c17 (not UNIVAC II)\n");
    fm_buf_puts(out, "\n(1) Compilation input listing (Directory excluded)\n");
    for (size_t i = 0; i < prog->ndesigns; i++) {
        const FmDataDesign *d = &prog->designs[i];
        char n[13];
        fm_word_to_cstr(&d->file_name, n);
        if (machine) {
            fm_buf_printf(out, "DESIGN %s L=%c W=%d F=%zu\n", n,
                          d->letter > 0 ? d->letter : '-', d->is_wstorage,
                          d->nfields);
        } else {
            fm_buf_printf(out, "  Data Design %s", n);
            if (d->is_wstorage) {
                fm_buf_puts(out, " (W-storage)");
            }
            if (d->letter > 0) {
                fm_buf_printf(out, " file %c", d->letter);
            }
            fm_buf_printf(out, ", %zu fields\n", d->nfields);
        }
    }
    fm_buf_puts(out, "\n(2) Storage allocation table\n");
    fm_buf_printf(out, "  W-storage symbolic W000 actual %04X through %04X\n",
                  ir->w_base, ir->w_base + ir->w_high);
    for (size_t i = 0; i < prog->ndesigns; i++) {
        int L = prog->designs[i].letter;
        if (L > 0 && !prog->designs[i].is_wstorage) {
            fm_buf_printf(out, "  file %c item area symbolic %c000 actual %04X\n",
                          L, L, ir->file_base[L]);
        }
    }
    fm_buf_puts(out, "\n(3) Field names with symbolic and allocated addresses\n");
    for (size_t i = 0; i < prog->ndesigns; i++) {
        const FmDataDesign *d = &prog->designs[i];
        int L = d->letter > 0 ? d->letter : '?';
        int base = d->is_wstorage ? ir->w_base : ir->file_base[L & 255];
        for (size_t f = 0; f < d->nfields; f++) {
            char n[13];
            fm_word_to_cstr(&d->fields[f].name, n);
            fm_buf_printf(out, "  %s (%c) symbolic %c%03u actual %04X\n", n, L,
                          L, d->fields[f].word_loc,
                          base + (int)d->fields[f].word_loc);
        }
    }
    fm_buf_puts(out, "\n(4) Operations: start, end, constants, exits, "
                     "entrances\n");
    for (size_t i = 0; i < ir->nops; i++) {
        const FmAllocOp *a = &ir->ops[i];
        const char *fn = fm_op_kind_name(prog->ops[i].kind);
        fm_buf_printf(out, "  OP %03d %-12s start=%04X end=%04X\n", a->number,
                      fn, a->start, a->end);
        if (a->nconst) {
            fm_buf_puts(out, "      CONSTANTS");
            for (int c = 0; c < a->nconst; c++) {
                fm_buf_printf(out, " %04X", a->const_addr[c]);
            }
            fm_buf_puts(out, "\n");
        }
        fm_buf_puts(out, "      EXITS");
        for (int e = 0; e < a->nexits; e++) {
            fm_buf_printf(out, " %d", a->exits[e]);
        }
        if (!a->nexits) {
            fm_buf_puts(out, " (fallthrough)");
        }
        fm_buf_puts(out, "\n      ENTRANCES");
        for (int e = 0; e < a->nentr; e++) {
            fm_buf_printf(out, " %d", a->entr[e]);
        }
        fm_buf_puts(out, "\n");
    }
    if (prog->nx1) {
        fm_buf_puts(out, "\nX-1 sections (relocated symbolically)\n");
        for (size_t s = 0; s < prog->nx1; s++) {
            fm_buf_printf(out, "  X-1 operation %d lines=%zu M-count=%d\n",
                          prog->x1[s].op_number, prog->x1[s].nlines,
                          prog->x1[s].body_m_count);
        }
    }
}
