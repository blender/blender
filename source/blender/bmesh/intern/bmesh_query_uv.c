/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_utildefines_stack.h"

#include "BKE_customdata.h"

#include "DNA_meshdata_types.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/**
 * Calculate the UV cross product (use the sign to check the winding).
 */
float BM_face_uv_calc_cross(const BMFace *f, const int cd_loop_uv_offset)
{
  float(*uvs)[2] = BLI_array_alloca(uvs, f->len);
  const BMLoop *l_iter;
  const BMLoop *l_first;
  int i = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
    copy_v2_v2(uvs[i++], luv->uv);
  } while ((l_iter = l_iter->next) != l_first);
  return cross_poly_v2(uvs, f->len);
}

/**
 * Check if two loops that share an edge also have the same UV coordinates.
 */
bool BM_loop_uv_share_edge_check(BMLoop *l_a, BMLoop *l_b, const int cd_loop_uv_offset)
{
  BLI_assert(l_a->e == l_b->e);
  MLoopUV *luv_a_curr = BM_ELEM_CD_GET_VOID_P(l_a, cd_loop_uv_offset);
  MLoopUV *luv_a_next = BM_ELEM_CD_GET_VOID_P(l_a->next, cd_loop_uv_offset);
  MLoopUV *luv_b_curr = BM_ELEM_CD_GET_VOID_P(l_b, cd_loop_uv_offset);
  MLoopUV *luv_b_next = BM_ELEM_CD_GET_VOID_P(l_b->next, cd_loop_uv_offset);
  if (l_a->v != l_b->v) {
    SWAP(MLoopUV *, luv_b_curr, luv_b_next);
  }
  return (equals_v2v2(luv_a_curr->uv, luv_b_curr->uv) &&
          equals_v2v2(luv_a_next->uv, luv_b_next->uv));
}

/**
 * Check if two loops that share a vertex also have the same UV coordinates.
 */
bool BM_loop_uv_share_vert_check(BMEdge *e, BMLoop *l_a, BMLoop *l_b, const int cd_loop_uv_offset)
{
  BLI_assert(l_a->v == l_b->v);

  {
    const MLoopUV *luv_a = BM_ELEM_CD_GET_VOID_P(l_a, cd_loop_uv_offset);
    const MLoopUV *luv_b = BM_ELEM_CD_GET_VOID_P(l_b, cd_loop_uv_offset);
    if (!equals_v2v2(luv_a->uv, luv_b->uv)) {
      return false;
    }
  }

  /* No need for NULL checks, these will always succeed. */
  const BMLoop *l_other_a = BM_loop_other_vert_loop_by_edge(l_a, e);
  const BMLoop *l_other_b = BM_loop_other_vert_loop_by_edge(l_b, e);

  {
    const MLoopUV *luv_other_a = BM_ELEM_CD_GET_VOID_P(l_other_a, cd_loop_uv_offset);
    const MLoopUV *luv_other_b = BM_ELEM_CD_GET_VOID_P(l_other_b, cd_loop_uv_offset);
    if (!equals_v2v2(luv_other_a->uv, luv_other_b->uv)) {
      return false;
    }
  }

  return true;
}
