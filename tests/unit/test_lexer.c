#include "test_harness.h"
#include "internal.h"
#include "flowmatic/compiler.h"

static void lex_str(const char *s, FmArena *a, FmLexResult *r, FmDiagList *d)
{
    FmSource src = {"t.fm", s, strlen(s), 0};
    fm_lex(a, &src, d, r);
}

int main(void)
{
    FmArena arena;
    fm_arena_init(&arena);
    FmDiagList diags;
    fm_diags_init(&diags);
    FmLexResult r;

    lex_str("INPUT INVENTORY FILE-A ; OUTPUT X FILE-B .", &arena, &r, &diags);
    TCHECK(r.ntokens >= 8);
    TCHECK(r.tokens[0].kind == FM_TOK_WORD);
    TCHECK(fm_word_is_kw(&r.tokens[0].lexeme, "INPUT"));
    TCHECK(fm_word_is_kw(&r.tokens[2].lexeme, "FILE-A"));
    int saw_semi = 0, saw_period = 0;
    for (size_t i = 0; i < r.ntokens; i++) {
        if (r.tokens[i].kind == FM_TOK_SEMI) {
            saw_semi = 1;
        }
        if (r.tokens[i].kind == FM_TOK_PERIOD) {
            saw_period = 1;
        }
    }
    TCHECK(saw_semi);
    TCHECK(saw_period);

    fm_diags_init(&diags);
    lex_str("PRODUCT-NO (A)", &arena, &r, &diags);
    TCHECK(r.tokens[0].kind == FM_TOK_WORD);
    TCHECK(r.tokens[1].kind == FM_TOK_GROUP);
    TCHECK(r.tokens[0].span.line_start == 1);
    TCHECK(r.tokens[0].span.col_start == 1);

    fm_diags_init(&diags);
    lex_str("(0) CLOSE-OUT FILES C , D .", &arena, &r, &diags);
    TCHECK(r.tokens[0].kind == FM_TOK_GROUP);
    TCHECK(r.tokens[3].kind == FM_TOK_WORD);
    int saw_comma = 0;
    for (size_t i = 0; i < r.ntokens; i++) {
        if (r.tokens[i].kind == FM_TOK_COMMA) {
            saw_comma = 1;
        }
    }
    TCHECK(saw_comma);

    fm_diags_init(&diags);
    lex_str("THIRTEENCHARSX", &arena, &r, &diags);
    TCHECK(fm_diags_has_error(&diags));
    TCHECK(strcmp(diags.items[0].code, "FM1101") == 0);

    fm_arena_free(&arena);
    fm_diags_free(&diags);
    return t_report();
}
