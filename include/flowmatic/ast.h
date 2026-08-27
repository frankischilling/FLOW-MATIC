#ifndef FLOWMATIC_AST_H
#define FLOWMATIC_AST_H

#include "flowmatic/source.h"
#include "flowmatic/token.h"
#include "flowmatic/word.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum FmOpKind {
    FM_OP_UNKNOWN = 0,
    FM_OP_INPUT,
    FM_OP_CLOSE_OUT,
    FM_OP_COMPARE,
    FM_OP_EXECUTE,
    FM_OP_JUMP,
    FM_OP_MOVE,
    FM_OP_READ_ITEM,
    FM_OP_REWIND,
    FM_OP_SET,
    FM_OP_STOP,
    FM_OP_TEST,
    FM_OP_TRANSFER,
    FM_OP_WRITE_ITEM,
    FM_OP_X1
} FmOpKind;

typedef enum FmCondKind {
    FM_COND_NONE = 0,
    FM_COND_EQUAL,
    FM_COND_GREATER,
    FM_COND_LESS,
    FM_COND_UNEQUAL,
    FM_COND_OTHERWISE
} FmCondKind;

typedef enum FmTransferKind {
    FM_XFER_ITEM_TO_ITEM = 1,
    FM_XFER_SUB_TO_ITEM,
    FM_XFER_ITEM_TO_SUB,
    FM_XFER_SUB_TO_SUB
} FmTransferKind;

typedef enum FmFieldType {
    FM_FTYPE_ALPHA = 1,
    FM_FTYPE_ALPHANUM = 2,
    FM_FTYPE_NUMERIC = 3
} FmFieldType;

typedef enum FmDecDir {
    FM_DEC_NONE = 0,
    FM_DEC_AT_REF, /* 00 */
    FM_DEC_LEFT,   /* nL */
    FM_DEC_RIGHT   /* nR */
} FmDecDir;

typedef struct FmFieldRef {
    FmWord name;
    int file_letter; /* A-I or W, or -1 */
    FmSpan span;
} FmFieldRef;

typedef struct FmBranch {
    FmCondKind cond;
    int target; /* operation number */
    FmSpan span;
} FmBranch;

typedef struct FmMoveDest {
    FmFieldRef field;
} FmMoveDest;

typedef struct FmMovePair {
    FmFieldRef src;
    FmMoveDest dests[12];
    size_t ndests;
} FmMovePair;

typedef struct FmSetPair {
    int from_op;
    int to_op;
    FmSpan span;
} FmSetPair;

typedef struct FmServoSpec {
    int nservos; /* 0 if omitted */
    int servo[2];
    FmSpan span;
} FmServoSpec;

typedef struct FmFileAssign {
    FmWord name;
    int letter;
    FmServoSpec servos;
    FmSpan span;
} FmFileAssign;

typedef struct FmOpInput {
    FmFileAssign inputs[8];
    size_t ninputs;
    FmFileAssign outputs[8];
    size_t noutputs;
    int preselection;
    int hsp[FM_WORD_WIDTH];
    size_t nhsp;
    int tc[FM_WORD_WIDTH];
    size_t ntc;
    int rerun;          /* 0 none, 1 ON, 2 FROM */
    int rerun_output;   /* file letter */
} FmOpInput;

typedef struct FmOpCompare {
    FmFieldRef left;
    FmFieldRef right;
    FmBranch branches[4];
    size_t nbranches;
} FmOpCompare;

typedef struct FmOpTest {
    FmFieldRef field;
    FmWord values[FM_WORD_WIDTH];
    size_t nvalues;
    int space_word;  /* SPACE/SPACES */
    int period_word; /* PERIOD/PERIODS */
    FmBranch branches[4];
    size_t nbranches;
} FmOpTest;

typedef struct FmOpJump {
    int target;
} FmOpJump;

typedef struct FmOpExecute {
    int from_op;
    int to_op; /* -1 if single operation form */
} FmOpExecute;

typedef struct FmOpMove {
    FmMovePair pairs[32];
    size_t npairs;
} FmOpMove;

typedef struct FmOpReadItem {
    int file_letter;
    int has_end;
    int end_target;
} FmOpReadItem;

typedef struct FmOpRewind {
    int files[FM_WORD_WIDTH];
    size_t nfiles;
} FmOpRewind;

