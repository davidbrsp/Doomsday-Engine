/**
 * @file dd_zone.c
 * Implementation of the memory zone. @ingroup memzone
 *
 * The zone is composed of multiple memory volumes.
 *
 * There is never any space between memblocks, and there will never be
 * two contiguous free memblocks.
 *
 * The rover can be left pointing at a non-empty block.
 *
 * It is of no value to free a cachable block, because it will get
 * overwritten automatically if needed.
 *
 * When fast malloc mode is enabled, memory volumes aren't checked for purgable
 * blocks. If the rover block isn't suitable, a new empty volume is created
 * without further checking. This is suitable for cases where lots of blocks
 * are being allocated in a rapid sequence, with no frees in between (e.g.,
 * map setup).
 *
 * @par Block Sequences
 * The PU_MAPSTATIC purge tag has a special purpose.
 * It works like PU_MAP so that it is purged on a per map basis, but
 * blocks allocated as PU_MAPSTATIC should not be freed at any time when the
 * map is being used. Internally, the map-static blocks are linked into
 * sequences so that Z_Malloc knows to skip all of them efficiently. This is
 * possible because no block inside the sequence could be purged by Z_Malloc
 * anyway.
 *
 * @authors Copyright © 1999-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 1993-1996 by id Software, Inc.
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <stdlib.h>
#include <assert.h> // Define NDEBUG in release builds.

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// Size of one memory zone volume.
#define MEMORY_VOLUME_SIZE  0x2000000   // 32 Mb

#define ZONEID  0x1d4a11

#define MINFRAGMENT (sizeof(memblock_t)+32)

#define ALIGNED(x) (((x) + sizeof(void*) - 1)&(~(sizeof(void*) - 1)))

/**
 * The memory is composed of multiple volumes.  New volumes are
 * allocated when necessary.
 */
typedef struct memvolume_s {
    memzone_t  *zone;
    size_t      size;
    struct memvolume_s *next;
} memvolume_t;

// Used for block allocation of memory from the zone.
typedef struct zblockset_block_s {
    /// Maximum number of elements.
    unsigned int max;

    /// Number of used elements.
    unsigned int count;

    /// Size of a single element.
    size_t elementSize;

    /// Block of memory where elements are.
    void* elements;
} zblockset_block_t;

static memvolume_t *volumeRoot;

/**
 * If false, Z_Malloc will free purgable blocks and aggressively look for
 * free memory blocks inside each memory volume before creating new volumes.
 * This leads to slower mallocing performance, but reduces memory fragmentation
 * as free and purgable blocks are utilized within the volumes. Fast mode is
 * enabled during map setup because a large number of mallocs will occur
 * during setup.
 */
static boolean fastMalloc = false;

static mutex_t zoneMutex = 0;

static __inline void lockZone(void)
{
    assert(zoneMutex != 0);
    Sys_Lock(zoneMutex);
}

static __inline void unlockZone(void)
{
    Sys_Unlock(zoneMutex);
}

/**
 * Conversion from string to long, with the "k" and "m" suffixes.
 */
long superatol(char *s)
{
    char           *endptr;
    long            val = strtol(s, &endptr, 0);

    if(*endptr == 'k' || *endptr == 'K')
        val *= 1024;
    else if(*endptr == 'm' || *endptr == 'M')
        val *= 1048576;
    return val;
}

/**
 * Enables or disables fast malloc mode. Enable for added performance during
 * map setup. Disable fast mode during other times to save memory and reduce
 * fragmentation.
 *
 * @param isEnabled  true or false.
 */
void Z_EnableFastMalloc(boolean isEnabled)
{
    lockZone();

    fastMalloc = isEnabled;

    unlockZone();
}

/**
 * Create a new memory volume.  The new volume is added to the list of
 * memory volumes.
 */
