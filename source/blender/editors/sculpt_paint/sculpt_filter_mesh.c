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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>

/* Filter orientation utils. */
void SCULPT_filter_to_orientation_space(float r_v[3], struct FilterCache *filter_cache)
{
  switch (filter_cache->orientation) {
    case SCULPT_FILTER_ORIENTATION_LOCAL:
      /* Do nothing, Sculpt Mode already works in object space. */
      break;
    case SCULPT_FILTER_ORIENTATION_WORLD:
      mul_mat3_m4_v3(filter_cache->obmat, r_v);
      break;
    case SCULPT_FILTER_ORIENTATION_VIEW:
      mul_mat3_m4_v3(filter_cache->obmat, r_v);
      mul_mat3_m4_v3(filter_cache->viewmat, r_v);
      break;
  }
}

void SCULPT_filter_to_object_space(float r_v[3], struct FilterCache *filter_cache)
{
  switch (filter_cache->orientation) {
    case SCULPT_FILTER_ORIENTATION_LOCAL:
      /* Do nothing, Sculpt Mode already works in object space. */
      break;
    case SCULPT_FILTER_ORIENTATION_WORLD:
      mul_mat3_m4_v3(filter_cache->obmat_inv, r_v);
      break;
    case SCULPT_FILTER_ORIENTATION_VIEW:
      mul_mat3_m4_v3(filter_cache->viewmat_inv, r_v);
      mul_mat3_m4_v3(filter_cache->obmat_inv, r_v);
      break;
  }
}

void SCULPT_filter_zero_disabled_axis_components(float r_v[3], struct FilterCache *filter_cache)
{
  SCULPT_filter_to_orientation_space(r_v, filter_cache);
  for (int axis = 0; axis < 3; axis++) {
    if (!filter_cache->enabled_force_axis[axis]) {
      r_v[axis] = 0.0f;
    }
  }
  SCULPT_filter_to_object_space(r_v, filter_cache);
}

static void filter_cache_init_task_cb(void *__restrict userdata,
                                      const int i,
                                      const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  PBVHNode *node = data->nodes[i];

  SCULPT_undo_push_node(data->ob, node, data->filter_undo_type);
}

void SCULPT_filter_cache_init(bContext *C, Object *ob, Sculpt *sd, const int undo_type)
{
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ob->sculpt->pbvh;

  ss->filter_cache = MEM_callocN(sizeof(FilterCache), "filter cache");

  ss->filter_cache->random_seed = rand();

  const float center[3] = {0.0f};
  SculptSearchSphereData search_data = {
      .original = true,
      .center = center,
      .radius_squared = FLT_MAX,
      .ignore_fully_ineffective = true,

  };
  BKE_pbvh_search_gather(pbvh,
                         SCULPT_search_sphere_cb,
                         &search_data,
                         &ss->filter_cache->nodes,
                         &ss->filter_cache->totnode);

  for (int i = 0; i < ss->filter_cache->totnode; i++) {
    BKE_pbvh_node_mark_normals_update(ss->filter_cache->nodes[i]);
  }

  /* mesh->runtime.subdiv_ccg is not available. Updating of the normals is done during drawing.
   * Filters can't use normals in multires. */
  if (BKE_pbvh_type(ss->pbvh) != PBVH_GRIDS) {
    BKE_pbvh_update_normals(ss->pbvh, NULL);
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
      .filter_undo_type = undo_type,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, ss->filter_cache->totnode);
  BLI_task_parallel_range(
      0, ss->filter_cache->totnode, &data, filter_cache_init_task_cb, &settings);

  /* Setup orientation matrices. */
  copy_m4_m4(ss->filter_cache->obmat, ob->obmat);
  invert_m4_m4(ss->filter_cache->obmat_inv, ob->obmat);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  copy_m4_m4(ss->filter_cache->viewmat, vc.rv3d->viewmat);
  copy_m4_m4(ss->filter_cache->viewmat_inv, vc.rv3d->viewinv);
}

