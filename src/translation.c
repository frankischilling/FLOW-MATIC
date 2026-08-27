#include "internal.h"
#include "ir.h"
#include "flowmatic/compiler.h"

/* Translation phase: glossary resolution already happened in parse/analyze.
   This module writes Operations File 1 and starts the Unedited Record. */

void fm_translation_run(const FmProgram *prog, FmBuf *op1, FmBuf *unedited)
{
    fm_dump_opfile1(prog, op1);
    fm_buf_puts(unedited, "UNEDITED RECORD\n");
    fm_buf_puts(unedited, "source listing captured during Translation\n");
    for (size_t i = 0; i < prog->nops; i++) {
        fm_buf_printf(unedited, "(%d) %s\n", prog->ops[i].number,
                      fm_op_kind_name(prog->ops[i].kind));
    }
}
