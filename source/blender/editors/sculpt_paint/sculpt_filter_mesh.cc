/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "DNA_modifier_types.h"
#include "DNA_windowmanager_types.h"
#include "MEM_guardedalloc.h"

#include "BLI_hash.h"
#include "BLI_index_range.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_meshdata_types.h"

#include "BKE_brush.hh"
#include "BKE_context.h"
#include "BKE_modifier.h"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "DEG_depsgraph.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_util.hh"
#include "ED_view3d.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "bmesh.h"

#include <cmath>
#include <cstdlib>

using blender::float2;
using blender::float3;
using blender::IndexRange;

void SCULPT_filter_to_orientation_space(float r_v[3], FilterCache *filter_cache)
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

void SCULPT_filter_to_object_space(float r_v[3], FilterCache *filter_cache)
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

void SCULPT_filter_zero_disabled_axis_components(float r_v[3], FilterCache *filter_cache)
{
  SCULPT_filter_to_orientation_space(r_v, filter_cache);
  for (int axis = 0; axis < 3; axis++) {
    if (!filter_cache->enabled_force_axis[axis]) {
      r_v[axis] = 0.0f;
    }
  }
  SCULPT_filter_to_object_space(r_v, filter_cache);
}

void SCULPT_filter_cache_init(bContext *C,
                              Object *ob,
                              Sculpt *sd,
                              const int undo_type,
                              const float mval_fl[2],
                              float area_normal_radius,
                              float start_strength)
{
  using namespace blender;
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ob->sculpt->pbvh;

  ss->filter_cache = MEM_new<FilterCache>(__func__);
  ss->filter_cache->start_filter_strength = start_strength;
  ss->filter_cache->random_seed = rand();

  if (undo_type == SCULPT_UNDO_COLOR) {
    BKE_pbvh_ensure_node_loops(ss->pbvh);
  }

  const float center[3] = {0.0f};
  SculptSearchSphereData search_data{};
  search_data.original = true;
  search_data.center = center;
  search_data.radius_squared = FLT_MAX;
  search_data.ignore_fully_ineffective = true;

  ss->filter_cache->nodes = blender::bke::pbvh::search_gather(
      pbvh, [&](PBVHNode &node) { return SCULPT_search_sphere(&node, &search_data); });

  for (PBVHNode *node : ss->filter_cache->nodes) {
    BKE_pbvh_node_mark_normals_update(node);
  }

  /* `mesh->runtime.subdiv_ccg` is not available. Updating of the normals is done during drawing.
   * Filters can't use normals in multi-resolution. */
  if (BKE_pbvh_type(ss->pbvh) != PBVH_GRIDS) {
    BKE_pbvh_update_normals(ss->pbvh, nullptr);
  }

  for (const int i : ss->filter_cache->nodes.index_range()) {
    SCULPT_undo_push_node(ob, ss->filter_cache->nodes[i], SculptUndoType(undo_type));
  }

  /* Setup orientation matrices. */
  copy_m4_m4(ss->filter_cache->obmat, ob->object_to_world);
  invert_m4_m4(ss->filter_cache->obmat_inv, ob->object_to_world);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  ss->filter_cache->vc = vc;
  if (vc.rv3d) {
    copy_m4_m4(ss->filter_cache->viewmat, vc.rv3d->viewmat);
    copy_m4_m4(ss->filter_cache->viewmat_inv, vc.rv3d->viewinv);
  }

  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

  float co[3];

  if (vc.rv3d && SCULPT_stroke_get_location(C, co, mval_fl, false)) {
    Vector<PBVHNode *> nodes;

    /* Get radius from brush. */
    Brush *brush = BKE_paint_brush(&sd->paint);
    float radius;

    if (brush) {
      if (BKE_brush_use_locked_size(scene, brush)) {
        radius = paint_calc_object_space_radius(
            &vc, co, float(BKE_brush_size_get(scene, brush) * area_normal_radius));
      }
      else {
        radius = BKE_brush_unprojected_radius_get(scene, brush) * area_normal_radius;
      }
    }
    else {
      radius = paint_calc_object_space_radius(&vc, co, float(ups->size) * area_normal_radius);
    }

    SculptSearchSphereData search_data2{};
    search_data2.original = true;
    search_data2.center = co;
    search_data2.radius_squared = radius * radius;
    search_data2.ignore_fully_ineffective = true;

    nodes = blender::bke::pbvh::search_gather(
        pbvh, [&](PBVHNode &node) { return SCULPT_search_sphere(&node, &search_data2); });

    if (BKE_paint_brush(&sd->paint) &&
        SCULPT_pbvh_calc_area_normal(brush, ob, nodes, ss->filter_cache->initial_normal))
    {
      copy_v3_v3(ss->last_normal, ss->filter_cache->initial_normal);
    }
    else {
      copy_v3_v3(ss->filter_cache->initial_normal, ss->last_normal);
    }

    /* Update last stroke location */

    mul_m4_v3(ob->object_to_world, co);

    add_v3_v3(ups->average_stroke_accum, co);
    ups->average_stroke_counter++;
    ups->last_stroke_valid = true;
  }
  else {
    /* Use last normal. */
    copy_v3_v3(ss->filter_cache->initial_normal, ss->last_normal);
  }

  /* Update view normal */
  float projection_mat[4][4];
  float mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};

  if (vc.rv3d) {
    ED_view3d_ob_project_mat_get(vc.rv3d, ob, projection_mat);

    invert_m4_m4(ob->world_to_object, ob->object_to_world);
    copy_m3_m4(mat, vc.rv3d->viewinv);
    mul_m3_v3(mat, viewDir);
    copy_m3_m4(mat, ob->world_to_object);
    mul_m3_v3(mat, viewDir);
    normalize_v3_v3(ss->filter_cache->view_normal, viewDir);
  }
}