void SCULPT_filter_cache_free(SculptSession *ss)
{
  if (ss->filter_cache->cloth_sim) {
    SCULPT_cloth_simulation_free(ss->filter_cache->cloth_sim);
  }
  if (ss->filter_cache->automasking) {
    SCULPT_automasking_cache_free(ss->filter_cache->automasking);
  }
  MEM_SAFE_FREE(ss->filter_cache->nodes);
  MEM_SAFE_FREE(ss->filter_cache->mask_update_it);
  MEM_SAFE_FREE(ss->filter_cache->prev_mask);
  MEM_SAFE_FREE(ss->filter_cache->normal_factor);
  MEM_SAFE_FREE(ss->filter_cache->prev_face_set);
  MEM_SAFE_FREE(ss->filter_cache->surface_smooth_laplacian_disp);
  MEM_SAFE_FREE(ss->filter_cache->sharpen_factor);
  MEM_SAFE_FREE(ss->filter_cache->detail_directions);
  MEM_SAFE_FREE(ss->filter_cache->limit_surface_co);
  MEM_SAFE_FREE(ss->filter_cache);
}

typedef enum eSculptMeshFilterType {
  MESH_FILTER_SMOOTH = 0,
  MESH_FILTER_SCALE = 1,
  MESH_FILTER_INFLATE = 2,
  MESH_FILTER_SPHERE = 3,
  MESH_FILTER_RANDOM = 4,
  MESH_FILTER_RELAX = 5,
  MESH_FILTER_RELAX_FACE_SETS = 6,
  MESH_FILTER_SURFACE_SMOOTH = 7,
  MESH_FILTER_SHARPEN = 8,
  MESH_FILTER_ENHANCE_DETAILS = 9,
  MESH_FILTER_ERASE_DISPLACEMENT = 10,
} eSculptMeshFilterType;

static EnumPropertyItem prop_mesh_filter_types[] = {
    {MESH_FILTER_SMOOTH, "SMOOTH", 0, "Smooth", "Smooth mesh"},
    {MESH_FILTER_SCALE, "SCALE", 0, "Scale", "Scale mesh"},
    {MESH_FILTER_INFLATE, "INFLATE", 0, "Inflate", "Inflate mesh"},
    {MESH_FILTER_SPHERE, "SPHERE", 0, "Sphere", "Morph into sphere"},
    {MESH_FILTER_RANDOM, "RANDOM", 0, "Random", "Randomize vertex positions"},
    {MESH_FILTER_RELAX, "RELAX", 0, "Relax", "Relax mesh"},
    {MESH_FILTER_RELAX_FACE_SETS,
     "RELAX_FACE_SETS",
     0,
     "Relax Face Sets",
     "Smooth the edges of all the Face Sets"},
    {MESH_FILTER_SURFACE_SMOOTH,
     "SURFACE_SMOOTH",
     0,
     "Surface Smooth",
     "Smooth the surface of the mesh, preserving the volume"},
    {MESH_FILTER_SHARPEN, "SHARPEN", 0, "Sharpen", "Sharpen the cavities of the mesh"},
    {MESH_FILTER_ENHANCE_DETAILS,
     "ENHANCE_DETAILS",
     0,
     "Enhance Details",
     "Enhance the high frequency surface detail"},
    {MESH_FILTER_ERASE_DISPLACEMENT,
     "ERASE_DISCPLACEMENT",
     0,
     "Erase Displacement",
     "Deletes the displacement of the Multires Modifier"},
    {0, NULL, 0, NULL, NULL},
};

typedef enum eMeshFilterDeformAxis {
  MESH_FILTER_DEFORM_X = 1 << 0,
  MESH_FILTER_DEFORM_Y = 1 << 1,
  MESH_FILTER_DEFORM_Z = 1 << 2,
} eMeshFilterDeformAxis;

static EnumPropertyItem prop_mesh_filter_deform_axis_items[] = {
    {MESH_FILTER_DEFORM_X, "X", 0, "X", "Deform in the X axis"},
    {MESH_FILTER_DEFORM_Y, "Y", 0, "Y", "Deform in the Y axis"},
    {MESH_FILTER_DEFORM_Z, "Z", 0, "Z", "Deform in the Z axis"},
    {0, NULL, 0, NULL, NULL},
};

