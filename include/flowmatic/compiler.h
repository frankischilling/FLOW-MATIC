#ifndef FLOWMATIC_COMPILER_H
#define FLOWMATIC_COMPILER_H

#include "flowmatic/ast.h"
#include "flowmatic/diagnostic.h"
#include "flowmatic/source.h"
#include "flowmatic/token.h"
#include "flowmatic/word.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct FmArena FmArena;

typedef struct FmLexResult {
    FmToken *tokens;
    size_t ntokens;
} FmLexResult;

bool fm_lex(FmArena *arena, const FmSource *src, FmDiagList *diags,
            FmLexResult *out);

typedef struct FmUnitPaths {
    const char **designs;
    size_t ndesigns;
    const char *w_storage;
    const char *directory;
    const char *code;
    const char *x1;
    const char *combined;
} FmUnitPaths;

typedef struct FmCompileOptions {
    int check_only;
    int emit_c;
    int x1_hooks; /* opt-in C callbacks for X-1 */
    const char *output_c;
    const char *dump_op1;
    const char *dump_op2;
    const char *dump_op3;
    const char *dump_library;
    const char *edited_record;
    int edited_machine; /* stable golden mode */
} FmCompileOptions;

typedef struct FmCompileResult {
    FmProgram program;
    char *c_source;
    char *op1;
    char *op2;
    char *op3;
    char *library;
    char *edited;
    int x1_requires_hooks;
} FmCompileResult;

#include "flowmatic/runtime.h"

bool fm_program_reserve_ops(FmArena *arena, FmProgram *prog, size_t need);
bool fm_program_reserve_x1(FmArena *arena, FmProgram *prog, size_t need);

bool fm_parse_program(FmArena *arena, const FmSource *src, FmDiagList *diags,
                      FmProgram *prog);

bool fm_parse_data_design_text(FmArena *arena, const FmSource *src,
                               FmDiagList *diags, FmDataDesign *out);

bool fm_parse_directory_text(FmArena *arena, const FmSource *src,
                             FmDiagList *diags, FmDirectory *out);

bool fm_parse_x1_text(FmArena *arena, const FmSource *src, FmDiagList *diags,
                      FmX1Section *sections, size_t *nsections, size_t cap);

bool fm_load_unit(FmArena *arena, const FmUnitPaths *paths, FmDiagList *diags,
                  FmProgram *prog);

bool fm_analyze(FmArena *arena, FmProgram *prog, FmDiagList *diags);
void fm_x1_validate(const FmProgram *prog, FmDiagList *diags);

bool fm_compile_unit(FmArena *arena, const FmUnitPaths *paths,
                     const FmCompileOptions *opt, FmDiagList *diags,
                     FmCompileResult *out);

void fm_compile_result_free(FmCompileResult *r);

const char *fm_compiler_version(void);

void fm_bind_runtime(const FmProgram *prog, FmRun *run);
int fm_exec_program(const FmProgram *prog, FmRun *run);

#endif
