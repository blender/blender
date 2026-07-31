/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "mesh_stroke_common.hh"

#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BLI_math_rotation_c.hh"

#include "DEG_depsgraph.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "sculpt_intern.hh"

#include "../paint_intern.hh"

namespace blender::ed::sculpt_paint {

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
  action_fn(depsgraph, scene, brush, object, nullptr);

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
}  // namespace blender::ed::sculpt_paint
