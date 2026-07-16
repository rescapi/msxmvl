// ui_text.c — the game's UI strings in every language, switched at runtime.
//
// All the text lives in one flat table: one row of TEXT_MAX pointers per language,
// the languages laid out back to back. Loc_SetLanguage repoints at a row (a pointer
// add, no copying); Loc_GetText is then a single array index.
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

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	ui_text_init();                        // binds the table; defaults to language 0

	R[1] = (u8)ui_text(TXT_YES)[0];        // 'Y' = 0x59
	R[2] = (u8)ui_text(TXT_NO)[0];         // 'N' = 0x4E

	ui_set_language(1);                    // switch to French
	R[3] = (u8)ui_text(TXT_YES)[0];        // 'O' = 0x4F
	R[4] = (u8)ui_text(TXT_NO)[0];         // 'N' = 0x4E

	R[0] = (R[1] == 'Y' && R[2] == 'N' && R[3] == 'O' && R[4] == 'N')
	     ? 0xA5 : 0x00;
	for (;;) {}
}
