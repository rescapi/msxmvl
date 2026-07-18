// Bank B (ROM segment 6). Leaf C function reached only when segment 6 is paged
// into the window. farA (segment 5) calls this through the resident dispatcher;
// the nesting must restore segment 5 afterwards so farA can keep executing.
typedef unsigned int u16;

u16 farB(u16 y)
{
	return y + 100;
}