static EnumPropertyItem prop_mesh_filter_orientation_items[] = {
    {SCULPT_FILTER_ORIENTATION_LOCAL,
     "LOCAL",
     0,
     "Local",
     "Use the local axis to limit the displacement"},
    {SCULPT_FILTER_ORIENTATION_WORLD,
     "WORLD",
     0,
     "World",
     "Use the global axis to limit the displacement"},
    {SCULPT_FILTER_ORIENTATION_VIEW,
     "VIEW",
     0,
     "View",
     "Use the view axis to limit the displacement"},
    {0, NULL, 0, NULL, NULL},
};

static bool sculpt_mesh_filter_needs_pmap(eSculptMeshFilterType filter_type)
{
  return ELEM(filter_type,
              MESH_FILTER_SMOOTH,
              MESH_FILTER_RELAX,
              MESH_FILTER_RELAX_FACE_SETS,
              MESH_FILTER_SURFACE_SMOOTH,
              MESH_FILTER_ENHANCE_DETAILS,
              MESH_FILTER_SHARPEN);
}

static void mesh_filter_task_cb(void *__restrict userdata,
                                const int i,
                                const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];

  const eSculptMeshFilterType filter_type = data->filter_type;

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[i]);

  /* When using the relax face sets meshes filter,
   * each 3 iterations, do a whole mesh relax to smooth the contents of the Face Set. */
  /* This produces better results as the relax operation is no completely focused on the
   * boundaries. */
  const bool relax_face_sets = !(ss->filter_cache->iteration_count % 3 == 0);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    SCULPT_orig_vert_data_update(&orig_data, &vd);
    float orig_co[3], val[3], avg[3], normal[3], disp[3], disp2[3], transform[3][3], final_pos[3];
    float fade = vd.mask ? *vd.mask : 0.0f;
    fade = 1.0f - fade;
    fade *= data->filter_strength;
    fade *= SCULPT_automasking_factor_get(ss->filter_cache->automasking, ss, vd.index);

    if (fade == 0.0f && filter_type != MESH_FILTER_SURFACE_SMOOTH) {
      /* Surface Smooth can't skip the loop for this vertex as it needs to calculate its
       * laplacian_disp. This value is accessed from the vertex neighbors when deforming the
       * vertices, so it is needed for all vertices even if they are not going to be displaced.
       */
      continue;
    }

    if (ELEM(filter_type, MESH_FILTER_RELAX, MESH_FILTER_RELAX_FACE_SETS)) {
      copy_v3_v3(orig_co, vd.co);
    }
    else {
      copy_v3_v3(orig_co, orig_data.co);
    }

    if (filter_type == MESH_FILTER_RELAX_FACE_SETS) {
      if (relax_face_sets == SCULPT_vertex_has_unique_face_set(ss, vd.index)) {
        continue;
      }
    }

    switch (filter_type) {
      case MESH_FILTER_SMOOTH:
        fade = clamp_f(fade, -1.0f, 1.0f);
        SCULPT_neighbor_coords_average_interior(ss, avg, vd.index);
        sub_v3_v3v3(val, avg, orig_co);
        madd_v3_v3v3fl(val, orig_co, val, fade);
        sub_v3_v3v3(disp, val, orig_co);
        break;
      case MESH_FILTER_INFLATE:
        normal_short_to_float_v3(normal, orig_data.no);
        mul_v3_v3fl(disp, normal, fade);
        break;
      case MESH_FILTER_SCALE:
        unit_m3(transform);
        scale_m3_fl(transform, 1.0f + fade);
        copy_v3_v3(val, orig_co);
        mul_m3_v3(transform, val);
        sub_v3_v3v3(disp, val, orig_co);
        break;
      case MESH_FILTER_SPHERE:
        normalize_v3_v3(disp, orig_co);
        if (fade > 0.0f) {
          mul_v3_v3fl(disp, disp, fade);
        }
        else {
          mul_v3_v3fl(disp, disp, -fade);
        }

        unit_m3(transform);
        if (fade > 0.0f) {
          scale_m3_fl(transform, 1.0f - fade);
        }
        else {
          scale_m3_fl(transform, 1.0f + fade);
        }
        copy_v3_v3(val, orig_co);
        mul_m3_v3(transform, val);
        sub_v3_v3v3(disp2, val, orig_co);

        mid_v3_v3v3(disp, disp, disp2);
        break;
      case MESH_FILTER_RANDOM: {
        normal_short_to_float_v3(normal, orig_data.no);
        /* Index is not unique for multires, so hash by vertex coordinates. */
        const uint *hash_co = (const uint *)orig_co;
        const uint hash = BLI_hash_int_2d(hash_co[0], hash_co[1]) ^
                          BLI_hash_int_2d(hash_co[2], ss->filter_cache->random_seed);
        mul_v3_fl(normal, hash * (1.0f / (float)0xFFFFFFFF) - 0.5f);
        mul_v3_v3fl(disp, normal, fade);
        break;
      }
      case MESH_FILTER_RELAX: {
        SCULPT_relax_vertex(ss, &vd, clamp_f(fade, 0.0f, 1.0f), false, val);
        sub_v3_v3v3(disp, val, vd.co);
        break;
      }
      case MESH_FILTER_RELAX_FACE_SETS: {
        SCULPT_relax_vertex(ss, &vd, clamp_f(fade, 0.0f, 1.0f), relax_face_sets, val);
        sub_v3_v3v3(disp, val, vd.co);
        break;
      }
      case MESH_FILTER_SURFACE_SMOOTH: {
        SCULPT_surface_smooth_laplacian_step(ss,
                                             disp,
                                             vd.co,
                                             ss->filter_cache->surface_smooth_laplacian_disp,
                                             vd.index,
                                             orig_data.co,
                                             ss->filter_cache->surface_smooth_shape_preservation);
        break;
      }
      case MESH_FILTER_SHARPEN: {
        const float smooth_ratio = ss->filter_cache->sharpen_smooth_ratio;

        /* This filter can't work at full strength as it needs multiple iterations to reach a
         * stable state. */
        fade = clamp_f(fade, 0.0f, 0.5f);
        float disp_sharpen[3] = {0.0f, 0.0f, 0.0f};

        SculptVertexNeighborIter ni;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.index, ni) {
          float disp_n[3];
          sub_v3_v3v3(
              disp_n, SCULPT_vertex_co_get(ss, ni.index), SCULPT_vertex_co_get(ss, vd.index));
          mul_v3_fl(disp_n, ss->filter_cache->sharpen_factor[ni.index]);
          add_v3_v3(disp_sharpen, disp_n);
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

        mul_v3_fl(disp_sharpen, 1.0f - ss->filter_cache->sharpen_factor[vd.index]);

        float disp_avg[3];
        float avg_co[3];
        SCULPT_neighbor_coords_average(ss, avg_co, vd.index);
        sub_v3_v3v3(disp_avg, avg_co, vd.co);
        mul_v3_v3fl(
            disp_avg, disp_avg, smooth_ratio * pow2f(ss->filter_cache->sharpen_factor[vd.index]));
        add_v3_v3v3(disp, disp_avg, disp_sharpen);

        /* Intensify details. */
        if (ss->filter_cache->sharpen_intensify_detail_strength > 0.0f) {
          float detail_strength[3];
          normal_short_to_float_v3(detail_strength, orig_data.no);
          copy_v3_v3(detail_strength, ss->filter_cache->detail_directions[vd.index]);
          madd_v3_v3fl(disp,
                       detail_strength,
                       -ss->filter_cache->sharpen_intensify_detail_strength *
                           ss->filter_cache->sharpen_factor[vd.index]);
        }
        break;
      }

      case MESH_FILTER_ENHANCE_DETAILS: {
        mul_v3_v3fl(disp, ss->filter_cache->detail_directions[vd.index], -fabsf(fade));
      } break;
      case MESH_FILTER_ERASE_DISPLACEMENT: {
        fade = clamp_f(fade, -1.0f, 1.0f);
        sub_v3_v3v3(disp, ss->filter_cache->limit_surface_co[vd.index], orig_co);
        mul_v3_fl(disp, fade);
        break;
      }
    }

    SCULPT_filter_to_orientation_space(disp, ss->filter_cache);
    for (int it = 0; it < 3; it++) {
      if (!ss->filter_cache->enabled_axis[it]) {
        disp[it] = 0.0f;
      }
    }
    SCULPT_filter_to_object_space(disp, ss->filter_cache);

    if (ELEM(filter_type, MESH_FILTER_SURFACE_SMOOTH, MESH_FILTER_SHARPEN)) {
      madd_v3_v3v3fl(final_pos, vd.co, disp, clamp_f(fade, 0.0f, 1.0f));
    }
    else {
      add_v3_v3v3(final_pos, orig_co, disp);
    }
    copy_v3_v3(vd.co, final_pos);
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(node);
}