memvolume_t *Z_Create(size_t volumeSize)
{
    memblock_t     *block;
    memvolume_t    *vol = M_Calloc(sizeof(memvolume_t));

    lockZone();

    vol->next = volumeRoot;
    volumeRoot = vol;
    vol->size = volumeSize;

    // Allocate memory for the zone volume.
    vol->zone = M_Malloc(vol->size);

    // Clear the start of the zone.
    memset(vol->zone, 0, sizeof(memzone_t) + sizeof(memblock_t));
    vol->zone->size = vol->size;

    // Set the entire zone to one free block.
    vol->zone->blockList.next
        = vol->zone->blockList.prev
        = block
        = (memblock_t *) ((byte *) vol->zone + sizeof(memzone_t));

    vol->zone->blockList.user = (void *) vol->zone;
    vol->zone->blockList.volume = vol;
    vol->zone->blockList.tag = PU_APPSTATIC;
    vol->zone->rover = block;

    block->prev = block->next = &vol->zone->blockList;
    block->user = NULL;         // free block
    block->seqFirst = block->seqLast = NULL;
    block->size = vol->zone->size - sizeof(memzone_t);

    unlockZone();

    VERBOSE(Con_Message("Z_Create: New %.1f MB memory volume.\n", vol->size / 1024.0 / 1024.0));

    return vol;
}

boolean Z_IsInited(void)
{
    return zoneMutex != 0;
}

int Z_Init(void)
{
    zoneMutex = Sys_CreateMutex("ZONE_MUTEX");

    // Create the first volume.
    Z_Create(MEMORY_VOLUME_SIZE);
    return true;
}

/**
 * Shut down the memory zone by destroying all the volumes.
 */
void Z_Shutdown(void)
{
    int             numVolumes = 0;
    size_t          totalMemory = 0;

    // Destroy all the memory volumes.
    while(volumeRoot)
    {
        memvolume_t *vol = volumeRoot;
        volumeRoot = vol->next;

        // Calculate stats.
        numVolumes++;
        totalMemory += vol->size;

#ifdef FAKE_MEMORY_ZONE
        Z_FreeTags(0, DDMAXINT);
#endif

        M_Free(vol->zone);
        M_Free(vol);
    }

    printf("Z_Shutdown: Used %i volumes, total %lu bytes.\n",
           numVolumes, (long unsigned int) totalMemory);

    Sys_DestroyMutex(zoneMutex);
    zoneMutex = 0;
}

#ifdef FAKE_MEMORY_ZONE
memblock_t *Z_GetBlock(void *ptr)
{
    memvolume_t    *volume;
    memblock_t     *block;

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = block->next)
        {
            if(block->area == ptr)
            {
                return block;
            }
        }
    }
    Con_Error("Z_GetBlock: There is no memory block for %p.\n", ptr);
    return NULL;
}
#endif

/**
 * Free memory that was allocated with Z_Malloc.
 */
void Z_Free(void *ptr)
{
    memblock_t     *block, *other;
    memvolume_t    *volume;

    if(!ptr)
    {
        VERBOSE(Con_Message("Z_Free: Warning: Attempt to free NULL ignored.\n") );
        return;
    }

    lockZone();

    block = Z_GetBlock(ptr);
    if(block->id != ZONEID)
    {
        unlockZone();
        Con_Error("Z_Free: Attempt to free pointer without ZONEID.");
    }

    // The block was allocated from this volume.
    volume = block->volume;

    if(block->user > (void **) 0x100) // Smaller values are not pointers.
        *block->user = 0; // Clear the user's mark.
    block->user = NULL; // Mark as free.
    block->tag = 0;
    block->volume = NULL;
    block->id = 0;

#ifdef FAKE_MEMORY_ZONE
    M_Free(block->area);
    block->area = NULL;
    block->areaSize = 0;
#endif

    /**
     * Erase the entire sequence, if there is one.
     * It would also be possible to carefully break the sequence in two
     * parts, but since PU_LEVELSTATICs aren't supposed to be freed one by
     * one, this this sufficient.
     */
    if(block->seqFirst)
    {
        memblock_t* first = block->seqFirst;
        memblock_t* iter = first;
        while(iter->seqFirst == first)
        {
            iter->seqFirst = iter->seqLast = NULL;
            iter = iter->next;
        }
    }

    other = block->prev;
    if(!other->user)
    {   // Merge with previous free block.
        other->size += block->size;
        other->next = block->next;
        other->next->prev = other;
        if(block == volume->zone->rover)
            volume->zone->rover = other;
        block = other;
    }

    other = block->next;
    if(!other->user)
    {   // Merge the next free block onto the end.
        block->size += other->size;
        block->next = other->next;
        block->next->prev = block;
        if(other == volume->zone->rover)
            volume->zone->rover = block;
    }

    unlockZone();
}

