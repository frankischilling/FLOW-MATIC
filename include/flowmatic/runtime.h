#ifndef FLOWMATIC_RUNTIME_H
#define FLOWMATIC_RUNTIME_H

#include "flowmatic/word.h"

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { FM_RT_MAX_FILES = 10 };
enum { FM_RT_MAX_FIELDS = 64 };
enum { FM_RT_MAX_OPS = 999 };
enum { FM_RT_MAX_ITEM_WORDS = 60 };

typedef struct FmRtField {
    char name[FM_WORD_WIDTH + 1];
    unsigned word_loc;
    int type; /* 1 alpha, 2 alphanum, 3 numeric */
    int dec_dir; /* 0 none, 1 at ref, 2 left, 3 right */
    int dec_n;
    int sign_pos;
    int left_pos;
    int length;
    unsigned char extractor[FM_WORD_WIDTH];
    int full_word;
} FmRtField;

typedef struct FmRtSub {
    char name[FM_WORD_WIDTH + 1];
    unsigned start_word;
    unsigned end_word;
} FmRtSub;

typedef struct FmRtFile {
    int letter;
    int is_input;
    int is_output;
    unsigned item_size;
    unsigned nfields;
    FmRtField fields[FM_RT_MAX_FIELDS];
    unsigned nsubs;
    FmRtSub subs[32];
    unsigned char item[FM_RT_MAX_ITEM_WORDS * FM_WORD_WIDTH];
    unsigned char eof_sen[FM_WORD_WIDTH];
    unsigned sen_word;
    FILE *fp;
    char path[256];
    int at_end;
    int hsp;
} FmRtFile;

typedef struct FmRun {
    FmRtFile files[FM_RT_MAX_FILES];
    unsigned nfiles;
    unsigned char wstorage[FM_RT_MAX_ITEM_WORDS * FM_WORD_WIDTH];
    unsigned w_high;
    int jump[FM_RT_MAX_OPS];
    int exec_ret[32];
    int exec_end[32];
    int exec_sp;
    int op;
    int stop;
    int exit_code;
    FILE *log;
    /* optional X-1 hooks keyed by operation number */
    int (*x1_hook)(struct FmRun *run, int op);
} FmRun;

int fm_rt_init(FmRun *run);
int fm_rt_add_file(FmRun *run, int letter, int is_input, unsigned item_size);
FmRtFile *fm_rt_file(FmRun *run, int letter);
int fm_rt_open_input(FmRun *run, int letter, const char *path);
int fm_rt_open_output(FmRun *run, int letter, const char *path);
int fm_rt_read_item(FmRun *run, int letter); /* 1=item, 0=end */
int fm_rt_write_item(FmRun *run, int letter);
int fm_rt_rewind(FmRun *run, int letter);
int fm_rt_close_out(FmRun *run, int letter);
int fm_rt_compare(FmRun *run, const char *n1, int f1, const char *n2, int f2);
int fm_rt_test(FmRun *run, const char *name, int file, const unsigned char *val);
int fm_rt_move(FmRun *run, const char *n1, int f1, const char *n2, int f2);
int fm_rt_transfer(FmRun *run, int src, int dst);
int fm_rt_transfer_sub(FmRun *run, int src, const char *ssub, int dst,
                       const char *dsub);
void fm_rt_close(FmRun *run);

/* Decimal helpers used by tests; no binary floating point. */
int fm_dec_compare_numeric(const unsigned char *a, int alen, int a_scale,
                           const unsigned char *b, int blen, int b_scale);

#ifdef __cplusplus
}
#endif

#endif
