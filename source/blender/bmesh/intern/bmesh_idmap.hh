/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_sys_types.h"

/** \file
 * \ingroup bmesh
 *
 * A simple self-contained element ID library.
 * Stores IDs in integer attributes.
 */

#include "BLI_compiler_compat.h"
#include "BLI_map.hh"
#include "BLI_sys_types.h"
#include "BLI_vector.hh"

#include "bmesh.h"

#define BM_ID_NONE 0

struct BMIdMap {
  int flag;

  uint maxid;
  int cd_id_off[15];
  BMesh *bm;

  blender::Vector<BMElem *> map; /* ID -> Element map. */
  blender::Vector<int> freelist;

  using FreeIdxMap = blender::Map<int, int>;

  /* maps ids to their position within the freelist
       only used if freelist is bigger then a certain size,
       see FREELIST_HASHMAP_THRESHOLD_HIGH in bmesh_construct.c.*/
  FreeIdxMap *free_idx_map;
};

BMIdMap *BM_idmap_new(BMesh *bm, int elem_mask);
void BM_idmap_destroy(BMIdMap *idmap);

/* Ensures idmap attributes exist. */
bool BM_idmap_check_attributes(BMIdMap *idmap);

/* Ensures every element has a unique ID. */
void BM_idmap_check_ids(BMIdMap *idmap);

/* Explicitly assign an ID. id cannot be BM_ID_NONE (zero). */
template<typename T = BMElem> void BM_idmap_assign(BMIdMap *idmap, T *elem, int id);

/* Automatically allocate an ID. */
template<typename T = BMElem> int BM_idmap_alloc(BMIdMap *idmap, T *elem);

/* Checks if an element needs an ID (it's id is BM_ID_NONE),
 * and if so allocates one.
 */
template<typename T = BMElem> int BM_idmap_check_assign(BMIdMap *idmap, T *elem);

/* Release an ID; if clear_id is true the id attribute for
 * that element will be set to BM_ID_NONE.
 */
template<typename T = BMElem> void BM_idmap_release(BMIdMap *idmap, T *elem, bool clear_id);

const char *BM_idmap_attr_name_get(int htype);

/* Deletes all id attributes. */
void BM_idmap_clear_attributes(BMesh *bm);
void BM_idmap_clear_attributes_mesh(Mesh *me);

/* Elem -> ID. */
template<typename T = BMElem> BLI_INLINE int BM_idmap_get_id(BMIdMap *map, T *elem)
{
  return BM_ELEM_CD_GET_INT(elem, map->cd_id_off[(int)elem->head.htype]);
}

/* ID -> elem. */
template<typename T = BMElem> BLI_INLINE T *BM_idmap_lookup(BMIdMap *map, int elem)
{
  return elem >= 0 ? reinterpret_cast<T *>(map->map[elem]) : NULL;
}
