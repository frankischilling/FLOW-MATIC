#include "internal.h"
#include "flowmatic/ast.h"
#include "flowmatic/compiler.h"

typedef struct FmFileMap {
    int letter;
    FmDataDesign *dd;
    int is_input;
    int is_output;
    int nread;
    int nread_end;
    int end_target;
} FmFileMap;

static FmDataDesign *fm_find_design(FmProgram *prog, const FmWord *name)
{
    for (size_t i = 0; i < prog->ndesigns; i++) {
        if (fm_word_equal(&prog->designs[i].file_name, name)) {
            return &prog->designs[i];
        }
    }
    return NULL;
}

static FmDataDesign *fm_design_by_letter(FmProgram *prog, int letter)
{
    for (size_t i = 0; i < prog->ndesigns; i++) {
        if (prog->designs[i].letter == letter) {
            return &prog->designs[i];
        }
    }
    return NULL;
}

static FmFieldDesc *fm_find_field(FmDataDesign *dd, const FmWord *name)
{
    if (!dd) {
        return NULL;
    }
    for (size_t i = 0; i < dd->nfields; i++) {
        if (fm_word_equal(&dd->fields[i].name, name)) {
            return &dd->fields[i];
        }
    }
    return NULL;
}

static FmSubItem *fm_find_sub(FmDataDesign *dd, const FmWord *name)
{
    if (!dd) {
        return NULL;
    }
    for (size_t i = 0; i < dd->item.nsubitems; i++) {
        if (fm_word_equal(&dd->item.subitems[i].name, name)) {
            return &dd->item.subitems[i];
        }
    }
    return NULL;
}

static FmOperation *fm_op_by_num(FmProgram *prog, int n)
{
    for (size_t i = 0; i < prog->nops; i++) {
        if (prog->ops[i].number == n) {
            return &prog->ops[i];
        }
    }
    return NULL;
}

static void fm_need_field(FmProgram *prog, FmDiagList *diags, FmFieldRef *r,
                          int op)
{
    char nbuf[13];
    fm_word_to_cstr(&r->name, nbuf);
    if (r->file_letter == 'W') {
        FmDataDesign *w = NULL;
        for (size_t i = 0; i < prog->ndesigns; i++) {
            if (prog->designs[i].is_wstorage) {
                w = &prog->designs[i];
                break;
            }
        }
        if (!w) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM4101", r->span, op,
                        "W-storage is referenced, but no W-storage Data Design "
                        "was supplied",
                        "Printed page 68: W-storage is described like a file "
                        "without tape-organization information.");
            return;
        }
        if (!prog->directory.present) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM4102", r->span, op,
                        "W-storage is used, so a Directory is required",
                        "The Directory must follow all Data Designs and "
                        "precede the FLOW-MATIC code.");
        }
        FmFieldDesc *f = fm_find_field(w, &r->name);
        if (!f) {
            char msg[192];
            snprintf(msg, sizeof msg, "unknown W-storage field '%s'", nbuf);
            fm_diag_add(diags, FM_SEV_ERROR, "FM4103", r->span, op, msg, NULL);
        } else if (prog->directory.present &&
                   f->word_loc > prog->directory.w_high) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM4104", r->span, op,
                        "W-storage field lies outside the Directory range",
                        "The Directory names the highest reserved W-storage "
                        "word, starting at W000.");
        }
        return;
    }
    FmDataDesign *dd = fm_design_by_letter(prog, r->file_letter);
    if (!dd) {
        char msg[160];
        snprintf(msg, sizeof msg, "file letter %c is not assigned in INPUT",
                 r->file_letter > 0 ? r->file_letter : '?');
        fm_diag_add(diags, FM_SEV_ERROR, "FM4105", r->span, op, msg, NULL);
        return;
    }
    if (!fm_find_field(dd, &r->name)) {
        char msg[192];
        snprintf(msg, sizeof msg, "unknown field '%s' in file %c", nbuf,
                 r->file_letter);
        fm_diag_add(diags, FM_SEV_ERROR, "FM4106", r->span, op, msg,
                    "Every field named in the code must appear in that file's "
                    "Field Design.");
    }
}

