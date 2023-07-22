/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "BLI_sys_types.h" /* for intptr_t support */
#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h" /* for BLI_assert */

#include "CCGSubSurf.h"
#include "CCGSubSurf_intern.h"

/**
 * Hash implementation.
 */

static int kHashSizes[] = {
    1,       3,       5,       11,      17,       37,       67,       131,       257,       521,
    1031,    2053,    4099,    8209,    16411,    32771,    65537,    131101,    262147,    524309,
    1048583, 2097169, 4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 268435459,
};

/* -------------------------------------------------------------------- */
/** \name Generic Hash Functions
 * \{ */

EHash *ccg_ehash_new(int estimatedNumEntries,
                     CCGAllocatorIFC *allocatorIFC,
                     CCGAllocatorHDL allocator)
{
  EHash *eh = static_cast<EHash *>(allocatorIFC->alloc(allocator, sizeof(*eh)));
  eh->allocatorIFC = *allocatorIFC;
  eh->allocator = allocator;
  eh->numEntries = 0;
  eh->curSizeIdx = 0;
  while (kHashSizes[eh->curSizeIdx] < estimatedNumEntries) {
    eh->curSizeIdx++;
  }
  eh->curSize = kHashSizes[eh->curSizeIdx];
  eh->buckets = EHASH_alloc(eh, eh->curSize * sizeof(*eh->buckets));
  memset(eh->buckets, 0, eh->curSize * sizeof(*eh->buckets));

  return eh;
}

void ccg_ehash_free(EHash *eh, EHEntryFreeFP freeEntry, void *userData)
{
  int numBuckets = eh->curSize;

  while (numBuckets--) {
    EHEntry *entry = eh->buckets[numBuckets];

    while (entry) {
      EHEntry *next = entry->next;

      freeEntry(entry, userData);

      entry = next;
    }
  }

  EHASH_free(eh, eh->buckets);
  EHASH_free(eh, eh);
}

void ccg_ehash_insert(EHash *eh, EHEntry *entry)
{
  int numBuckets = eh->curSize;
  int hash = EHASH_hash(eh, entry->key);
  entry->next = eh->buckets[hash];
  eh->buckets[hash] = entry;
  eh->numEntries++;

  if (UNLIKELY(eh->numEntries > (numBuckets * 3))) {
    EHEntry **oldBuckets = eh->buckets;
    eh->curSize = kHashSizes[++eh->curSizeIdx];

    eh->buckets = EHASH_alloc(eh, eh->curSize * sizeof(*eh->buckets));
    memset(eh->buckets, 0, eh->curSize * sizeof(*eh->buckets));

    while (numBuckets--) {
      for (entry = oldBuckets[numBuckets]; entry;) {
        EHEntry *next = entry->next;

        hash = EHASH_hash(eh, entry->key);
        entry->next = eh->buckets[hash];
        eh->buckets[hash] = entry;

        entry = next;
      }
    }

    EHASH_free(eh, oldBuckets);
  }
}

void *ccg_ehash_lookupWithPrev(EHash *eh, void *key, void ***prevp_r)
{
  int hash = EHASH_hash(eh, key);
  void **prevp = (void **)&eh->buckets[hash];
  EHEntry *entry;

  for (; (entry = static_cast<EHEntry *>(*prevp)); prevp = (void **)&entry->next) {
    if (entry->key == key) {
      *prevp_r = (void **)prevp;
      return entry;
    }
  }

  return nullptr;
}

