#include "internal.h"
#include "flowmatic/ast.h"
#include "flowmatic/compiler.h"

typedef struct FmParser {
    FmArena *arena;
    const FmSource *src;
    const FmToken *toks;
    size_t ntoks;
    size_t pos;
    FmDiagList *diags;
    int recovering;
    int cur_op;
} FmParser;

static int fm_at_op_header(FmParser *p);

static const FmToken *fm_eof_token(void)
{
    static FmToken eof;
    eof.kind = FM_TOK_EOF;
    return &eof;
}

static const FmToken *fm_cur(FmParser *p)
{
    if (p->ntoks == 0 || p->toks == NULL) {
        return fm_eof_token();
    }
    if (p->pos >= p->ntoks) {
        return &p->toks[p->ntoks - 1u];
    }
    return &p->toks[p->pos];
}

static const FmToken *fm_la(FmParser *p, size_t k)
{
    if (p->ntoks == 0 || p->toks == NULL) {
        return fm_eof_token();
    }
    size_t i = p->pos + k;
    if (i >= p->ntoks) {
        return &p->toks[p->ntoks - 1u];
    }
    return &p->toks[i];
}

static int fm_at_eof(FmParser *p)
{
    return fm_cur(p)->kind == FM_TOK_EOF;
}

static int fm_is_kw(const FmToken *t, const char *kw)
{
    return t->kind == FM_TOK_WORD && fm_word_is_kw(&t->lexeme, kw);
}

static int fm_at_kw(FmParser *p, const char *kw)
{
    return fm_is_kw(fm_cur(p), kw);
}

static void fm_error(FmParser *p, const char *code, const char *msg,
                     const char *fix)
{
    fm_diag_add(p->diags, FM_SEV_ERROR, code, fm_cur(p)->span, p->cur_op, msg,
                fix);
}

static void fm_advance(FmParser *p)
{
    if (!fm_at_eof(p)) {
        p->pos++;
    }
}

static int fm_accept_kw(FmParser *p, const char *kw)
{
    if (fm_at_kw(p, kw)) {
        fm_advance(p);
        return 1;
    }
    return 0;
}

static int fm_accept_kind(FmParser *p, FmTokKind k)
{
    if (fm_cur(p)->kind == k) {
        fm_advance(p);
        return 1;
    }
    return 0;
}

static void fm_skip_to_period(FmParser *p)
{
    p->recovering = 1;
    while (!fm_at_eof(p) && fm_cur(p)->kind != FM_TOK_PERIOD) {
        if (fm_at_op_header(p)) {
            break;
        }
        fm_advance(p);
    }
    if (fm_cur(p)->kind == FM_TOK_PERIOD) {
        fm_advance(p);
    }
    p->recovering = 0;
}

static int fm_expect_kw(FmParser *p, const char *kw, const char *code)
{
    if (fm_accept_kw(p, kw)) {
        return 1;
    }
    char msg[192];
    snprintf(msg, sizeof msg, "expected %s, found a different word", kw);
    fm_error(p, code, msg, NULL);
    return 0;
}

static int fm_group_inner(const FmToken *t, char *inner, size_t inner_cap,
                          size_t *inner_len)
{
    if (t->kind != FM_TOK_GROUP) {
        return 0;
    }
    size_t n = fm_word_used_len(&t->lexeme);
    if (n < 3) {
        return 0;
    }
    size_t il = n - 2u;
    if (il >= inner_cap) {
        return 0;
    }
    memcpy(inner, t->lexeme.pos + 1, il);
    inner[il] = '\0';
    if (inner_len) {
        *inner_len = il;
    }
    return 1;
}

static int fm_parse_op_header(FmParser *p, int *number, FmSpan *span)
{
    const FmToken *t = fm_cur(p);
    if (t->kind != FM_TOK_GROUP) {
        return 0;
    }
    char inner[16];
    size_t il = 0;
    if (!fm_group_inner(t, inner, sizeof inner, &il)) {
        return 0;
    }
    unsigned v = 0;
    if (!fm_parse_uint(inner, il, &v)) {
        return 0;
    }
    if (v > 998u) {
        fm_error(p, "FM2108",
                 "operation numbers run from 0 through 998 (at most 999 "
                 "operations)",
                 "Renumber the operations so they form a continuous sequence "
                 "starting at zero.");
    }
    *number = (int)v;
    *span = t->span;
    fm_advance(p);
    return 1;
}

static int fm_at_op_header(FmParser *p)
{
    const FmToken *t = fm_cur(p);
    if (t->kind != FM_TOK_GROUP) {
        return 0;
    }
    char inner[16];
    size_t il = 0;
    if (!fm_group_inner(t, inner, sizeof inner, &il)) {
        return 0;
    }
    unsigned v = 0;
    return fm_parse_uint(inner, il, &v);
}

