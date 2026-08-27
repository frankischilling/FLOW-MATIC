#include "internal.h"
#include "flowmatic/ast.h"
#include "flowmatic/compiler.h"

static int fm_x1_header_op(const FmWord *w, int *op)
{
    char buf[13];
    fm_word_to_cstr(w, buf);
    if (strncmp(buf, "X-1", 3) != 0 && strncmp(buf, "X-I", 3) != 0) {
        return 0;
    }
    unsigned v = 0;
    if (!fm_parse_uint(buf + 9, 3, &v)) {
        return 0;
    }
    *op = (int)v;
    return 1;
}

static int fm_is_end_sub(const FmWord *w)
{
    return fm_word_is_kw(w, "END SUBROUTN");
}

static int fm_is_constants(const FmWord *w)
{
    return fm_word_is_kw(w, "CONSTANTS");
}

static int fm_is_code_consts(const FmWord *w)
{
    /* Figure 44 uses CODE CONSTS (12 positions). Appendix C prose says
       CODE CONSTANTS, which does not fit a UNIVAC word. */
    return fm_word_is_kw(w, "CODE CONSTS") ||
           fm_word_is_kw(w, "CODE CONSTANTS");
}

static int fm_is_j_opline(const FmWord *w)
{
    /* Second half-word OP.nnn, first half spaces or zeros.
       Printed page 106-107, Figure 44. */
    int i;
    for (i = 0; i < 6; i++) {
        if (w->pos[i] != ' ' && w->pos[i] != '0' && w->pos[i] != '-') {
            /* might still be OP in first half; fall through */
        }
    }
    /* look for OP. */
    for (i = 0; i <= 6; i++) {
        if (w->pos[i] == 'O' && w->pos[i + 1] == 'P' && w->pos[i + 2] == '.') {
            unsigned v = 0;
            if (fm_parse_uint((const char *)w->pos + i + 3, 3, &v)) {
                return 1;
            }
        }
    }
    return 0;
}

static int fm_x1_addr_letter(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'I') {
        return 1;
    }
    return ch == 'W' || ch == 'M' || ch == 'J' || ch == 'T';
}

bool fm_parse_x1_text(FmArena *arena, const FmSource *src, FmDiagList *diags,
                      FmX1Section *sections, size_t *nsections, size_t cap)
{
    (void)arena;
    /* Same line-oriented 12-word transport as Data Designs. */
    const char *p = src->bytes;
    size_t i = 0;
    unsigned line = 1;
    FmX1Section *cur = NULL;
    int mode = 0; /* 0 header-seek, 1 body, 2 consts, 3 code consts */
    *nsections = 0;
    while (i < src->size) {
        size_t ls = i;
        unsigned col = 1;
        while (i < src->size && p[i] != '\n' && p[i] != '\r') {
            i++;
        }
        size_t le = i;
        if (i < src->size && p[i] == '\r') {
            i++;
        }
        if (i < src->size && p[i] == '\n') {
            i++;
        }
        size_t a = ls;
        while (a < le && (p[a] == ' ' || p[a] == '\t')) {
            a++;
            col++;
        }
        if (a == le || p[a] == '#') {
            line++;
            continue;
        }
        size_t b = le;
        while (b > a && (p[b - 1] == ' ' || p[b - 1] == '\t')) {
            b--;
        }
        FmWord word;
        bool overflow = false;
        fm_word_from_text(&word, p + a, b - a, &overflow);
        FmSpan span;
        fm_span_init(&span, src->path, a, b, line, col, line,
                     col + (unsigned)(b > a ? b - a : 1));
        if (overflow) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM6101", span, -1,
                        "X-1 word exceeds 12 character positions", NULL);
        }
        int hop = 0;
        if (fm_x1_header_op(&word, &hop)) {
            if (*nsections >= cap) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM6102", span, hop,
                            "too many X-1 sections", NULL);
                return false;
            }
            cur = &sections[*nsections];
            memset(cur, 0, sizeof(*cur));
            cur->op_number = hop;
            cur->span = span;
            (*nsections)++;
            mode = 1;
            line++;
            continue;
        }
        if (!cur) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM6103", span, -1,
                        "X-1 body appeared before an operation-number header",
                        "Each section begins with X-1 followed by six spaces "
                        "and a three-digit operation number.");
            line++;
            continue;
        }
        if (cur->nlines >= 512) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM6104", span, cur->op_number,
                        "X-1 section is too long", NULL);
            line++;
            continue;
        }
        FmX1Line *L = &cur->lines[cur->nlines];
        L->word = word;
        L->span = span;
        L->m_addr = -1;
        if (fm_is_end_sub(&word)) {
            L->kind = FM_X1_END;
            cur->has_end = 1;
            cur->nlines++;
            /* extra END SUBROUTN copies fill the blockette */
            line++;
            continue;
        }
        if (fm_is_constants(&word)) {
            L->kind = FM_X1_CONSTANTS_TITLE;
            mode = 2;
            cur->nlines++;
            line++;
            continue;
        }
        if (fm_is_code_consts(&word)) {
            L->kind = FM_X1_CODE_TITLE;
            mode = 3;
            cur->nlines++;
            line++;
            continue;
        }
        if (fm_is_j_opline(&word)) {
            L->kind = FM_X1_J_OPLINE;
            cur->nlines++;
            line++;
            continue;
        }
        if (mode == 2) {
            L->kind = FM_X1_CONSTANT;
            L->m_addr = cur->body_m_count + cur->const_count;
            cur->const_count++;
        } else if (mode == 3) {
            L->kind = FM_X1_CODE_CONSTANT;
            L->m_addr = cur->body_m_count + cur->const_count +
                        cur->code_const_count;
            cur->code_const_count++;
        } else {
            L->kind = FM_X1_BODY;
            L->m_addr = cur->body_m_count;
            cur->body_m_count++;
        }
        cur->nlines++;
        line++;
    }
    for (size_t s = 0; s < *nsections; s++) {
        if (sections[s].const_count + sections[s].code_const_count > 59) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM6105", sections[s].span,
                        sections[s].op_number,
                        "a single X-1 section may have at most 59 constants "
                        "and code constants together",
                        "Appendix C, printed page 107.");
        }
        if (!sections[s].has_end) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM6106", sections[s].span,
                        sections[s].op_number,
                        "X-1 section is missing END SUBROUTN",
                        "Write END SUBROUTN after the last line and in word 09 "
                        "of the blockette.");
        }
    }
    return !fm_diags_has_error(diags);
}

