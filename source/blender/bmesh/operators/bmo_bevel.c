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
 *
 * Bevel wrapper around #BM_mesh_bevel
 */

#include "BLI_utildefines.h"

#include "BKE_curveprofile.h"
#include "DNA_curveprofile_types.h"
#include "bmesh.h"
#include "bmesh_tools.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_bevel_exec(BMesh *bm, BMOperator *op)
{
  const float offset = BMO_slot_float_get(op->slots_in, "offset");
  const int offset_type = BMO_slot_int_get(op->slots_in, "offset_type");
  const int profile_type = BMO_slot_int_get(op->slots_in, "profile_type");
  const int seg = BMO_slot_int_get(op->slots_in, "segments");
  const bool vonly = BMO_slot_bool_get(op->slots_in, "vertex_only");
  const float profile = BMO_slot_float_get(op->slots_in, "profile");
  const bool clamp_overlap = BMO_slot_bool_get(op->slots_in, "clamp_overlap");
  const int material = BMO_slot_int_get(op->slots_in, "material");
  const bool loop_slide = BMO_slot_bool_get(op->slots_in, "loop_slide");
  const bool mark_seam = BMO_slot_bool_get(op->slots_in, "mark_seam");
  const bool mark_sharp = BMO_slot_bool_get(op->slots_in, "mark_sharp");
  const bool harden_normals = BMO_slot_bool_get(op->slots_in, "harden_normals");
  const int face_strength_mode = BMO_slot_int_get(op->slots_in, "face_strength_mode");
  const int miter_outer = BMO_slot_int_get(op->slots_in, "miter_outer");
  const int miter_inner = BMO_slot_int_get(op->slots_in, "miter_inner");
  const float spread = BMO_slot_float_get(op->slots_in, "spread");
  const float smoothresh = BMO_slot_float_get(op->slots_in, "smoothresh");
  const CurveProfile *custom_profile = BMO_slot_ptr_get(op->slots_in, "custom_profile");
  const int vmesh_method = BMO_slot_int_get(op->slots_in, "vmesh_method");

  if (offset > 0) {
    BMOIter siter;
    BMEdge *e;
    BMVert *v;

    /* first flush 'geom' into flags, this makes it possible to check connected data,
     * BM_FACE is cleared so we can put newly created faces into a bmesh slot. */
    BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

    BMO_ITER (v, &siter, op->slots_in, "geom", BM_VERT) {
      BM_elem_flag_enable(v, BM_ELEM_TAG);
    }

    BMO_ITER (e, &siter, op->slots_in, "geom", BM_EDGE) {
      if (BM_edge_is_manifold(e)) {
        BM_elem_flag_enable(e, BM_ELEM_TAG);
        /* in case verts were not also included in the geom */
        BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
        BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
      }
    }

    BM_mesh_bevel(bm,
                  offset,
                  offset_type,
                  profile_type,
                  seg,
                  profile,
                  vonly,
                  false,
                  clamp_overlap,
                  NULL,
                  -1,
                  material,
                  loop_slide,
                  mark_seam,
                  mark_sharp,
                  harden_normals,
                  face_strength_mode,
                  miter_outer,
                  miter_inner,
                  spread,
                  smoothresh,
                  custom_profile,
                  vmesh_method);

    BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "faces.out", BM_FACE, BM_ELEM_TAG);
    BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "edges.out", BM_EDGE, BM_ELEM_TAG);
    BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "verts.out", BM_VERT, BM_ELEM_TAG);
  }
}