static int fm_parse_file_letter_token(const FmToken *t, int *letter)
{
    size_t n = fm_word_used_len(&t->lexeme);
    if (t->kind == FM_TOK_WORD && n == 1) {
        unsigned char c = t->lexeme.pos[0];
        if ((c >= 'A' && c <= 'I') || c == 'W') {
            *letter = (int)c;
            return 1;
        }
    }
    if (t->kind == FM_TOK_GROUP) {
        char inner[8];
        size_t il = 0;
        if (fm_group_inner(t, inner, sizeof inner, &il) && il == 1) {
            unsigned char c = (unsigned char)inner[0];
            if ((c >= 'A' && c <= 'I') || c == 'W') {
                *letter = (int)c;
                return 1;
            }
        }
    }
    return 0;
}

static int fm_parse_file_letter(FmParser *p, int *letter, const char *what)
{
    const FmToken *t = fm_cur(p);
    if (fm_parse_file_letter_token(t, letter)) {
        fm_advance(p);
        return 1;
    }
    char msg[160];
    snprintf(msg, sizeof msg, "expected a file letter A-I or W for %s", what);
    fm_error(p, "FM2101", msg,
             "Use the letter assigned in the INPUT statement, or W for "
             "W-storage.");
    return 0;
}

static int fm_parse_field_ref(FmParser *p, FmFieldRef *ref)
{
    memset(ref, 0, sizeof(*ref));
    const FmToken *name = fm_cur(p);
    if (name->kind != FM_TOK_WORD) {
        fm_error(p, "FM2102", "expected a field name",
                 "Field names are at most 12 positions and are followed by a "
                 "file letter in parentheses, for example PRODUCT-NO (A).");
        return 0;
    }
    ref->name = name->lexeme;
    ref->span = name->span;
    fm_advance(p);
    const FmToken *g = fm_cur(p);
    if (g->kind != FM_TOK_GROUP) {
        fm_error(p, "FM2103",
                 "a field name must be followed immediately by its file letter "
                 "in parentheses",
                 "Write the file letter as a separate word, for example "
                 "UNIT-PRICE (B).");
        ref->file_letter = -1;
        return 0;
    }
    int letter = -1;
    char inner[8];
    size_t il = 0;
    if (!fm_group_inner(g, inner, sizeof inner, &il) || il != 1) {
        fm_error(p, "FM2103", "the parenthesized file letter is malformed",
                 "Use a single letter A-I or W inside the parentheses.");
        fm_advance(p);
        return 0;
    }
    letter = (int)(unsigned char)inner[0];
    if (!((letter >= 'A' && letter <= 'I') || letter == 'W')) {
        fm_error(p, "FM2103", "the parenthesized file letter is not A-I or W",
                 "Use the letter assigned in INPUT, or W for W-storage.");
    }
    ref->file_letter = letter;
    ref->span = fm_span_merge(ref->span, g->span);
    fm_advance(p);
    return 1;
}

static int fm_parse_opnum_kw(FmParser *p, int *n)
{
    if (!fm_expect_kw(p, "OPERATION", "FM2104")) {
        return 0;
    }
    const FmToken *t = fm_cur(p);
    if (t->kind != FM_TOK_NUMBER) {
        fm_error(p, "FM2104", "expected an operation number",
                 "Operation numbers are numeric and are not parenthesized in "
                 "the body of a statement.");
        return 0;
    }
    unsigned v = 0;
    size_t ln = fm_word_used_len(&t->lexeme);
    if (!fm_parse_uint((const char *)t->lexeme.pos, ln, &v) || v > 998u) {
        fm_error(p, "FM2104", "operation number is out of range", NULL);
        fm_advance(p);
        return 0;
    }
    *n = (int)v;
    fm_advance(p);
    return 1;
}

static int fm_parse_go_to_op(FmParser *p, int *n)
{
    if (!fm_expect_kw(p, "GO", "FM2105")) {
        return 0;
    }
    if (!fm_expect_kw(p, "TO", "FM2105")) {
        return 0;
    }
    return fm_parse_opnum_kw(p, n);
}

static int fm_parse_if_cond(FmParser *p, FmCondKind *ck)
{
    if (!fm_accept_kw(p, "IF")) {
        return 0;
    }
    if (fm_accept_kw(p, "EQUAL")) {
        *ck = FM_COND_EQUAL;
        return 1;
    }
    if (fm_accept_kw(p, "GREATER")) {
        *ck = FM_COND_GREATER;
        return 1;
    }
    if (fm_accept_kw(p, "LESS")) {
        *ck = FM_COND_LESS;
        return 1;
    }
    if (fm_accept_kw(p, "UNEQUAL")) {
        *ck = FM_COND_UNEQUAL;
        return 1;
    }
    fm_error(p, "FM2106",
             "expected EQUAL, GREATER, LESS, or UNEQUAL after IF",
             "Appendix A lists the allowed TEST and COMPARE conditions.");
    return 0;
}

