# Compiler architecture

`flowmaticc` is a C17 pipeline with no global parser state. AST, tokens, and dumps live in an arena plus a few malloc buffers that `fm_compile_result_free` releases.

## Modules

- `src/lexer.c` splits on spaces and keeps source spans (file, byte, line, column).
- `src/parser.c` is a recursive-descent parser of Appendix A operations.
- `src/data_design.c` reads File, Item, and Field packets as 12-position words.
- `src/x1.c` parses and checks X-1 sections (Appendix C).
- `src/semantic.c` numbers operations, binds file letters, resolves names, and applies READ-ITEM / TRANSFER / Directory rules.
- `src/control_flow.c` builds fallthrough edges and explicit branches. SET is not rewritten at compile time.
- `src/translation.c` writes Operations File 1 and starts the Unedited Record.
- `src/selection.c` attaches a generator name and runtime capability to each operation (Operations File 2, Generated Library).
- `src/allocation.c` assigns deterministic dummy addresses (Operations File 3). These addresses are symbolic slots for the modern backend, not UNIVAC memory maps.
- `src/processing.c` and `src/c_backend.c` emit a `switch (run->op)` dispatcher. `run->jump[]` is the SET table.
- `src/edited_record.c` prints the four Edited Record categories from printed page 88.
- `src/runtime.c` holds current items, field extract/deposit, and decimal digit comparison.

`FmProgram.ops` and `FmProgram.x1` are allocated from the compilation arena. Embedding 999 MOVE operations in a stack local overflows a typical host stack.

Ownership: `FmArena` owns tokens and AST for one compilation. Diagnostics and dump strings are malloc'd. The runtime owns FILE handles and item buffers until `fm_rt_close`.

## Dispatcher

Ordinary operations fall through numerically. JUMP reads `run->jump[op]`. SET writes that table. EXECUTE pushes a return operation and a range end; leaving the range pops the frame. READ-ITEM end-of-data is a second exit. STOP clears the run flag.
