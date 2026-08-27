#include "internal.h"
#include "ir.h"

const FmGenInfo *fm_select_generator(FmOpKind k)
{
    static const FmGenInfo table[] = {
        {NULL, NULL, 0, 0, 0},
        {"INPUT-GEN", "fm_rt_input_prime", 0, 1, 0},
        {"CLOSE-OUT-GEN", "fm_rt_close_out", 0, 1, 0},
        {"COMPARE-GEN", "fm_rt_compare", 1, 0, 0},
        {"EXECUTE-GEN", "fm_rt_execute", 0, 0, 0},
        {"JUMP-GEN", "fm_rt_jump", 0, 0, 0},
        {"MOVE-GEN", "fm_rt_move", 0, 0, 0},
        {"READ-ITEM-GEN", "fm_rt_read_item", 0, 1, 0},
        {"REWIND-GEN", "fm_rt_rewind", 0, 1, 0},
        {"SET-GEN", "fm_rt_set_jump", 0, 0, 1},
        {"STOP-GEN", "fm_rt_stop", 0, 1, 0},
        {"TEST-GEN", "fm_rt_test", 1, 0, 0},
        {"TRANSFER-GEN", "fm_rt_transfer", 0, 0, 0},
        {"WRITE-ITEM-GEN", "fm_rt_write_item", 0, 1, 0},
        {"X1-PLACEHOLDER", "fm_rt_x1_hook", 0, 0, 0},
    };
    if (k < 0 || (size_t)k >= sizeof table / sizeof table[0]) {
        return &table[0];
    }
    return &table[k];
}

static void fm_dump_word(FmBuf *out, const FmWord *w)
{
    char b[13];
    fm_word_to_cstr(w, b);
    size_t n = fm_word_used_len(w);
    fm_buf_append(out, b, n);
}

void fm_dump_opfile1(const FmProgram *prog, FmBuf *out)
{
    fm_buf_puts(out, "OPERATIONS FILE 1\n");
    fm_buf_puts(out, "phase: Translation\n");
    fm_buf_printf(out, "operations: %zu\n", prog->nops);
    fm_buf_printf(out, "data-designs: %zu\n", prog->ndesigns);
    fm_buf_printf(out, "directory: %s\n",
                  prog->directory.present ? "yes" : "no");
    if (prog->directory.present) {
        fm_buf_printf(out, "w-high: %u\n", prog->directory.w_high);
    }
    for (size_t i = 0; i < prog->nops; i++) {
        const FmOperation *op = &prog->ops[i];
        fm_buf_printf(out, "op %03d %s words=%zu\n", op->number,
                      fm_op_kind_name(op->kind), op->stmt_word_count);
        if (op->kind == FM_OP_JUMP) {
            fm_buf_printf(out, "  jump %d\n", op->u.jump.target);
        }
        if (op->kind == FM_OP_SET) {
            for (size_t k = 0; k < op->u.set.npairs; k++) {
                fm_buf_printf(out, "  set %d -> %d\n",
                              op->u.set.pairs[k].from_op,
                              op->u.set.pairs[k].to_op);
            }
        }
    }
    for (size_t i = 0; i < prog->ndesigns; i++) {
        const FmDataDesign *d = &prog->designs[i];
        fm_buf_puts(out, "design ");
        fm_dump_word(out, &d->file_name);
        fm_buf_printf(out, " letter=%c fields=%zu wstorage=%d\n",
                      d->letter > 0 ? d->letter : '?', d->nfields,
                      d->is_wstorage);
    }
}

void fm_dump_opfile2(const FmProgram *prog, FmBuf *out)
{
    fm_buf_puts(out, "OPERATIONS FILE 2\n");
    fm_buf_puts(out, "phase: Selection\n");
    for (size_t i = 0; i < prog->nops; i++) {
        const FmOperation *op = &prog->ops[i];
        const FmGenInfo *g = fm_select_generator(op->kind);
        fm_buf_printf(out, "op %03d %s generator=%s runtime=%s\n", op->number,
                      fm_op_kind_name(op->kind),
                      g->name ? g->name : "none",
                      g->runtime_fn ? g->runtime_fn : "none");
    }
}