static int fm_file_dash_letter(const FmWord *w, int *letter)
{
    /* FILE-A ... FILE-I */
    char buf[13];
    fm_word_to_cstr(w, buf);
    if (strncmp(buf, "FILE-", 5) != 0) {
        return 0;
    }
    if (buf[5] == '\0' || buf[6] != ' ') {
        /* need exactly FILE-X padded */
    }
    unsigned char c = (unsigned char)buf[5];
    if (c < 'A' || c > 'I') {
        return 0;
    }
    if (fm_word_used_len(w) != 6) {
        return 0;
    }
    *letter = (int)c;
    return 1;
}

static int fm_parse_servo(FmParser *p, FmServoSpec *s)
{
    memset(s, 0, sizeof(*s));
    int plural = 0;
    if (fm_accept_kw(p, "SERVOS")) {
        plural = 1;
    } else if (fm_accept_kw(p, "SERVO")) {
        plural = 0;
    } else {
        return 1; /* omitted */
    }
    const FmToken *t = fm_cur(p);
    if (t->kind != FM_TOK_NUMBER) {
        fm_error(p, "FM2110", "expected a servo number after SERVO or SERVOS",
                 "If servos are omitted, the compiler assigns them.");
        return 0;
    }
    unsigned v = 0;
    fm_parse_uint((const char *)t->lexeme.pos, fm_word_used_len(&t->lexeme), &v);
    s->servo[0] = (int)v;
    s->nservos = 1;
    fm_advance(p);
    if (fm_accept_kind(p, FM_TOK_COMMA)) {
        t = fm_cur(p);
        if (t->kind != FM_TOK_NUMBER) {
            fm_error(p, "FM2110", "expected a second servo number after the comma",
                     NULL);
            return 0;
        }
        fm_parse_uint((const char *)t->lexeme.pos, fm_word_used_len(&t->lexeme),
                      &v);
        s->servo[1] = (int)v;
        s->nservos = 2;
        fm_advance(p);
        if (!plural) {
            fm_diag_add(p->diags, FM_SEV_WARNING, "FM2111", t->span, p->cur_op,
                        "two servo numbers were given after SERVO; Appendix A "
                        "writes SERVOS for a pair",
                        "Use SERVOS when two Uniservo numbers are listed.");
        }
    }
    return 1;
}

static int fm_parse_file_assign(FmParser *p, FmFileAssign *fa)
{
    memset(fa, 0, sizeof(*fa));
    if (fm_at_op_header(p)) {
        fm_error(p, "FM2120", "each statement must end with a period",
                 "Put a period after the last word of the operation. The "
                 "period is the only punctuation the compiler treats as "
                 "mandatory.");
        return 0;
    }
    const FmToken *name = fm_cur(p);
    if (name->kind != FM_TOK_WORD) {
        fm_error(p, "FM2112", "expected a file name",
                 "A file name may not begin with FILE-.");
        return 0;
    }
    fa->name = name->lexeme;
    fa->span = name->span;
    char buf[13];
    fm_word_to_cstr(&fa->name, buf);
    if (strncmp(buf, "FILE-", 5) == 0) {
        fm_error(p, "FM2113",
                 "the assigned file-name may not begin with the digits FILE-",
                 "Choose a name such as INVENTORY, then write FILE-A after it.");
    }
    fm_advance(p);
    const FmToken *fl = fm_cur(p);
    int letter = 0;
    if (!fm_file_dash_letter(&fl->lexeme, &letter)) {
        fm_error(p, "FM2114", "expected FILE- followed by a letter A-I",
                 "Example: INVENTORY FILE-A");
        return 0;
    }
    fa->letter = letter;
    fa->span = fm_span_merge(fa->span, fl->span);
    fm_advance(p);
    return fm_parse_servo(p, &fa->servos);
}

static int fm_parse_letter_list(FmParser *p, int *out, size_t cap, size_t *n)
{
    *n = 0;
    do {
        if (*n >= cap) {
            fm_error(p, "FM2115", "too many file letters in this list", NULL);
            return 0;
        }
        int L = 0;
        if (!fm_parse_file_letter(p, &L, "a file list")) {
            return 0;
        }
        out[(*n)++] = L;
        if (!fm_accept_kind(p, FM_TOK_COMMA)) {
            break;
        }
    } while (1);
    return 1;
}

static void fm_count_stmt_words(FmParser *p, size_t start_tok, size_t end_tok,
                                size_t *count)
{
    /* start_tok is the keyword after the operation number.
       end_tok is the period (excluded). */
    size_t n = 0;
    for (size_t i = start_tok; i < end_tok && i < p->ntoks; i++) {
        if (p->toks[i].kind != FM_TOK_EOF) {
            n++;
        }
    }
    *count = n;
}

