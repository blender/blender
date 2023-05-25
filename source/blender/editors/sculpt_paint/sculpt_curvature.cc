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
 *
 * The Original Code is Copyright (C) 2021 by Joseph Eagar
 * All rights reserved.
 * Implements curvature analysis for sculpt tools
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_ghash.h"
#include "BLI_gsqueue.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_solvers.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_kelvinlet.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_mirror.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"

#include "IMB_colormanagement.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_space_api.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.hh"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

using namespace blender::bke::paint;

/*
If you're working with uniform triangle tesselations, the math for
calculating principle curvatures reduces to doing an eigen decomposition
of the smoothed normal covariance matrix.

The normal covariance matrix is just:

nx*nx nx*ny nx*nz
ny*nx ny*ny ny*nz
nz*nx nz*ny nz*nz

To find principle curvatures, simply subtract neighboring covariance matrices.
You can do this over any number of neighborhood rings to get more accurate result

*/

BLI_INLINE void normal_covariance(float mat[3][3], float no[3])
{
  mat[0][0] = no[0] * no[0];
  mat[0][1] = no[0] * no[1];
  mat[0][2] = no[0] * no[2];
  mat[1][0] = no[1] * no[0];
  mat[1][1] = no[1] * no[1];
  mat[1][2] = no[1] * no[2];
  mat[2][0] = no[2] * no[0];
  mat[2][1] = no[2] * no[1];
  mat[2][2] = no[2] * no[2];
}

bool SCULPT_calc_principle_curvatures(SculptSession *ss,
                                      PBVHVertRef vertex,
                                      SculptCurvatureData *out,
                                      bool useAccurateSolver)
{
  SculptVertexNeighborIter ni;
  float nmat[3][3], nmat2[3][3];
  float no[3], no2[3];

  memset(out, 0, sizeof(SculptCurvatureData));

  SCULPT_vertex_normal_get(ss, vertex, no);
  normal_covariance(nmat, no);

#if 0
    int val = SCULPT_vertex_valence_get(ss, vertex);
    float *ws = (float *)BLI_array_alloca(ws, val);
    float *cot1 = (float *)BLI_array_alloca(cot1, val);
    float *cot2 = (float *)BLI_array_alloca(cot2, val);
    float *areas = (float *)BLI_array_alloca(areas, val);
    float totarea = 0.0f;

    SCULPT_get_cotangents(ss, vertex, ws, cot1, cot2, areas, &totarea);

    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
      SCULPT_vertex_normal_get(ss, ni.vertex, no2);
      sub_v3_v3(no2, no);

      normal_covariance(nmat2, no2);
      madd_m3_m3m3fl(nmat, nmat, nmat2, ws[ni.i]);
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
#else
  /* TODO: review the math here. We're deriving the curvature
   * via an eigen decomposition of the weighted summed
   * normal covariance matrices of the surrounding topology.
   */
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    SCULPT_vertex_normal_get(ss, ni.vertex, no2);
    sub_v3_v3(no2, no);

    SculptVertexNeighborIter ni2;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, ni.vertex, ni2) {
      float no3[3];
      SCULPT_vertex_normal_get(ss, ni2.vertex, no3);

      normal_covariance(nmat2, no3);
      madd_m3_m3m3fl(nmat, nmat, nmat2, 1.0f / ni2.size);
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni2);

    normal_covariance(nmat2, no2);
    madd_m3_m3m3fl(nmat, nmat, nmat2, 1.0f / ni.size);
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
#endif

  if (!useAccurateSolver || !BLI_eigen_solve_selfadjoint_m3(nmat, out->ks, out->principle)) {
    /* Do simple power solve in one direction. */

    float t[3];
    float t2[3];

    SCULPT_vertex_normal_get(ss, vertex, no);
    copy_v3_v3(t, no);

    for (int i = 0; i < 25; i++) {
      if (i > 0) {
        normalize_v3(t);

        if (i > 5 && len_squared_v3v3(t, t2) < 0.000001f) {
          break;
        }

        copy_v3_v3(t2, t);
      }

      mul_m3_v3(nmat, t);
    }

    out->ks[1] = normalize_v3(t);
    copy_v3_v3(out->principle[1], t);

    cross_v3_v3v3(out->principle[0], out->principle[1], no);
    if (dot_v3v3(out->principle[0], out->principle[0]) > FLT_EPSILON * 50.0f) {
      normalize_v3(out->principle[0]);
    }
    else {
      zero_v3(out->principle[0]);
    }
  }

  if (is_zero_v3(out->principle[0])) {
    /* Choose an orthoganoal direction. */
    copy_v3_v3(out->principle[0], no);
    float axis[3] = {0.0f, 0.0f, 0.0f};

    if (fabsf(no[0]) > fabs(no[1]) && fabs(no[0]) >= fabs(no[2])) {
      axis[1] = 1.0f;
    }
    else if (fabsf(no[1]) > fabs(no[0]) && fabs(no[1]) >= fabs(no[2])) {
      axis[2] = 1.0f;
    }
    else {
      axis[0] = 1.0f;
    }

    cross_v3_v3v3(out->principle[0], no, axis);
    cross_v3_v3v3(out->principle[1], out->principle[0], no);
    copy_v3_v3(out->principle[2], no);

    normalize_v3(out->principle[0]);
    normalize_v3(out->principle[1]);
  }

  return true;
}

void SCULPT_curvature_dir_get(SculptSession *ss,
                              PBVHVertRef v,
                              float dir[3],
                              bool useAccurateSolver)
{
  if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
    SculptCurvatureData curv;
    SCULPT_calc_principle_curvatures(ss, v, &curv, useAccurateSolver);

    copy_v3_v3(dir, curv.principle[0]);
    return;
  }

  copy_v3_v3(dir, vertex_attr_ptr<float>(v, ss->attrs.curvature_dir));
}

void SCULPT_curvature_begin(SculptSession *ss, struct PBVHNode *node, bool useAccurateSolver)
{
  if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
    /* Caching only happens for bmesh for now. */
    return;
  }

  if (BKE_pbvh_curvature_update_get(node)) {
    PBVHVertexIter vi;

    BKE_pbvh_curvature_update_set(node, false);

    BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vi, PBVH_ITER_UNIQUE) {
      BMVert *v = (BMVert *)vi.vertex.i;

      SculptCurvatureData curv;
      SCULPT_calc_principle_curvatures(ss, vi.vertex, &curv, useAccurateSolver);

      copy_v3_v3(vertex_attr_ptr<float>(vi.vertex, ss->attrs.curvature_dir), curv.principle[0]);
    }
    BKE_pbvh_vertex_iter_end;
  }
}
