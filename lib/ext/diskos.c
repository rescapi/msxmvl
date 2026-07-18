// msxmvl extension: mini-diskOS runtime — resolve a blob id to sectors and read it.
//
// No FAT walk: the whole "directory" is the fixed sector index that mkbootdsk.sh
// wrote into sector 0 (see diskos.h for the byte layout). We cache sector 0 on the
// first call and answer every lookup out of that one buffer.
#include "diskos.h"
#include "disk.h"

//=============================================================================
// GLOBALS
//=============================================================================

static u8 s_Index[512];         // cached copy of sector 0 (boot sector + index)
static u8 s_Loaded = 0;         // 0 until s_Index holds a valid index

//=============================================================================
// INDEX LOOKUP
//=============================================================================

// Read + validate the index once, then return a pointer to the 7-byte record for
// `id`, or 0 if the index is unreadable/invalid or no entry matches.
static u8* DiskOS_Find(u8 id)
{
	u8* p;
	u8  n, i;

	if (!s_Loaded)
	{
		if (DiskDOS_ReadSectors(DISKOS_INDEX_SECTOR, 1, s_Index) != 0)
			return 0;                       // disk read failed
		// magic 'D','K' at the index offset marks a real diskOS image
		if (s_Index[DISKOS_INDEX_OFFSET + 0] != 0x44 ||
		    s_Index[DISKOS_INDEX_OFFSET + 1] != 0x4B)
			return 0;
		s_Loaded = 1;
	}

	n = s_Index[DISKOS_INDEX_OFFSET + 2];           // entry count
	p = &s_Index[DISKOS_INDEX_OFFSET + 3];          // first entry
	for (i = 0; i < n; i++)
	{
		if (p[0] == id)
			return p;
		p += DISKOS_ENTRY_SIZE;
	}
	return 0;                                       // no such id
}

//=============================================================================
// FUNCTIONS
//=============================================================================

u8 DiskOS_Load(u8 id, void* dst)
{
	u8* e = DiskOS_Find(id);
	u8* d = (u8*)dst;
	u16 start, count;
	u8  chunk, err;

	if (!e)
		return 0xFF;                            // not found

	start = e[1] | ((u16)e[2] << 8);
	count = e[3] | ((u16)e[4] << 8);

	// DiskDOS_ReadSectors takes a u8 sector count; read in <= 64-sector (32 KB)
	// chunks so both the count and the dst pointer advance stay in range.
	while (count)
	{
		chunk = (count > 64) ? 64 : (u8)count;
		err = DiskDOS_ReadSectors(start, chunk, d);
		if (err)
			return err;
		start += chunk;
		count -= chunk;
		d     += (u16)chunk * 512;
	}
	return 0;
}

u16 DiskOS_Length(u8 id)
{
	u8* e = DiskOS_Find(id);
	if (!e)
		return 0;
	return e[5] | ((u16)e[6] << 8);
}

u16 DiskOS_Sectors(u8 id)
{
	u8* e = DiskOS_Find(id);
	if (!e)
		return 0;
	return e[3] | ((u16)e[4] << 8);
}