/**
 * You can pass a NULL user if the tag is < PU_PURGELEVEL.
 */
void *Z_Malloc(size_t size, int tag, void *user)
{
    size_t          extra;
    memblock_t     *start, *rover, *new, *base;
    memvolume_t    *volume;
    boolean         gotoNextVolume;

    if(tag < PU_APPSTATIC || tag > PU_CACHE)
    {
        Con_Error("Z_Malloc: Invalid purgelevel %i.", tag);
    }

    if(!size)
    {
        // You can't allocate "nothing."
        return NULL;
    }

    lockZone();

    // Align to pointer size.
    size = ALIGNED(size);

    // Account for size of block header.
    size += sizeof(memblock_t);

    // Iterate through memory volumes until we can find one with
    // enough free memory.  (Note: we *will* find one that's large
    // enough.)
    for(volume = volumeRoot; ; volume = volume->next)
    {
        if(volume == NULL)
        {
            // We've run out of volumes.  Let's allocate a new one
            // with enough memory.
            size_t newVolumeSize = MEMORY_VOLUME_SIZE;

            if(newVolumeSize < size + 0x1000)
                newVolumeSize = size + 0x1000; // with some spare memory

            volume = Z_Create(newVolumeSize);
        }

        if(!volume->zone)
        {
            unlockZone();
            Con_Error("Z_Malloc: Volume without zone.");
        }

        // Scan through the block list looking for the first free block of
        // sufficient size, throwing out any purgable blocks along the
        // way.

        // If there is a free block behind the rover, back up over them.
        base = volume->zone->rover;
        assert(base->prev);
        if(!base->prev->user)
            base = base->prev;

        gotoNextVolume = false;
        if(fastMalloc)
        {
            // In fast malloc mode, if the rover block isn't large enough,
            // just give up and move to the next volume right away.
            if(base->user || base->size < size)
            {
                gotoNextVolume = true;
            }
        }

        if(!gotoNextVolume)
        {
            boolean     isDone;

            rover = base;
            start = base->prev;

            // If the start is in a sequence, move it to the beginning of the
            // entire sequence. Sequences are handled as a single unpurgable entity,
            // so we can stop checking at its start.
            if(start->seqFirst)
            {
                start = start->seqFirst;
            }

            isDone = false;
            do
            {
                if(rover != start)
                {
                    if(rover->user)
                    {
                        if(rover->tag < PU_PURGELEVEL)
                        {
                            if(rover->seqFirst)
                            {
                                // This block is part of a sequence of blocks, none of
                                // which can be purged. Skip the entire sequence.
                                base = rover = rover->seqFirst->seqLast->next;
                            }
                            else
                            {
                                // Hit a block that can't be purged, so move base
                                // past it.
                                base = rover = rover->next;
                            }
                        }
                        else
                        {
                            // Free the rover block (adding the size to base).
                            base = base->prev;  // the rover can be the base block
#ifdef FAKE_MEMORY_ZONE
                            Z_Free(rover->area);
#else
                            Z_Free((byte *) rover + sizeof(memblock_t));
#endif
                            base = base->next;
                            rover = base->next;
                        }
                    }
                    else
                    {
                        rover = rover->next;
                    }
                }
                else
                {
                    // Scanned all the way around the list.
                    // Move over to the next volume.
                    gotoNextVolume = true;
                }

                // Have we finished?
                if(gotoNextVolume)
                    isDone = true;
                else
                    isDone = !(base->user || base->size < size);
            } while(!isDone);

            // At this point we've found/created a big enough block or we are
            // skipping this volume entirely.

            if(!gotoNextVolume)
            {
                // Found a block big enough.
                extra = base->size - size;
                if(extra > MINFRAGMENT)
                {
                    // There will be a free fragment after the allocated
                    // block.
                    new = (memblock_t *) ((byte *) base + size);
                    new->size = extra;
                    new->user = NULL;       // free block
                    new->tag = 0;
                    new->volume = NULL;
                    new->prev = base;
                    new->next = base->next;
                    new->next->prev = new;
                    new->seqFirst = new->seqLast = NULL;
#ifdef FAKE_MEMORY_ZONE
                    new->area = 0;
                    new->areaSize = 0;
#endif
                    base->next = new;
                    base->size = size;
                }

#ifdef FAKE_MEMORY_ZONE
                base->areaSize = size - sizeof(memblock_t);
                base->area = M_Malloc(base->areaSize);
#endif

                if(user)
                {
                    base->user = user;      // mark as an in use block
#ifdef FAKE_MEMORY_ZONE
                    *(void **) user = base->area;
#else
                    *(void **) user = (void *) ((byte *) base + sizeof(memblock_t));
#endif
                }
                else
                {
                    if(tag >= PU_PURGELEVEL)
                    {
                        unlockZone();
                        Con_Error("Z_Malloc: an owner is required for "
                                  "purgable blocks.\n");
                    }
                    base->user = (void *) 2;    // mark as in use, but unowned
                }
                base->tag = tag;

                if(tag == PU_MAPSTATIC)
                {
                    // Level-statics are linked into unpurgable sequences so they can
                    // be skipped en masse.
                    base->seqFirst = base;
                    base->seqLast = base;
                    if(base->prev->seqFirst)
                    {
                        base->seqFirst = base->prev->seqFirst;
                        base->seqFirst->seqLast = base;
                    }
                }
                else
                {
                    // Not part of a sequence.
                    base->seqLast = base->seqFirst = NULL;
                }

                // next allocation will start looking here
                volume->zone->rover = base->next;

                base->volume = volume;
                base->id = ZONEID;

                unlockZone();

#ifdef FAKE_MEMORY_ZONE
                return base->area;
#else
                return (void *) ((byte *) base + sizeof(memblock_t));
#endif
            }
        }
        // Move to the next volume.
    }
}

