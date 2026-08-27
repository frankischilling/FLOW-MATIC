#include "internal.h"
#include "ir.h"
#include "flowmatic/compiler.h"

void fm_cfg_build(const FmProgram *prog, FmCfgEdge *edges, size_t *nedges,
                  size_t cap, FmDiagList *diags)
{
    *nedges = 0;
    for (size_t i = 0; i < prog->nops; i++) {
        const FmOperation *op = &prog->ops[i];
        int fall = (i + 1u < prog->nops) ? prog->ops[i + 1u].number : -1;
        switch (op->kind) {
        case FM_OP_STOP:
            break;
        case FM_OP_JUMP:
            if (*nedges < cap) {
                edges[(*nedges)++] =
                    (FmCfgEdge){op->number, op->u.jump.target, 1};
            }
            break;
        case FM_OP_COMPARE:
            for (size_t b = 0; b < op->u.compare.nbranches; b++) {
                if (*nedges < cap) {
                    edges[(*nedges)++] = (FmCfgEdge){
                        op->number, op->u.compare.branches[b].target, 1};
                }
            }
            break;
        case FM_OP_TEST:
            for (size_t b = 0; b < op->u.test.nbranches; b++) {
                if (*nedges < cap) {
                    edges[(*nedges)++] = (FmCfgEdge){
                        op->number, op->u.test.branches[b].target, 1};
                }
            }
            break;
        case FM_OP_READ_ITEM:
            if (fall >= 0 && *nedges < cap) {
                edges[(*nedges)++] = (FmCfgEdge){op->number, fall, 0};
            }
            if (op->u.read_item.has_end && *nedges < cap) {
                edges[(*nedges)++] =
                    (FmCfgEdge){op->number, op->u.read_item.end_target, 1};
            }
            break;
        case FM_OP_EXECUTE:
            if (*nedges < cap) {
                edges[(*nedges)++] =
                    (FmCfgEdge){op->number, op->u.execute.from_op, 1};
            }
            if (fall >= 0 && *nedges < cap) {
                edges[(*nedges)++] = (FmCfgEdge){op->number, fall, 0};
            }
            break;
        case FM_OP_SET:
            /* SET does not itself transfer control; fall through. */
            if (fall >= 0 && *nedges < cap) {
                edges[(*nedges)++] = (FmCfgEdge){op->number, fall, 0};
            }
            break;
        default:
            if (fall >= 0 && *nedges < cap) {
                edges[(*nedges)++] = (FmCfgEdge){op->number, fall, 0};
            }
            break;
        }
    }
    /* reachability warnings */
    int maxn = 0;
    for (size_t i = 0; i < prog->nops; i++) {
        if (prog->ops[i].number > maxn) {
            maxn = prog->ops[i].number;
        }
    }
    unsigned char reach[FM_MAX_OPERATIONS];
    memset(reach, 0, sizeof reach);
    int stack[FM_MAX_OPERATIONS];
    int sp = 0;
    stack[sp++] = 0;
    reach[0] = 1;
    while (sp) {
        int n = stack[--sp];
        for (size_t e = 0; e < *nedges; e++) {
            if (edges[e].from == n) {
                int t = edges[e].to;
                if (t >= 0 && t < FM_MAX_OPERATIONS && !reach[t]) {
                    reach[t] = 1;
                    stack[sp++] = t;
                }
            }
        }
        /* SET can retarget JUMP; treat those JUMP destinations as reachable. */
        for (size_t i = 0; i < prog->nops; i++) {
            if (prog->ops[i].kind != FM_OP_SET) {
                continue;
            }
            for (size_t k = 0; k < prog->ops[i].u.set.npairs; k++) {
                int t = prog->ops[i].u.set.pairs[k].to_op;
                if (t >= 0 && t < FM_MAX_OPERATIONS && !reach[t] &&
                    reach[prog->ops[i].number]) {
                    reach[t] = 1;
                    stack[sp++] = t;
                }
            }
        }
    }
    for (size_t i = 0; i < prog->nops; i++) {
        int n = prog->ops[i].number;
        if (n >= 0 && n < FM_MAX_OPERATIONS && !reach[n]) {
            fm_diag_add(diags, FM_SEV_WARNING, "FM4301", prog->ops[i].span, n,
                        "this operation is unreachable from INPUT under the "
                        "static control-flow graph (SET is treated as adding "
                        "its destinations)",
                        "The 1958 manual does not require this check.");
        }
    }
    int stop_n = prog->ops[prog->nops - 1u].number;
    unsigned char tostop[FM_MAX_OPERATIONS];
    memset(tostop, 0, sizeof tostop);
    if (stop_n >= 0 && stop_n < FM_MAX_OPERATIONS) {
        tostop[stop_n] = 1;
    }
    int changed = 1;
    while (changed) {
        changed = 0;
        for (size_t e = 0; e < *nedges; e++) {
            int f = edges[e].from;
            int t = edges[e].to;
            if (t >= 0 && t < FM_MAX_OPERATIONS && tostop[t] && f >= 0 &&
                f < FM_MAX_OPERATIONS && !tostop[f]) {
                tostop[f] = 1;
                changed = 1;
            }
        }
    }
    if (reach[0] && stop_n >= 0 && stop_n < FM_MAX_OPERATIONS && !tostop[0]) {
        fm_diag_add(diags, FM_SEV_WARNING, "FM4302", prog->ops[0].span, 0,
                    "no path from INPUT to STOP was found in the static graph",
                    "The 1958 manual does not require this check. SET and "
                    "EXECUTE make some paths only visible at run time.");
    }
    (void)maxn;
}