void fm_dump_library(const FmProgram *prog, FmBuf *out)
{
    fm_buf_puts(out, "GENERATED LIBRARY\n");
    fm_buf_puts(out, "phase: Selection\n");
    for (size_t i = 0; i < prog->nops; i++) {
        const FmOperation *op = &prog->ops[i];
        const FmGenInfo *g = fm_select_generator(op->kind);
        fm_buf_printf(out, "routine op=%03d name=%s\n", op->number,
                      g->name ? g->name : "none");
        fm_buf_printf(out, "  inputs: specialized for referenced files/fields\n");
        fm_buf_printf(out, "  outputs: next-operation exits\n");
        fm_buf_printf(out, "  constants: operation literals\n");
        fm_buf_printf(out, "  runtime: %s\n",
                      g->runtime_fn ? g->runtime_fn : "none");
        fm_buf_printf(out, "  caps: io=%d compare=%d set=%d\n", g->uses_io,
                      g->uses_compare, g->mutates_jump);
    }
}

void fm_allocate(const FmProgram *prog, FmIr *ir)
{
    memset(ir, 0, sizeof(*ir));
    ir->next_code = FM_ADDR_CODE_BASE;
    ir->next_const = FM_ADDR_CONST_BASE;
    ir->w_base = FM_ADDR_W_BASE;
    ir->w_high = (int)prog->directory.w_high;
    ir->nops = prog->nops;
    int fb = FM_ADDR_FILE_BASE;
    for (size_t i = 0; i < prog->ndesigns; i++) {
        int L = prog->designs[i].letter;
        if (L > 0 && L < 256) {
            ir->file_base[L] = fb;
            unsigned sz = prog->designs[i].item.item_size
                              ? prog->designs[i].item.item_size
                              : 1u;
            fb += (int)sz;
        }
    }
    for (size_t i = 0; i < prog->nops; i++) {
        FmAllocOp *a = &ir->ops[i];
        a->number = prog->ops[i].number;
        a->start = ir->next_code;
        int len = 1;
        if (prog->ops[i].kind == FM_OP_COMPARE ||
            prog->ops[i].kind == FM_OP_MOVE) {
            len = 2;
        }
        a->end = a->start + len - 1;
        ir->next_code = a->end + 1;
        if (prog->ops[i].kind == FM_OP_TEST) {
            a->nconst = 1;
            a->const_addr[0] = ir->next_const++;
        }
        if (prog->ops[i].kind == FM_OP_JUMP) {
            a->nexits = 1;
            a->exits[0] = prog->ops[i].u.jump.target;
        }
        if (prog->ops[i].kind == FM_OP_COMPARE) {
            a->nexits = (int)prog->ops[i].u.compare.nbranches;
            for (size_t b = 0; b < prog->ops[i].u.compare.nbranches; b++) {
                a->exits[b] = prog->ops[i].u.compare.branches[b].target;
            }
        }
    }
    /* entrances: who jumps here */
    for (size_t i = 0; i < prog->nops; i++) {
        for (size_t e = 0; e < (size_t)ir->ops[i].nexits; e++) {
            int t = ir->ops[i].exits[e];
            for (size_t j = 0; j < prog->nops; j++) {
                if (ir->ops[j].number == t &&
                    ir->ops[j].nentr < 8) {
                    ir->ops[j].entr[ir->ops[j].nentr++] = ir->ops[i].number;
                }
            }
        }
        if (i + 1u < prog->nops && prog->ops[i].kind != FM_OP_JUMP &&
            prog->ops[i].kind != FM_OP_STOP &&
            prog->ops[i].kind != FM_OP_COMPARE &&
            prog->ops[i].kind != FM_OP_TEST) {
            int t = prog->ops[i + 1u].number;
            for (size_t j = 0; j < prog->nops; j++) {
                if (ir->ops[j].number == t && ir->ops[j].nentr < 8) {
                    ir->ops[j].entr[ir->ops[j].nentr++] = ir->ops[i].number;
                }
            }
        }
    }
}

void fm_dump_opfile3(const FmProgram *prog, const FmIr *ir, FmBuf *out)
{
    fm_buf_puts(out, "OPERATIONS FILE 3\n");
    fm_buf_puts(out, "phase: Allocation\n");
    fm_buf_printf(out, "w-base=%04X w-high=%d\n", ir->w_base, ir->w_high);
    for (size_t i = 0; i < prog->ndesigns; i++) {
        int L = prog->designs[i].letter;
        if (L > 0) {
            fm_buf_printf(out, "file %c base=%04X\n", L, ir->file_base[L]);
        }
    }
    for (size_t i = 0; i < ir->nops; i++) {
        const FmAllocOp *a = &ir->ops[i];
        fm_buf_printf(out, "op %03d start=%04X end=%04X exits=", a->number,
                      a->start, a->end);
        for (int e = 0; e < a->nexits; e++) {
            fm_buf_printf(out, "%d%s", a->exits[e],
                          e + 1 < a->nexits ? "," : "");
        }
        fm_buf_puts(out, "\n");
    }
}
