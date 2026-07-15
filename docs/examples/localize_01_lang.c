// Example: Loc_Initialize / Loc_SetLanguage / Loc_GetText — swap languages.
#include "localize.h"
volatile u8 __at(0xE000) R[8];

#define TEXT_MAX  2         // entries per language
#define TXT_YES   0
#define TXT_NO    1

// One pointer row per language, laid out back to back: row L starts at
// DataRoot + TEXT_MAX*2*L — exactly what Loc_SetLanguage computes.
static const c8* g_Loc[2 * TEXT_MAX] =
{
	"YES", "NO",            // language 0 — English
	"OUI", "NON",           // language 1 — French
};

void main(void)
{
	Loc_Initialize((const void*)g_Loc, TEXT_MAX);   // defaults to language 0

	R[1] = (u8)Loc_GetText(TXT_YES)[0];    // 'Y' = 0x59
	R[2] = (u8)Loc_GetText(TXT_NO)[0];     // 'N' = 0x4E

	Loc_SetLanguage(1);                    // switch to French
	R[3] = (u8)Loc_GetText(TXT_YES)[0];    // 'O' = 0x4F
	R[4] = (u8)Loc_GetText(TXT_NO)[0];     // 'N' = 0x4E

	R[0] = (R[1] == 'Y' && R[2] == 'N' && R[3] == 'O' && R[4] == 'N')
	     ? 0xA5 : 0x00;
	for (;;) {}
}