static void fm_need_target(FmProgram *prog, FmDiagList *diags, int from, int to,
                           FmSpan span)
{
    if (!fm_op_by_num(prog, to)) {
        char msg[160];
        snprintf(msg, sizeof msg,
                 "operation %d branches to operation %d, but operation %d is "
                 "not defined",
                 from, to, to);
        fm_diag_add(diags, FM_SEV_ERROR, "FM1204", span, from, msg,
                    "Operation numbers form an unbroken sequence. Name a "
                    "target that exists.");
    }
}

static unsigned fm_item_words(FmDataDesign *dd)
{
    if (!dd) {
        return 0;
    }
    if (dd->item.present && dd->item.item_size) {
        return dd->item.item_size;
    }
    unsigned m = 0;
    for (size_t i = 0; i < dd->nfields; i++) {
        if (dd->fields[i].word_loc + 1u > m) {
            m = dd->fields[i].word_loc + 1u;
        }
    }
    return m;
}

bool fm_analyze(FmArena *arena, FmProgram *prog, FmDiagList *diags)
{
    (void)arena;
    if (prog->nops == 0) {
        FmSpan sp = {NULL, 0, 0, 1, 1, 1, 1};
        fm_diag_add(diags, FM_SEV_ERROR, "FM4001", sp, -1,
                    "no FLOW-MATIC operations were found", NULL);
        return false;
    }
    /* numbering */
    int maxn = -1;
    for (size_t i = 0; i < prog->nops; i++) {
        if (prog->ops[i].number > maxn) {
            maxn = prog->ops[i].number;
        }
    }
    if (prog->ops[0].number != 0 || prog->ops[0].kind != FM_OP_INPUT) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM4002", prog->ops[0].span,
                    prog->ops[0].number,
                    "operation zero must be the INPUT statement",
                    "The operation number sequence starts with zero, and "
                    "operation zero is always the input statement.");
    }
    FmOperation *last = &prog->ops[prog->nops - 1u];
    if (last->kind != FM_OP_STOP || last->number != maxn) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM4003", last->span, last->number,
                    "the last operation must be STOP, and it must be the "
                    "highest numbered operation",
                    "Write STOP . (END) as the final operation.");
    }
    for (int n = 0; n <= maxn; n++) {
        int count = 0;
        size_t first_i = 0;
        for (size_t i = 0; i < prog->nops; i++) {
            if (prog->ops[i].number == n) {
                if (count == 0) {
                    first_i = i;
                }
                count++;
            }
        }
        if (count == 0) {
            char msg[128];
            snprintf(msg, sizeof msg, "operation number %d is missing", n);
            fm_diag_add(diags, FM_SEV_ERROR, "FM4004", prog->ops[0].span, n, msg,
                        "Operations are written in unbroken numeric sequence.");
        } else if (count > 1) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM4005",
                        prog->ops[first_i].span, n,
                        "duplicate operation number",
                        "Each operation number is used once.");
        }
    }
    /* Directory vs W-storage */
    int has_w = 0;
    for (size_t i = 0; i < prog->ndesigns; i++) {
        if (prog->designs[i].is_wstorage) {
            has_w = 1;
            prog->designs[i].letter = 'W';
        }
    }
    if (has_w && !prog->directory.present) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM4006", prog->ops[0].span, 0,
                    "W-storage Data Design is present without a Directory",
                    "Whenever W-storage is used, the Directory block must be "
                    "written and must follow the Data Designs.");
    }
    if (prog->directory.present && !has_w) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM4007", prog->directory.span, -1,
                    "a Directory is present without W-storage Data Design",
                    NULL);
    }
    /* bind INPUT letters to designs */
    if (prog->ops[0].kind == FM_OP_INPUT) {
        FmOpInput *in = &prog->ops[0].u.input;
        int used[256];
        memset(used, 0, sizeof used);
        for (size_t f = 0; f < in->ninputs; f++) {
            int L = in->inputs[f].letter;
            if (L >= 0 && L < 256 && used[L]) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM4008", in->inputs[f].span, 0,
                            "duplicate file letter",
                            "Each file letter is assigned once.");
            }
            if (L >= 0 && L < 256) {
                used[L] = 1;
            }
            FmDataDesign *dd = fm_find_design(prog, &in->inputs[f].name);
            if (!dd) {
                char buf[13];
                fm_word_to_cstr(&in->inputs[f].name, buf);
                char msg[160];
                snprintf(msg, sizeof msg,
                         "INPUT names file '%s', but no Data Design uses that "
                         "name",
                         buf);
                fm_diag_add(diags, FM_SEV_ERROR, "FM4009", in->inputs[f].span, 0,
                            msg, NULL);
            } else {
                dd->letter = L;
            }
        }
        for (size_t f = 0; f < in->noutputs; f++) {
            int L = in->outputs[f].letter;
            if (L >= 0 && L < 256 && used[L]) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM4008", in->outputs[f].span,
                            0, "duplicate file letter", NULL);
            }
            if (L >= 0 && L < 256) {
                used[L] = 1;
            }
            FmDataDesign *dd = fm_find_design(prog, &in->outputs[f].name);
            if (!dd) {
                char buf[13];
                fm_word_to_cstr(&in->outputs[f].name, buf);
                char msg[160];
                snprintf(msg, sizeof msg,
                         "OUTPUT names file '%s', but no Data Design uses that "
                         "name",
                         buf);
                fm_diag_add(diags, FM_SEV_ERROR, "FM4009", in->outputs[f].span,
                            0, msg, NULL);
            } else {
                dd->letter = L;
            }
        }
        if (in->ninputs >= 8 && !in->preselection) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM4010", prog->ops[0].span, 0,
                        "more than three input files require PRESELECTION",
                        "Appendix A INPUT special note 3, printed page 96.");
        }
    }
    /* per-operation checks */
    for (size_t i = 0; i < prog->nops; i++) {
        FmOperation *op = &prog->ops[i];
        switch (op->kind) {
        case FM_OP_COMPARE:
            fm_need_field(prog, diags, &op->u.compare.left, op->number);
            fm_need_field(prog, diags, &op->u.compare.right, op->number);
            for (size_t b = 0; b < op->u.compare.nbranches; b++) {
                fm_need_target(prog, diags, op->number,
                               op->u.compare.branches[b].target,
                               op->u.compare.branches[b].span);
            }
            break;
        case FM_OP_TEST:
            fm_need_field(prog, diags, &op->u.test.field, op->number);
            for (size_t b = 0; b < op->u.test.nbranches; b++) {
                fm_need_target(prog, diags, op->number,
                               op->u.test.branches[b].target,
                               op->u.test.branches[b].span);
            }
            break;
        case FM_OP_JUMP:
            fm_need_target(prog, diags, op->number, op->u.jump.target, op->span);
            break;
        case FM_OP_EXECUTE:
            fm_need_target(prog, diags, op->number, op->u.execute.from_op,
                           op->span);
            if (op->u.execute.to_op >= 0) {
                fm_need_target(prog, diags, op->number, op->u.execute.to_op,
                               op->span);
                if (op->u.execute.to_op < op->u.execute.from_op) {
                    fm_diag_add(diags, FM_SEV_ERROR, "FM4020", op->span,
                                op->number,
                                "EXECUTE range is reversed",
                                "THROUGH names the later operation number.");
                }
            }
            break;
        case FM_OP_MOVE:
            for (size_t k = 0; k < op->u.move.npairs; k++) {
                fm_need_field(prog, diags, &op->u.move.pairs[k].src, op->number);
                for (size_t d = 0; d < op->u.move.pairs[k].ndests; d++) {
                    fm_need_field(prog, diags,
                                  &op->u.move.pairs[k].dests[d].field,
                                  op->number);
                }
            }
            break;
        case FM_OP_READ_ITEM: {
            FmDataDesign *dd =
                fm_design_by_letter(prog, op->u.read_item.file_letter);
            if (!dd) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM4105", op->span, op->number,
                            "READ-ITEM names an unknown file letter", NULL);
            }
            if (op->u.read_item.has_end) {
                fm_need_target(prog, diags, op->number, op->u.read_item.end_target,
                               op->span);
            }
            break;
        }
        case FM_OP_SET:
            for (size_t k = 0; k < op->u.set.npairs; k++) {
                fm_need_target(prog, diags, op->number, op->u.set.pairs[k].from_op,
                               op->u.set.pairs[k].span);
                fm_need_target(prog, diags, op->number, op->u.set.pairs[k].to_op,
                               op->u.set.pairs[k].span);
                FmOperation *tgt =
                    fm_op_by_num(prog, op->u.set.pairs[k].from_op);
                if (tgt && tgt->kind != FM_OP_JUMP) {
                    fm_diag_add(
                        diags, FM_SEV_ERROR, "FM4030", op->span, op->number,
                        "SET names an operation that is not JUMP",
                        "The sample problems retarget JUMP. The manual says "
                        "SET alters an operation, but it does not show SET "
                        "applied to COMPARE or TEST. This compiler only "
                        "accepts SET of JUMP.");
                }
            }
            break;
        case FM_OP_TRANSFER: {
            FmDataDesign *s = fm_design_by_letter(prog, op->u.transfer.src_file);
            FmDataDesign *d = fm_design_by_letter(prog, op->u.transfer.dst_file);
            if (!s || !d) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM4105", op->span, op->number,
                            "TRANSFER names an unknown file letter", NULL);
                break;
            }
            unsigned ss = fm_item_words(s);
            unsigned ds = fm_item_words(d);
            if (op->u.transfer.has_src_sub) {
                FmSubItem *si = fm_find_sub(s, &op->u.transfer.src_sub);
                if (!si) {
                    fm_diag_add(diags, FM_SEV_ERROR, "FM4040", op->span,
                                op->number, "unknown source sub-item", NULL);
                } else {
                    ss = si->end_word - si->start_word + 1u;
                }
            }
            if (op->u.transfer.has_dst_sub) {
                FmSubItem *si = fm_find_sub(d, &op->u.transfer.dst_sub);
                if (!si) {
                    fm_diag_add(diags, FM_SEV_ERROR, "FM4040", op->span,
                                op->number, "unknown destination sub-item",
                                NULL);
                } else {
                    ds = si->end_word - si->start_word + 1u;
                }
            }
            if (ss != ds) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM4041", op->span, op->number,
                            "TRANSFER requires equal item or sub-item sizes",
                            "Appendix A TRANSFER special note 1, printed page "
                            "99.");
            }
            break;
        }
        case FM_OP_WRITE_ITEM:
            if (!fm_design_by_letter(prog, op->u.write_item.file_letter)) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM4105", op->span, op->number,
                            "WRITE-ITEM names an unknown file letter", NULL);
            }
            break;
        case FM_OP_CLOSE_OUT:
            for (size_t k = 0; k < op->u.close_out.nfiles; k++) {
                if (!fm_design_by_letter(prog, op->u.close_out.files[k])) {
                    fm_diag_add(diags, FM_SEV_ERROR, "FM4105", op->span,
                                op->number,
                                "CLOSE-OUT names an unknown file letter", NULL);
                }
            }
            break;
        case FM_OP_REWIND:
            for (size_t k = 0; k < op->u.rewind.nfiles; k++) {
                if (!fm_design_by_letter(prog, op->u.rewind.files[k])) {
                    fm_diag_add(diags, FM_SEV_ERROR, "FM4105", op->span,
                                op->number, "REWIND names an unknown file letter",
                                NULL);
                }
            }
            break;
        default:
            break;
        }
    }
    /* READ-ITEM rules */
    if (prog->ops[0].kind == FM_OP_INPUT) {
        FmOpInput *in = &prog->ops[0].u.input;
        for (size_t f = 0; f < in->ninputs; f++) {
            int L = in->inputs[f].letter;
            int nread = 0;
            int nend = 0;
            int endt = -1;
            int mismatch = 0;
            for (size_t i = 0; i < prog->nops; i++) {
                if (prog->ops[i].kind != FM_OP_READ_ITEM) {
                    continue;
                }
                if (prog->ops[i].u.read_item.file_letter != L) {
                    continue;
                }
                nread++;
                if (prog->ops[i].u.read_item.has_end) {
                    nend++;
                    if (endt < 0) {
                        endt = prog->ops[i].u.read_item.end_target;
                    } else if (endt != prog->ops[i].u.read_item.end_target) {
                        mismatch = 1;
                    }
                }
            }
            if (nread == 0) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM4050", in->inputs[f].span, 0,
                            "each input file in INPUT must have at least one "
                            "READ-ITEM operation",
                            "Appendix A READ-ITEM special note 1, printed page "
                            "97.");
            }
            if (nend == 0) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM4051", in->inputs[f].span, 0,
                            "at least one READ-ITEM for each input file must "
                            "include IF END OF DATA",
                            "Appendix A READ-ITEM special note 2.");
            }
            if (mismatch) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM4052", in->inputs[f].span, 0,
                            "all IF END OF DATA paths for one input file must "
                            "name the same operation",
                            "Appendix A READ-ITEM special note 3.");
            }
        }
    }
    fm_x1_validate(prog, diags);
    return !fm_diags_has_error(diags);
}
