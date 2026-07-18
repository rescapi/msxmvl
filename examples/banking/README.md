# Banking examples — tested, runnable MegaROM programs

Each directory is a complete banked-ROM program: plain C + a `bank.manifest`,
built with `tools/bankpack.sh` and asserted headless on openMSX C-BIOS by its
`run.sh` (`apt install sdcc openmsx` is the whole toolchain). These are the same
programs the library's CI runs — if they are here, they passed.

| Example | Shows |
|---|---|
| `bank2/` | assembly banks; window switch + bank→resident call |
| `bankcall/` | C banks; nested bank→bank call via a dispatch table |
| `bankperf/` | the measured far-call cost (~170 T-states) |
| `farturnkey/` | the all-C turnkey flow: manifest + generated thunks |
| `bankdata/` | writable statics in every bank + `RESERVE` collision check |
| `bank16/` | ASCII-16: 16 KB segments, a checksummed >8 KB payload |
| `bankbig/` | 512 KB ASCII-8: statics in segment 63, geometry checks |
| `bankkonami/` | Konami mapper (resident pinned to segment 0) |
| `bankscc/` | Konami-SCC mapper + the segment-0x3F sound-chip refusal |

Run one: `bash examples/banking/farturnkey/run.sh`
