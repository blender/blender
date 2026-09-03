/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "mesh_paint.hh"

#include "BKE_mesh.h"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_scene.hh"

#include "BLI_math_geom_c.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation_c.hh"

#include "DEG_depsgraph.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ED_image.hh"
#include "ED_paint.hh"

#include "sculpt_intern.hh"

#include "../paint_intern.hh"

namespace blender::ed::sculpt_paint {

/* -------------------------------------------------------------------- */
/** \name Mode toggling
 * \{ */

static void ensure_valid_pivot(const Object &ob, Paint &paint)
{
  bke::PaintRuntime &paint_runtime = *paint.runtime;
  const bke::pbvh::Tree *pbvh = bke::object::pbvh_get(ob);

  /* Account for the case where no objects are evaluated. */
  if (!pbvh) {
    return;
  }

  /* No valid pivot? Use bounding box center. */
  if (paint_runtime.average_stroke_counter == 0 || !paint_runtime.last_stroke_valid) {
    const Bounds<float3> bounds = bke::pbvh::bounds_get(*pbvh);
    const float3 center = math::midpoint(bounds.min, bounds.max);
    const float3 location = math::transform_point(ob.object_to_world(), center);

    bke::paint::stroke_set_location(paint, location);
  }
}

static void init_session(
    Main &bmain, Depsgraph &depsgraph, Paint &paint, Object &ob, eObjectMode object_mode)
{
  BLI_assert(ob.runtime->sculpt_session == nullptr);
  ob.runtime->sculpt_session = MEM_new<SculptSession>(__func__);
  ob.runtime->sculpt_session->mode_type = object_mode;

  DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_evaluated_ensure(&depsgraph, &bmain);
  BKE_sculptsession_update_for_edit(&depsgraph, &ob, true);

  ensure_valid_pivot(ob, paint);
}

void mode_enter_generic(
    Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob, const eObjectMode mode_flag)
{
  ob.mode |= mode_flag;

  BKE_object_free_derived_caches(&ob);

  Paint *paint = nullptr;
  if (mode_flag == OB_MODE_VERTEX_PAINT) {
    const PaintMode paint_mode = PaintMode::Vertex;

    BKE_paint_init(&bmain, &scene, paint_mode);
    paint = BKE_paint_get_active_from_paintmode(&scene, paint_mode);
    ED_paint_cursor_start(paint, vertex_paint_poll);
  }
  else if (mode_flag == OB_MODE_WEIGHT_PAINT) {
    const PaintMode paint_mode = PaintMode::Weight;

    BKE_paint_init(&bmain, &scene, paint_mode);
    paint = BKE_paint_get_active_from_paintmode(&scene, paint_mode);
    ED_paint_cursor_start(paint, weight_paint_poll);
  }
  else if (mode_flag == OB_MODE_SCULPT) {
    const PaintMode paint_mode = PaintMode::Sculpt;

    BKE_paint_init(&bmain, &scene, paint_mode);
    paint = BKE_paint_get_active_from_paintmode(&scene, paint_mode);
    ED_paint_cursor_start(paint, brush_cursor_poll);
  }
  else {
    BLI_assert(0);
  }

  BKE_paint_brushes_validate(&bmain, paint);

  if (ob.runtime->sculpt_session) {
    MEM_delete(ob.runtime->sculpt_session->cache);
    ob.runtime->sculpt_session->cache = nullptr;
    BKE_sculptsession_free(&ob);
  }

  BLI_assert(paint != nullptr);
  init_session(bmain, depsgraph, *paint, ob, mode_flag);
}

void mode_exit_generic(Object &ob, const eObjectMode mode_flag)
{
  Mesh *mesh = BKE_mesh_from_object(&ob);
  ob.mode &= ~mode_flag;

  if (ELEM(mode_flag, OB_MODE_VERTEX_PAINT, OB_MODE_WEIGHT_PAINT)) {
    if (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) {
      bke::mesh_select_face_flush(*mesh);
    }
    else if (mesh->editflag & ME_EDIT_PAINT_VERT_SEL) {
      bke::mesh_select_vert_flush(*mesh);
    }
  }

  /* If the cache is not released by a cancel or a done, free it now. */
  if (ob.runtime->sculpt_session) {
    MEM_delete(ob.runtime->sculpt_session->cache);
    ob.runtime->sculpt_session->cache = nullptr;
  }

  BKE_sculptsession_free(&ob);

  paint_cursor_delete_textures();

  /* Never leave derived meshes behind. */
  BKE_object_free_derived_caches(&ob);

  /* Flush object mode. */
  DEG_id_tag_update(&ob.id, ID_RECALC_SYNC_TO_EVAL);
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name Stroke-level Symmetry
 * \{ */

static void do_radial_symmetry(const Depsgraph &depsgraph,
                               const Scene &scene,
                               const Paint & /*paint*/,
                               const Brush &brush,
                               Object &object,
                               const BrushActionFn action_fn,
                               PaintModeData *paint_mode_data,
                               const ePaintSymmetryFlags symm,
                               const int axis)

{
  SculptSession &ss = *object.runtime->sculpt_session;
  const Mesh &mesh = *id_cast<Mesh *>(object.data);

  for (int i = 1; i < mesh.radial_symmetry[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / mesh.radial_symmetry[axis - 'X'];
    ss.cache->radial_symmetry_pass = i;
    cache_calc_brushdata_symm(*ss.cache, symm, axis, angle);
    action_fn(depsgraph, scene, brush, object, paint_mode_data);
  }
}

void do_symmetrical_brush_actions(const Depsgraph &depsgraph,
                                  const Scene &scene,
                                  const Paint &paint,
                                  Object &object,
                                  const BrushActionFn action_fn,
                                  PaintModeData *paint_mode_data)
{
  const Brush &brush = *BKE_paint_brush_for_read(&paint);
  SculptSession &ss = *object.runtime->sculpt_session;
  StrokeCache &cache = *ss.cache;
  const ePaintSymmetryFlags symm = mesh_symmetry_xyz_get(object);

  cache.bstrength = cache.base_brush_strength;

  /* `symm` is a bit combination of XYZ -
   * 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  for (int i = 0; i <= symm; i++) {
    if (!is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    const ePaintSymmetryFlags symm_pass = ePaintSymmetryFlags(i);
    cache.mirror_symmetry_pass = symm_pass;
    cache.radial_symmetry_pass = 0;

    cache_calc_brushdata_symm(cache, symm_pass, 0, 0);
    action_fn(depsgraph, scene, brush, object, paint_mode_data);

    do_radial_symmetry(
        depsgraph, scene, paint, brush, object, action_fn, paint_mode_data, symm_pass, 'X');
    do_radial_symmetry(
        depsgraph, scene, paint, brush, object, action_fn, paint_mode_data, symm_pass, 'Y');
    do_radial_symmetry(
        depsgraph, scene, paint, brush, object, action_fn, paint_mode_data, symm_pass, 'Z');
  }
}

static float calc_overlap(const StrokeCache &cache,
                          const ePaintSymmetryFlags symm,
                          const char axis,
                          const float angle)
{
  float3 mirror = symmetry_flip(cache.location, symm);

  if (axis != 0) {
    float mat[3][3];
    axis_angle_to_mat3_single(mat, axis, angle);
    mul_m3_v3(mat, mirror);
  }

  const float distsq = len_squared_v3v3(mirror, cache.location);

  if (distsq <= 4.0f * (cache.radius_squared)) {
    return (2.0f * (cache.radius) - sqrtf(distsq)) / (2.0f * (cache.radius));
  }
  return 0.0f;
}

static float calc_radial_symmetry_feather(const Mesh &mesh,
                                          const StrokeCache &cache,
                                          const ePaintSymmetryFlags symm,
                                          const char axis)
{
  float overlap = 0.0f;

  for (int i = 1; i < mesh.radial_symmetry[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / mesh.radial_symmetry[axis - 'X'];
    overlap += calc_overlap(cache, symm, axis, angle);
  }

  return overlap;
}

static float calc_symmetry_feather(const Paint &paint,
                                   const ePaintSymmetryFlags symm,
                                   const Mesh &mesh,
                                   const StrokeCache &cache)
{
  if (!(paint.symmetry_flags & PAINT_SYMMETRY_FEATHER)) {
    return 1.0f;
  }

  float overlap = 0.0f;
  for (int i = 0; i <= symm; i++) {
    if (!is_symmetry_iteration_valid(i, symm)) {
      continue;
    }

    overlap += calc_overlap(cache, ePaintSymmetryFlags(i), 0, 0);

    overlap += calc_radial_symmetry_feather(mesh, cache, ePaintSymmetryFlags(i), 'X');
    overlap += calc_radial_symmetry_feather(mesh, cache, ePaintSymmetryFlags(i), 'Y');
    overlap += calc_radial_symmetry_feather(mesh, cache, ePaintSymmetryFlags(i), 'Z');
  }
  return 1.0f / overlap;
}

static void do_tiled(const Depsgraph &depsgraph,
                     const Scene &scene,
                     const Paint &paint,
                     const Brush &brush,
                     Object &object,
                     const BrushActionFn action_fn,
                     PaintModeData *paint_mode_data)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  StrokeCache *cache = ss.cache;
  const float radius = cache->radius;
  const Bounds<float3> bb = *BKE_object_boundbox_get(&object);
  const float *bbMin = bb.min;
  const float *bbMax = bb.max;
  const float *step = paint.tile_offset;

  /* These are integer locations, for real location: multiply with step and add orgLoc.
   * So 0,0,0 is at orgLoc. */
  int start[3];
  int end[3];
  int cur[3];

  /* Position of the "prototype" stroke for tiling. */
  float orgLoc[3];
  float original_initial_location[3];
  copy_v3_v3(orgLoc, cache->location_symm);
  copy_v3_v3(original_initial_location, cache->initial_location_symm);

  for (int dim = 0; dim < 3; dim++) {
    if ((paint.symmetry_flags & (PAINT_TILE_X << dim)) && step[dim] > 0) {
      start[dim] = (bbMin[dim] - orgLoc[dim] - radius) / step[dim];
      end[dim] = (bbMax[dim] - orgLoc[dim] + radius) / step[dim];
    }
    else {
      start[dim] = end[dim] = 0;
    }
  }

  /* First do the "un-tiled" position to initialize the stroke for this location. */
  cache->tile_pass = 0;
  action_fn(depsgraph, scene, brush, object, paint_mode_data);

  /* Now do it for all the tiles. */
  copy_v3_v3_int(cur, start);
  for (cur[0] = start[0]; cur[0] <= end[0]; cur[0]++) {
    for (cur[1] = start[1]; cur[1] <= end[1]; cur[1]++) {
      for (cur[2] = start[2]; cur[2] <= end[2]; cur[2]++) {
        if (!cur[0] && !cur[1] && !cur[2]) {
          /* Skip tile at orgLoc, this was already handled before all others. */
          continue;
        }

        ++cache->tile_pass;

        for (int dim = 0; dim < 3; dim++) {
          cache->location_symm[dim] = cur[dim] * step[dim] + orgLoc[dim];
          cache->plane_offset[dim] = cur[dim] * step[dim];
          cache->initial_location_symm[dim] = cur[dim] * step[dim] +
                                              original_initial_location[dim];
        }
        action_fn(depsgraph, scene, brush, object, paint_mode_data);
      }
    }
  }
}

static void do_radial_symmetry_with_tiling(const Depsgraph &depsgraph,
                                           const Scene &scene,
                                           const Paint &paint,
                                           const Brush &brush,
                                           Object &object,
                                           const BrushActionFn action_fn,
                                           PaintModeData *paint_mode_data,
                                           const ePaintSymmetryFlags symm,
                                           const int axis)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  const Mesh &mesh = *id_cast<Mesh *>(object.data);

  for (int i = 1; i < mesh.radial_symmetry[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / mesh.radial_symmetry[axis - 'X'];
    ss.cache->radial_symmetry_pass = i;
    cache_calc_brushdata_symm(*ss.cache, symm, axis, angle);
    do_tiled(depsgraph, scene, paint, brush, object, action_fn, paint_mode_data);
  }
}

void do_symmetrical_brush_actions_with_tiling_and_feathering(const Depsgraph &depsgraph,
                                                             const Scene &scene,
                                                             const Paint &paint,
                                                             Object &object,
                                                             const BrushActionFn action_fn,
                                                             PaintModeData *paint_mode_data)
{
  const Brush &brush = *BKE_paint_brush_for_read(&paint);
  const Mesh &mesh = *id_cast<Mesh *>(object.data);
  SculptSession &ss = *object.runtime->sculpt_session;
  StrokeCache &cache = *ss.cache;
  const ePaintSymmetryFlags symm = mesh_symmetry_xyz_get(object);

  cache.feather = calc_symmetry_feather(paint, symm, mesh, *ss.cache);
  cache.bstrength = cache.base_brush_strength * cache.feather;

  /* `symm` is a bit combination of XYZ -
   * 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  for (int i = 0; i <= symm; i++) {
    if (!is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    const ePaintSymmetryFlags symm_pass = ePaintSymmetryFlags(i);
    cache.mirror_symmetry_pass = symm_pass;
    cache.radial_symmetry_pass = 0;

    cache_calc_brushdata_symm(cache, symm_pass, 0, 0);
    do_tiled(depsgraph, scene, paint, brush, object, action_fn, paint_mode_data);

    do_radial_symmetry_with_tiling(
        depsgraph, scene, paint, brush, object, action_fn, paint_mode_data, symm_pass, 'X');
    do_radial_symmetry_with_tiling(
        depsgraph, scene, paint, brush, object, action_fn, paint_mode_data, symm_pass, 'Y');
    do_radial_symmetry_with_tiling(
        depsgraph, scene, paint, brush, object, action_fn, paint_mode_data, symm_pass, 'Z');
  }
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name StrokeCache Helpers
 * \{ */

void stroke_cache_common_init(
    ViewContext &vc, const Paint &paint, const Brush &brush, Object &object, const float2 mval)
{
  SculptSession &ss = *object.runtime->sculpt_session;
  StrokeCache *cache = ss.cache;

  cache->initial_mouse = mval;
  cache->mouse = cache->initial_mouse;
  cache->mouse_event = cache->initial_mouse;

  /* Truly temporary data that isn't stored in properties. */
  cache->vc = &vc;
  cache->brush = &brush;
  cache->paint = &paint;
  cache->first_time = true;

  ED_view3d_init_mats_rv3d(&object, cache->vc->rv3d);
  /* Cache projection matrix. */
  cache->projection_mat = ED_view3d_ob_project_mat_get(cache->vc->rv3d, &object);

  const float3 z_axis(0.0f, 0.0f, 1.0f);
  object.runtime->world_to_object = math::invert(object.object_to_world());
  cache->view_normal = math::normalize(math::transform_direction(
      object.world_to_object() * float4x4(cache->vc->rv3d->viewinv), z_axis));

  cache->initial_location_symm = ss.cursor_location;
  cache->initial_location = ss.cursor_location;
  cache->initial_normal_symm = ss.cursor_sampled_normal.value_or(ss.cursor_normal);
  cache->initial_normal = ss.cursor_sampled_normal.value_or(ss.cursor_normal);
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name BVH Query Helper
 * \{ */

IndexMask gather_brush_nodes(const Object &ob,
                             const Brush &brush,
                             IndexMaskMemory &memory,
                             FunctionRef<bool(const bke::pbvh::Node &)> node_ignore_fn)
{
  SculptSession &ss = *ob.runtime->sculpt_session;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  /* Build a list of all nodes that are potentially within the brush's area of influence */
  switch (brush.falloff_shape) {
    case PAINT_FALLOFF_SHAPE_SPHERE:
      return bke::pbvh::search_nodes(pbvh, memory, [&](const bke::pbvh::Node &node) {
        if (node_ignore_fn(node)) {
          return false;
        }
        return node_in_sphere(node, ss.cache->location_symm, ss.cache->radius_squared, true);
      });
    case PAINT_FALLOFF_SHAPE_TUBE:
      const DistRayAABB_Precalc ray_dist_precalc = dist_squared_ray_to_aabb_v3_precalc(
          ss.cache->location_symm, ss.cache->view_normal_symm);
      return bke::pbvh::search_nodes(pbvh, memory, [&](const bke::pbvh::Node &node) {
        if (node_ignore_fn(node)) {
          return false;
        }
        return node_in_cylinder(ray_dist_precalc, node, ss.cache->radius_squared, true);
      });
  }
  BLI_assert_unreachable();
  return {};
}

/** \} */

}  // namespace blender::ed::sculpt_paint
