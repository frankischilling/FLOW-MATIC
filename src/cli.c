#include "internal.h"
#include "flowmatic/compiler.h"

static void fm_usage(FILE *fp)
{
    fputs(
        "flowmaticc - documented 1958 FLOW-MATIC subset compiler\n"
        "\n"
        "Usage:\n"
        "  flowmaticc --help\n"
        "  flowmaticc --version\n"
        "  flowmaticc --check [inputs]\n"
        "  flowmaticc --compile [inputs] -o program.c\n"
        "  flowmaticc --dump-op1 path --dump-op2 path --dump-op3 path \\\n"
        "             --dump-library path --edited-record path [inputs]\n"
        "\n"
        "Inputs (modern transport; not new FLOW-MATIC syntax):\n"
        "  --design FILE        File/Item/Field Data Design (repeatable)\n"
        "  --w-storage FILE     W-storage Data Design\n"
        "  --directory FILE     Directory block\n"
        "  --code FILE          FLOW-MATIC statements\n"
        "  --x1 FILE            X-1 sections\n"
        "  --unit FILE          Combined file with @section markers\n"
        "\n"
        "Other:\n"
        "  --x1-hooks           Allow C callbacks for X-1 operations\n"
        "  --edited-machine     Stable Edited Record layout for golden tests\n"
        "\n"
        "This compiler implements the documented 1958 subset from U1518.\n"
        "It does not emit a UNIVAC II program tape.\n",
        fp);
}

int main(int argc, char **argv)
{
    const char *designs[16];
    size_t nd = 0;
    FmUnitPaths paths;
    memset(&paths, 0, sizeof paths);
    paths.designs = designs;
    FmCompileOptions opt;
    memset(&opt, 0, sizeof opt);
    int want_help = 0;
    int want_ver = 0;
    int want_compile = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            want_help = 1;
        } else if (strcmp(argv[i], "--version") == 0) {
            want_ver = 1;
        } else if (strcmp(argv[i], "--check") == 0) {
            opt.check_only = 1;
        } else if (strcmp(argv[i], "--compile") == 0) {
            want_compile = 1;
            opt.emit_c = 1;
        } else if (strcmp(argv[i], "--x1-hooks") == 0) {
            opt.x1_hooks = 1;
        } else if (strcmp(argv[i], "--edited-machine") == 0) {
            opt.edited_machine = 1;
        } else if (strcmp(argv[i], "--design") == 0 && i + 1 < argc) {
            if (nd < 16) {
                designs[nd++] = argv[++i];
            }
        } else if (strcmp(argv[i], "--w-storage") == 0 && i + 1 < argc) {
            paths.w_storage = argv[++i];
        } else if (strcmp(argv[i], "--directory") == 0 && i + 1 < argc) {
            paths.directory = argv[++i];
        } else if (strcmp(argv[i], "--code") == 0 && i + 1 < argc) {
            paths.code = argv[++i];
        } else if (strcmp(argv[i], "--x1") == 0 && i + 1 < argc) {
            paths.x1 = argv[++i];
        } else if (strcmp(argv[i], "--unit") == 0 && i + 1 < argc) {
            paths.combined = argv[++i];
        } else if ((strcmp(argv[i], "-o") == 0 ||
                    strcmp(argv[i], "--emit-c") == 0) &&
                   i + 1 < argc) {
            opt.output_c = argv[++i];
            opt.emit_c = 1;
        } else if (strcmp(argv[i], "--dump-op1") == 0 && i + 1 < argc) {
            opt.dump_op1 = argv[++i];
        } else if (strcmp(argv[i], "--dump-op2") == 0 && i + 1 < argc) {
            opt.dump_op2 = argv[++i];
        } else if (strcmp(argv[i], "--dump-op3") == 0 && i + 1 < argc) {
            opt.dump_op3 = argv[++i];
        } else if (strcmp(argv[i], "--dump-library") == 0 && i + 1 < argc) {
            opt.dump_library = argv[++i];
        } else if (strcmp(argv[i], "--edited-record") == 0 && i + 1 < argc) {
            opt.edited_record = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "unknown option %s\n", argv[i]);
            fm_usage(stderr);
            return 2;
        } else {
            paths.combined = argv[i];
        }
    }
    paths.ndesigns = nd;
    if (want_help || argc == 1) {
        fm_usage(stdout);
        return 0;
    }
    if (want_ver) {
        printf("flowmaticc %s\n", fm_compiler_version());
        return 0;
    }
    if (!paths.code && !paths.combined) {
        fputs("no FLOW-MATIC code given (--code or --unit)\n", stderr);
        return 2;
    }
    FmArena arena;
    fm_arena_init(&arena);
    FmDiagList diags;
    fm_diags_init(&diags);
    FmCompileResult result;
    memset(&result, 0, sizeof result);
    int ok = fm_compile_unit(&arena, &paths, &opt, &diags, &result);
    if (fm_diags_has_error(&diags)) {
        fm_diags_print(&diags, stderr);
        fm_compile_result_free(&result);
        fm_diags_free(&diags);
        fm_arena_free(&arena);
        return 1;
    }
    if (diags.warning_count) {
        fm_diags_print(&diags, stderr);
    }
    if (!ok) {
        fm_compile_result_free(&result);
        fm_diags_free(&diags);
        fm_arena_free(&arena);
        return 1;
    }
    if (opt.emit_c && !opt.output_c && want_compile) {
        fputs(result.c_source ? result.c_source : "", stdout);
    }
    if (opt.check_only) {
        fputs("ok\n", stdout);
    }
    fm_compile_result_free(&result);
    fm_diags_free(&diags);
    fm_arena_free(&arena);
    return 0;
}
