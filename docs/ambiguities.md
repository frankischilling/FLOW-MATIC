# Ambiguities

Each entry notes what the two sources show, what they omit, the conservative choice, and whether it affects historical compatibility.

## 1. Character collation

- Source: COMPARE and TEST speak of magnitude and equality (p.92, p.95, p.98). Field types include alphabetic, alphanumeric, and numeric (p.43).
- Known: numeric fields have assumed decimal points and optional signs.
- Not known: UNIVAC collation for letters, ignore, and mixed alphanumeric.
- Decision: numeric fields use decimal digit comparison with aligned assumed points (`fm_dec_compare_numeric`). Other fields use `memcmp` on extracted bytes. The policy is isolated behind `fm_rt_compare`.
- Compatibility: yes. A UNIVAC backend would need the real collating sequence, which is not in these files.

## 2. Ignore glyph in field descriptors

- Source: PP and S may be "ignores" (p.43-44, Form 3). The scanned glyph is not an ASCII character. OCR writes `/` or `I`.
- Decision: accept `/`, `i`, and `I` as ignore in PP and S only. Document `/` as the fixture transcription.
- Compatibility: host text only. Unityper ignore is not reproduced as a 6-bit code.

## 3. SET of operations other than JUMP

- Source: "Alters an operation, changing the order of execution" (p.92). Format names any operation number (p.97). Samples only SET JUMP (Fig.23 op 12; Fig.37 ops 3 and 7).
- Decision: SET is accepted only when the named operation is JUMP. The runtime mutates `run->jump[op]`.
- Compatibility: programs that SET a COMPARE or TEST would be rejected. None appear in the sample problems.

## 4. EXECUTE and JUMP out of range

- Source: EXECUTE "Performs designated operation or sequence of operations" (p.92). Sample 3 EXECUTE 13 THROUGH 17 includes JUMP 17 to 1 and SET of 13 to 18 (outside the range).
- Decision: EXECUTE pushes a return address. JUMP and COMPARE inside the range are honored. If the next operation would pass the range end, the frame pops. Jumping outside abandons the frame by taking the jump.
- Compatibility: possible. The manual does not define nested EXECUTE or JUMP-out.

## 5. CODE CONSTS vs CODE CONSTANTS

- Source: Appendix C prose (p.107) says "CODE CONSTANTS". Figure 44 (p.109) shows the 12-position title `CODE CONSTS`.
- Decision: the figure wins. The parser also accepts a longer phrase if someone types it, but dumps use `CODE CONSTS`.
- Compatibility: low. The tape word cannot be 13 characters.

## 6. J extra-line layout

- Source: p.106 shows `U0J011` then `000000QP.021`. Figure 44 shows a space-filled first half and `OP.013`.
- Decision: a line is a J operand line if it contains `OP.` followed by three digits. It is not given an M address.
- Compatibility: both documented forms parse.

## 7. Allocation addresses

- Source: Allocation assigns memory addresses (p.85). No map from FLOW-MATIC objects to UNIVAC locations is given.
- Decision: deterministic host addresses starting at fixed bases (`FM_ADDR_*`). Byte-identical dumps for identical input. Not UNIVAC locations.
- Compatibility: modern backend only.

## 8. Item sizes not on Form 2's list

- Source: Form 2 lists 1,2,3,4,5,6,10,12,15,20,30,60. Chapter 4 describes size as a word count (p.41).
- Decision: accept other sizes with warning FM3130.
- Compatibility: unknown whether the 1958 compiler rejected them.

## 9. Directory placement vs CLI files

- Source: Directory follows all Data Designs and precedes code (p.69).
- Decision: when files are passed separately, the CLI order `--design*` `--w-storage` `--directory` `--code` `--x1` is that historical order. A Directory file supplied without W-storage is an error. The compiler does not re-order files to "fix" a mistake.
- Compatibility: transport only.

## 10. Case

- Source: listings are uppercase Unityper text.
- Decision: matching is case-sensitive as written. There is no lenient mode.
- Compatibility: lowercase source is rejected.

## 11. X-1 instruction execution

- Source: examples such as `B0W001A-A001` (Fig.39). No opcode definition.
- Decision: parse, validate, relocate M fields symbolically, list in the Edited Record. Do not invent arithmetic from the English sentence. Execution requires `--x1-hooks`.
- Compatibility: cannot run Sample Problem 3's priced-path math without a programmer-supplied hook or a future UNIVAC interpreter.
