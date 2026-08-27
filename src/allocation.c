#include "internal.h"
#include "ir.h"
#include "flowmatic/compiler.h"

void fm_allocation_run(const FmProgram *prog, FmIr *ir, FmBuf *op3)
{
    fm_allocate(prog, ir);
    fm_dump_opfile3(prog, ir, op3);
}