static void mesh_filter_enhance_details_init_directions(SculptSession *ss)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  FilterCache *filter_cache = ss->filter_cache;

  filter_cache->detail_directions = MEM_malloc_arrayN(
      totvert, sizeof(float[3]), "detail directions");
  for (int i = 0; i < totvert; i++) {
    float avg[3];
    SCULPT_neighbor_coords_average(ss, avg, i);
    sub_v3_v3v3(filter_cache->detail_directions[i], avg, SCULPT_vertex_co_get(ss, i));
  }
}

static void mesh_filter_surface_smooth_init(SculptSession *ss,
                                            const float shape_preservation,
                                            const float current_vertex_displacement)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  FilterCache *filter_cache = ss->filter_cache;

  filter_cache->surface_smooth_laplacian_disp = MEM_malloc_arrayN(
      totvert, sizeof(float[3]), "surface smooth displacement");
  filter_cache->surface_smooth_shape_preservation = shape_preservation;
  filter_cache->surface_smooth_current_vertex = current_vertex_displacement;
}

static void mesh_filter_init_limit_surface_co(SculptSession *ss)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  FilterCache *filter_cache = ss->filter_cache;

  filter_cache->limit_surface_co = MEM_malloc_arrayN(
      sizeof(float[3]), totvert, "limit surface co");
  for (int i = 0; i < totvert; i++) {
    SCULPT_vertex_limit_surface_get(ss, i, filter_cache->limit_surface_co[i]);
  }
}

