#include "internal.h"
#include "flowmatic/ast.h"
#include "flowmatic/compiler.h"

typedef struct FmWordSrc {
    FmWord word;
    FmSpan span;
} FmWordSrc;

static int fm_title_eq(const FmWord *w, const char *title)
{
    return fm_word_is_kw(w, title);
}

static int fm_collect_words(FmArena *arena, const FmSource *src,
                            FmDiagList *diags, FmWordSrc **out, size_t *n_out)
{
    /* Modern transport: one 12-position word per non-empty line.
       '#' starts a comment to end of line and is not historical syntax. */
    size_t cap = 64;
    FmWordSrc *ws = (FmWordSrc *)fm_arena_calloc(arena, cap, sizeof(FmWordSrc));
    if (!ws) {
        return 0;
    }
    size_t n = 0;
    const char *p = src->bytes;
    size_t i = 0;
    unsigned line = 1;
    while (i < src->size) {
        unsigned col = 1;
        size_t line_start = i;
        while (i < src->size && p[i] != '\n' && p[i] != '\r') {
            i++;
        }
        size_t line_end = i;
        if (i < src->size && p[i] == '\r') {
            i++;
        }
        if (i < src->size && p[i] == '\n') {
            i++;
        }
        size_t ls = line_start;
        while (ls < line_end && (p[ls] == ' ' || p[ls] == '\t')) {
            ls++;
            col++;
        }
        if (ls == line_end) {
            /* Empty lines are skipped. A line of only spaces is a 12-position
               space word, used for single-reel sentinels and blank key slots. */
            if (line_end > line_start) {
                if (n >= cap) {
                    size_t ncap = cap * 2u;
                    FmWordSrc *nw =
                        (FmWordSrc *)fm_arena_calloc(arena, ncap, sizeof(FmWordSrc));
                    if (!nw) {
                        return 0;
                    }
                    memcpy(nw, ws, n * sizeof(FmWordSrc));
                    ws = nw;
                    cap = ncap;
                }
                fm_word_spaces(&ws[n].word);
                fm_span_init(&ws[n].span, src->path, line_start, line_end, line,
                             1, line, (unsigned)(line_end - line_start + 1u));
                n++;
            }
            line++;
            continue;
        }
        if (p[ls] == '#') {
            line++;
            continue;
        }
        size_t le = line_end;
        while (le > ls && (p[le - 1] == ' ' || p[le - 1] == '\t')) {
            le--;
        }
        size_t ln = le - ls;
        if (n >= cap) {
            size_t ncap = cap * 2u;
            FmWordSrc *nw =
                (FmWordSrc *)fm_arena_calloc(arena, ncap, sizeof(FmWordSrc));
            if (!nw) {
                return 0;
            }
            memcpy(nw, ws, n * sizeof(FmWordSrc));
            ws = nw;
            cap = ncap;
        }
        bool overflow = false;
        fm_word_from_text(&ws[n].word, p + ls, ln, &overflow);
        fm_span_init(&ws[n].span, src->path, ls, le, line, col, line,
                     col + (unsigned)(ln > 0 ? ln : 1));
        if (overflow) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM3101", ws[n].span, -1,
                        "Data Design word exceeds 12 character positions",
                        "Each Data Design packet word is a UNIVAC word of 12 "
                        "positions.");
        }
        n++;
        line++;
    }
    *out = ws;
    *n_out = n;
    return 1;
}

static int fm_right3(const FmWord *w, unsigned *v)
{
    return fm_parse_uint((const char *)w->pos + 9, 3, v);
}

static int fm_right1(const FmWord *w, unsigned *v)
{
    unsigned char c = w->pos[11];
    if (c < '0' || c > '9') {
        return 0;
    }
    *v = (unsigned)(c - '0');
    return 1;
}