static void fm_check_word_limit(FmParser *p, FmOperation *op, size_t kw_pos)
{
    size_t n = 0;
    for (size_t i = kw_pos; i < p->ntoks; i++) {
        FmTokKind k = p->toks[i].kind;
        if (k == FM_TOK_EOF || k == FM_TOK_PERIOD) {
            break;
        }
        if (k == FM_TOK_GROUP) {
            char inner[16];
            size_t il = 0;
            unsigned v = 0;
            if (fm_group_inner(&p->toks[i], inner, sizeof inner, &il) &&
                fm_parse_uint(inner, il, &v)) {
                break;
            }
        }
        n++;
    }
    op->stmt_word_count = n;
    if (n > (size_t)FM_MAX_STMT_WORDS) {
        char msg[160];
        snprintf(msg, sizeof msg,
                 "this statement has %zu code words; the limit is 60, counting "
                 "punctuation but not the operation number or the ending "
                 "period",
                 n);
        fm_diag_add(p->diags, FM_SEV_ERROR, "FM2121", op->span, op->number, msg,
                    "Split the work across more operations.");
    }
}

static int fm_finish_stmt(FmParser *p, FmOperation *op, size_t kw_pos)
{
    fm_check_word_limit(p, op, kw_pos);
    if (fm_at_op_header(p)) {
        fm_error(p, "FM2120", "each statement must end with a period",
                 "Put a period after the last word of the operation. The "
                 "period is the only punctuation the compiler treats as "
                 "mandatory.");
        return 0;
    }
    if (fm_cur(p)->kind != FM_TOK_PERIOD) {
        fm_error(p, "FM2120", "each statement must end with a period",
                 "Put a period after the last word of the operation. The "
                 "period is the only punctuation the compiler treats as "
                 "mandatory.");
        fm_skip_to_period(p);
        return 0;
    }
    size_t period_pos = p->pos;
    fm_count_stmt_words(p, kw_pos, period_pos, &op->stmt_word_count);
    op->span = fm_span_merge(op->span, fm_cur(p)->span);
    fm_advance(p);
    return 1;
}

