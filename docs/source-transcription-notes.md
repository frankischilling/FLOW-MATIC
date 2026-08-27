# Source transcription notes

The scanned PDF is the authority when the OCR text file disagrees.

| Topic | TXT / OCR | PDF | Action |
|---|---|---|---|
| Sample 1 COMPARE punctuation | `s IF 6REATER`, `j OTHERWISE` | semicolons; GREATER | Fixture uses PDF Figure 23 |
| Sample 1 CLOSE-OUT | `CLOSE-OUT FILE C . D .` | `CLOSE-OUT FILES C , D .` | PDF |
| Sample 3 MOVE to file D | OCR `QUANTITY (P)` | `QUANTITY (D)` Figure 37 | PDF |
| Sample 3 lowercase `w` | `PRODUCT-NO (w)` | `(W)` | PDF |
| Appendix A spaces | `A` used as space stand-in | delta glyph Δ | Compiler source uses ASCII space |
| Field ignore | `///` or `III` | ignore glyph on Form 3 | Fixtures use `/` |
| CODE CONSTANTS | prose p.107 | Figure 44 `CODE CONSTS` | Figure |
| STOP (END) | sometimes `STOP. (EHD)` | `STOP . (END)` | PDF |
| Directory sentinel | `ENDADIRECTRY` with OCR noise | `END DIRECTRY` | 12-position `END DIRECTRY` |
| X-1 header | `X-l` (ell) | `X-1` | Digit 1. `X-I` also accepted as the same token |
| INPUT SERVOS | mixed SERVO/SERVOS | p.96 stacked SERVO / SERVOS | both accepted |
| Figure 23 op 1 greater target | some OCR says 10 | 10 in Figure 23 | 10 |
| Figure 31 greater target | OCR `rll` / `Ml` | 14 | 14 |
| UNIT-PRICE descriptor | `0000033R 850` broken | packed 3R, extractor `000000011111` | `0000033R/850` in fixtures |
| Edited Record sample Figure 43 | "not an actual listing from the Sample Problems" (p.89) | same | Not used as a golden listing of Sample 1 |

Delta (Δ) in the manual is a space in a 12-position word, not a character the compiler requires in UTF-8 source.
