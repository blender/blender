/* SPDX-FileCopyrightText: 2012 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_assert.h"
#include "BLI_math_vector_types.hh"

struct CCGSubSurf;

/* Each CCGElem is CCGSubSurf's representation of a subdivided
 * vertex. All CCGElems in a particular CCGSubSurf have the same
 * layout, but the layout can vary from one CCGSubSurf to another. For
 * this reason, CCGElem is presented as an opaque pointer, and
 * elements should always be accompanied by a CCGKey, which provides
 * the necessary offsets to access components of a CCGElem.
 */
struct CCGElem;

struct CCGKey {
  int level;

  /* number of bytes in each element (one float per layer, plus
   * three floats for normals if enabled) */
  int elem_size;

  /* number of elements along each side of grid */
  int grid_size;
  /* number of elements in the grid (grid size squared) */
  int grid_area;
  /* number of bytes in each grid (grid_area * elem_size) */
  int grid_bytes;

  /* currently always the last three floats, unless normals are
   * disabled */
  int normal_offset;

  /* offset in bytes of mask value; only valid if 'has_mask' is
   * true */
  int mask_offset;

  int has_normals;
  int has_mask;
};

/* initialize 'key' at the specified level */
void CCG_key(CCGKey *key, const CCGSubSurf *ss, int level);
void CCG_key_top_level(CCGKey *key, const CCGSubSurf *ss);

inline blender::float3 &CCG_elem_co(const CCGKey & /*key*/, CCGElem *elem)
{
  return *reinterpret_cast<blender::float3 *>(elem);
}

inline blender::float3 &CCG_elem_no(const CCGKey &key, CCGElem *elem)
{
  BLI_assert(key.has_normals);
  return *reinterpret_cast<blender::float3 *>(reinterpret_cast<char *>(elem) + key.normal_offset);
}

inline float &CCG_elem_mask(const CCGKey &key, CCGElem *elem)
{
  BLI_assert(key.has_mask);
  return *reinterpret_cast<float *>(reinterpret_cast<char *>(elem) + (key.mask_offset));
}

inline CCGElem *CCG_elem_offset(const CCGKey &key, CCGElem *elem, int offset)
{
  return reinterpret_cast<CCGElem *>((reinterpret_cast<char *>(elem)) + key.elem_size * offset);
}

inline CCGElem *CCG_grid_elem(const CCGKey &key, CCGElem *elem, int x, int y)
{
  //  BLI_assert(x < key.grid_size && y < key.grid_size);
  return CCG_elem_offset(key, elem, (y * key.grid_size + x));
}

inline blender::float3 &CCG_grid_elem_co(const CCGKey &key, CCGElem *elem, int x, int y)
{
  return CCG_elem_co(key, CCG_grid_elem(key, elem, x, y));
}

inline blender::float3 &CCG_grid_elem_no(const CCGKey &key, CCGElem *elem, int x, int y)
{
  return CCG_elem_no(key, CCG_grid_elem(key, elem, x, y));
}

inline float &CCG_grid_elem_mask(const CCGKey &key, CCGElem *elem, int x, int y)
{
  return CCG_elem_mask(key, CCG_grid_elem(key, elem, x, y));
}

inline blender::float3 &CCG_elem_offset_co(const CCGKey &key, CCGElem *elem, int offset)
{
  return CCG_elem_co(key, CCG_elem_offset(key, elem, offset));
}

inline blender::float3 &CCG_elem_offset_no(const CCGKey &key, CCGElem *elem, int offset)
{
  return CCG_elem_no(key, CCG_elem_offset(key, elem, offset));
}

inline float &CCG_elem_offset_mask(const CCGKey &key, CCGElem *elem, int offset)
{
  return CCG_elem_mask(key, CCG_elem_offset(key, elem, offset));
}

inline CCGElem *CCG_elem_next(const CCGKey &key, CCGElem *elem)
{
  return CCG_elem_offset(key, elem, 1);
}