typedef struct FmOpCloseOut {
    int files_kw; /* 0 none, 1 FILE, 2 FILES */
    int files[FM_WORD_WIDTH];
    size_t nfiles;
} FmOpCloseOut;

typedef struct FmOpSet {
    FmSetPair pairs[16];
    size_t npairs;
} FmOpSet;

typedef struct FmOpTransfer {
    FmTransferKind kind;
    int src_file;
    int dst_file;
    FmWord src_sub;
    FmWord dst_sub;
    int has_src_sub;
    int has_dst_sub;
} FmOpTransfer;

typedef struct FmOpWriteItem {
    int file_letter;
} FmOpWriteItem;

typedef struct FmOpX1 {
    FmWord english[60];
    size_t nenglish;
} FmOpX1;

typedef struct FmOperation {
    int number;
    FmOpKind kind;
    FmSpan span;
    size_t stmt_word_count; /* excluding op number and ending period */
    union {
        FmOpInput input;
        FmOpCompare compare;
        FmOpTest test;
        FmOpJump jump;
        FmOpExecute execute;
        FmOpMove move;
        FmOpReadItem read_item;
        FmOpRewind rewind;
        FmOpCloseOut close_out;
        FmOpSet set;
        FmOpTransfer transfer;
        FmOpWriteItem write_item;
        FmOpX1 x1;
    } u;
} FmOperation;

typedef struct FmFieldDesc {
    FmWord name;
    unsigned word_loc;
    FmFieldType type;
    FmDecDir dec_dir;
    int dec_n; /* 0-35; ignore if NONE */
    int sign_pos; /* 1-12, or 0 if ignore */
    int left_pos; /* 1-12 */
    int length;   /* 1-12 excluding sign */
    FmWord extractor;
    int full_word; /* extractor all zeros */
    FmSpan span;
} FmFieldDesc;

typedef struct FmSubItem {
    FmWord name;
    unsigned start_word;
    unsigned end_word;
    FmSpan span;
} FmSubItem;

typedef struct FmFileDesign {
    FmWord label;
    unsigned label_loc;
    int multi_reel; /* 0/1 */
    int blk_ct_ind;
    unsigned blk_ct_loc;
    FmWord end_reel_sen;
    FmWord end_file_sen;
    unsigned sen_first;
    unsigned sen_last;
    int nextra;
    FmWord extra_title[8];
    FmWord extra_info[8];
} FmFileDesign;

typedef struct FmItemDesign {
    unsigned item_size;
    unsigned nkeys;
    FmWord keys[9];
    FmSubItem subitems[32];
    size_t nsubitems;
    int present;
} FmItemDesign;

typedef struct FmDataDesign {
    FmWord file_name;
    int letter; /* -1 until INPUT assigns, W-storage uses W */
    int is_wstorage;
    int has_file_design;
    FmFileDesign file;
    FmItemDesign item;
    FmFieldDesc fields[128];
    size_t nfields;
    FmSpan span;
} FmDataDesign;

typedef struct FmDirectory {
    int present;
    unsigned w_high; /* highest reserved W-storage word */
    FmSpan span;
} FmDirectory;

typedef enum FmX1Kind {
    FM_X1_HEADER = 0,
    FM_X1_BODY,
    FM_X1_J_OPLINE,
    FM_X1_CONSTANTS_TITLE,
    FM_X1_CONSTANT,
    FM_X1_CODE_TITLE,
    FM_X1_CODE_CONSTANT,
    FM_X1_END
} FmX1Kind;

typedef struct FmX1Line {
    FmWord word;
    FmX1Kind kind;
    int m_addr; /* -1 if not an M-counted line */
    FmSpan span;
} FmX1Line;

typedef struct FmX1Section {
    int op_number;
    FmX1Line lines[512];
    size_t nlines;
    int body_m_count;
    int const_count;
    int code_const_count;
    int has_end;
    FmSpan span;
} FmX1Section;

typedef struct FmProgram {
    FmDataDesign designs[12];
    size_t ndesigns;
    FmDirectory directory;
    /* ops and x1 are arena-owned. Do not put FmProgram on the stack with
       inline tables; 999 MOVE operations would overflow the host stack. */
    FmOperation *ops;
    size_t nops;
    size_t ops_cap;
    FmX1Section *x1;
    size_t nx1;
    size_t x1_cap;
    int has_code;
} FmProgram;

const char *fm_op_kind_name(FmOpKind k);

#endif
