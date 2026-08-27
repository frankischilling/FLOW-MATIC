# FLOW-MATIC language (documented 1958 subset)

The 1958 U1518 manual is the authority. This page restates the rules the compiler enforces. If an EBNF line disagrees with the manual, follow the manual.

## General rules (printed page 93)

1. A code word has at most 12 positions and contains no space.
2. Words are separated by spaces.
3. A statement has at most 60 code words, excluding the operation number and the ending period, including other words and punctuation.
4. Punctuation follows ordinary English. Marks count as words. Only the ending period is mandatory.
5. Field and file names may contain hyphens.
6. A field name is followed by its file letter in parentheses, as a separate word: `PRODUCT-NO (A)`. In other uses the file letter is not parenthesized.
7. Operation numbers are numeric.
8. Numbering starts at zero. Operation zero is always INPUT.
9. Numbers form an unbroken sequence. At most 999 operations.
10. The last operation is STOP followed by END in parentheses: `STOP . (END)`.

Normal execution proceeds by increasing operation number unless an operation names another path.

## Operations (Appendix A, printed pages 95-99)

Spaces in the format lines are required between words. `h` is an operation number, `f` a file letter, names are programmer-assigned.

**CLOSE-OUT** `(h) CLOSE-OUT [ FILE | FILES ] f1 [ f2 ... ] .`

**COMPARE** four options, always ending with OTHERWISE:

- IF EQUAL / OTHERWISE
- IF GREATER / OTHERWISE
- IF EQUAL / IF GREATER / OTHERWISE
- IF GREATER / IF EQUAL / OTHERWISE

**EXECUTE** `(h) EXECUTE OPERATION h1 [ THROUGH OPERATION h2 ] .`

**INPUT** (operation 0): input files `name FILE-f [ SERVO s | SERVOS s , s ]`, then `; OUTPUT` and output files, then optional `; PRESELECTION`, `; HSP f , ...`, `; T/C f , ...`, `; RERUN { ON | FROM } OUTPUT f`. File names may not begin with `FILE-`. Servos may be omitted.

**JUMP** `(h) JUMP TO OPERATION h1 .`

**MOVE** one or more `field (f) TO field (f) [, field (f)]` groups, extra groups after `;`.

**READ-ITEM** `(h) READ-ITEM f [ ; IF END OF DATA GO TO OPERATION h1 ] .`

Special notes (printed page 97): every INPUT file needs a READ-ITEM; at least one per file includes IF END OF DATA; those end targets for one file must match.

**REWIND** `(h) REWIND f1 [ , f2 ... ] .`

**SET** `(h) SET OPERATION h1 TO GO TO OPERATION h2 [ , OPERATION h3 TO GO TO OPERATION h4 ... ] .`

**STOP** `(h) STOP . (END)` as the highest numbered operation.

**TEST** AGAINST a constant (or SPACE, SPACES, PERIOD, PERIODS). IF GREATER, IF EQUAL, IF LESS may appear singly or two together in any order. IF UNEQUAL is a third option. OTHERWISE is required and last. Multiple AGAINST values are allowed except with SPACE/PERIOD words.

**TRANSFER** item or sub-item, four options. Sizes must be equal (printed page 99).

**WRITE-ITEM** `(h) WRITE-ITEM f .`

**X-1** is not in the Appendix A function list. Chapter 6 (printed page 76) places `X-1` plus an English sentence in the FLOW-MATIC code. The English is not processed.

## Data Designs (Chapter 4, Appendix B)

Nine two-word File Design packets: LABEL, LOC OF LABEL, MULTI REEL, BLK CT IND, BLK CT LOC, END REEL SEN, END FILE SEN, LOC IN FIRST, LOC IN LAST. Extra title/info packets may follow.

Item Design: ITEM SIZE, NO OF KEYS, KEY n in decreasing significance, then optional sub-items `name` / `000SSSOOOEEE`.

Field Design four-word packets: name, `OOOWWWOOOOOO` word location, descriptor `OOOOOTPPSLNO`, extractor. T=1 alphabetic, 2 alphanumeric, 3 numeric. PP is 00, ignore, nL, or nR (n up to 35 via 1-9 then A-Z). S, L, N use 1-9, A, B, C. Full-word extractor is twelve zeros. Sentinel `END FILE DES` after the last packet and in word 059.

## W-storage and Directory (printed pages 68-69)

W-storage omits tape packets. Directory: `DIRECTORY`, spaces, `00W00000Wxxx`, `W-STORAGE`, `END DIRECTRY` at the first sentinel word and word 059. W-storage always starts at word zero.

## X-1 (Appendix C)

Header `X-1` + six spaces + three-digit operation number. Body from M000. J extra line `OP.nnn` is not an M address. `CONSTANTS` then constants (they do receive M numbers). `CODE CONSTS` then relocatable constants (no M or J). At most 59 constants plus code constants. `END SUBROUTN` after the last line and in word 09 of the blockette.

Address classes: A-I item words, W, M, J, T.

## EBNF sketch (non-authoritative)

```
program     = { operation } ;
operation   = "(" number ")" statement "." [ "(END)" ] ;
statement   = input | compare | ... | x1 ;
field-ref   = name "(" letter ")" ;
```

Use Appendix A when this sketch is short.