static int fm_parse_descriptor(const FmWord *w, FmFieldDesc *f, FmDiagList *diags,
                              FmSpan span)
{
    /* OOOOOTPPSLNO : positions 0-4 zeros, 5=T, 6-7=PP, 8=S, 9=L, 10=N, 11=0 */
    f->type = (FmFieldType)0;
    f->dec_dir = FM_DEC_NONE;
    f->dec_n = 0;
    f->sign_pos = 0;
    f->left_pos = 0;
    f->length = 0;
    unsigned char T = w->pos[5];
    if (T == '1') {
        f->type = FM_FTYPE_ALPHA;
    } else if (T == '2') {
        f->type = FM_FTYPE_ALPHANUM;
    } else if (T == '3') {
        f->type = FM_FTYPE_NUMERIC;
    } else {
        fm_diag_add(diags, FM_SEV_ERROR, "FM3201", span, -1,
                    "field type T must be 1 (alphabetic), 2 (alpha-numeric), "
                    "or 3 (numeric)",
                    "See Field Design, printed pages 43-44 and Form 3.");
        return 0;
    }
    unsigned char p0 = w->pos[6];
    unsigned char p1 = w->pos[7];
    if (p0 == '0' && p1 == '0') {
        f->dec_dir = FM_DEC_AT_REF;
        f->dec_n = 0;
    } else if (fm_is_ignore_char(p0) && fm_is_ignore_char(p1)) {
        f->dec_dir = FM_DEC_NONE;
        f->dec_n = 0;
    } else if (p1 == 'L' || p1 == 'R') {
        int n = fm_radix36_value(p0);
        if (n < 1 || n > 35) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM3202", span, -1,
                        "decimal-point displacement n is out of range",
                        "n is 1-9 then A-Z for 10 through 35.");
            return 0;
        }
        f->dec_n = n;
        f->dec_dir = (p1 == 'L') ? FM_DEC_LEFT : FM_DEC_RIGHT;
    } else {
        fm_diag_add(diags, FM_SEV_ERROR, "FM3202", span, -1,
                    "PP must be 00, ignore-ignore, nL, or nR",
                    "Assumed decimal points may be at most 35 places left or "
                    "right of the reference point.");
        return 0;
    }
    unsigned char S = w->pos[8];
    if (fm_is_ignore_char(S)) {
        f->sign_pos = 0;
    } else {
        int sp = fm_digit_pos_value(S);
        if (sp < 1) {
            fm_diag_add(diags, FM_SEV_ERROR, "FM3203", span, -1,
                        "sign position S must be 1-9, A, B, C, or ignore",
                        NULL);
            return 0;
        }
        f->sign_pos = sp;
    }
    int L = fm_digit_pos_value(w->pos[9]);
    int N = fm_digit_pos_value(w->pos[10]);
    if (L < 1 || N < 1) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM3204", span, -1,
                    "L and N must be 1-9, A, B, or C (A=10, B=11, C=12)",
                    NULL);
        return 0;
    }
    f->left_pos = L;
    f->length = N;
    return 1;
}