static int fm_x1_last_m(const FmX1Section *s)
{
    int last = -1;
    for (size_t i = 0; i < s->nlines; i++) {
        if (s->lines[i].m_addr > last) {
            last = s->lines[i].m_addr;
        }
    }
    return last;
}

void fm_x1_validate(const FmProgram *prog, FmDiagList *diags)
{
    int seen[FM_MAX_OPERATIONS];
    memset(seen, 0, sizeof seen);
    if (prog->nx1 > 0 && prog->x1 == NULL) {
        return;
    }
    if (prog->nops > 0 && prog->ops == NULL) {
        return;
    }
    for (size_t i = 0; i < prog->nx1; i++) {
        int n = prog->x1[i].op_number;
        if (n < 0 || n >= FM_MAX_OPERATIONS) {
            continue;
        }
        if (seen[n]) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM6201", prog->x1[i].span, n,
                        "duplicate X-1 section number",
                        "Each X-1 section header carries a unique operation "
                        "number.");
        }
        seen[n] = 1;
    }
    for (size_t o = 0; o < prog->nops; o++) {
        if (prog->ops[o].kind != FM_OP_X1) {
            continue;
        }
        int n = prog->ops[o].number;
        if (n >= 0 && n < FM_MAX_OPERATIONS && !seen[n]) {
            char msg[160];
            snprintf(msg, sizeof msg,
                     "operation %d is X-1, but no matching X-1 section is on "
                     "the input",
                     n);
            fm_diag_add(diags, FM_SEV_ERROR, "FM6202", prog->ops[o].span, n, msg,
                        "This is the CANNOT FIND X-1 ... SUBROUT ON INPUT TAPE "
                        "condition from Appendix C, printed page 108.");
        }
    }
    for (size_t i = 0; i < prog->nx1; i++) {
        const FmX1Section *s = &prog->x1[i];
        int lastm = fm_x1_last_m(s);
        for (size_t ln = 0; ln < s->nlines; ln++) {
            const FmX1Line *L = &s->lines[ln];
            if (L->kind == FM_X1_END || L->kind == FM_X1_CONSTANTS_TITLE ||
                L->kind == FM_X1_CODE_TITLE || L->kind == FM_X1_J_OPLINE ||
                L->kind == FM_X1_HEADER) {
                continue;
            }
            unsigned char letters[2] = {L->word.pos[2], L->word.pos[8]};
            int is_j = 0;
            for (int h = 0; h < 2; h++) {
                unsigned char ch = letters[h];
                if (ch >= '0' && ch <= '9') {
                    continue;
                }
                if (ch == ' ' || ch == 0) {
                    continue;
                }
                if (ch >= 'A' && ch <= 'Z') {
                    if (!fm_x1_addr_letter(ch)) {
                        char msg[192];
                        snprintf(msg, sizeof msg,
                                 "line M%03d of X-1 operation %d contains an "
                                 "alphabetic other than A-I, J, M, T, W",
                                 L->m_addr < 0 ? 0 : L->m_addr, s->op_number);
                        fm_diag_add(diags, FM_SEV_ERROR, "FM6203", L->span,
                                    s->op_number, msg,
                                    "Appendix C BAD ADDRESS printout, printed "
                                    "page 108.");
                    }
                    if (ch == 'J') {
                        is_j = 1;
                    }
                    if (ch == 'M') {
                        unsigned m = 0;
                        fm_parse_uint((const char *)L->word.pos + (h == 0 ? 3 : 9),
                                      3, &m);
                        if ((int)m > lastm) {
                            char msg[192];
                            snprintf(msg, sizeof msg,
                                     "line M%03d contains an M reference "
                                     "greater than the last M address of the "
                                     "section",
                                     L->m_addr < 0 ? 0 : L->m_addr);
                            fm_diag_add(diags, FM_SEV_ERROR, "FM6204", L->span,
                                        s->op_number, msg,
                                        "Appendix C WRONG REL AD, printed "
                                        "page 108.");
                        }
                    }
                    if (L->kind == FM_X1_CODE_CONSTANT &&
                        (ch == 'M' || ch == 'J')) {
                        fm_diag_add(diags, FM_SEV_ERROR, "FM6205", L->span,
                                    s->op_number,
                                    "code constants may not contain M or J; "
                                    "those words must stay in the body",
                                    "Appendix C CANNOT USE, printed page 108.");
                    }
                    if (ch >= 'A' && ch <= 'I') {
                        int found = 0;
                        for (size_t o = 0; o < prog->nops; o++) {
                            if (prog->ops[o].kind != FM_OP_INPUT) {
                                continue;
                            }
                            const FmOpInput *in = &prog->ops[o].u.input;
                            for (size_t f = 0; f < in->ninputs; f++) {
                                if (in->inputs[f].letter == (int)ch) {
                                    found = 1;
                                }
                            }
                            for (size_t f = 0; f < in->noutputs; f++) {
                                if (in->outputs[f].letter == (int)ch) {
                                    found = 1;
                                }
                            }
                        }
                        if (!found) {
                            fm_diag_add(diags, FM_SEV_ERROR, "FM6206", L->span,
                                        s->op_number,
                                        "X-1 address uses a file letter that "
                                        "is not assigned in INPUT",
                                        "Appendix C HALF WORD ADDRESS IN.");
                        }
                    }
                }
            }
            if (is_j) {
                if (ln + 1 >= s->nlines ||
                    s->lines[ln + 1].kind != FM_X1_J_OPLINE) {
                    fm_diag_add(diags, FM_SEV_ERROR, "FM6207", L->span,
                                s->op_number,
                                "a J address must be followed by a line with "
                                "OP.nnn in the proper half-word",
                                "Appendix C, printed pages 106 and 108.");
                }
            }
        }
    }
}

int fm_x1_relocate_word(FmWord *w, const FmX1Section *self, int start_m,
                        const FmProgram *prog, const int *op_start)
{
    /* Replace Mxxx with start_m+xxx encoded as 4-digit decimal in the
       address field. This is a modern relocation, not UNIVAC II packing. */
    int changed = 0;
    for (int h = 0; h < 2; h++) {
        int base = h * 6;
        unsigned char L = w->pos[base + 2];
        if (L == 'M') {
            unsigned m = 0;
            fm_parse_uint((const char *)w->pos + base + 3, 3, &m);
            unsigned abs = (unsigned)start_m + m;
            char tmp[4];
            snprintf(tmp, sizeof tmp, "%03u", abs % 1000u);
            w->pos[base + 2] = '0' + (unsigned char)((abs / 1000u) % 10u);
            memcpy(w->pos + base + 3, tmp, 3);
            changed = 1;
            (void)self;
            (void)prog;
            (void)op_start;
        }
    }
    return changed;
}