void SCULPT_filter_cache_free(SculptSession *ss, Object *ob)
{
  if (ss->filter_cache->cloth_sim) {
    SCULPT_cloth_simulation_free(ss->filter_cache->cloth_sim);
  }
  if (ss->filter_cache->automasking) {
    SCULPT_automasking_cache_free(ss, nullptr, ss->filter_cache->automasking);
  }
  MEM_SAFE_FREE(ss->filter_cache->mask_update_it);
  MEM_SAFE_FREE(ss->filter_cache->prev_mask);
  MEM_SAFE_FREE(ss->filter_cache->normal_factor);
  MEM_SAFE_FREE(ss->filter_cache->prev_face_set);
  MEM_SAFE_FREE(ss->filter_cache->sharpen_factor);
  MEM_SAFE_FREE(ss->filter_cache->detail_directions);
  MEM_SAFE_FREE(ss->filter_cache->limit_surface_co);
  MEM_SAFE_FREE(ss->filter_cache->pre_smoothed_color);
  MEM_delete<FilterCache>(ss->filter_cache);
  ss->filter_cache = nullptr;
}

enum eSculptMeshFilterType {
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
};

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
    {0, nullptr, 0, nullptr, nullptr},
};

enum eMeshFilterDeformAxis {
  MESH_FILTER_DEFORM_X = 1 << 0,
  MESH_FILTER_DEFORM_Y = 1 << 1,
  MESH_FILTER_DEFORM_Z = 1 << 2,
};

static EnumPropertyItem prop_mesh_filter_deform_axis_items[] = {
    {MESH_FILTER_DEFORM_X, "X", 0, "X", "Deform in the X axis"},
    {MESH_FILTER_DEFORM_Y, "Y", 0, "Y", "Deform in the Y axis"},
    {MESH_FILTER_DEFORM_Z, "Z", 0, "Z", "Deform in the Z axis"},
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
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

static bool sculpt_mesh_filter_is_continuous(eSculptMeshFilterType type)
{
  return ELEM(type,
              MESH_FILTER_SHARPEN,
              MESH_FILTER_SMOOTH,
              MESH_FILTER_RELAX,
              MESH_FILTER_RELAX_FACE_SETS);
}

static void mesh_filter_task_update_boundaries(Object *ob, PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;

  BKE_pbvh_check_tri_areas(ss->pbvh, node);

  /* Ensure boundaries and valences are up to date. */
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_vertex_is_boundary(ss, vd.vertex, SCULPT_BOUNDARY_ALL);
    SCULPT_vertex_valence_get(ss, vd.vertex);
  }
  BKE_pbvh_vertex_iter_end;
}

