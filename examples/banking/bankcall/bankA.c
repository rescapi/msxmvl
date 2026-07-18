// Bank A (ROM segment 5), a C function that calls a function in ANOTHER bank.
// farA lives only in segment 5's window; it invokes farB (segment 6) through the
// resident thunk call_farB -- a bank -> bank call routed via the dispatch table.
//
// call_farB is a RESIDENT symbol (bankpack injects its address, sdld -g); the
// dispatch table entry for farB is patched by bankpack after all banks link.
typedef unsigned int u16;

extern u16 call_farB(u16 y);   // resident thunk -> segment 6

u16 farA(u16 x)
{
	return call_farB(x + 1) + 10;
}