static void mesh_filter_sharpen_init(SculptSession *ss,
                                     const float smooth_ratio,
                                     const float intensify_detail_strength,
                                     const int curvature_smooth_iterations)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  FilterCache *filter_cache = ss->filter_cache;

  filter_cache->sharpen_smooth_ratio = smooth_ratio;
  filter_cache->sharpen_intensify_detail_strength = intensify_detail_strength;
  filter_cache->sharpen_curvature_smooth_iterations = curvature_smooth_iterations;
  filter_cache->sharpen_factor = MEM_malloc_arrayN(sizeof(float), totvert, "sharpen factor");
  filter_cache->detail_directions = MEM_malloc_arrayN(
      totvert, sizeof(float[3]), "sharpen detail direction");

  for (int i = 0; i < totvert; i++) {
    float avg[3];
    SCULPT_neighbor_coords_average(ss, avg, i);
    sub_v3_v3v3(filter_cache->detail_directions[i], avg, SCULPT_vertex_co_get(ss, i));
    filter_cache->sharpen_factor[i] = len_v3(filter_cache->detail_directions[i]);
  }

  float max_factor = 0.0f;
  for (int i = 0; i < totvert; i++) {
    if (filter_cache->sharpen_factor[i] > max_factor) {
      max_factor = filter_cache->sharpen_factor[i];
    }
  }

  max_factor = 1.0f / max_factor;
  for (int i = 0; i < totvert; i++) {
    filter_cache->sharpen_factor[i] *= max_factor;
    filter_cache->sharpen_factor[i] = 1.0f - pow2f(1.0f - filter_cache->sharpen_factor[i]);
  }

  /* Smooth the calculated factors and directions to remove high frequency detail. */
  for (int smooth_iterations = 0;
       smooth_iterations < filter_cache->sharpen_curvature_smooth_iterations;
       smooth_iterations++) {
    for (int i = 0; i < totvert; i++) {
      float direction_avg[3] = {0.0f, 0.0f, 0.0f};
      float sharpen_avg = 0;
      int total = 0;

      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, i, ni) {
        add_v3_v3(direction_avg, filter_cache->detail_directions[ni.index]);
        sharpen_avg += filter_cache->sharpen_factor[ni.index];
        total++;
      }
      SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

      if (total > 0) {
        mul_v3_v3fl(filter_cache->detail_directions[i], direction_avg, 1.0f / total);
        filter_cache->sharpen_factor[i] = sharpen_avg / total;
      }
    }
  }
}