static void mesh_filter_task(Object *ob,
                             const eSculptMeshFilterType filter_type,
                             const float filter_strength,
                             PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, ob, node, SCULPT_UNDO_COORDS);

  /* When using the relax face sets meshes filter,
   * each 3 iterations, do a whole mesh relax to smooth the contents of the Face Set. */
  /* This produces better results as the relax operation is no completely focused on the
   * boundaries. */
  const bool relax_face_sets = !(ss->filter_cache->iteration_count % 3 == 0);
  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(ob, ss, ss->filter_cache->automasking, &automask_data, node);

  /* Smooth parameters. */
  float projection = 0.0f;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(ss, &orig_data, vd.vertex);
    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    float orig_co[3], val[3], avg[3], disp[3], disp2[3], transform[3][3], final_pos[3];
    float fade = vd.mask ? *vd.mask : 0.0f;
    fade = 1.0f - fade;
    fade *= filter_strength;
    fade *= SCULPT_automasking_factor_get(
        ss->filter_cache->automasking, ss, vd.vertex, &automask_data);

    if (fade == 0.0f && filter_type != MESH_FILTER_SURFACE_SMOOTH) {
      /* Surface Smooth can't skip the loop for this vertex as it needs to calculate its
       * laplacian_disp. This value is accessed from the vertex neighbors when deforming the
       * vertices, so it is needed for all vertices even if they are not going to be displaced.
       */
      continue;
    }

    if (ELEM(filter_type, MESH_FILTER_RELAX, MESH_FILTER_RELAX_FACE_SETS) ||
        ss->filter_cache->no_orig_co)
    {
      copy_v3_v3(orig_co, vd.co);
    }
    else {
      copy_v3_v3(orig_co, orig_data.co);
    }

    if (filter_type == MESH_FILTER_RELAX_FACE_SETS) {
      if (relax_face_sets == SCULPT_vertex_has_unique_face_set(ss, vd.vertex)) {
        continue;
      }
    }

    switch (filter_type) {
      case MESH_FILTER_SMOOTH:
        fade = clamp_f(fade, -1.0f, 1.0f);
        SCULPT_neighbor_coords_average_interior(
            ss, avg, vd.vertex, projection, ss->filter_cache->hard_corner_pin, true);
        sub_v3_v3v3(val, avg, orig_co);
        madd_v3_v3v3fl(val, orig_co, val, fade);
        sub_v3_v3v3(disp, val, orig_co);
        break;
      case MESH_FILTER_INFLATE:
        mul_v3_v3fl(disp, orig_data.no, fade);
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
        float normal[3];
        copy_v3_v3(normal, orig_data.no);
        /* Index is not unique for multi-resolution, so hash by vertex coordinates. */
        const uint *hash_co = (const uint *)orig_co;
        const uint hash = BLI_hash_int_2d(hash_co[0], hash_co[1]) ^
                          BLI_hash_int_2d(hash_co[2], ss->filter_cache->random_seed);
        mul_v3_fl(normal, hash * (1.0f / float(0xFFFFFFFF)) - 0.5f);
        mul_v3_v3fl(disp, normal, fade);
        break;
      }
      case MESH_FILTER_RELAX: {
        SCULPT_relax_vertex(ss, &vd, clamp_f(fade, 0.0f, 1.0f), SCULPT_BOUNDARY_MESH, val);
        sub_v3_v3v3(disp, val, vd.co);
        break;
      }
      case MESH_FILTER_RELAX_FACE_SETS: {
        eSculptBoundary boundtype = SCULPT_BOUNDARY_MESH;
        if (relax_face_sets) {
          boundtype |= SCULPT_BOUNDARY_FACE_SET;
        }
        SCULPT_relax_vertex(ss, &vd, clamp_f(fade, 0.0f, 1.0f), boundtype, val);
        sub_v3_v3v3(disp, val, vd.co);
        break;
      }
      case MESH_FILTER_SURFACE_SMOOTH: {
        SCULPT_surface_smooth_laplacian_step(ss, disp, vd.co, vd.vertex, orig_data.co, 1.0f, true);
        break;
      }
      case MESH_FILTER_SHARPEN: {
        const float smooth_ratio = ss->filter_cache->sharpen_smooth_ratio;

        /* This filter can't work at full strength as it needs multiple iterations to reach a
         * stable state. */
        fade = clamp_f(fade, 0.0f, 0.5f);
        float disp_sharpen[3] = {0.0f, 0.0f, 0.0f};

        SculptVertexNeighborIter ni;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
          float disp_n[3];
          sub_v3_v3v3(
              disp_n, SCULPT_vertex_co_get(ss, ni.vertex), SCULPT_vertex_co_get(ss, vd.vertex));
          mul_v3_fl(disp_n, ss->filter_cache->sharpen_factor[ni.index]);
          add_v3_v3(disp_sharpen, disp_n);
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

        mul_v3_fl(disp_sharpen, 1.0f - ss->filter_cache->sharpen_factor[vd.index]);

        float disp_avg[3];
        float avg_co[3];
        SCULPT_neighbor_coords_average(
            ss, avg_co, vd.vertex, projection, ss->filter_cache->hard_corner_pin, true);
        sub_v3_v3v3(disp_avg, avg_co, vd.co);
        mul_v3_v3fl(
            disp_avg, disp_avg, smooth_ratio * pow2f(ss->filter_cache->sharpen_factor[vd.index]));
        add_v3_v3v3(disp, disp_avg, disp_sharpen);

        /* Intensify details. */
        if (ss->filter_cache->sharpen_intensify_detail_strength > 0.0f) {
          float detail_strength[3];
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
    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
    BKE_sculpt_sharp_boundary_flag_update(ss, vd.vertex);
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(node);
}

static void mesh_filter_enhance_details_init_directions(SculptSession *ss)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  FilterCache *filter_cache = ss->filter_cache;

  filter_cache->detail_directions = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(totvert, sizeof(float[3]), __func__));
  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    float avg[3];
    SCULPT_neighbor_coords_average(ss, avg, vertex, 0.0f, filter_cache->hard_corner_pin, true);
    sub_v3_v3v3(filter_cache->detail_directions[i], avg, SCULPT_vertex_co_get(ss, vertex));
  }
}

