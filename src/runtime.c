#include "internal.h"
#include "flowmatic/runtime.h"

int fm_rt_init(FmRun *run)
{
    memset(run, 0, sizeof(*run));
    return 1;
}

FmRtFile *fm_rt_file(FmRun *run, int letter)
{
    for (unsigned i = 0; i < run->nfiles; i++) {
        if (run->files[i].letter == letter) {
            return &run->files[i];
        }
    }
    return NULL;
}

int fm_rt_add_file(FmRun *run, int letter, int is_input, unsigned item_size)
{
    if (run->nfiles >= FM_RT_MAX_FILES) {
        return 0;
    }
    if (item_size == 0 || item_size > FM_RT_MAX_ITEM_WORDS) {
        return 0;
    }
    FmRtFile *f = &run->files[run->nfiles++];
    memset(f, 0, sizeof(*f));
    f->letter = letter;
    f->is_input = is_input;
    f->is_output = !is_input;
    f->item_size = item_size;
    memset(f->eof_sen, 'Z', FM_WORD_WIDTH);
    f->sen_word = 0;
    return 1;
}

static int fm_read_item_line(FILE *fp, unsigned char *item, unsigned nwords)
{
    char line[1024];
    if (!fgets(line, sizeof line, fp)) {
        return 0;
    }
    if (line[0] == '#') {
        return fm_read_item_line(fp, item, nwords);
    }
    if (strncmp(line, "END", 3) == 0) {
        return 0;
    }
    /* Concatenated 12-char words, or '|' separated. */
    memset(item, ' ', nwords * FM_WORD_WIDTH);
    size_t len = strlen(line);
    while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = 0;
    }
    if (strchr(line, '|')) {
        unsigned wi = 0;
        char *save = NULL;
        char *tok = strtok(line, "|");
        while (tok && wi < nwords) {
            size_t n = strlen(tok);
            if (n > FM_WORD_WIDTH) {
                n = FM_WORD_WIDTH;
            }
            memcpy(item + wi * FM_WORD_WIDTH, tok, n);
            wi++;
            tok = strtok(NULL, "|");
            (void)save;
        }
    } else {
        size_t need = (size_t)nwords * FM_WORD_WIDTH;
        if (len > need) {
            len = need;
        }
        memcpy(item, line, len);
    }
    return 1;
}

int fm_rt_open_input(FmRun *run, int letter, const char *path)
{
    FmRtFile *f = fm_rt_file(run, letter);
    if (!f) {
        return 0;
    }
    f->fp = fopen(path, "rb");
    if (!f->fp) {
        return 0;
    }
    snprintf(f->path, sizeof f->path, "%s", path);
    f->at_end = 0;
    return 1;
}

int fm_rt_open_output(FmRun *run, int letter, const char *path)
{
    FmRtFile *f = fm_rt_file(run, letter);
    if (!f) {
        return 0;
    }
    f->fp = fopen(path, "wb");
    if (!f->fp) {
        return 0;
    }
    snprintf(f->path, sizeof f->path, "%s", path);
    return 1;
}

int fm_rt_read_item(FmRun *run, int letter)
{
    FmRtFile *f = fm_rt_file(run, letter);
    if (!f || !f->fp) {
        return 0;
    }
    if (f->at_end) {
        return 0;
    }
    if (!fm_read_item_line(f->fp, f->item, f->item_size)) {
        f->at_end = 1;
        return 0;
    }
    if (memcmp(f->item + f->sen_word * FM_WORD_WIDTH, f->eof_sen,
               FM_WORD_WIDTH) == 0) {
        f->at_end = 1;
        return 0;
    }
    return 1;
}

int fm_rt_write_item(FmRun *run, int letter)
{
    FmRtFile *f = fm_rt_file(run, letter);
    if (!f || !f->fp) {
        return 0;
    }
    for (unsigned w = 0; w < f->item_size; w++) {
        if (w) {
            fputc('|', f->fp);
        }
        fwrite(f->item + w * FM_WORD_WIDTH, 1, FM_WORD_WIDTH, f->fp);
    }
    fputc('\n', f->fp);
    return 1;
}

