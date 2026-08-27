# Conformance matrix

Printed page numbers are from the 1958 manual. PDF page index is printed page plus 9 (front matter). Status: done = implemented and tested; partial = parsed or modeled with a documented limit.

| Feature | Manual | Implemented in | Tests | Status | Notes |
|---|---|---|---|---|---|
| Code-word length 12, no spaces | p.93 Guide 1 | lexer.c | test_lexer | done | FM1101 |
| Words separated by spaces | p.93 | lexer.c | test_lexer | done | |
| 60-word statement cap | p.93 Guide 3 | parser.c | test_negative | done | FM2121 |
| Punctuation counts as words; period required | p.93-94 | parser.c | test_negative, test_parser | done | |
| Hyphenated names | p.93 | lexer/parser | sample fixtures | done | |
| Field (letter) | p.93 Guide 6 | parser.c | test_parser | done | |
| Ops 0..n continuous, max 999 | p.93 | semantic.c | test_negative | done | |
| INPUT is op 0 | p.93, p.96 | semantic.c | test_semantic | done | |
| STOP highest, `(END)` | p.93, p.98 | parser/semantic | test_parser | done | |
| CLOSE-OUT | p.92, p.95 | parser.c | test_parser | done | FILE/FILES; commas optional |
| COMPARE options 1-4 | p.95 | parser.c | test_parser | done | |
| EXECUTE [THROUGH] | p.95 | parser.c | sample3, test_parser | done | |
| INPUT files, OUTPUT, HSP, T/C, PRESELECTION, RERUN | p.96 | parser.c | sample1 HSP | partial | T/C and RERUN parsed; HSP servo reservation is a host concern |
| JUMP | p.97 | parser.c | sample1 | done | |
| MOVE multi dest / multi pair | p.97 | parser.c | sample3 | done | |
| READ-ITEM + special notes | p.97 | parser/semantic | sample1, test_negative | done | |
| REWIND | p.97 | parser.c | sample1 | done | |
| SET | p.97 | parser.c, runtime jump table | sample1 | done | SET of non-JUMP is rejected; see ambiguities |
| TEST options + SPACE/PERIOD | p.98 | parser.c | sample1 AGAINST Z's | partial | SPACE/PERIOD words parsed; samples use Z sentinels |
| TRANSFER 4 options, equal size | p.98-99 | parser/semantic | sample1 | done | |
| WRITE-ITEM | p.99 | parser.c | sample1 | done | |
| X-1 statement in code | Ch.6 p.76 | parser.c | sample3 | done | English not executed |
| File Design 9 packets | p.38-40 | data_design.c | test_data_design | done | |
| Item size, keys, sub-items | p.41-42 | data_design.c | test_data_design | done | |
| Field descriptor OOOOOTPPSLNO | p.43-44, Form 3 p.104 | data_design.c | test_data_design | done | `/` for ignore |
| Packed and overlapping fields | p.44 | data_design.c | test_data_design | done | |
| W-storage | p.68 | data_design/semantic | sample2, sample3 | done | |
| Directory | p.68-69 | data_design.c | test_data_design | done | END DIRECTRY spelling |
| Four phases | Ch.7 p.83-86 | translation/selection/allocation/processing | test_phases | done | |
| Operations Files 1-3 dumps | Ch.7 | dump.c | test_phases | done | Host text, not UNIVAC tape records |
| Generated Library dump | Ch.7 | dump.c | test_phases | done | Metadata, not copied UNIVAC generators |
| Edited Record categories | p.88 | edited_record.c | test_phases | done | Directory excluded from listing |
| X-1 address classes | p.106 | x1.c | test_x1 | done | |
| X-1 section format | p.107, Fig.44 p.109 | x1.c | test_x1, fig44 fixture | done | CODE CONSTS from figure |
| X-1 Selector errors | p.108 | x1.c | test_x1 | done | Mapped to FM62xx |
| Sample Problem 1 | Fig.23 p.34 | fixtures + integration | test_integration | done | |
| Sample Problem 2 | Fig.31 p.65 | fixtures + integration | test_integration | done | |
| Sample Problem 3 | Fig.37-39 p.78-80 | fixtures + phases | test_phases, test_integration | done | Executable needs X-1 hooks |
| Modern C backend | (fallback) | c_backend.c, runtime.c | test_integration | done | Not UNIVAC II |