static void mesh_filter_surface_smooth_init(SculptSession *ss,
                                            const float shape_preservation,
                                            const float current_vertex_displacement)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  FilterCache *filter_cache = ss->filter_cache;

  filter_cache->surface_smooth_shape_preservation = shape_preservation;
  filter_cache->surface_smooth_current_vertex = current_vertex_displacement;
}

static void mesh_filter_init_limit_surface_co(SculptSession *ss)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  FilterCache *filter_cache = ss->filter_cache;

  filter_cache->limit_surface_co = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(totvert, sizeof(float[3]), __func__));
  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    SCULPT_vertex_limit_surface_get(ss, vertex, filter_cache->limit_surface_co[i]);
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
  filter_cache->sharpen_factor = static_cast<float *>(
      MEM_malloc_arrayN(totvert, sizeof(float), __func__));
  filter_cache->detail_directions = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(totvert, sizeof(float[3]), __func__));

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    float avg[3];
    SCULPT_neighbor_coords_average(ss, avg, vertex, 0.0f, filter_cache->hard_corner_pin, true);
    sub_v3_v3v3(filter_cache->detail_directions[i], avg, SCULPT_vertex_co_get(ss, vertex));
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
       smooth_iterations++)
  {
    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

      float direction_avg[3] = {0.0f, 0.0f, 0.0f};
      float sharpen_avg = 0;
      int total = 0;

      SculptVertexNeighborIter ni;
      SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
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

static void mesh_filter_surface_smooth_displace_task(Object *ob,
                                                     const float filter_strength,
                                                     PBVHNode *node)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(ob, ss, ss->filter_cache->automasking, &automask_data, node);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    float fade = vd.mask ? *vd.mask : 0.0f;
    fade = 1.0f - fade;
    fade *= filter_strength;
    fade *= SCULPT_automasking_factor_get(
        ss->filter_cache->automasking, ss, vd.vertex, &automask_data);
    if (fade == 0.0f) {
      continue;
    }

    SCULPT_surface_smooth_displace_step(ss,
                                        vd.co,
                                        vd.vertex,
                                        ss->filter_cache->surface_smooth_current_vertex,
                                        clamp_f(fade, 0.0f, 1.0f));
  }
  BKE_pbvh_vertex_iter_end;
}

