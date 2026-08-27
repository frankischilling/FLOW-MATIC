# FLOW-MATIC (documented 1958 subset)

This repository is a C17 compiler for the FLOW-MATIC programming system as described in Remington Rand Univac's 1958 manual FLOW-MATIC Programming System (U1518). It implements the operations, Data Designs, W-storage, Directory, and X-1 relative coding that those two source files actually specify. The manual says its function list is not a complete list of every FLOW-MATIC function, so this project calls the result the documented 1958 subset. It does not invent missing operations, and it does not claim the complete historical language.

The compiler's four phases keep the historical names: Translation, Selection, Allocation, and Processing. The final executable is a portable C17 program linked to a small runtime. That is a modern backend. The two primary sources do not contain enough encoding, library, and hardware detail to emit an authentic UNIVAC II program tape. See `docs/backend-feasibility.md`.

## Primary sources

Keep these files next to the project (they are not rewritten by the compiler):

- `U1518_FLOW-MATIC_Programming_System_1958.pdf` (scanned manual; punctuation and figures win when OCR disagrees)
- `U1518_FLOW-MATIC_Programming_System_1958.txt` (searchable OCR)

Working notes live in:

- `docs/flowmatic-language.md`
- `docs/conformance-matrix.md`
- `docs/ambiguities.md`
- `docs/source-transcription-notes.md`
- `docs/architecture.md`

## Build

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

On GCC or Clang you can add sanitizers:

```
cmake -S . -B build -DFLOWMATIC_SANITIZE=ON
cmake --build build
```

Warnings are treated as errors.

The executable is `flowmaticc` (on Windows, `flowmaticc.exe` in the build directory).

## CLI

```
flowmaticc --help
flowmaticc --version
flowmaticc --check --design inv.dd --design price.dd --code prog.fm
flowmaticc --compile --design inv.dd --code prog.fm -o prog.c
flowmaticc --dump-op1 op1.txt --dump-op2 op2.txt --dump-op3 op3.txt --dump-library lib.txt --edited-record edited.txt --design inv.dd --code prog.fm
```

Repeat `--design` for each File Data Design. Add `--w-storage`, `--directory`, and `--x1` when those blocks are part of the compilation unit. `--unit` reads a combined file whose `@section` markers are a modern transport format, not FLOW-MATIC syntax.

Failed compilation returns a nonzero status and prints diagnostics on stderr. Successful `--check` prints `ok`.

## Input layout

Historical order on the FLOW-MATIC input tape (printed pages 69 and 81):

1. Data Designs for input and output files
2. optional W-storage Data Design
3. optional Directory (required if W-storage is used)
4. FLOW-MATIC code
5. optional X-1 sections
6. ending sentinel block (Z in words 000 and 059)

This compiler accepts those components as separate UTF-8 text files. That split is only a file-transport convenience.

### FLOW-MATIC code

Statements follow Appendix A (printed pages 93-99). Code words are at most 12 positions and contain no spaces. Punctuation counts as words. The ending period is required. Operation zero is INPUT. The highest operation is `STOP . (END)`.

### Data Design files

Each non-comment line is one 12-position word, right-padded with spaces. `#` comments are a modern fixture convention. Packet titles such as `NAME OF FILE`, `LABEL`, and `END FILE DES` are the historical words from Chapter 4 and Appendix B.

### W-storage and Directory

W-storage is described like a file without tape-organization packets (printed page 68). Fields are referenced with `W` in parentheses. The Directory follows all Data Designs and precedes the code. Word 02 of the Directory is `00W00000Wxxx`, with W-storage always starting at word zero.

### X-1

X-1 operations in the FLOW-MATIC code are parsed and matched to sections (Chapter 6, Appendix C). The English sentence after `X-1` is documentation. It is not compiled into arithmetic. Strict `--compile` of a program that contains X-1 fails unless you pass `--x1-hooks`, which emits a C callback keyed by operation number.

## Conformance and limits

See `docs/conformance-matrix.md` for what is implemented and which tests cover it. See `docs/ambiguities.md` for decisions that are not fully determined by the two sources. Comparison of non-numeric fields uses byte-for-byte order of the 12-position words. That is not a claimed UNIVAC collation sequence.

## Tests

```
ctest --test-dir build --output-on-failure
```

Fixtures for Sample Problems 1-3 are under `tests/fixtures/manual/`. Runtime tape images under `tests/fixtures/runtime/` were created for this project; they are not pages copied from the manual.