/**
 * Only resizes blocks with no user. If a block with a user is
 * reallocated, the user will lose its current block and be set to
 * NULL. Does not change the tag of existing blocks.
 */
void *Z_Realloc(void *ptr, size_t n, int mallocTag)
{
    int     tag = ptr ? Z_GetTag(ptr) : mallocTag;
    void   *p;

    lockZone();

    n = ALIGNED(n);
    p = Z_Malloc(n, tag, 0);    // User always 0;

    if(ptr)
    {
        size_t bsize;

        // Has old data; copy it.
        memblock_t *block = Z_GetBlock(ptr);
#ifdef FAKE_MEMORY_ZONE
        bsize = block->areaSize;
#else
        bsize = block->size - sizeof(memblock_t);
#endif
        memcpy(p, ptr, MIN_OF(n, bsize));
        Z_Free(ptr);
    }

    unlockZone();
    return p;
}

/**
 * Free memory blocks in all volumes with a tag in the specified range.
 */
void Z_FreeTags(int lowTag, int highTag)
{
    memvolume_t *volume;
    memblock_t *block, *next;

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = next)
        {
            next = block->next;
            if(block->user)
            {
                if(block->tag >= lowTag && block->tag <= highTag)
#ifdef FAKE_MEMORY_ZONE
                    Z_Free(block->area);
#else
                    Z_Free((byte *) block + sizeof(memblock_t));
#endif
            }
        }
    }
}