static int fm_parse_input(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_INPUT;
    memset(&op->u.input, 0, sizeof(op->u.input));
    /* input files until semicolon OUTPUT */
    while (!fm_at_eof(p) && fm_cur(p)->kind != FM_TOK_SEMI &&
           fm_cur(p)->kind != FM_TOK_PERIOD) {
        if (op->u.input.ninputs >= 8) {
            fm_error(p, "FM2130",
                     "PRESELECTION allows up to eight input files; this INPUT "
                     "statement lists more",
                     NULL);
            break;
        }
        if (!fm_parse_file_assign(p, &op->u.input.inputs[op->u.input.ninputs])) {
            fm_skip_to_period(p);
            return 0;
        }
        op->u.input.ninputs++;
    }
    if (!fm_accept_kind(p, FM_TOK_SEMI) || !fm_accept_kw(p, "OUTPUT")) {
        fm_error(p, "FM2131", "INPUT must include ; OUTPUT and the output files",
                 "See Appendix A, printed page 96.");
        fm_skip_to_period(p);
        return 0;
    }
    while (!fm_at_eof(p) && fm_cur(p)->kind != FM_TOK_SEMI &&
           fm_cur(p)->kind != FM_TOK_PERIOD) {
        if (op->u.input.noutputs >= 8) {
            fm_error(p, "FM2132", "too many output files on the INPUT statement",
                     NULL);
            break;
        }
        if (!fm_parse_file_assign(p,
                                  &op->u.input.outputs[op->u.input.noutputs])) {
            fm_skip_to_period(p);
            return 0;
        }
        op->u.input.noutputs++;
    }
    while (fm_accept_kind(p, FM_TOK_SEMI)) {
        if (fm_accept_kw(p, "PRESELECTION")) {
            op->u.input.preselection = 1;
            continue;
        }
        if (fm_accept_kw(p, "HSP")) {
            if (!fm_parse_letter_list(p, op->u.input.hsp, 12,
                                      &op->u.input.nhsp)) {
                fm_skip_to_period(p);
                return 0;
            }
            continue;
        }
        if (fm_is_kw(fm_cur(p), "T/C")) {
            fm_advance(p);
            if (!fm_parse_letter_list(p, op->u.input.tc, 12, &op->u.input.ntc)) {
                fm_skip_to_period(p);
                return 0;
            }
            continue;
        }
        if (fm_accept_kw(p, "RERUN")) {
            if (fm_accept_kw(p, "ON")) {
                op->u.input.rerun = 1;
            } else if (fm_accept_kw(p, "FROM")) {
                op->u.input.rerun = 2;
            } else {
                fm_error(p, "FM2133", "RERUN must be followed by ON or FROM",
                         "Appendix A writes RERUN ON OUTPUT or RERUN FROM "
                         "OUTPUT.");
            }
            if (!fm_expect_kw(p, "OUTPUT", "FM2133")) {
                fm_skip_to_period(p);
                return 0;
            }
            if (!fm_parse_file_letter(p, &op->u.input.rerun_output, "RERUN")) {
                fm_skip_to_period(p);
                return 0;
            }
            continue;
        }
        fm_error(p, "FM2134",
                 "unrecognized option on the INPUT statement",
                 "Documented options are PRESELECTION, HSP, T/C, and RERUN.");
        fm_skip_to_period(p);
        return 0;
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_compare(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_COMPARE;
    memset(&op->u.compare, 0, sizeof(op->u.compare));
    if (!fm_parse_field_ref(p, &op->u.compare.left)) {
        fm_skip_to_period(p);
        return 0;
    }
    if (!fm_expect_kw(p, "WITH", "FM2140")) {
        fm_skip_to_period(p);
        return 0;
    }
    if (!fm_parse_field_ref(p, &op->u.compare.right)) {
        fm_skip_to_period(p);
        return 0;
    }
    int saw_otherwise = 0;
    while (fm_accept_kind(p, FM_TOK_SEMI)) {
        FmCondKind ck = FM_COND_NONE;
        if (fm_accept_kw(p, "OTHERWISE")) {
            ck = FM_COND_OTHERWISE;
            saw_otherwise = 1;
        } else if (!fm_parse_if_cond(p, &ck)) {
            fm_skip_to_period(p);
            return 0;
        }
        if (ck == FM_COND_LESS || ck == FM_COND_UNEQUAL) {
            fm_error(p, "FM2141",
                     "COMPARE may use IF EQUAL and IF GREATER, not IF LESS or "
                     "IF UNEQUAL",
                     "LESS and UNEQUAL belong to TEST. See Appendix A, printed "
                     "pages 95 and 98.");
        }
        int tgt = 0;
        if (!fm_parse_go_to_op(p, &tgt)) {
            fm_skip_to_period(p);
            return 0;
        }
        if (op->u.compare.nbranches >= 4) {
            fm_error(p, "FM2142", "too many COMPARE branches", NULL);
            break;
        }
        FmBranch *b = &op->u.compare.branches[op->u.compare.nbranches++];
        b->cond = ck;
        b->target = tgt;
        b->span = fm_cur(p)->span;
        if (saw_otherwise) {
            break;
        }
    }
    if (!saw_otherwise) {
        fm_error(p, "FM2143",
                 "COMPARE must end with OTHERWISE GO TO OPERATION n",
                 "Appendix A requires OTHERWISE as the last branch.");
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_test(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_TEST;
    memset(&op->u.test, 0, sizeof(op->u.test));
    if (!fm_parse_field_ref(p, &op->u.test.field)) {
        fm_skip_to_period(p);
        return 0;
    }
    if (!fm_expect_kw(p, "AGAINST", "FM2150")) {
        fm_skip_to_period(p);
        return 0;
    }
    int saw_otherwise = 0;
    /* first test-value, then optional ; IF ... and ; AGAINST ... */
    if (fm_at_kw(p, "SPACE") || fm_at_kw(p, "SPACES")) {
        op->u.test.space_word = fm_at_kw(p, "SPACES") ? 2 : 1;
        fm_advance(p);
    } else if (fm_at_kw(p, "PERIOD") || fm_at_kw(p, "PERIODS")) {
        op->u.test.period_word = fm_at_kw(p, "PERIODS") ? 2 : 1;
        fm_advance(p);
    } else if (fm_cur(p)->kind == FM_TOK_WORD ||
               fm_cur(p)->kind == FM_TOK_NUMBER) {
        op->u.test.values[op->u.test.nvalues++] = fm_cur(p)->lexeme;
        fm_advance(p);
    } else {
        fm_error(p, "FM2151", "expected a test value after AGAINST",
                 "Use a 12-position constant, or SPACE, SPACES, PERIOD, or "
                 "PERIODS.");
        fm_skip_to_period(p);
        return 0;
    }
    while (fm_accept_kind(p, FM_TOK_SEMI)) {
        if (fm_accept_kw(p, "AGAINST")) {
            if (op->u.test.space_word || op->u.test.period_word) {
                fm_error(p, "FM2152",
                         "SPACE, SPACES, PERIOD, and PERIODS allow only one "
                         "test value",
                         "Appendix A, TEST special note 2, printed page 98.");
            }
            if (op->u.test.nvalues >= 12) {
                fm_error(p, "FM2153", "too many AGAINST values", NULL);
                fm_skip_to_period(p);
                return 0;
            }
            if (fm_cur(p)->kind == FM_TOK_WORD ||
                fm_cur(p)->kind == FM_TOK_NUMBER) {
                op->u.test.values[op->u.test.nvalues++] = fm_cur(p)->lexeme;
                fm_advance(p);
            } else {
                fm_error(p, "FM2151", "expected a test value after AGAINST",
                         NULL);
                fm_skip_to_period(p);
                return 0;
            }
            continue;
        }
        FmCondKind ck = FM_COND_NONE;
        if (fm_accept_kw(p, "OTHERWISE")) {
            ck = FM_COND_OTHERWISE;
            saw_otherwise = 1;
        } else if (!fm_parse_if_cond(p, &ck)) {
            fm_skip_to_period(p);
            return 0;
        }
        int tgt = 0;
        if (!fm_parse_go_to_op(p, &tgt)) {
            fm_skip_to_period(p);
            return 0;
        }
        if (op->u.test.nbranches >= 4) {
            break;
        }
        FmBranch *b = &op->u.test.branches[op->u.test.nbranches++];
        b->cond = ck;
        b->target = tgt;
        if (saw_otherwise) {
            break;
        }
    }
    if (!saw_otherwise) {
        fm_error(p, "FM2154",
                 "TEST requires OTHERWISE as the last phrase",
                 "The OTHERWISE phrase must appear and must be written last.");
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_jump(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_JUMP;
    if (!fm_expect_kw(p, "TO", "FM2160")) {
        fm_skip_to_period(p);
        return 0;
    }
    if (!fm_parse_opnum_kw(p, &op->u.jump.target)) {
        fm_skip_to_period(p);
        return 0;
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_execute(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_EXECUTE;
    op->u.execute.to_op = -1;
    if (!fm_parse_opnum_kw(p, &op->u.execute.from_op)) {
        fm_skip_to_period(p);
        return 0;
    }
    if (fm_accept_kw(p, "THROUGH")) {
        if (!fm_parse_opnum_kw(p, &op->u.execute.to_op)) {
            fm_skip_to_period(p);
            return 0;
        }
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_move(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_MOVE;
    memset(&op->u.move, 0, sizeof(op->u.move));
    int first = 1;
    do {
        if (!first && !fm_accept_kind(p, FM_TOK_SEMI)) {
            break;
        }
        first = 0;
        if (op->u.move.npairs >= 32) {
            fm_error(p, "FM2170", "too many MOVE pairs", NULL);
            break;
        }
        FmMovePair *pair = &op->u.move.pairs[op->u.move.npairs];
        memset(pair, 0, sizeof(*pair));
        if (!fm_parse_field_ref(p, &pair->src)) {
            fm_skip_to_period(p);
            return 0;
        }
        if (!fm_expect_kw(p, "TO", "FM2171")) {
            fm_skip_to_period(p);
            return 0;
        }
        do {
            if (pair->ndests >= 12) {
                fm_error(p, "FM2172", "too many MOVE destinations", NULL);
                break;
            }
            if (!fm_parse_field_ref(p, &pair->dests[pair->ndests].field)) {
                fm_skip_to_period(p);
                return 0;
            }
            pair->ndests++;
            if (!fm_accept_kind(p, FM_TOK_COMMA)) {
                break;
            }
        } while (1);
        op->u.move.npairs++;
    } while (fm_cur(p)->kind == FM_TOK_SEMI);
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_read_item(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_READ_ITEM;
    memset(&op->u.read_item, 0, sizeof(op->u.read_item));
    if (!fm_parse_file_letter(p, &op->u.read_item.file_letter, "READ-ITEM")) {
        fm_skip_to_period(p);
        return 0;
    }
    if (fm_accept_kind(p, FM_TOK_SEMI)) {
        if (!fm_expect_kw(p, "IF", "FM2180") || !fm_expect_kw(p, "END", "FM2180") ||
            !fm_expect_kw(p, "OF", "FM2180") || !fm_expect_kw(p, "DATA", "FM2180")) {
            fm_skip_to_period(p);
            return 0;
        }
        op->u.read_item.has_end = 1;
        if (!fm_parse_go_to_op(p, &op->u.read_item.end_target)) {
            fm_skip_to_period(p);
            return 0;
        }
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_rewind(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_REWIND;
    memset(&op->u.rewind, 0, sizeof(op->u.rewind));
    if (!fm_parse_letter_list(p, op->u.rewind.files, 12, &op->u.rewind.nfiles)) {
        fm_skip_to_period(p);
        return 0;
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_close_out(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_CLOSE_OUT;
    memset(&op->u.close_out, 0, sizeof(op->u.close_out));
    if (fm_accept_kw(p, "FILES")) {
        op->u.close_out.files_kw = 2;
    } else if (fm_accept_kw(p, "FILE")) {
        op->u.close_out.files_kw = 1;
    }
    /* file letters, commas optional per punctuation rule */
    while (!fm_at_eof(p) && fm_cur(p)->kind != FM_TOK_PERIOD) {
        if (fm_accept_kind(p, FM_TOK_COMMA)) {
            continue;
        }
        int L = 0;
        if (!fm_parse_file_letter_token(fm_cur(p), &L)) {
            fm_error(p, "FM2190", "expected a file letter in CLOSE-OUT",
                     "Appendix A: CLOSE-OUT [FILE|FILES] f1 [f2 ...].");
            fm_skip_to_period(p);
            return 0;
        }
        if (op->u.close_out.nfiles >= 12) {
            break;
        }
        op->u.close_out.files[op->u.close_out.nfiles++] = L;
        fm_advance(p);
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_set(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_SET;
    memset(&op->u.set, 0, sizeof(op->u.set));
    int first = 1;
    do {
        if (!first) {
            if (!fm_accept_kind(p, FM_TOK_COMMA)) {
                break;
            }
        }
        first = 0;
        if (op->u.set.npairs >= 16) {
            break;
        }
        int from = 0, to = 0;
        if (!fm_parse_opnum_kw(p, &from)) {
            fm_skip_to_period(p);
            return 0;
        }
        if (!fm_expect_kw(p, "TO", "FM2195") || !fm_expect_kw(p, "GO", "FM2195") ||
            !fm_expect_kw(p, "TO", "FM2195")) {
            fm_skip_to_period(p);
            return 0;
        }
        if (!fm_parse_opnum_kw(p, &to)) {
            fm_skip_to_period(p);
            return 0;
        }
        FmSetPair *sp = &op->u.set.pairs[op->u.set.npairs++];
        sp->from_op = from;
        sp->to_op = to;
        sp->span = fm_cur(p)->span;
    } while (fm_cur(p)->kind == FM_TOK_COMMA);
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_write_item(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_WRITE_ITEM;
    if (!fm_parse_file_letter(p, &op->u.write_item.file_letter, "WRITE-ITEM")) {
        fm_skip_to_period(p);
        return 0;
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_transfer(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_TRANSFER;
    memset(&op->u.transfer, 0, sizeof(op->u.transfer));
    /* sub-item-name IN f  or  f */
    const FmToken *t = fm_cur(p);
    int letter = 0;
    if (fm_parse_file_letter_token(t, &letter) &&
        fm_word_used_len(&t->lexeme) == 1) {
        op->u.transfer.src_file = letter;
        fm_advance(p);
        op->u.transfer.has_src_sub = 0;
    } else if (t->kind == FM_TOK_WORD) {
        op->u.transfer.src_sub = t->lexeme;
        op->u.transfer.has_src_sub = 1;
        fm_advance(p);
        if (!fm_expect_kw(p, "IN", "FM2200")) {
            fm_skip_to_period(p);
            return 0;
        }
        if (!fm_parse_file_letter(p, &op->u.transfer.src_file, "TRANSFER source")) {
            fm_skip_to_period(p);
            return 0;
        }
    } else {
        fm_error(p, "FM2200", "malformed TRANSFER source",
                 "Use a file letter or sub-item-name IN file-letter.");
        fm_skip_to_period(p);
        return 0;
    }
    if (!fm_expect_kw(p, "TO", "FM2201")) {
        fm_skip_to_period(p);
        return 0;
    }
    t = fm_cur(p);
    if (fm_parse_file_letter_token(t, &letter) &&
        fm_word_used_len(&t->lexeme) == 1 && !fm_is_kw(fm_la(p, 1), "IN")) {
        op->u.transfer.dst_file = letter;
        fm_advance(p);
        op->u.transfer.has_dst_sub = 0;
    } else if (t->kind == FM_TOK_WORD) {
        op->u.transfer.dst_sub = t->lexeme;
        op->u.transfer.has_dst_sub = 1;
        fm_advance(p);
        if (!fm_expect_kw(p, "IN", "FM2201")) {
            fm_skip_to_period(p);
            return 0;
        }
        if (!fm_parse_file_letter(p, &op->u.transfer.dst_file,
                                 "TRANSFER destination")) {
            fm_skip_to_period(p);
            return 0;
        }
    } else {
        fm_error(p, "FM2201", "malformed TRANSFER destination", NULL);
        fm_skip_to_period(p);
        return 0;
    }
    if (op->u.transfer.has_src_sub && op->u.transfer.has_dst_sub) {
        op->u.transfer.kind = FM_XFER_SUB_TO_SUB;
    } else if (op->u.transfer.has_src_sub) {
        op->u.transfer.kind = FM_XFER_SUB_TO_ITEM;
    } else if (op->u.transfer.has_dst_sub) {
        op->u.transfer.kind = FM_XFER_ITEM_TO_SUB;
    } else {
        op->u.transfer.kind = FM_XFER_ITEM_TO_ITEM;
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_stop(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_STOP;
    if (!fm_finish_stmt(p, op, kw_pos)) {
        return 0;
    }
    const FmToken *t = fm_cur(p);
    char inner[16];
    size_t il = 0;
    if (t->kind != FM_TOK_GROUP || !fm_group_inner(t, inner, sizeof inner, &il) ||
        !fm_cstr_eq(inner, "END")) {
        fm_error(p, "FM2210",
                 "STOP must be followed by the word END in parentheses",
                 "Write STOP . (END) as the highest-numbered operation.");
        return 0;
    }
    op->span = fm_span_merge(op->span, t->span);
    fm_advance(p);
    return 1;
}

static int fm_parse_x1_op(FmParser *p, FmOperation *op, size_t kw_pos)
{
    op->kind = FM_OP_X1;
    memset(&op->u.x1, 0, sizeof(op->u.x1));
    while (!fm_at_eof(p) && fm_cur(p)->kind != FM_TOK_PERIOD) {
        if (op->u.x1.nenglish >= 60) {
            break;
        }
        op->u.x1.english[op->u.x1.nenglish++] = fm_cur(p)->lexeme;
        fm_advance(p);
    }
    return fm_finish_stmt(p, op, kw_pos);
}

static int fm_parse_operation(FmParser *p, FmOperation *op)
{
    memset(op, 0, sizeof(*op));
    FmSpan hspan;
    int num = 0;
    if (!fm_parse_op_header(p, &num, &hspan)) {
        fm_error(p, "FM2001",
                 "expected an operation number in parentheses, such as (0)",
                 "Label each statement with its operation number.");
        fm_skip_to_period(p);
        return 0;
    }
    op->number = num;
    op->span = hspan;
    p->cur_op = num;
    size_t kw_pos = p->pos;
    if (fm_accept_kw(p, "INPUT")) {
        return fm_parse_input(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "COMPARE")) {
        return fm_parse_compare(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "TEST")) {
        return fm_parse_test(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "JUMP")) {
        return fm_parse_jump(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "EXECUTE")) {
        return fm_parse_execute(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "MOVE")) {
        return fm_parse_move(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "READ-ITEM")) {
        return fm_parse_read_item(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "REWIND")) {
        return fm_parse_rewind(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "CLOSE-OUT")) {
        return fm_parse_close_out(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "SET")) {
        return fm_parse_set(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "WRITE-ITEM")) {
        return fm_parse_write_item(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "TRANSFER")) {
        return fm_parse_transfer(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "STOP")) {
        return fm_parse_stop(p, op, kw_pos);
    }
    if (fm_accept_kw(p, "X-1") || fm_accept_kw(p, "X-I")) {
        return fm_parse_x1_op(p, op, kw_pos);
    }
    char buf[13];
    fm_word_to_cstr(&fm_cur(p)->lexeme, buf);
    char msg[192];
    snprintf(msg, sizeof msg,
             "'%s' is not a documented FLOW-MATIC function in this subset",
             buf);
    fm_error(p, "FM2099", msg,
             "The 1958 manual lists CLOSE-OUT, COMPARE, EXECUTE, INPUT, JUMP, "
             "MOVE, READ-ITEM, REWIND, SET, STOP, TEST, TRANSFER, WRITE-ITEM, "
             "and X-1. The function list on printed page 92 is not complete, "
             "so an undocumented word is rejected rather than guessed.");
    fm_skip_to_period(p);
    op->kind = FM_OP_UNKNOWN;
    return 0;
}

bool fm_parse_program(FmArena *arena, const FmSource *src, FmDiagList *diags,
                      FmProgram *prog)
{
    FmLexResult lex;
    if (!fm_lex(arena, src, diags, &lex) && fm_diags_has_error(diags)) {
        /* still try to parse if we only had overlong words */
    }
    FmParser p;
    memset(&p, 0, sizeof(p));
    p.arena = arena;
    p.src = src;
    p.toks = lex.tokens;
    p.ntoks = lex.ntokens;
    p.diags = diags;
    p.cur_op = -1;
    prog->has_code = 1;
    if (!lex.tokens || lex.ntokens == 0) {
        return false;
    }
    while (!fm_at_eof(&p)) {
        if (prog->nops >= FM_MAX_OPERATIONS) {
            fm_error(&p, "FM2002",
                     "more than 999 operations",
                     "The operations are written in unbroken numeric sequence, "
                     "at most 999 of them.");
            break;
        }
        if (!fm_program_reserve_ops(arena, prog, prog->nops + 1u)) {
            fm_error(&p, "FM0003", "out of memory while parsing operations",
                     NULL);
            break;
        }
        FmOperation op;
        memset(&op, 0, sizeof op);
        int ok = fm_parse_operation(&p, &op);
        prog->ops[prog->nops++] = op;
        (void)ok;
    }
    return !fm_diags_has_error(diags);
}

const char *fm_op_kind_name(FmOpKind k)
{
    switch (k) {
    case FM_OP_INPUT:
        return "INPUT";
    case FM_OP_CLOSE_OUT:
        return "CLOSE-OUT";
    case FM_OP_COMPARE:
        return "COMPARE";
    case FM_OP_EXECUTE:
        return "EXECUTE";
    case FM_OP_JUMP:
        return "JUMP";
    case FM_OP_MOVE:
        return "MOVE";
    case FM_OP_READ_ITEM:
        return "READ-ITEM";
    case FM_OP_REWIND:
        return "REWIND";
    case FM_OP_SET:
        return "SET";
    case FM_OP_STOP:
        return "STOP";
    case FM_OP_TEST:
        return "TEST";
    case FM_OP_TRANSFER:
        return "TRANSFER";
    case FM_OP_WRITE_ITEM:
        return "WRITE-ITEM";
    case FM_OP_X1:
        return "X-1";
    default:
        return "UNKNOWN";
    }
}
