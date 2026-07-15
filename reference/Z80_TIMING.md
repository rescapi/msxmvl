# Z80 instruction timing (T-states) — reference for msxmvl

Authoritative cycle table for optimization. **T-states = base Z80 clock cycles.** On the MSX the CPU
runs at 3.579545 MHz and adds **+1 T-state per M1 (opcode) fetch** (memory contention on the M1 cycle),
so effective MSX time = base + (number of M1 fetches). Prefixed opcodes (CB/ED/DD/FD) have an extra M1
fetch for the prefix → +1 extra on MSX. Numbers below are **base**; MSX-effective notes added for the
hot cases used in VDP/VRAM loops.

Notation: r = 8-bit reg (A B C D E H L); rr = 16-bit (BC DE HL SP); (HL)/(IX+d) memory operand.
"taken/not" = conditional branch taken vs not taken.

## 8-bit load
| instr | T |
|---|---|
| ld r,r' | 4 |
| ld r,n | 7 |
| ld r,(HL) | 7 |
| ld r,(IX+d) / (IY+d) | 19 |
| ld (HL),r | 7 |
| ld (HL),n | 10 |
| ld (IX+d),r | 19 |
| ld (IX+d),n | 19 |
| ld A,(BC)/(DE) | 7 |
| ld A,(nn) | 13 |
| ld (BC)/(DE),A | 7 |
| ld (nn),A | 13 |
| ld A,I / A,R / I,A / R,A | 9 (ED-prefixed) |

## 16-bit load
| instr | T |
|---|---|
| ld rr,nn | 10 |
| ld HL,(nn) | 16 |
| ld rr,(nn) (ED) | 20 |
| ld (nn),HL | 16 |
| ld (nn),rr (ED) | 20 |
| ld SP,HL | 6 |
| push rr | 11 |
| pop rr | 10 |
| push/pop IX/IY | 15 |

## Exchange / block
| instr | T |
|---|---|
| ex DE,HL | 4 |
| ex AF,AF' | 4 |
| exx | 4 |
| ex (SP),HL | 19 |
| ex (SP),IX/IY | 23 |
| ldi / ldd | 16 |
| **ldir / lddr** | **21/byte** (16 last byte when BC→0) |
| cpi / cpd | 16 |
| cpir / cpdr | 21 (16 last) |

## 8-bit arithmetic / logic
| instr | T |
|---|---|
| add/adc/sub/sbc/and/or/xor/cp A,r | 4 |
| ... A,n | 7 |
| ... A,(HL) | 7 |
| ... A,(IX+d) | 19 |
| inc/dec r | 4 |
| inc/dec (HL) | 11 |
| inc/dec (IX+d) | 23 |
| daa, cpl, ccf, scf | 4 |
| neg (ED) | 8 |

## 16-bit arithmetic
| instr | T |
|---|---|
| add HL,rr | 11 |
| adc/sbc HL,rr (ED) | 15 |
| add IX/IY,rr | 15 |
| inc/dec rr | 6 |
| inc/dec IX/IY | 10 |

## Rotate / shift
| instr | T |
|---|---|
| rlca/rrca/rla/rra | 4 |
| rlc/rrc/rl/rr/sla/sra/srl/sll r (CB) | 8 |
| ... (HL) (CB) | 15 |
| ... (IX+d) (DDCB) | 23 |
| rld / rrd (ED) | 18 |

## Bit ops (CB-prefixed)
| instr | T |
|---|---|
| bit b,r | 8 |
| bit b,(HL) | 12 |
| set/res b,r | 8 |
| set/res b,(HL) | 15 |
| set/res/bit b,(IX+d) | 20/23 |

## Jump / call / return
| instr | T |
|---|---|
| jp nn | 10 |
| jp cc,nn | 10 (both taken and not — no penalty) |
| jp (HL) | 4 |
| jp (IX)/(IY) | 8 |
| jr e | 12 |
| **jr cc,e** | **12 taken / 7 not** |
| **djnz e** | **13 taken / 8 not** |
| call nn | 17 |
| call cc,nn | 17 taken / 10 not |
| ret | 10 |
| ret cc | 11 taken / 5 not |
| reti/retn (ED) | 14 |
| rst n | 11 |

## I/O — the VRAM-critical group
| instr | T (base) | MSX-effective | note |
|---|---|---|---|
| **out (n),A** | 11 | **12** (+1 M1) | fastest port write; **<15 T → unsafe in active display** |
| **in A,(n)** | 11 | 12 | fastest port read |
| out (C),r / in r,(C) (ED) | 12 | **14** (+2: prefix+M1) | any-register port I/O |
| **outi / ind / ini / outd (ED)** | 16 | **18** (+2) | block I/O one step; **≥15 T → active-display safe** |
| **otir / inir / otdr / indr (ED)** | 21/byte, 16 last | **~23/byte** (repeats; +2 per byte) | block I/O loop, no per-byte branch |

## Misc
| instr | T |
|---|---|
| nop | 4 |
| halt | 4 |
| di / ei | 4 |
| im 0/1/2 (ED) | 8 |

## VRAM-loop cheat sheet (msxmvl, from these numbers + V9938_REFERENCE.md)
- **Fastest safe VRAM copy (active display):** `otir` ≈ 23 T/byte, ≥15 T → safe screens 1–8.
- **Fastest safe VRAM byte-step:** `outi` = 18 T (safe screens 1–8; NOT screen 0 which needs ≥20).
- **Fastest raw fill (BLANKED only):** unrolled `out (0x98),a` = 12 T/byte (floor). During active
  display this is unsafe (<15 T) → blank or vblank.
- **Safe active-display fill:** `out`(12) + `djnz`(13) = 25 T/byte (≥20 → safe all modes incl. screen 0).
- **Keep the fill value in A**: `out (0x98),a` never alters A; use `djnz` (doesn't touch A) for the
  loop counter so you never reload the value inside the loop.
- Conditional `jp cc` costs the same taken or not (10 T) — prefer over `jr cc` (12/7) when the body is
  reached often and you're not size-bound.