/**
 * Check all zone volumes for consistency.
 */
void Z_CheckHeap(void)
{
    memvolume_t *volume;
    memblock_t *block;
    boolean     isDone;

#ifdef _DEBUG
    VERBOSE2( Con_Message("Z_CheckHeap\n") );
#endif

    lockZone();

    for(volume = volumeRoot; volume; volume = volume->next)
    {
        block = volume->zone->blockList.next;
        isDone = false;
        while(!isDone)
        {
            if(block->next != &volume->zone->blockList)
            {
                if(block->size == 0)
                    Con_Error("Z_CheckHeap: zero-size block\n");
                if((byte *) block + block->size != (byte *) block->next)
                    Con_Error("Z_CheckHeap: block size does not touch the "
                              "next block\n");
                if(block->next->prev != block)
                    Con_Error("Z_CheckHeap: next block doesn't have proper "
                              "back link\n");
                if(!block->user && !block->next->user)
                    Con_Error("Z_CheckHeap: two consecutive free blocks\n");
                if(block->user == (void **) -1)
                    Con_Error("Z_CheckHeap: bad user pointer %p\n", block->user);

                /*
                if(block->seqFirst == block)
                {
                    // This is the first.
                    printf("sequence begins at (%p): start=%p, end=%p\n", block,
                           block->seqFirst, block->seqLast);
                }
                 */
                if(block->seqFirst)
                {
                    //printf("  seq member (%p): start=%p\n", block, block->seqFirst);
                    if(block->seqFirst->seqLast == block)
                    {
                        //printf("  -=- last member of seq %p -=-\n", block->seqFirst);
                    }
                    else
                    {
                        if(block->next->seqFirst != block->seqFirst)
                        {
                            Con_Error("Z_CheckHeap: disconnected sequence\n");
                        }
                    }
                }

                block = block->next;
            }
            else
                isDone = true; // all blocks have been hit
        }
    }

    unlockZone();
}

/**
 * Change the tag of a memory block.
 */
void Z_ChangeTag2(void *ptr, int tag)
{
    lockZone();
    {
        memblock_t *block = Z_GetBlock(ptr);

        if(block->id != ZONEID)
            Con_Error("Z_ChangeTag: Modifying a block without ZONEID.");

        if(tag >= PU_PURGELEVEL && (unsigned long) block->user < 0x100)
            Con_Error("Z_ChangeTag: An owner is required for purgable blocks.");
        block->tag = tag;
    }
    unlockZone();
}

/**
 * Change the user of a memory block.
 */
void Z_ChangeUser(void *ptr, void *newUser)
{
    lockZone();
    {
        memblock_t *block = Z_GetBlock(ptr);

        if(block->id != ZONEID)
            Con_Error("Z_ChangeUser: Block without ZONEID.");
        block->user = newUser;
    }
    unlockZone();
}

/**
 * Get the user of a memory block.
 */
void *Z_GetUser(void *ptr)
{
    memblock_t     *block = Z_GetBlock(ptr);

    if(block->id != ZONEID)
        Con_Error("Z_GetUser: Block without ZONEID.");
    return block->user;
}

/**
 * Get the tag of a memory block.
 */
int Z_GetTag(void *ptr)
{
    memblock_t     *block = Z_GetBlock(ptr);

    if(block->id != ZONEID)
        Con_Error("Z_GetTag: Block without ZONEID.");
    return block->tag;
}

/**
 * Memory allocation utility: malloc and clear.
 */
void *Z_Calloc(size_t size, int tag, void *user)
{
    void *ptr = Z_Malloc(size, tag, user);

    memset(ptr, 0, ALIGNED(size));
    return ptr;
}

/**
 * Realloc and set possible new memory to zero.
 */
