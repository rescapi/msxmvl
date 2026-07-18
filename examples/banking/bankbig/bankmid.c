// Mid-image bank (SEG 5) — proves an ordinary bank still works unchanged when
// the image around it is 512 KB.
#include "types.h"
#include "far.h"

FAR_FN u16 mid_tag(u16 unused)
{
	(void)unused;
	return 0x0C0D;
}