bool fm_parse_data_design_text(FmArena *arena, const FmSource *src,
                               FmDiagList *diags, FmDataDesign *out)
{
    memset(out, 0, sizeof(*out));
    out->letter = -1;
    FmWordSrc *ws = NULL;
    size_t n = 0;
    if (!fm_collect_words(arena, src, diags, &ws, &n)) {
        return false;
    }
    if (n == 0) {
        FmSpan sp;
        fm_span_init(&sp, src->path, 0, 0, 1, 1, 1, 1);
        fm_diag_add(diags, FM_SEV_ERROR, "FM3100", sp, -1,
                    "Data Design is empty", NULL);
        return false;
    }
    size_t i = 0;
    if (!fm_title_eq(&ws[i].word, "NAME OF FILE")) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM3110", ws[i].span, -1,
                    "Data Design must begin with NAME OF FILE",
                    "Printed page 38: the programmer writes the file name, "
                    "which may not start with FILE.");
        return false;
    }
    i++;
    if (i >= n) {
        return false;
    }
    out->file_name = ws[i].word;
    out->span = fm_span_merge(ws[0].span, ws[i].span);
    char nbuf[13];
    fm_word_to_cstr(&out->file_name, nbuf);
    if (strncmp(nbuf, "FILE-", 5) == 0) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM3111", ws[i].span, -1,
                    "the assigned file-name may not begin with FILE-", NULL);
    }
    if (fm_word_is_kw(&out->file_name, "W-STORAGE")) {
        out->is_wstorage = 1;
    }
    i++;
    int saw_end = 0;
    int in_fields = 0;
    while (i < n) {
        const FmWord *w = &ws[i].word;
        if (fm_title_eq(w, "END FILE DES")) {
            saw_end = 1;
            /* remaining copies and zeros are allowed */
            i++;
            continue;
        }
        if (fm_word_is_zeros(w) || fm_word_is_spaces(w)) {
            i++;
            continue;
        }
        if (fm_title_eq(w, "FILE DESIGN")) {
            out->has_file_design = 1;
            i++;
            if (i < n && fm_word_is_spaces(&ws[i].word)) {
                i++;
            }
            continue;
        }
        if (fm_title_eq(w, "ITEM DESIGN")) {
            out->item.present = 1;
            i++;
            if (i < n && fm_word_is_spaces(&ws[i].word)) {
                i++;
            }
            continue;
        }
        if (fm_title_eq(w, "FIELD DESIGN")) {
            in_fields = 1;
            i++;
            if (i < n && fm_word_is_spaces(&ws[i].word)) {
                i++;
            }
            continue;
        }
        if (fm_title_eq(w, "LABEL") && i + 1 < n) {
            out->file.label = ws[i + 1].word;
            out->has_file_design = 1;
            i += 2;
            continue;
        }
        if (fm_title_eq(w, "LOC OF LABEL") && i + 1 < n) {
            fm_right3(&ws[i + 1].word, &out->file.label_loc);
            if (out->file.label_loc > 59u) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM3120", ws[i + 1].span, -1,
                            "label location must be 000 through 059",
                            "The sixty words in the label block are addressed "
                            "000-059.");
            }
            i += 2;
            continue;
        }
        if (fm_title_eq(w, "MULTI REEL") && i + 1 < n) {
            unsigned v = 0;
            fm_right1(&ws[i + 1].word, &v);
            out->file.multi_reel = (int)v;
            if (v > 1u) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM3121", ws[i + 1].span, -1,
                            "MULTI REEL n must be 0 or 1", NULL);
            }
            i += 2;
            continue;
        }
        if (fm_title_eq(w, "BLK CT IND") && i + 1 < n) {
            unsigned v = 0;
            fm_right1(&ws[i + 1].word, &v);
            out->file.blk_ct_ind = (int)v;
            i += 2;
            continue;
        }
        if (fm_title_eq(w, "BLK CT LOC") && i + 1 < n) {
            fm_right3(&ws[i + 1].word, &out->file.blk_ct_loc);
            i += 2;
            continue;
        }
        if (fm_title_eq(w, "END REEL SEN") && i + 1 < n) {
            out->file.end_reel_sen = ws[i + 1].word;
            i += 2;
            continue;
        }
        if (fm_title_eq(w, "END FILE SEN") && i + 1 < n) {
            out->file.end_file_sen = ws[i + 1].word;
            i += 2;
            continue;
        }
        if (fm_title_eq(w, "LOC IN FIRST") && i + 1 < n) {
            fm_right3(&ws[i + 1].word, &out->file.sen_first);
            i += 2;
            continue;
        }
        if (fm_title_eq(w, "LOC IN LAST") && i + 1 < n) {
            fm_right3(&ws[i + 1].word, &out->file.sen_last);
            i += 2;
            continue;
        }
        if (fm_title_eq(w, "ITEM SIZE") && i + 1 < n) {
            fm_right3(&ws[i + 1].word, &out->item.item_size);
            out->item.present = 1;
            static const unsigned allowed[] = {1,  2,  3,  4,  5,  6,
                                               10, 12, 15, 20, 30, 60};
            int ok = 0;
            for (size_t a = 0; a < sizeof allowed / sizeof allowed[0]; a++) {
                if (out->item.item_size == allowed[a]) {
                    ok = 1;
                    break;
                }
            }
            if (!ok && out->item.item_size != 0) {
                fm_diag_add(diags, FM_SEV_WARNING, "FM3130", ws[i + 1].span, -1,
                            "item size is not one of the Form 2 listed sizes "
                            "(1,2,3,4,5,6,10,12,15,20,30,60)",
                            "Form 2 lists those sizes; the chapter text only "
                            "requires a word count.");
            }
            i += 2;
            continue;
        }
        if (fm_title_eq(w, "NO OF KEYS") && i + 1 < n) {
            unsigned v = 0;
            fm_right1(&ws[i + 1].word, &v);
            out->item.nkeys = v;
            if (v > 9u) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM3131", ws[i + 1].span, -1,
                            "number of keys must be 0 through 9", NULL);
            }
            i += 2;
            continue;
        }
        /* KEY n */
        char tbuf[13];
        fm_word_to_cstr(w, tbuf);
        if (strncmp(tbuf, "KEY ", 4) == 0 && i + 1 < n) {
            unsigned kn = 0;
            if (tbuf[4] >= '1' && tbuf[4] <= '9') {
                kn = (unsigned)(tbuf[4] - '0');
            }
            if (kn >= 1 && kn <= 9 && kn - 1u < 9u) {
                out->item.keys[kn - 1u] = ws[i + 1].word;
            }
            i += 2;
            continue;
        }
        if (in_fields && i + 3 < n) {
            /* four-word field packet, or two-word sub-item if word2 matches
               000SSSOOOEEE and word3 is not a descriptor. Distinguishing:
               field packets are 4 words. Sub-items are in ITEM section. */
            if (out->nfields >= 128) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM3140", ws[i].span, -1,
                            "too many fields in this Data Design", NULL);
                break;
            }
            FmFieldDesc *fd = &out->fields[out->nfields];
            memset(fd, 0, sizeof(*fd));
            fd->name = ws[i].word;
            fd->span = ws[i].span;
            unsigned loc = 0;
            fm_right3(&ws[i + 1].word, &loc);
            /* OOOWWWOOOOOO : WWW in digits 4-6 (0-based 3-5) per printed p.43
               Sample packets also use 000001000000 with the location in
               positions 4-6. */
            unsigned loc_mid = 0;
            fm_parse_uint((const char *)ws[i + 1].word.pos + 3, 3, &loc_mid);
            if (loc_mid != 0 && loc == 0) {
                loc = loc_mid;
            } else if (loc_mid != 0) {
                loc = loc_mid;
            }
            fd->word_loc = loc;
            fd->extractor = ws[i + 3].word;
            fd->full_word = fm_word_is_zeros(&fd->extractor) ? 1 : 0;
            fm_parse_descriptor(&ws[i + 2].word, fd, diags, ws[i + 2].span);
            out->nfields++;
            i += 4;
            continue;
        }
        /* sub-item two-word packet: name + 000SSSOOOEEE */
        if (out->item.present && i + 1 < n && !in_fields) {
            const FmWord *fmt = &ws[i + 1].word;
            int looks = 1;
            for (int k = 0; k < 3; k++) {
                if (fmt->pos[k] != '0' || fmt->pos[k + 6] != '0') {
                    looks = 0;
                }
            }
            if (looks && out->item.nsubitems < 32) {
                FmSubItem *si = &out->item.subitems[out->item.nsubitems++];
                si->name = ws[i].word;
                si->span = ws[i].span;
                unsigned sss = 0, eee = 0;
                fm_parse_uint((const char *)fmt->pos + 3, 3, &sss);
                fm_parse_uint((const char *)fmt->pos + 9, 3, &eee);
                si->start_word = sss;
                si->end_word = eee;
                i += 2;
                continue;
            }
        }
        /* extra file-design two-word packet */
        if (!in_fields && i + 1 < n && out->file.nextra < 8) {
            out->file.extra_title[out->file.nextra] = ws[i].word;
            out->file.extra_info[out->file.nextra] = ws[i + 1].word;
            out->file.nextra++;
            i += 2;
            continue;
        }
        fm_diag_add(diags, FM_SEV_ERROR, "FM3199", ws[i].span, -1,
                    "unrecognized Data Design packet",
                    "Use the File, Item, and Field packets from Chapter 4 and "
                    "Appendix B.");
        i++;
    }
    if (!saw_end) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM3180", out->span, -1,
                    "Data Design must end with END FILE DES in the last "
                    "four-word packet's following word and in word 059 of "
                    "that block",
                    "Form 3, printed page 104.");
    }
    if (out->is_wstorage && out->has_file_design) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM3181", out->span, -1,
                    "W-storage is described like file data but without "
                    "tape-organization information",
                    "Printed page 68.");
    }
    return !fm_diags_has_error(diags);
}