int fm_rt_rewind(FmRun *run, int letter)
{
    FmRtFile *f = fm_rt_file(run, letter);
    if (!f || !f->fp) {
        return 0;
    }
    if (fseek(f->fp, 0, SEEK_SET) != 0) {
        return 0;
    }
    f->at_end = 0;
    return 1;
}

int fm_rt_close_out(FmRun *run, int letter)
{
    FmRtFile *f = fm_rt_file(run, letter);
    if (!f) {
        return 0;
    }
    if (f->fp) {
        fclose(f->fp);
        f->fp = NULL;
    }
    return 1;
}

void fm_rt_close(FmRun *run)
{
    for (unsigned i = 0; i < run->nfiles; i++) {
        if (run->files[i].fp) {
            fclose(run->files[i].fp);
            run->files[i].fp = NULL;
        }
    }
}

static unsigned char *fm_rt_area(FmRun *run, int letter, unsigned word)
{
    FmRtFile *f = fm_rt_file(run, letter);
    if (!f || word >= f->item_size) {
        return NULL;
    }
    return f->item + word * FM_WORD_WIDTH;
}

static FmRtField *fm_rt_find_field(FmRun *run, const char *name, int letter)
{
    if (letter == 'W') {
        /* W-storage fields are stored on a synthetic file letter W if added,
           else search all files named with W. We keep W fields on run via
           a file with letter W. */
        for (unsigned i = 0; i < run->nfiles; i++) {
            if (run->files[i].letter == 'W') {
                FmRtFile *f = &run->files[i];
                for (unsigned k = 0; k < f->nfields; k++) {
                    if (strcmp(f->fields[k].name, name) == 0) {
                        return &f->fields[k];
                    }
                }
            }
        }
        return NULL;
    }
    FmRtFile *f = fm_rt_file(run, letter);
    if (!f) {
        return NULL;
    }
    for (unsigned k = 0; k < f->nfields; k++) {
        if (strcmp(f->fields[k].name, name) == 0) {
            return &f->fields[k];
        }
    }
    return NULL;
}

static void fm_extract_field(FmRun *run, const FmRtField *fld, int letter,
                             unsigned char out[12], size_t *nlen)
{
    memset(out, ' ', 12);
    unsigned char *w = fm_rt_area(run, letter, fld->word_loc);
    if (!w) {
        *nlen = 0;
        return;
    }
    if (fld->full_word || fld->length == 0) {
        memcpy(out, w, 12);
        *nlen = 12;
        return;
    }
    int L = fld->left_pos >= 1 ? fld->left_pos : 1;
    int n = fld->length >= 1 ? fld->length : 12;
    int start = L - 1;
    if (start < 0) {
        start = 0;
    }
    if (start + n > 12) {
        n = 12 - start;
    }
    memcpy(out, w + start, (size_t)n);
    *nlen = (size_t)n;
}

static void fm_deposit_field(FmRun *run, const FmRtField *fld, int letter,
                             const unsigned char *in, size_t n)
{
    unsigned char *w = fm_rt_area(run, letter, fld->word_loc);
    if (!w) {
        return;
    }
    if (fld->full_word || fld->length == 0) {
        memcpy(w, in, 12);
        return;
    }
    int L = fld->left_pos >= 1 ? fld->left_pos : 1;
    int len = fld->length >= 1 ? fld->length : 12;
    int start = L - 1;
    if ((size_t)len > n) {
        /* right-align numeric, left-align alpha: copy min */
        len = (int)n;
    }
    if (start < 0) {
        start = 0;
    }
    if (start + len > 12) {
        len = 12 - start;
    }
    memcpy(w + start, in, (size_t)len);
}