void *Z_Recalloc(void *ptr, size_t n, int callocTag)
{
    memblock_t     *block;
    void           *p;
    size_t          bsize;

    lockZone();

    n = ALIGNED(n);

    if(ptr)                     // Has old data.
    {
        p = Z_Malloc(n, Z_GetTag(ptr), NULL);
        block = Z_GetBlock(ptr);
#ifdef FAKE_MEMORY_ZONE
        bsize = block->areaSize;
#else
        bsize = block->size - sizeof(memblock_t);
#endif
        if(bsize <= n)
        {
            memcpy(p, ptr, bsize);
            memset((char *) p + bsize, 0, n - bsize);
        }
        else
        {
            // New block is smaller.
            memcpy(p, ptr, n);
        }
        Z_Free(ptr);
    }
    else
    {   // Totally new allocation.
        p = Z_Calloc(n, callocTag, NULL);
    }

    unlockZone();

    return p;
}

/**
 * Calculate the amount of unused memory in all volumes combined.
 */
size_t Z_FreeMemory(void)
{
    memvolume_t    *volume;
    memblock_t     *block;
    size_t          free = 0;

    lockZone();

    Z_CheckHeap();
    for(volume = volumeRoot; volume; volume = volume->next)
    {
        for(block = volume->zone->blockList.next;
            block != &volume->zone->blockList;
            block = block->next)
        {
            if(!block->user)
            {
                free += block->size;
            }
        }
    }

    unlockZone();
    return free;
}

/**
 * Allocate a new block of memory to be used for linear object allocations.
 * A "zblock" (its from the zone).
 *
 * @param zblockset_t*  Block set into which the new block is added.
 */
static void addBlockToSet(zblockset_t* set)
{
    assert(set);
    {
    zblockset_block_t* block = 0;

    // Get a new block by resizing the blocks array. This is done relatively
    // seldom, since there is a large number of elements per each block.
    set->_blockCount++;
    set->_blocks = Z_Recalloc(set->_blocks, sizeof(zblockset_block_t) * set->_blockCount, set->_tag);

    // Initialize the block's data.
    block = &set->_blocks[set->_blockCount - 1];
    block->max = set->_elementsPerBlock;
    block->elementSize = set->_elementSize;
    block->elements = Z_Malloc(block->elementSize * block->max, set->_tag, NULL);
    block->count = 0;
    }
}

void* ZBlockSet_Allocate(zblockset_t* set)
{
    zblockset_block_t* block = 0;
    void* element = 0;

    assert(set);
    lockZone();

    block = &set->_blocks[set->_blockCount - 1];

    // When this is called, there is always an available element in the topmost
    // block. We will return it.
    element = ((byte*)block->elements) + (block->elementSize * block->count);

    // Reserve the element.
    block->count++;

    // If we run out of space in the topmost block, add a new one.
    if(block->count == block->max)
    {
        // Just being cautious: adding a new block invalidates existing
        // pointers to the blocks.
        block = 0;

        addBlockToSet(set);
    }

    unlockZone();
    return element;
}

zblockset_t* ZBlockSet_New(size_t sizeOfElement, unsigned int batchSize, int tag)
{
    zblockset_t* set;

    if(sizeOfElement == 0)
        Con_Error("Attempted ZBlockSet_New with invalid sizeOfElement (==0).");

    if(batchSize == 0)
        Con_Error("Attempted ZBlockSet_New with invalid batchSize (==0).");

    // Allocate the blockset.
    set = Z_Calloc(sizeof(*set), tag, NULL);
    set->_elementsPerBlock = batchSize;
    set->_elementSize = sizeOfElement;
    set->_tag = tag;

    // Allocate the first block right away.
    addBlockToSet(set);

    return set;
}

void ZBlockSet_Delete(zblockset_t* set)
{
    lockZone();
    assert(set);

    // Free the elements from each block.
    { uint i;
    for(i = 0; i < set->_blockCount; ++i)
        Z_Free(set->_blocks[i].elements);
    }

    Z_Free(set->_blocks);
    Z_Free(set);
    unlockZone();
}
