#ifndef FM_IR_H
#define FM_IR_H

#include "internal.h"
#include "flowmatic/ast.h"
#include "flowmatic/diagnostic.h"

enum { FM_ADDR_CONST_BASE = 0x1000 };
enum { FM_ADDR_CODE_BASE = 0x2000 };
enum { FM_ADDR_FILE_BASE = 0x0100 };
enum { FM_ADDR_W_BASE = 0x0800 };

typedef struct FmGenInfo {
    const char *name;
    const char *runtime_fn;
    int uses_compare;
    int uses_io;
    int mutates_jump;
} FmGenInfo;

typedef struct FmAllocOp {
    int number;
    int start;
    int end;
    int nconst;
    int const_addr[8];
    int nexits;
    int exits[8];
    int nentr;
    int entr[8];
} FmAllocOp;

typedef struct FmIr {
    FmAllocOp ops[FM_MAX_OPERATIONS];
    size_t nops;
    int w_base;
    int w_high;
    int file_base[256];
    int next_code;
    int next_const;
} FmIr;

typedef struct FmCfgEdge {
    int from;
    int to;
    int explicit_branch;
} FmCfgEdge;

const FmGenInfo *fm_select_generator(FmOpKind k);
void fm_dump_opfile1(const FmProgram *prog, FmBuf *out);
void fm_dump_opfile2(const FmProgram *prog, FmBuf *out);
void fm_dump_opfile3(const FmProgram *prog, const FmIr *ir, FmBuf *out);
void fm_dump_library(const FmProgram *prog, FmBuf *out);
void fm_write_edited(const FmProgram *prog, const FmIr *ir, int machine,
                     FmBuf *out);
int fm_emit_c(const FmProgram *prog, const FmIr *ir, int x1_hooks, FmBuf *out);
void fm_allocate(const FmProgram *prog, FmIr *ir);
void fm_cfg_build(const FmProgram *prog, FmCfgEdge *edges, size_t *nedges,
                  size_t cap, FmDiagList *diags);
void fm_translation_run(const FmProgram *prog, FmBuf *op1, FmBuf *unedited);
void fm_selection_run(const FmProgram *prog, FmBuf *op2, FmBuf *lib);
void fm_allocation_run(const FmProgram *prog, FmIr *ir, FmBuf *op3);
void fm_processing_run(const FmProgram *prog, const FmIr *ir, int x1_hooks,
                       FmBuf *c_out, FmBuf *edited, FmBuf *unedited);
void fm_x1_validate(const FmProgram *prog, FmDiagList *diags);

#endif