enum {
  FILTER_MESH_MODAL_CANCEL = 1,
  FILTER_MESH_MODAL_CONFIRM,
};

wmKeyMap *filter_mesh_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {FILTER_MESH_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {FILTER_MESH_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Mesh Filter Modal Map");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return nullptr;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Mesh Filter Modal Map", modal_items);

  WM_modalkeymap_assign(keymap, "SCULPT_OT_mesh_filter");

  return keymap;
}

static void sculpt_mesh_update_status_bar(bContext *C, wmOperator *op)
{
  char header[UI_MAX_DRAW_STR];
  char buf[UI_MAX_DRAW_STR];
  int available_len = sizeof(buf);

  char *p = buf;
#define WM_MODALKEY(_id) \
  WM_modalkeymap_operator_items_to_string_buf( \
      op->type, (_id), true, UI_MAX_SHORTCUT_STR, &available_len, &p)

  SNPRINTF(header,
           TIP_("%s: Confirm, %s: Cancel"),
           WM_MODALKEY(FILTER_MESH_MODAL_CONFIRM),
           WM_MODALKEY(FILTER_MESH_MODAL_CANCEL));

#undef WM_MODALKEY

  ED_workspace_status_text(C, TIP_(header));
}

static void sculpt_mesh_filter_apply(bContext *C, wmOperator *op)
{
  using namespace blender;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  eSculptMeshFilterType filter_type = eSculptMeshFilterType(RNA_enum_get(op->ptr, "type"));
  float filter_strength = RNA_float_get(op->ptr, "strength");

  SCULPT_vertex_random_access_ensure(ss);

  if (filter_type == MESH_FILTER_SURFACE_SMOOTH) {
    SCULPT_surface_smooth_laplacian_init(ob);
  }

  ss->filter_cache->preserve_fset_boundaries = !ss->hard_edge_mode;

  if (ELEM(filter_type,
           MESH_FILTER_SMOOTH,
           MESH_FILTER_SURFACE_SMOOTH,
           MESH_FILTER_ENHANCE_DETAILS,
           MESH_FILTER_SHARPEN))
  {
    BKE_pbvh_face_areas_begin(ss->pbvh);
    threading::parallel_for(
        ss->filter_cache->nodes.index_range(), 1, [&](const IndexRange &range) {
          for (const int i : range) {
            mesh_filter_task_update_boundaries(ob, ss->filter_cache->nodes[i]);
          }
        });
  }

  threading::parallel_for(ss->filter_cache->nodes.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      mesh_filter_task(
          ob, eSculptMeshFilterType(filter_type), filter_strength, ss->filter_cache->nodes[i]);
    }
  });

  if (filter_type == MESH_FILTER_SURFACE_SMOOTH) {
    threading::parallel_for(ss->filter_cache->nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        mesh_filter_surface_smooth_displace_task(ob, filter_strength, ss->filter_cache->nodes[i]);
      }
    });
  }

  ss->filter_cache->iteration_count++;

  if (ss->deform_modifiers_active || ss->shapekey_active) {
    SCULPT_flush_stroke_deform(sd, ob, true);
  }

  /* The relax mesh filter needs the updated normals of the modified mesh after each iteration.
   */
  if (ELEM(MESH_FILTER_RELAX, MESH_FILTER_RELAX_FACE_SETS)) {
    BKE_pbvh_update_normals(ss->pbvh, ss->subdiv_ccg);
  }

  SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
}

