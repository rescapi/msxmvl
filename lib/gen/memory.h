// msxmvl clean-room reimplementation of MSXgl "memory" module.
// Drop-in: exposes MSXgl's exact public names + signatures.
#ifndef MSXMVL_MEMORY_H
#define MSXMVL_MEMORY_H

#include "types.h"

// Feature switches. memory.c gates its bodies on these exactly as MSXgl does, so
// they MUST have defaults here: without them every `#if (MEM_USE_*)` evaluates to 0
// and a standalone build silently loses Mem_FastCopy, Mem_FastSet and the whole
// dynamic allocator, while memory.h keeps declaring them. In an MSXgl drop-in build
// the engine's config defines these first and these guards yield to it.
#ifndef MEM_USE_BUILTIN
	#define MEM_USE_BUILTIN		0	// provide real Mem_Copy/Mem_Set (not SDCC builtins)
#endif
#ifndef MEM_USE_FASTCOPY
	#define MEM_USE_FASTCOPY	1
#endif
#ifndef MEM_USE_FASTSET
	#define MEM_USE_FASTSET		1
#endif
#ifndef MEM_USE_DYNAMIC
	#define MEM_USE_DYNAMIC		1
#endif

//=============================================================================
// DEFINES
//=============================================================================

// Current heap top address
extern u16 g_HeapStartAddress;

// Dynamic memory chunk header (4 bytes). Size field first so that the inline
// Mem_GetDynamicSize() can read it at (ptr - sizeof(MemChunkHeader)).
typedef struct MemChunkHeader
{
	u16 Size;
	struct MemChunkHeader* Next;
} MemChunkHeader;

//=============================================================================
// FUNCTIONS
//=============================================================================

//.............................................................................
// Group: Heap handling

// Get the current address of the stack top (lower address).
u16 Mem_GetStackAddress();

// Get the current address of the heap top (higher address).
inline u16 Mem_GetHeapAddress() { return g_HeapStartAddress; }

// Get the amount of free space in the heap.
inline u16 Mem_GetHeapSize() { return Mem_GetStackAddress() - Mem_GetHeapAddress(); }

// Alloc a part of the heap.
inline void* Mem_HeapAlloc(u16 size) { u16 addr = g_HeapStartAddress; g_HeapStartAddress += size; return (void*)addr; }

// Free the last allocated area of the heap.
inline void Mem_HeapFree(u16 size) { g_HeapStartAddress -= size; }

//.............................................................................
// Group: Memory content modification

// Copy a memory block (minimal size of 1 byte). A size of 0 means 65536.
void Mem_Copy(const void* src, void* dest, u16 size);

// Copy a 16-bits memory block. size can be 1 to 32767. 0 means 32768.
inline void Mem_Copy_16b(const void* src, void* dest, u16 size) { Mem_Copy(src, dest, size * 2); }

// Fast copy a memory block. A size of 0 means 65536.
void Mem_FastCopy(const void* src, void* dest, u16 size);

// Fast copy a 16-bits memory block. size can be 1 to 32767. 0 means 32768.
inline void Mem_FastCopy_16b(const void* src, void* dest, u16 size) { Mem_FastCopy(src, dest, size * 2); }

// Fill a memory block with a given 8-bits value. A size of 0 means 65536.
void Mem_Set(u8 val, void* dest, u16 size);

// Fill a memory block with a given 16-bits value (stored big-endian, matching
// MSXgl). `size` is a BYTE count. 0 means 32768.
void Mem_Set_16b(u16 val, void* dest, u16 size);

// Fast fill a memory chunk with a given value. A size of 0 means 65536.
void Mem_FastSet(u8 val, void* dest, u16 size);

//.............................................................................
// Group: Dynamic allocator

// Initialize a static buffer used to allocate chunks dynamically.
void Mem_DynamicInitialize(void* base, u16 size);

// Initialize a dynamic allocator buffer taken from the heap.
inline void Mem_DynamicInitializeHeap(u16 size) { Mem_DynamicInitialize(Mem_HeapAlloc(size), size); }

// Allocate a memory chunk from the dynamic buffer (NULL if none available).
void* Mem_DynamicAlloc(u16 size);

// Free a memory chunk from the dynamic buffer.
void Mem_DynamicFree(void* ptr);

// Get the size of a dynamically allocated memory chunk.
inline u16 Mem_GetDynamicSize(void* ptr) { MemChunkHeader* chunk = (MemChunkHeader*)((u16)ptr - sizeof(MemChunkHeader)); return chunk->Size; }

#endif // MSXMVL_MEMORY_H