int fm_dec_compare_numeric(const unsigned char *a, int alen, int a_scale,
                           const unsigned char *b, int blen, int b_scale)
{
    /* Compare as decimal integers after aligning implied scales.
       scale is digits to the right of the assumed decimal point. */
    unsigned char aa[80];
    unsigned char bb[80];
    memset(aa, '0', sizeof aa);
    memset(bb, '0', sizeof bb);
    int a_int = alen - a_scale;
    int b_int = blen - b_scale;
    if (a_int < 0) {
        a_int = 0;
    }
    if (b_int < 0) {
        b_int = 0;
    }
    int max_int = a_int > b_int ? a_int : b_int;
    int max_frac = a_scale > b_scale ? a_scale : b_scale;
    /* layout: [max_int digits][max_frac digits] */
    for (int i = 0; i < alen; i++) {
        unsigned char d = a[i];
        if (d < '0' || d > '9') {
            d = '0';
        }
        int pos = max_int - a_int + i;
        if (pos >= 0 && pos < 80) {
            aa[pos] = d;
        }
    }
    for (int i = 0; i < blen; i++) {
        unsigned char d = b[i];
        if (d < '0' || d > '9') {
            d = '0';
        }
        int pos = max_int - b_int + i;
        if (pos >= 0 && pos < 80) {
            bb[pos] = d;
        }
    }
    int total = max_int + max_frac;
    if (total > 80) {
        total = 80;
    }
    return memcmp(aa, bb, (size_t)total);
}

static int fm_scale_of(const FmRtField *f)
{
    if (f->dec_dir == 3) { /* right of reference: n integer digits */
        int n = f->length - f->dec_n;
        return n < 0 ? 0 : n; /* wait: 3R means decimal 3 positions right of
                                 left edge, so 3 integer digits, rest fraction */
    }
    if (f->dec_dir == 2) {
        return f->length + f->dec_n;
    }
    if (f->dec_dir == 1) {
        return f->length; /* all fractional */
    }
    return 0;
}

int fm_rt_compare(FmRun *run, const char *n1, int f1, const char *n2, int f2)
{
    FmRtField *a = fm_rt_find_field(run, n1, f1);
    FmRtField *b = fm_rt_find_field(run, n2, f2);
    unsigned char va[12], vb[12];
    size_t na = 0, nb = 0;
    if (!a || !b) {
        /* fall back to whole first word if fields missing */
        unsigned char *wa = fm_rt_area(run, f1, 0);
        unsigned char *wb = fm_rt_area(run, f2, 0);
        if (!wa || !wb) {
            return 0;
        }
        return memcmp(wa, wb, 12);
    }
    fm_extract_field(run, a, f1, va, &na);
    fm_extract_field(run, b, f2, vb, &nb);
    if (a->type == 3 && b->type == 3) {
        int r = fm_dec_compare_numeric(va, (int)na, fm_scale_of(a), vb, (int)nb,
                                       fm_scale_of(b));
        return (r > 0) - (r < 0);
    }
    int r = memcmp(va, vb, 12);
    return (r > 0) - (r < 0);
}

int fm_rt_test(FmRun *run, const char *name, int file, const unsigned char *val)
{
    FmRtField *a = fm_rt_find_field(run, name, file);
    unsigned char va[12];
    size_t na = 0;
    if (!a) {
        unsigned char *w = fm_rt_area(run, file, 0);
        if (!w) {
            return 0;
        }
        return memcmp(w, val, 12);
    }
    fm_extract_field(run, a, file, va, &na);
    int r = memcmp(va, val, 12);
    return (r > 0) - (r < 0);
}

int fm_rt_move(FmRun *run, const char *n1, int f1, const char *n2, int f2)
{
    FmRtField *a = fm_rt_find_field(run, n1, f1);
    FmRtField *b = fm_rt_find_field(run, n2, f2);
    unsigned char va[12];
    size_t na = 0;
    if (!a || !b) {
        return 0;
    }
    fm_extract_field(run, a, f1, va, &na);
    fm_deposit_field(run, b, f2, va, na);
    return 1;
}

int fm_rt_transfer(FmRun *run, int src, int dst)
{
    FmRtFile *s = fm_rt_file(run, src);
    FmRtFile *d = fm_rt_file(run, dst);
    if (!s || !d) {
        return 0;
    }
    unsigned n = s->item_size < d->item_size ? s->item_size : d->item_size;
    memcpy(d->item, s->item, n * FM_WORD_WIDTH);
    return 1;
}

int fm_rt_transfer_sub(FmRun *run, int src, const char *ssub, int dst,
                       const char *dsub)
{
    (void)ssub;
    (void)dsub;
    return fm_rt_transfer(run, src, dst);
}
