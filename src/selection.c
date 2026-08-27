#include "internal.h"
#include "ir.h"
#include "flowmatic/compiler.h"

void fm_selection_run(const FmProgram *prog, FmBuf *op2, FmBuf *lib)
{
    fm_dump_opfile2(prog, op2);
    fm_dump_library(prog, lib);
}