static void sculpt_mesh_update_strength(wmOperator *op,
                                        SculptSession *ss,
                                        float2 prev_press_mouse,
                                        float2 mouse)
{
  const float len = prev_press_mouse[0] - mouse[0];

  float filter_strength = ss->filter_cache->start_filter_strength * -len * 0.001f * UI_SCALE_FAC;
  RNA_float_set(op->ptr, "strength", filter_strength);
}
static void sculpt_mesh_filter_apply_with_history(bContext *C, wmOperator *op)
{
  /* Event history is only stored for smooth and relax filters. */
  if (!RNA_collection_length(op->ptr, "event_history")) {
    sculpt_mesh_filter_apply(C, op);
    return;
  }

  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  float2 start_mouse;
  bool first = true;
  float initial_strength = ss->filter_cache->start_filter_strength;

  RNA_BEGIN (op->ptr, item, "event_history") {
    float2 mouse;
    RNA_float_get_array(&item, "mouse_event", mouse);

    if (first) {
      first = false;
      start_mouse = mouse;
      continue;
    }

    sculpt_mesh_update_strength(op, ss, start_mouse, mouse);
    sculpt_mesh_filter_apply(C, op);
  }
  RNA_END;

  RNA_float_set(op->ptr, "strength", initial_strength);
}

static void sculpt_mesh_filter_end(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  SCULPT_filter_cache_free(ss, ob);
  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
}

static int sculpt_mesh_filter_confirm(SculptSession *ss,
                                      wmOperator *op,
                                      const eSculptMeshFilterType filter_type)
{

  float initial_strength = ss->filter_cache->start_filter_strength;
  /* Don't update strength property if we're storing an event history. */
  if (sculpt_mesh_filter_is_continuous(filter_type)) {
    RNA_float_set(op->ptr, "strength", initial_strength);
  }

  return OPERATOR_FINISHED;
}

static void sculpt_mesh_filter_cancel(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss || !ss->pbvh) {
    return;
  }

  /* Gather all PBVH leaf nodes. */
  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, {});

  for (PBVHNode *node : nodes) {
    PBVHVertexIter vd;

    SculptOrigVertData orig_data;
    SCULPT_orig_vert_data_init(&orig_data, ob, node, SCULPT_UNDO_COORDS);

    BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
      SCULPT_orig_vert_data_update(ss, &orig_data, vd.vertex);

      copy_v3_v3(vd.co, orig_data.co);
    }
    BKE_pbvh_vertex_iter_end;

    BKE_pbvh_node_mark_update(node);
  }

  BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB);
}

