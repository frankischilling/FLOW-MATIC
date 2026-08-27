# Backend feasibility

Question: do the two primary sources specify enough to generate an authentic, executable, bit-for-bit UNIVAC II program tape?

## What the sources do describe

Chapter 7 (printed pages 83-89) names four compilation phases (Translation, Selection, Allocation, Processing), Operations Files 1-3, the Generated Library, the Unedited Record, and the Edited Record. It shows compilation printouts and a diagram of the compiled program tape at the level of blocks and segments, not instruction encodings.

Appendix C (printed pages 105-109) describes X-1 relative addresses (file letters A-I, W, M, J, T), section layout, constants, code constants, and Selector-phase error printouts. Figure 44 shows 12-character instruction words with address letters in the third and ninth positions. It does not give a complete opcode map or a binary packing of those characters onto UNIVAC II media.

Chapter 4 and Appendix B describe Data Design packets, field descriptors `OOOOOTPPSLNO`, extractors, and sentinels. That is enough to model items and fields in a host runtime.

## What is missing

The two files do not specify:

- machine instruction encodings (bit patterns, not just character mnemonics in examples)
- complete FLOW-MATIC library-generator bodies
- exact memory and tape encoding of the compiled program
- character representation and comparison order (collation)
- all relocation arithmetic for UNIVAC memory
- hardware-specific runtime routines (Uniservo label checks, High-Speed Printer, Tape-to-Card, breakpoints, rerun)

Without those pieces, a bit-for-bit UNIVAC II tape cannot be traced to the supplied documents.

## Decision

This project keeps the four-phase architecture and emits Operations Files 1-3, a Generated Library dump, and an Edited Record. Processing writes portable C17 and links it to a C runtime that models files, items, fields, W-storage, sentinels, JUMP, SET, EXECUTE, and STOP. The backend interface is modular so a verified UNIVAC backend could be added later. Exact UNIVAC II binary compatibility is not claimed.
