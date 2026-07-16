# Localization (`localize`)

Keep your UI text in one table per language and swap languages at runtime with a single call. Link
`localize.c`, include `localize.h`.

## The model

All the text lives in one flat array of string pointers, laid out as a **matrix**: one contiguous
row of `TEXT_MAX` pointers per language, languages back to back.

```
         index 0    index 1
lang 0 [  "YES",     "NO"   ]   <- data root
lang 1 [  "OUI",     "NON"  ]   <- root + TEXT_MAX*2 bytes
```

`Loc_SetLanguage(L)` just repoints `g_Loc_CurLang` at row `L` (`root + TEXT_MAX * 2 * L`), so
switching language is a pointer add — no copying, no search. `Loc_GetText(i)` is then a single
array index. Both are inline; only `Loc_Initialize` is a real call.

Keep the whole table `const` and it lives in ROM, costing you just the 2-byte `g_Loc_CurLang`
pointer in RAM.

## API

```c
void        Loc_Initialize(const void* data, u8 num);  // bind the table; selects language 0
void        Loc_SetLanguage(u8 lang);                  // 0 .. LANG_MAX-1
const c8*   Loc_GetText(u16 text);                     // 0 .. TEXT_MAX-1
```

`num` is the number of entries **per language** — the row stride. Getting it wrong silently points
`Loc_SetLanguage` at the wrong row, so define it once and use the symbol everywhere.

## Switching language

Put the whole UI text table in ROM, one row per language, and wrap the three calls in verbs the
rest of the game can use. `ui_text_init` binds the table (and selects language 0); `ui_set_language`
flips rows; `ui_text` fetches one string.

```c
// ui_text.c — the game's UI strings in every language, switched at runtime.
#include "localize.h"

#define TEXT_MAX  2         // strings per language
#define TXT_YES   0         // name each string slot
#define TXT_NO    1

// One pointer row per language: English, then French, back to back.
static const c8* g_Strings[2 * TEXT_MAX] =
{
	"YES", "NO",            // language 0 — English
	"OUI", "NON",           // language 1 — French
};

void ui_text_init(void)    { Loc_Initialize((const void*)g_Strings, TEXT_MAX); }
void ui_set_language(u8 l)  { Loc_SetLanguage(l); }
const c8* ui_text(u16 id)  { return Loc_GetText(id); }
```

Straight after `ui_text_init()`, `ui_text(TXT_YES)` starts with `'Y'` and `ui_text(TXT_NO)` with
`'N'`. Call `ui_set_language(1)` and the same two slots now read `'O'` (OUI) and `'N'` (NON) — the
switch is a single pointer add. *(tested: `localize_01_lang.c`)*

## Using it

Give every string an enum so the call sites read well and the indices stay in sync with the table:

```c
enum { TXT_START, TXT_OPTIONS, TXT_QUIT, TEXT_MAX };

Loc_Initialize(g_Loc, TEXT_MAX);
Loc_SetLanguage(g_Config.Language);

Print_DrawText(Loc_GetText(TXT_START));
```

There is **no bounds check** on either call: an out-of-range text index reads whatever follows the
table, and an out-of-range language index points at memory past it. Both are just pointer
arithmetic, which is what makes them fast.

## See also

- [Text Output](Text-Output.md) — drawing the strings you get back.
- [String Conversion](String-Conversion.md) — numbers → text, for the parts you can't localize.