static int sculpt_mesh_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SculptSession *ss = ob->sculpt;
  const eSculptMeshFilterType filter_type = eSculptMeshFilterType(RNA_enum_get(op->ptr, "type"));

  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_EW_SCROLL);
  sculpt_mesh_update_status_bar(C, op);

  if (event->type == EVT_MODAL_MAP) {
    int ret = OPERATOR_FINISHED;
    switch (event->val) {
      case FILTER_MESH_MODAL_CANCEL:
        sculpt_mesh_filter_cancel(C, op);
        SCULPT_undo_push_end_ex(ob, true);
        ret = OPERATOR_CANCELLED;
        break;

      case FILTER_MESH_MODAL_CONFIRM:
        ret = sculpt_mesh_filter_confirm(ss, op, filter_type);
        SCULPT_undo_push_end_ex(ob, false);
        break;
    }

    sculpt_mesh_filter_end(C);
    ED_workspace_status_text(C, nullptr); /* Clear status bar */
    WM_cursor_modal_restore(CTX_wm_window(C));

    return ret;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* Note: some filter types are continuous, for these we store an
   * event history in RNA for continuous.
   * This way the user can tweak the last operator properties
   * or repeat the op and get expected results. */
  if (sculpt_mesh_filter_is_continuous(filter_type)) {
    if (RNA_collection_length(op->ptr, "event_history") == 0) {
      /* First entry is the start mouse position, event->prev_press_xy. */
      PointerRNA startptr;
      RNA_collection_add(op->ptr, "event_history", &startptr);

      float2 mouse_start(float(event->prev_press_xy[0]), float(event->prev_press_xy[1]));
      RNA_float_set_array(&startptr, "mouse_event", mouse_start);
    }

    PointerRNA itemptr;
    RNA_collection_add(op->ptr, "event_history", &itemptr);

    float2 mouse(float(event->xy[0]), float(event->xy[1]));
    RNA_float_set_array(&itemptr, "mouse_event", mouse);
    RNA_float_set(&itemptr, "pressure", WM_event_tablet_data(event, nullptr, nullptr));
  }

  float2 prev_mval(float(event->prev_press_xy[0]), float(event->prev_press_xy[1]));
  float2 mval(float(event->xy[0]), float(event->xy[1]));

  sculpt_mesh_update_strength(op, ss, prev_mval, mval);

  bool needs_pmap = sculpt_mesh_filter_needs_pmap(filter_type);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, needs_pmap, false, false);

  sculpt_mesh_filter_apply(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void sculpt_filter_specific_init(const eSculptMeshFilterType filter_type,
                                        wmOperator *op,
                                        SculptSession *ss)
{
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
}

/* Returns OPERATOR_PASS_THROUGH on success. */
static int sculpt_mesh_filter_start(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  ToolSettings *tool_settings = CTX_data_tool_settings(C);
  Sculpt *sd = tool_settings->sculpt;
  int mval[2];
  RNA_int_get_array(op->ptr, "start_mouse", mval);

  const eSculptMeshFilterType filter_type = eSculptMeshFilterType(RNA_enum_get(op->ptr, "type"));
  const bool use_automasking = SCULPT_is_automasking_enabled(sd, nullptr, nullptr);
  const bool needs_topology_info = sculpt_mesh_filter_needs_pmap(filter_type) || use_automasking;

  BKE_sculpt_update_object_for_edit(depsgraph, ob, needs_topology_info, false, false);
  SculptSession *ss = ob->sculpt;

  const eMeshFilterDeformAxis deform_axis = eMeshFilterDeformAxis(
      RNA_enum_get(op->ptr, "deform_axis"));

  if (deform_axis == 0) {
    /* All axis are disabled, so the filter is not going to produce any deformation. */
    return OPERATOR_CANCELLED;
  }

  SCULPT_stroke_id_next(ob);
  float mval_fl[2] = {float(mval[0]), float(mval[1])};

  if (use_automasking) {
    /* Update the active face set manually as the paint cursor is not enabled when using the Mesh
     * Filter Tool. */
    SculptCursorGeometryInfo sgi;
    SCULPT_cursor_geometry_info_update(C, &sgi, mval_fl, false, false);
  }

  SCULPT_vertex_random_access_ensure(ss);
  if (needs_topology_info) {
    SCULPT_boundary_info_ensure(ob);
  }

  SCULPT_undo_push_begin(ob, op);

  SCULPT_filter_cache_init(C,
                           ob,
                           sd,
                           SCULPT_UNDO_COORDS,
                           mval_fl,
                           RNA_float_get(op->ptr, "area_normal_radius"),
                           RNA_float_get(op->ptr, "strength"));

  FilterCache *filter_cache = ss->filter_cache;
  filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  filter_cache->automasking = SCULPT_automasking_cache_init(sd, nullptr, ob);

  sculpt_filter_specific_init(filter_type, op, ss);

  ss->filter_cache->enabled_axis[0] = deform_axis & MESH_FILTER_DEFORM_X;
  ss->filter_cache->enabled_axis[1] = deform_axis & MESH_FILTER_DEFORM_Y;
  ss->filter_cache->enabled_axis[2] = deform_axis & MESH_FILTER_DEFORM_Z;

  SculptFilterOrientation orientation = SculptFilterOrientation(
      RNA_enum_get(op->ptr, "orientation"));
  ss->filter_cache->orientation = orientation;

  ss->filter_cache->hard_corner_pin = RNA_float_get(op->ptr, "hard_corner_pin");
  ss->hard_edge_mode = tool_settings->unified_paint_settings.hard_edge_mode;

  return OPERATOR_PASS_THROUGH;
}

static int sculpt_mesh_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_int_set_array(op->ptr, "start_mouse", event->mval);
  int ret = sculpt_mesh_filter_start(C, op);

  if (ret == OPERATOR_PASS_THROUGH) {
    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  return ret;
}

static int sculpt_mesh_filter_exec(bContext *C, wmOperator *op)
{
  int ret = sculpt_mesh_filter_start(C, op);

  if (ret == OPERATOR_PASS_THROUGH) {
    Object *ob = CTX_data_active_object(C);
    SculptSession *ss = ob->sculpt;

    int iterations = RNA_int_get(op->ptr, "iteration_count");
    bool has_history = RNA_collection_length(op->ptr, "event_history") > 0;

    if (!has_history) {
      ss->filter_cache->no_orig_co = true;
    }

    for (int i = 0; i < iterations; i++) {
      sculpt_mesh_filter_apply_with_history(C, op);

      ss->filter_cache->no_orig_co = true;
    }

    sculpt_mesh_filter_end(C);

    return OPERATOR_FINISHED;
  }

  return ret;
}

void SCULPT_mesh_filter_properties(wmOperatorType *ot)
{
  RNA_def_int_array(
      ot->srna, "start_mouse", 2, nullptr, 0, 1 << 14, "Starting Mouse", "", 0, 1 << 14);

  RNA_def_float(
      ot->srna,
      "area_normal_radius",
      0.25,
      0.001,
      5.0,
      "Normal Radius",
      "Radius used for calculating area normal on initial click,\nin percentage of brush radius",
      0.01,
      1.0);
  RNA_def_float(
      ot->srna, "strength", 1.0f, -10.0f, 10.0f, "Strength", "Filter strength", -10.0f, 10.0f);
  RNA_def_int(ot->srna,
              "iteration_count",
              1,
              1,
              10000,
              "Repeat",
              "How many times to repeat the filter",
              1,
              100);

  /* Smooth filter requires entire event history. */
  PropertyRNA *prop = RNA_def_collection_runtime(
      ot->srna, "event_history", &RNA_OperatorStrokeElement, "", "");
  RNA_def_property_flag(prop, PropertyFlag(int(PROP_HIDDEN) | int(PROP_SKIP_SAVE)));

  RNA_def_float(ot->srna,
                "hard_corner_pin",
                1.0f,
                0.0,
                1.0f,
                "Corner Pin",
                "How much to pin corners in hard edge mode",
                0.0f,
                1.0f);
}

static void sculpt_mesh_ui_exec(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;

  uiItemR(layout, op->ptr, "strength", eUI_Item_Flag(0), nullptr, ICON_NONE);
  if (ELEM(RNA_enum_get(op->ptr, "type"),
           MESH_FILTER_SMOOTH,
           MESH_FILTER_SURFACE_SMOOTH,
           MESH_FILTER_ENHANCE_DETAILS,
           MESH_FILTER_SHARPEN))
  {
    uiItemR(layout, op->ptr, "hard_corner_pin", eUI_Item_Flag(0), nullptr, ICON_NONE);
  }

  uiItemR(layout, op->ptr, "iteration_count", eUI_Item_Flag(0), nullptr, ICON_NONE);
  uiItemR(layout, op->ptr, "orientation", eUI_Item_Flag(0), nullptr, ICON_NONE);

  layout = uiLayoutRow(layout, true);
  uiItemR(layout, op->ptr, "deform_axis", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

void SCULPT_OT_mesh_filter(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Filter Mesh";
  ot->idname = "SCULPT_OT_mesh_filter";
  ot->description = "Applies a filter to modify the current mesh";

  /* API callbacks. */
  ot->invoke = sculpt_mesh_filter_invoke;
  ot->modal = sculpt_mesh_filter_modal;
  ot->poll = SCULPT_mode_poll;
  ot->exec = sculpt_mesh_filter_exec;
  ot->ui = sculpt_mesh_ui_exec;

  /* Doesn't seem to actually be called?
   * Check `sculpt_mesh_filter_modal` to see where it's really called. */
  ot->cancel = sculpt_mesh_filter_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_CURSOR_X | OPTYPE_BLOCKING |
             OPTYPE_DEPENDS_ON_CURSOR;

  /* RNA. */
  SCULPT_mesh_filter_properties(ot);

  RNA_def_enum(ot->srna,
               "type",
               prop_mesh_filter_types,
               MESH_FILTER_INFLATE,
               "Filter Type",
               "Operation that is going to be applied to the mesh");
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
