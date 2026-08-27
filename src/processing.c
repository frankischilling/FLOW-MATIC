#include "internal.h"
#include "ir.h"
#include "flowmatic/compiler.h"
#include "flowmatic/runtime.h"

static const FmOperation *fm_opn(const FmProgram *p, int n)
{
    for (size_t i = 0; i < p->nops; i++) {
        if (p->ops[i].number == n) {
            return &p->ops[i];
        }
    }
    return NULL;
}

static int fm_next_num(const FmProgram *p, int n)
{
    for (size_t i = 0; i < p->nops; i++) {
        if (p->ops[i].number == n && i + 1u < p->nops) {
            return p->ops[i + 1u].number;
        }
    }
    return -1;
}

static void fm_trim_name(const FmWord *w, char *out)
{
    size_t n = fm_word_used_len(w);
    memcpy(out, w->pos, n);
    out[n] = '\0';
}

int fm_exec_program(const FmProgram *prog, FmRun *run)
{
    for (size_t i = 0; i < prog->nops; i++) {
        if (prog->ops[i].kind == FM_OP_JUMP) {
            run->jump[prog->ops[i].number] = prog->ops[i].u.jump.target;
        }
    }
    /* prime input items */
    const FmOperation *inp = fm_opn(prog, 0);
    if (inp && inp->kind == FM_OP_INPUT) {
        for (size_t f = 0; f < inp->u.input.ninputs; f++) {
            fm_rt_read_item(run, inp->u.input.inputs[f].letter);
        }
    }
    run->op = (inp && inp->kind == FM_OP_INPUT) ? fm_next_num(prog, 0) : 0;
    run->stop = 0;
    int guard = 0;
    while (!run->stop && guard++ < 100000) {
        const FmOperation *op = fm_opn(prog, run->op);
        if (!op) {
            run->exit_code = 1;
            break;
        }
        int next = fm_next_num(prog, op->number);
        switch (op->kind) {
        case FM_OP_JUMP:
            run->op = run->jump[op->number];
            continue;
        case FM_OP_SET:
            for (size_t k = 0; k < op->u.set.npairs; k++) {
                run->jump[op->u.set.pairs[k].from_op] =
                    op->u.set.pairs[k].to_op;
            }
            run->op = next;
            break;
        case FM_OP_COMPARE: {
            char a[13], b[13];
            fm_trim_name(&op->u.compare.left.name, a);
            fm_trim_name(&op->u.compare.right.name, b);
            int c = fm_rt_compare(run, a, op->u.compare.left.file_letter, b,
                                  op->u.compare.right.file_letter);
            int dest = next;
            int eq = -1, gt = -1, ow = -1;
            for (size_t br = 0; br < op->u.compare.nbranches; br++) {
                if (op->u.compare.branches[br].cond == FM_COND_EQUAL) {
                    eq = op->u.compare.branches[br].target;
                } else if (op->u.compare.branches[br].cond == FM_COND_GREATER) {
                    gt = op->u.compare.branches[br].target;
                } else if (op->u.compare.branches[br].cond == FM_COND_OTHERWISE) {
                    ow = op->u.compare.branches[br].target;
                }
            }
            if (c > 0 && gt >= 0) {
                dest = gt;
            } else if (c == 0 && eq >= 0) {
                dest = eq;
            } else if (ow >= 0) {
                dest = ow;
            }
            run->op = dest;
            break;
        }
        case FM_OP_TEST: {
            char a[13];
            fm_trim_name(&op->u.test.field.name, a);
            unsigned char tv[12];
            memset(tv, ' ', 12);
            if (op->u.test.nvalues) {
                memcpy(tv, op->u.test.values[0].pos, 12);
            } else if (op->u.test.period_word) {
                memset(tv, '.', 12);
            }
            int c = fm_rt_test(run, a, op->u.test.field.file_letter, tv);
            int dest = next;
            int eq = -1, gt = -1, lt = -1, ne = -1, ow = -1;
            for (size_t br = 0; br < op->u.test.nbranches; br++) {
                int t = op->u.test.branches[br].target;
                switch (op->u.test.branches[br].cond) {
                case FM_COND_EQUAL:
                    eq = t;
                    break;
                case FM_COND_GREATER:
                    gt = t;
                    break;
                case FM_COND_LESS:
                    lt = t;
                    break;
                case FM_COND_UNEQUAL:
                    ne = t;
                    break;
                default:
                    ow = t;
                    break;
                }
            }
            if (ne >= 0) {
                dest = (c != 0) ? ne : ow;
            } else if (c > 0 && gt >= 0) {
                dest = gt;
            } else if (c == 0 && eq >= 0) {
                dest = eq;
            } else if (c < 0 && lt >= 0) {
                dest = lt;
            } else if (ow >= 0) {
                dest = ow;
            }
            run->op = dest;
            break;
        }
        case FM_OP_MOVE:
            for (size_t k = 0; k < op->u.move.npairs; k++) {
                char s[13], d[13];
                fm_trim_name(&op->u.move.pairs[k].src.name, s);
                for (size_t j = 0; j < op->u.move.pairs[k].ndests; j++) {
                    fm_trim_name(&op->u.move.pairs[k].dests[j].field.name, d);
                    fm_rt_move(run, s, op->u.move.pairs[k].src.file_letter, d,
                               op->u.move.pairs[k].dests[j].field.file_letter);
                }
            }
            run->op = next;
            break;
        case FM_OP_TRANSFER:
            if (!op->u.transfer.has_src_sub && !op->u.transfer.has_dst_sub) {
                fm_rt_transfer(run, op->u.transfer.src_file,
                               op->u.transfer.dst_file);
            } else {
                char ss[13] = {0}, ds[13] = {0};
                if (op->u.transfer.has_src_sub) {
                    fm_trim_name(&op->u.transfer.src_sub, ss);
                }
                if (op->u.transfer.has_dst_sub) {
                    fm_trim_name(&op->u.transfer.dst_sub, ds);
                }
                fm_rt_transfer_sub(run, op->u.transfer.src_file, ss,
                                   op->u.transfer.dst_file, ds);
            }
            run->op = next;
            break;
        case FM_OP_WRITE_ITEM:
            fm_rt_write_item(run, op->u.write_item.file_letter);
            run->op = next;
            break;
        case FM_OP_READ_ITEM:
            if (!fm_rt_read_item(run, op->u.read_item.file_letter)) {
                run->op = op->u.read_item.has_end ? op->u.read_item.end_target
                                                  : next;
            } else {
                run->op = next;
            }
            break;
        case FM_OP_REWIND:
            for (size_t k = 0; k < op->u.rewind.nfiles; k++) {
                fm_rt_rewind(run, op->u.rewind.files[k]);
            }
            run->op = next;
            break;
        case FM_OP_CLOSE_OUT:
            for (size_t k = 0; k < op->u.close_out.nfiles; k++) {
                fm_rt_close_out(run, op->u.close_out.files[k]);
            }
            run->op = next;
            break;
        case FM_OP_EXECUTE: {
            int to = op->u.execute.to_op >= 0 ? op->u.execute.to_op
                                              : op->u.execute.from_op;
            if (run->exec_sp < 31) {
                run->exec_ret[run->exec_sp] = next;
                run->exec_end[run->exec_sp] = to;
                run->exec_sp++;
            }
            run->op = op->u.execute.from_op;
            break;
        }
        case FM_OP_STOP:
            run->stop = 1;
            break;
        case FM_OP_X1:
            if (run->x1_hook) {
                if (run->x1_hook(run, op->number) != 0) {
                    run->exit_code = 2;
                    run->stop = 1;
                } else {
                    run->op = next;
                }
            } else {
                run->exit_code = 2;
                run->stop = 1;
            }
            break;
        case FM_OP_INPUT:
            run->op = next;
            break;
        default:
            run->op = next;
            break;
        }
        if (run->exec_sp > 0 && run->op >= 0 &&
            run->op > run->exec_end[run->exec_sp - 1]) {
            run->exec_sp--;
            run->op = run->exec_ret[run->exec_sp];
        }
    }
    if (guard >= 100000) {
        run->exit_code = 3;
    }
    return run->exit_code;
}

void fm_processing_run(const FmProgram *prog, const FmIr *ir, int x1_hooks,
                       FmBuf *c_out, FmBuf *edited, FmBuf *unedited)
{
    (void)unedited;
    fm_emit_c(prog, ir, x1_hooks, c_out);
    fm_write_edited(prog, ir, 0, edited);
}