bool fm_parse_directory_text(FmArena *arena, const FmSource *src,
                             FmDiagList *diags, FmDirectory *out)
{
    memset(out, 0, sizeof(*out));
    FmWordSrc *ws = NULL;
    size_t n = 0;
    if (!fm_collect_words(arena, src, diags, &ws, &n) || n == 0) {
        return false;
    }
    if (!fm_title_eq(&ws[0].word, "DIRECTORY")) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM3300", ws[0].span, -1,
                    "Directory must begin with DIRECTORY",
                    "Printed page 68.");
        return false;
    }
    out->present = 1;
    out->span = ws[0].span;
    int saw_end = 0;
    for (size_t i = 1; i < n; i++) {
        if (fm_title_eq(&ws[i].word, "END DIRECTRY")) {
            saw_end = 1;
            continue;
        }
        if (fm_title_eq(&ws[i].word, "W-STORAGE")) {
            continue;
        }
        if (fm_word_is_spaces(&ws[i].word) || fm_word_is_zeros(&ws[i].word)) {
            continue;
        }
        /* 00W00000Wxxx */
        const FmWord *w = &ws[i].word;
        if (w->pos[0] == '0' && w->pos[1] == '0' && w->pos[2] == 'W' &&
            w->pos[8] == 'W') {
            unsigned hi = 0;
            fm_parse_uint((const char *)w->pos + 9, 3, &hi);
            out->w_high = hi;
            /* W-storage always starts at word zero (printed page 68). */
            unsigned lo = 0;
            fm_parse_uint((const char *)w->pos + 3, 3, &lo);
            if (lo != 0) {
                fm_diag_add(diags, FM_SEV_ERROR, "FM3301", ws[i].span, -1,
                            "W-storage always starts with the zero word",
                            "Directory word 02 is 00W00000Wxxx.");
            }
        }
    }
    if (!saw_end) {
        fm_diag_add(diags, FM_SEV_ERROR, "FM3302", out->span, -1,
                    "Directory must include END DIRECTRY in the first sentinel "
                    "word and in word 059",
                    "Note the spelling DIRECTRY (12 positions).");
    }
    return !fm_diags_has_error(diags);
}
