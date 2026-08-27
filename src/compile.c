#include "internal.h"
#include "ir.h"
#include "flowmatic/compiler.h"
#include "flowmatic/runtime.h"

const char *fm_compiler_version(void)
{
    return FM_VERSION_STRING;
}

void fm_compile_result_free(FmCompileResult *r)
{
    free(r->c_source);
    free(r->op1);
    free(r->op2);
    free(r->op3);
    free(r->library);
    free(r->edited);
    memset(r, 0, sizeof(*r));
}

static char *fm_buf_steal(FmBuf *b)
{
    char *p = b->data;
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    if (!p) {
        p = (char *)malloc(1);
        if (p) {
            p[0] = '\0';
        }
    }
    return p;
}

static int fm_write_path(const char *path, const char *text)
{
    if (!path || !text) {
        return 1;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        return 0;
    }
    fputs(text, f);
    fclose(f);
    return 1;
}

void fm_bind_runtime(const FmProgram *prog, FmRun *run)
{
    fm_rt_init(run);
    const FmOperation *inp = NULL;
    if (prog->nops && prog->ops[0].kind == FM_OP_INPUT) {
        inp = &prog->ops[0];
    }
    for (size_t i = 0; i < prog->ndesigns; i++) {
        const FmDataDesign *d = &prog->designs[i];
        int L = d->letter;
        if (L <= 0) {
            continue;
        }
        int is_in = d->is_wstorage ? 0 : 0;
        if (inp) {
            for (size_t f = 0; f < inp->u.input.ninputs; f++) {
                if (inp->u.input.inputs[f].letter == L) {
                    is_in = 1;
                }
            }
        }
        unsigned sz = d->item.item_size ? d->item.item_size : 1u;
        if (d->is_wstorage) {
            sz = prog->directory.w_high + 1u;
            if (sz == 0) {
                sz = 1;
            }
            L = 'W';
        }
        fm_rt_add_file(run, L, is_in, sz);
        FmRtFile *rf = fm_rt_file(run, L);
        if (!rf) {
            continue;
        }
        memcpy(rf->eof_sen, d->file.end_file_sen.pos, FM_WORD_WIDTH);
        rf->sen_word = d->file.sen_first;
        for (size_t f = 0; f < d->nfields && rf->nfields < FM_RT_MAX_FIELDS;
             f++) {
            FmRtField *fld = &rf->fields[rf->nfields++];
            memset(fld, 0, sizeof(*fld));
            size_t nl = fm_word_used_len(&d->fields[f].name);
            memcpy(fld->name, d->fields[f].name.pos, nl);
            fld->name[nl] = '\0';
            fld->word_loc = d->fields[f].word_loc;
            fld->type = (int)d->fields[f].type;
            fld->dec_dir = (int)d->fields[f].dec_dir;
            fld->dec_n = d->fields[f].dec_n;
            fld->sign_pos = d->fields[f].sign_pos;
            fld->left_pos = d->fields[f].left_pos;
            fld->length = d->fields[f].length;
            memcpy(fld->extractor, d->fields[f].extractor.pos, FM_WORD_WIDTH);
            fld->full_word = d->fields[f].full_word;
        }
    }
}

bool fm_compile_unit(FmArena *arena, const FmUnitPaths *paths,
                     const FmCompileOptions *opt, FmDiagList *diags,
                     FmCompileResult *out)
{
    memset(out, 0, sizeof(*out));
    if (!fm_load_unit(arena, paths, diags, &out->program)) {
        return false;
    }
    FmCfgEdge edges[4096];
    size_t nedges = 0;
    if (!fm_analyze(arena, &out->program, diags)) {
        fm_cfg_build(&out->program, edges, &nedges, 4096, diags);
        return false;
    }
    fm_cfg_build(&out->program, edges, &nedges, 4096, diags);
    if (opt && opt->check_only && !opt->emit_c && !opt->dump_op1) {
        /* still produce dumps if requested */
    }
    FmBuf op1, op2, op3, lib, uned, cbuf, edited;
    fm_buf_init(&op1);
    fm_buf_init(&op2);
    fm_buf_init(&op3);
    fm_buf_init(&lib);
    fm_buf_init(&uned);
    fm_buf_init(&cbuf);
    fm_buf_init(&edited);
    fm_translation_run(&out->program, &op1, &uned);
    fm_selection_run(&out->program, &op2, &lib);
    FmIr ir;
    fm_allocation_run(&out->program, &ir, &op3);
    int x1_hooks = opt ? opt->x1_hooks : 0;
    for (size_t i = 0; i < out->program.nops; i++) {
        if (out->program.ops[i].kind == FM_OP_X1) {
            out->x1_requires_hooks = 1;
        }
    }
    if (out->x1_requires_hooks && opt && opt->emit_c && !x1_hooks) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM7001", out->program.ops[0].span, -1,
                    "this program contains X-1 operations; the modern backend "
                    "cannot execute them unless --x1-hooks is given",
                    "X-1 English text is documentation, not machine semantics. "
                    "Supply a callback keyed by operation number, or stop at "
                    "--check / dumps.");
        fm_buf_free(&op1);
        fm_buf_free(&op2);
        fm_buf_free(&op3);
        fm_buf_free(&lib);
        fm_buf_free(&uned);
        fm_buf_free(&cbuf);
        fm_buf_free(&edited);
        return false;
    }
    fm_processing_run(&out->program, &ir, x1_hooks, &cbuf, &edited, &uned);
    out->op1 = fm_buf_steal(&op1);
    out->op2 = fm_buf_steal(&op2);
    out->op3 = fm_buf_steal(&op3);
    out->library = fm_buf_steal(&lib);
    out->c_source = fm_buf_steal(&cbuf);
    out->edited = fm_buf_steal(&edited);
    fm_buf_free(&uned);
    if (opt) {
        if (opt->dump_op1 && !fm_write_path(opt->dump_op1, out->op1)) {
            return false;
        }
        if (opt->dump_op2 && !fm_write_path(opt->dump_op2, out->op2)) {
            return false;
        }
        if (opt->dump_op3 && !fm_write_path(opt->dump_op3, out->op3)) {
            return false;
        }
        if (opt->dump_library && !fm_write_path(opt->dump_library, out->library)) {
            return false;
        }
        if (opt->edited_record && !fm_write_path(opt->edited_record, out->edited)) {
            return false;
        }
        if (opt->output_c && !fm_write_path(opt->output_c, out->c_source)) {
            return false;
        }
    }
    return !fm_diags_has_error(diags);
}