void *ccg_ehash_lookup(EHash *eh, void *key)
{
  int hash = EHASH_hash(eh, key);
  EHEntry *entry;

  for (entry = eh->buckets[hash]; entry; entry = entry->next) {
    if (entry->key == key) {
      break;
    }
  }

  return entry;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hash Elements Iteration
 * \{ */

void ccg_ehashIterator_init(EHash *eh, EHashIterator *ehi)
{
  /* fill all members */
  ehi->eh = eh;
  ehi->curBucket = -1;
  ehi->curEntry = nullptr;

  while (!ehi->curEntry) {
    ehi->curBucket++;
    if (ehi->curBucket == ehi->eh->curSize) {
      break;
    }
    ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
  }
}

void *ccg_ehashIterator_getCurrent(EHashIterator *ehi)
{
  return ehi->curEntry;
}

void ccg_ehashIterator_next(EHashIterator *ehi)
{
  if (ehi->curEntry) {
    ehi->curEntry = ehi->curEntry->next;
    while (!ehi->curEntry) {
      ehi->curBucket++;
      if (ehi->curBucket == ehi->eh->curSize) {
        break;
      }
      ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
    }
  }
}
int ccg_ehashIterator_isStopped(EHashIterator *ehi)
{
  return !ehi->curEntry;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Standard allocator implementation
 * \{ */

static void *_stdAllocator_alloc(CCGAllocatorHDL /*a*/, int numBytes)
{
  return MEM_mallocN(numBytes, "CCG standard alloc");
}

static void *_stdAllocator_realloc(CCGAllocatorHDL /*a*/, void *ptr, int newSize, int /*oldSize*/)
{
  return MEM_reallocN(ptr, newSize);
}

static void _stdAllocator_free(CCGAllocatorHDL /*a*/, void *ptr)
{
  MEM_freeN(ptr);
}

CCGAllocatorIFC *ccg_getStandardAllocatorIFC()
{
  static CCGAllocatorIFC ifc;

  ifc.alloc = _stdAllocator_alloc;
  ifc.realloc = _stdAllocator_realloc;
  ifc.free = _stdAllocator_free;
  ifc.release = nullptr;

  return &ifc;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name * Catmull-Clark Gridding Subdivision Surface
 * \{ */

#ifdef DUMP_RESULT_GRIDS
void ccgSubSurf__dumpCoords(CCGSubSurf *ss)
{
  int vertDataSize = ss->meshIFC.vertDataSize;
  int subdivLevels = ss->subdivLevels;
  int gridSize = ccg_gridsize(subdivLevels);
  int edgeSize = ccg_edgesize(subdivLevels);
  int i, index, S;

  for (i = 0, index = 0; i < ss->vMap->curSize; i++) {
    CCGVert *v = (CCGVert *)ss->vMap->buckets[i];
    for (; v; v = v->next, index++) {
      float *co = VERT_getCo(v, subdivLevels);
      printf("vertex index=%d, co=(%f, %f, %f)\n", index, co[0], co[1], co[2]);
    }
  }

  for (i = 0, index = 0; i < ss->eMap->curSize; i++) {
    CCGEdge *e = (CCGEdge *)ss->eMap->buckets[i];
    for (; e; e = e->next, index++) {
      int x;
      float *co = VERT_getCo(e->v0, subdivLevels);
      printf("edge index=%d, start_co=(%f, %f, %f)\n", index, co[0], co[1], co[2]);
      for (x = 0; x < edgeSize; x++) {
        float *co = EDGE_getCo(e, subdivLevels, x);
        printf("edge index=%d, seg=%d, co=(%f, %f, %f)\n", index, x, co[0], co[1], co[2]);
      }
      co = VERT_getCo(e->v1, subdivLevels);
      printf("edge index=%d, end_co=(%f, %f, %f)\n", index, co[0], co[1], co[2]);
    }
  }

  for (i = 0, index = 0; i < ss->fMap->curSize; i++) {
    CCGFace *f = (CCGFace *)ss->fMap->buckets[i];
    for (; f; f = f->next, index++) {
      for (S = 0; S < f->numVerts; S++) {
        CCGVert *v = FACE_getVerts(f)[S];
        float *co = VERT_getCo(v, subdivLevels);
        printf("face index=%d, vertex=%d, coord=(%f, %f, %f)\n", index, S, co[0], co[1], co[2]);
      }
    }
  }

  for (i = 0, index = 0; i < ss->fMap->curSize; i++) {
    CCGFace *f = (CCGFace *)ss->fMap->buckets[i];
    for (; f; f = f->next, index++) {
      for (S = 0; S < f->numVerts; S++) {
        CCGEdge *e = FACE_getEdges(f)[S];
        float *co1 = VERT_getCo(e->v0, subdivLevels);
        float *co2 = VERT_getCo(e->v1, subdivLevels);
        printf("face index=%d, edge=%d, coord1=(%f, %f, %f), coord2=(%f, %f, %f)\n",
               index,
               S,
               co1[0],
               co1[1],
               co1[2],
               co2[0],
               co2[1],
               co2[2]);
      }
    }
  }

  for (i = 0, index = 0; i < ss->fMap->curSize; i++) {
    CCGFace *f = (CCGFace *)ss->fMap->buckets[i];
    for (; f; f = f->next, index++) {
      for (S = 0; S < f->numVerts; S++) {
        int x, y;
        for (x = 0; x < gridSize; x++) {
          for (y = 0; y < gridSize; y++) {
            float *co = FACE_getIFCo(f, subdivLevels, S, x, y);
            printf("face index=%d. corner=%d, x=%d, y=%d, coord=(%f, %f, %f)\n",
                   index,
                   S,
                   x,
                   y,
                   co[0],
                   co[1],
                   co[2]);
          }
        }
        for (x = 0; x < gridSize; x++) {
          float *co = FACE_getIECo(f, subdivLevels, S, x);
          printf("face index=%d. corner=%d, ie_index=%d, coord=(%f, %f, %f)\n",
                 index,
                 S,
                 x,
                 co[0],
                 co[1],
                 co[2]);
        }
      }
    }
  }
}
#endif /* DUMP_RESULT_GRIDS */

/** \} */
