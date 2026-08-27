#include "internal.h"
#include "flowmatic/compiler.h"

bool fm_program_reserve_ops(FmArena *arena, FmProgram *prog, size_t need)
{
    if (need <= prog->ops_cap) {
        return true;
    }
    if (need > (size_t)FM_MAX_OPERATIONS) {
        return false;
    }
    size_t cap = prog->ops_cap ? prog->ops_cap : 8u;
    while (cap < need) {
        if (cap > (size_t)FM_MAX_OPERATIONS / 2u) {
            cap = (size_t)FM_MAX_OPERATIONS;
            break;
        }
        cap *= 2u;
    }
    if (cap > (size_t)FM_MAX_OPERATIONS) {
        cap = (size_t)FM_MAX_OPERATIONS;
    }
    if (need > cap) {
        return false;
    }
    FmOperation *ops = (FmOperation *)fm_arena_calloc(arena, cap, sizeof(FmOperation));
    if (!ops) {
        return false;
    }
    if (prog->ops != NULL && prog->nops > 0u) {
        memcpy(ops, prog->ops, prog->nops * sizeof(FmOperation));
    }
    prog->ops = ops;
    prog->ops_cap = cap;
    return true;
}

bool fm_program_reserve_x1(FmArena *arena, FmProgram *prog, size_t need)
{
    enum { FM_MAX_X1_SECTIONS = 64 };
    if (need <= prog->x1_cap) {
        return true;
    }
    if (need > (size_t)FM_MAX_X1_SECTIONS) {
        return false;
    }
    size_t cap = prog->x1_cap ? prog->x1_cap : 2u;
    while (cap < need) {
        if (cap > (size_t)FM_MAX_X1_SECTIONS / 2u) {
            cap = (size_t)FM_MAX_X1_SECTIONS;
            break;
        }
        cap *= 2u;
    }
    if (cap > (size_t)FM_MAX_X1_SECTIONS) {
        cap = (size_t)FM_MAX_X1_SECTIONS;
    }
    if (need > cap) {
        return false;
    }
    FmX1Section *x1 =
        (FmX1Section *)fm_arena_calloc(arena, cap, sizeof(FmX1Section));
    if (!x1) {
        return false;
    }
    if (prog->x1 != NULL && prog->nx1 > 0u) {
        memcpy(x1, prog->x1, prog->nx1 * sizeof(FmX1Section));
    }
    prog->x1 = x1;
    prog->x1_cap = cap;
    return true;
}