static void mesh_filter_surface_smooth_displace_task_cb(
    void *__restrict userdata, const int i, const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE)
  {
    float fade = vd.mask ? *vd.mask : 0.0f;
    fade = 1.0f - fade;
    fade *= data->filter_strength;
    fade *= SCULPT_automasking_factor_get(ss->filter_cache->automasking, ss, vd.index);
    if (fade == 0.0f) {
      continue;
    }

    SCULPT_surface_smooth_displace_step(ss,
                                        vd.co,
                                        ss->filter_cache->surface_smooth_laplacian_disp,
                                        vd.index,
                                        ss->filter_cache->surface_smooth_current_vertex,
                                        clamp_f(fade, 0.0f, 1.0f));
  }
  BKE_pbvh_vertex_iter_end;
}

static int sculpt_mesh_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  eSculptMeshFilterType filter_type = RNA_enum_get(op->ptr, "type");
  float filter_strength = RNA_float_get(op->ptr, "strength");

  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    SCULPT_filter_cache_free(ss);
    SCULPT_undo_push_end();
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  const float len = event->prevclickx - event->x;
  filter_strength = filter_strength * -len * 0.001f * UI_DPI_FAC;

  SCULPT_vertex_random_access_ensure(ss);

  bool needs_pmap = sculpt_mesh_filter_needs_pmap(filter_type);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, needs_pmap, false, false);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = ss->filter_cache->nodes,
      .filter_type = filter_type,
      .filter_strength = filter_strength,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, ss->filter_cache->totnode);
  BLI_task_parallel_range(0, ss->filter_cache->totnode, &data, mesh_filter_task_cb, &settings);

  if (filter_type == MESH_FILTER_SURFACE_SMOOTH) {
    BLI_task_parallel_range(0,
                            ss->filter_cache->totnode,
                            &data,
                            mesh_filter_surface_smooth_displace_task_cb,
                            &settings);
  }

  ss->filter_cache->iteration_count++;

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, ob, true);
  }

  /* The relax mesh filter needs the updated normals of the modified mesh after each iteration. */
  if (ELEM(MESH_FILTER_RELAX, MESH_FILTER_RELAX_FACE_SETS)) {
    BKE_pbvh_update_normals(ss->pbvh, ss->subdiv_ccg);
  }

  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_mesh_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;

  const eMeshFilterDeformAxis deform_axis = RNA_enum_get(op->ptr, "deform_axis");
  const eSculptMeshFilterType filter_type = RNA_enum_get(op->ptr, "type");
  const bool use_automasking = SCULPT_is_automasking_enabled(sd, ss, NULL);
  const bool needs_topology_info = sculpt_mesh_filter_needs_pmap(filter_type) || use_automasking;

  if (deform_axis == 0) {
    /* All axis are disabled, so the filter is not going to produce any deformation. */
    return OPERATOR_CANCELLED;
  }

  if (use_automasking) {
    /* Update the active face set manually as the paint cursor is not enabled when using the Mesh
     * Filter Tool. */
    float mouse[2];
    SculptCursorGeometryInfo sgi;
    mouse[0] = event->mval[0];
    mouse[1] = event->mval[1];
    SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false);
  }

  SCULPT_vertex_random_access_ensure(ss);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, needs_topology_info, false, false);
  if (needs_topology_info) {
    SCULPT_boundary_info_ensure(ob);
  }

  SCULPT_undo_push_begin(ob, "Mesh Filter");

  SCULPT_filter_cache_init(C, ob, sd, SCULPT_UNDO_COORDS);

  FilterCache *filter_cache = ss->filter_cache;
  filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  filter_cache->automasking = SCULPT_automasking_cache_init(sd, NULL, ob);

  switch (filter_type) {
    case MESH_FILTER_SURFACE_SMOOTH: {
      const float shape_preservation = RNA_float_get(op->ptr, "surface_smooth_shape_preservation");
      const float current_vertex_displacement = RNA_float_get(op->ptr,
                                                              "surface_smooth_current_vertex");
      mesh_filter_surface_smooth_init(ss, shape_preservation, current_vertex_displacement);
      break;
    }
    case MESH_FILTER_SHARPEN: {
      const float smooth_ratio = RNA_float_get(op->ptr, "sharpen_smooth_ratio");
      const float intensify_detail_strength = RNA_float_get(op->ptr,
                                                            "sharpen_intensify_detail_strength");
      const int curvature_smooth_iterations = RNA_int_get(op->ptr,
                                                          "sharpen_curvature_smooth_iterations");
      mesh_filter_sharpen_init(
          ss, smooth_ratio, intensify_detail_strength, curvature_smooth_iterations);
      break;
    }
    case MESH_FILTER_ENHANCE_DETAILS: {
      mesh_filter_enhance_details_init_directions(ss);
      break;
    }
    case MESH_FILTER_ERASE_DISPLACEMENT: {
      mesh_filter_init_limit_surface_co(ss);
      break;
    }
    default:
      break;
  }

  ss->filter_cache->enabled_axis[0] = deform_axis & MESH_FILTER_DEFORM_X;
  ss->filter_cache->enabled_axis[1] = deform_axis & MESH_FILTER_DEFORM_Y;
  ss->filter_cache->enabled_axis[2] = deform_axis & MESH_FILTER_DEFORM_Z;

  SculptFilterOrientation orientation = RNA_enum_get(op->ptr, "orientation");
  ss->filter_cache->orientation = orientation;

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SCULPT_OT_mesh_filter(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Filter Mesh";
  ot->idname = "SCULPT_OT_mesh_filter";
  ot->description = "Applies a filter to modify the current mesh";

  /* API callbacks. */
  ot->invoke = sculpt_mesh_filter_invoke;
  ot->modal = sculpt_mesh_filter_modal;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* RNA. */
  RNA_def_enum(ot->srna,
               "type",
               prop_mesh_filter_types,
               MESH_FILTER_INFLATE,
               "Filter Type",
               "Operation that is going to be applied to the mesh");
  RNA_def_float(
      ot->srna, "strength", 1.0f, -10.0f, 10.0f, "Strength", "Filter strength", -10.0f, 10.0f);
  RNA_def_enum_flag(ot->srna,
                    "deform_axis",
                    prop_mesh_filter_deform_axis_items,
                    MESH_FILTER_DEFORM_X | MESH_FILTER_DEFORM_Y | MESH_FILTER_DEFORM_Z,
                    "Deform Axis",
                    "Apply the deformation in the selected axis");
  RNA_def_enum(ot->srna,
               "orientation",
               prop_mesh_filter_orientation_items,
               SCULPT_FILTER_ORIENTATION_LOCAL,
               "Orientation",
               "Orientation of the axis to limit the filter displacement");

  /* Surface Smooth Mesh Filter properties. */
  RNA_def_float(ot->srna,
                "surface_smooth_shape_preservation",
                0.5f,
                0.0f,
                1.0f,
                "Shape Preservation",
                "How much of the original shape is preserved when smoothing",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "surface_smooth_current_vertex",
                0.5f,
                0.0f,
                1.0f,
                "Per Vertex Displacement",
                "How much the position of each individual vertex influences the final result",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "sharpen_smooth_ratio",
                0.35f,
                0.0f,
                1.0f,
                "Smooth Ratio",
                "How much smoothing is applied to polished surfaces",
                0.0f,
                1.0f);

  RNA_def_float(ot->srna,
                "sharpen_intensify_detail_strength",
                0.0f,
                0.0f,
                10.0f,
                "Intensify Details",
                "How much creases and valleys are intensified",
                0.0f,
                1.0f);

  RNA_def_int(ot->srna,
              "sharpen_curvature_smooth_iterations",
              0,
              0,
              10,
              "Curvature Smooth Iterations",
              "How much smooth the resulting shape is, ignoring high frequency details",
              0,
              10);
}
