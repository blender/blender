/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "editors/sculpt_paint/paint_cursor.hh"

#include "DNA_mesh_types.h"

#include "BKE_brush.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"

#include "BLI_math_axis_angle.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"

#include "ED_view3d.hh"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "WM_api.hh"

#include "sculpt_boundary.hh"
#include "sculpt_cloth.hh"
#include "sculpt_expand.hh"
#include "sculpt_intern.hh"
#include "sculpt_pose.hh"

#include "brushes/brushes.hh"

#include "editors/sculpt_paint/paint_intern.hh"

#include "bmesh.hh"

namespace blender::ed::sculpt_paint {
static int brush_radius_project(ViewContext *vc, float radius, const float location[3])
{
  float view[3], nonortho[3], ortho[3], offset[3], p1[2], p2[2];

  ED_view3d_global_to_vector(vc->rv3d, location, view);

  /* Create a vector that is not orthogonal to view. */

  if (fabsf(view[0]) < 0.1f) {
    nonortho[0] = view[0] + 1.0f;
    nonortho[1] = view[1];
    nonortho[2] = view[2];
  }
  else if (fabsf(view[1]) < 0.1f) {
    nonortho[0] = view[0];
    nonortho[1] = view[1] + 1.0f;
    nonortho[2] = view[2];
  }
  else {
    nonortho[0] = view[0];
    nonortho[1] = view[1];
    nonortho[2] = view[2] + 1.0f;
  }

  /* Get a vector in the plane of the view. */
  cross_v3_v3v3(ortho, nonortho, view);
  normalize_v3(ortho);

  /* Make a point on the surface of the brush tangent to the view. */
  mul_v3_fl(ortho, radius);
  add_v3_v3v3(offset, location, ortho);

  /* Project the center of the brush, and the tangent point to the view onto the screen. */
  if ((ED_view3d_project_float_global(vc->region, location, p1, V3D_PROJ_TEST_NOP) ==
       V3D_PROJ_RET_OK) &&
      (ED_view3d_project_float_global(vc->region, offset, p2, V3D_PROJ_TEST_NOP) ==
       V3D_PROJ_RET_OK))
  {
    /* The distance between these points is the size of the projected brush in pixels. */
    return len_v2v2(p1, p2);
  }
  /* Assert because the code that sets up the vectors should disallow this. */
  BLI_assert(0);
  return 0;
}

static void pixel_radius_update(PaintCursorContext &pcontext)
{
  if (pcontext.is_cursor_over_mesh) {
    pcontext.pixel_radius = brush_radius_project(
        &pcontext.vc,
        BKE_brush_unprojected_radius_get(pcontext.paint, pcontext.brush),
        pcontext.location);

    if (pcontext.pixel_radius == 0) {
      pcontext.pixel_radius = BKE_brush_radius_get(pcontext.paint, pcontext.brush);
    }

    pcontext.scene_space_location = math::transform_point(pcontext.vc.obact->object_to_world(),
                                                          pcontext.location);
  }
  else {
    pcontext.pixel_radius = BKE_brush_radius_get(pcontext.paint, pcontext.brush);
  }
}

/* Special actions taken when paint cursor goes over mesh */
/* TODO: sculpt only for now. */
/* TODO: We should not be updating data as part of the drawing callbacks. */
static void brush_unprojected_size_update(Paint &paint,
                                          Brush &brush,
                                          const ViewContext &vc,
                                          const float location[3])
{
  const bke::PaintRuntime &paint_runtime = *paint.runtime;
  /* Update the brush's cached 3D radius. */
  if (!BKE_brush_use_locked_size(&paint, &brush)) {
    float projected_radius;
    /* Get 2D brush radius. */
    if (paint_runtime.draw_anchored) {
      projected_radius = paint_runtime.anchored_size;
    }
    else {
      if (brush.stroke_method == BRUSH_STROKE_ANCHORED) {
        projected_radius = 8;
      }
      else {
        projected_radius = BKE_brush_radius_get(&paint, &brush);
      }
    }

    /* Convert brush radius from 2D to 3D. */
    float unprojected_radius = paint_calc_object_space_radius(vc, location, projected_radius);

    /* Scale 3D brush radius by pressure. */
    if (paint_runtime.stroke_active && BKE_brush_use_size_pressure(&brush)) {
      unprojected_radius *= paint_runtime.size_pressure_value;
    }

    /* Set cached value in either Brush or UnifiedPaintSettings. */
    BKE_brush_unprojected_size_set(&paint, &brush, unprojected_radius * 2.0f);
  }
}

void mesh_cursor_update_and_init(PaintCursorContext &pcontext)
{
  BLI_assert(pcontext.ss != nullptr);
  BLI_assert(pcontext.mode == PaintMode::Sculpt);

  SculptSession &ss = *pcontext.ss;
  Brush &brush = *pcontext.brush;
  bke::PaintRuntime &paint_runtime = *pcontext.paint->runtime;
  ViewContext &vc = pcontext.vc;
  CursorGeometryInfo gi;

  const float2 mval_fl = {
      float(pcontext.mval.x - pcontext.region->winrct.xmin),
      float(pcontext.mval.y - pcontext.region->winrct.ymin),
  };

  /* Ensure that the PBVH is generated before we call #cursor_geometry_info_update because
   * the PBVH is needed to do a ray-cast to find the active vertex. */
  bke::object::pbvh_ensure(*pcontext.depsgraph, *pcontext.vc.obact);

  /* This updates the active vertex, which is needed for most of the Sculpt/Vertex Colors tools to
   * work correctly */
  vert_random_access_ensure(*vc.obact);
  pcontext.prev_active_vert_index = ss.active_vert_index();
  if (!paint_runtime.stroke_active) {
    pcontext.is_cursor_over_mesh = cursor_geometry_info_update(
        *pcontext.depsgraph,
        *pcontext.sd,
        pcontext.vc,
        pcontext.base,
        &gi,
        mval_fl,
        (pcontext.brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE));
    pcontext.location = gi.location;
    pcontext.normal = gi.normal;
  }
  else {
    pcontext.is_cursor_over_mesh = paint_runtime.last_hit;
    pcontext.location = paint_runtime.last_location;
  }

  pixel_radius_update(pcontext);

  if (BKE_brush_use_locked_size(pcontext.paint, &brush)) {
    BKE_brush_size_set(pcontext.paint, &brush, pcontext.pixel_radius * 2.0f);
  }

  if (pcontext.is_cursor_over_mesh) {
    brush_unprojected_size_update(*pcontext.paint, brush, vc, pcontext.scene_space_location);
  }
}

static void geometry_preview_lines_draw(const Depsgraph &depsgraph,
                                        const uint gpuattr,
                                        const Brush &brush,
                                        const Object &object)
{
  if (!(brush.flag & BRUSH_GRAB_ACTIVE_VERTEX)) {
    return;
  }

  const SculptSession &ss = *object.runtime->sculpt_session;
  if (bke::object::pbvh_get(object)->type() != bke::pbvh::Type::Mesh) {
    return;
  }

  if (!ss.deform_modifiers_active) {
    return;
  }

  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.6f);

  /* Cursor normally draws on top, but for this part we need depth tests. */
  const GPUDepthTest depth_test = GPU_depth_test_get();
  if (!depth_test) {
    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  }

  GPU_line_width(1.0f);
  if (!ss.preview_verts.is_empty()) {
    const Span<float3> positions = vert_positions_for_grab_active_get(depsgraph, object);
    immBegin(GPU_PRIM_LINES, ss.preview_verts.size());
    for (const int vert : ss.preview_verts) {
      immVertex3fv(gpuattr, positions[vert]);
    }
    immEnd();
  }

  /* Restore depth test value. */
  if (!depth_test) {
    GPU_depth_test(GPU_DEPTH_NONE);
  }
}

void mesh_cursor_active_draw(PaintCursorContext &pcontext)
{
  BLI_assert(pcontext.ss != nullptr);
  BLI_assert(pcontext.mode == PaintMode::Sculpt);

  SculptSession &ss = *pcontext.ss;
  Brush &brush = *pcontext.brush;

  /* The cursor can be marked as active before creating the StrokeCache. */
  if (!ss.cache) {
    return;
  }

  /* Most of the brushes initialize the necessary data for the custom cursor drawing after the
   * first brush step. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    return;
  }

  /* Setup drawing. */
  wmViewport(&pcontext.region->winrct);
  GPU_matrix_push_projection();
  ED_view3d_draw_setup_view(pcontext.wm,
                            pcontext.win,
                            pcontext.depsgraph,
                            pcontext.scene,
                            pcontext.region,
                            pcontext.vc.v3d,
                            nullptr,
                            nullptr,
                            nullptr);
  GPU_matrix_push();
  GPU_matrix_mul(pcontext.vc.obact->object_to_world().ptr());

  switch (brush.sculpt_brush_type) {
    case SCULPT_BRUSH_TYPE_GRAB:
      geometry_preview_lines_draw(*pcontext.depsgraph, pcontext.pos, brush, *pcontext.vc.obact);
      break;
    case SCULPT_BRUSH_TYPE_MULTIPLANE_SCRAPE:
      brushes::multiplane_scrape_preview_draw(
          pcontext.pos, brush, ss, pcontext.outline_col, pcontext.outline_alpha);
      break;
    case SCULPT_BRUSH_TYPE_CLOTH: {
      if (brush.cloth_force_falloff_type == BRUSH_CLOTH_FORCE_FALLOFF_PLANE) {
        /* By definition, the 'Plane Falloff' mode does not have drawable limits.*/
        cloth::plane_falloff_preview_draw(
            pcontext.pos, ss, pcontext.outline_col, pcontext.outline_alpha);
      }
      else if (brush.cloth_force_falloff_type == BRUSH_CLOTH_FORCE_FALLOFF_RADIAL &&
               brush.cloth_simulation_area_type == BRUSH_CLOTH_SIMULATION_AREA_LOCAL)
      {
        /* Display the simulation limits if sculpting outside them. */
        if (math::distance(ss.cache->location, ss.cache->initial_location) >
            ss.cache->radius * (1.0f + brush.cloth_sim_limit))
        {
          const float3 red = {1.0f, 0.2f, 0.2f};
          cloth::simulation_limits_draw(pcontext.pos,
                                        brush,
                                        ss.cache->initial_location,
                                        ss.cache->initial_normal,
                                        ss.cache->radius,
                                        2.0f,
                                        red,
                                        0.8f);
        }
      }
      break;
    }
    default:
      break;
  }

  GPU_matrix_pop();

  GPU_matrix_pop_projection();
  wmWindowViewport(pcontext.win);
}

static void screen_space_point_draw(const uint gpuattr,
                                    const ARegion *region,
                                    const float true_location[3],
                                    const float obmat[4][4],
                                    const int size)
{
  float translation_vertex_cursor[3], location[3];
  copy_v3_v3(location, true_location);
  mul_m4_v3(obmat, location);
  ED_view3d_project_v3(region, location, translation_vertex_cursor);
  /* Do not draw points behind the view. Z [near, far] is mapped to [-1, 1]. */
  if (translation_vertex_cursor[2] <= 1.0f) {
    imm_draw_circle_fill_3d(
        gpuattr, translation_vertex_cursor[0], translation_vertex_cursor[1], size, 10);
  }
}

static void tiling_preview_draw(const uint gpuattr,
                                const ARegion *region,
                                const float true_location[3],
                                const Sculpt &sd,
                                const Object &ob,
                                const float radius)
{
  BLI_assert(ob.type == OB_MESH);
  const Mesh *mesh = BKE_object_get_evaluated_mesh_no_subsurf(&ob);
  if (!mesh) {
    mesh = id_cast<const Mesh *>(ob.data);
  }
  const Bounds<float3> bounds = *mesh->bounds_min_max();
  float orgLoc[3], location[3];
  int tile_pass = 0;
  int start[3];
  int end[3];
  int cur[3];
  const float *step = sd.paint.tile_offset;

  copy_v3_v3(orgLoc, true_location);
  for (int dim = 0; dim < 3; dim++) {
    if ((sd.paint.symmetry_flags & (PAINT_TILE_X << dim)) && step[dim] > 0) {
      start[dim] = (bounds.min[dim] - orgLoc[dim] - radius) / step[dim];
      end[dim] = (bounds.max[dim] - orgLoc[dim] + radius) / step[dim];
    }
    else {
      start[dim] = end[dim] = 0;
    }
  }
  copy_v3_v3_int(cur, start);
  for (cur[0] = start[0]; cur[0] <= end[0]; cur[0]++) {
    for (cur[1] = start[1]; cur[1] <= end[1]; cur[1]++) {
      for (cur[2] = start[2]; cur[2] <= end[2]; cur[2]++) {
        if (!cur[0] && !cur[1] && !cur[2]) {
          /* Skip tile at orgLoc, this was already handled before all others. */
          continue;
        }
        tile_pass++;
        for (int dim = 0; dim < 3; dim++) {
          location[dim] = cur[dim] * step[dim] + orgLoc[dim];
        }
        screen_space_point_draw(gpuattr, region, location, ob.object_to_world().ptr(), 3);
      }
    }
  }
  (void)tile_pass; /* Quiet set-but-unused warning (may be removed). */
}

static void point_with_symmetry_draw(const uint gpuattr,
                                     const ARegion *region,
                                     const float true_location[3],
                                     const Sculpt &sd,
                                     const Object &ob,
                                     const float radius)
{
  const Mesh *mesh = id_cast<const Mesh *>(ob.data);
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  float3 location;
  float symm_rot_mat[4][4];

  for (int i = 0; i <= symm; i++) {
    if (is_symmetry_iteration_valid(i, symm)) {

      /* Axis Symmetry. */
      location = symmetry_flip(true_location, ePaintSymmetryFlags(i));
      screen_space_point_draw(gpuattr, region, location, ob.object_to_world().ptr(), 3);

      /* Tiling. */
      tiling_preview_draw(gpuattr, region, location, sd, ob, radius);

      /* Radial Symmetry. */
      for (char raxis = 0; raxis < 3; raxis++) {
        for (int r = 1; r < mesh->radial_symmetry[raxis]; r++) {
          float angle = 2 * M_PI * r / mesh->radial_symmetry[int(raxis)];
          location = symmetry_flip(true_location, ePaintSymmetryFlags(i));
          unit_m4(symm_rot_mat);
          rotate_m4(symm_rot_mat, raxis + 'X', angle);
          mul_m4_v3(symm_rot_mat, location);

          tiling_preview_draw(gpuattr, region, location, sd, ob, radius);
          screen_space_point_draw(gpuattr, region, location, ob.object_to_world().ptr(), 3);
        }
      }
    }
  }
}

static void inactive_cursor_draw(PaintCursorContext &pcontext)
{
  GPU_line_width(1.0f);
  /* Reduce alpha to increase the contrast when the cursor is over the mesh. */
  immUniformColor3fvAlpha(pcontext.outline_col, pcontext.outline_alpha * 0.8);
  imm_draw_circle_wire_3d(
      pcontext.pos, pcontext.translation[0], pcontext.translation[1], pcontext.final_radius, 80);
  immUniformColor3fvAlpha(pcontext.outline_col, pcontext.outline_alpha * 0.35f);
  imm_draw_circle_wire_3d(
      pcontext.pos,
      pcontext.translation[0],
      pcontext.translation[1],
      pcontext.final_radius *
          clamp_f(BKE_brush_alpha_get(pcontext.paint, pcontext.brush), 0.0f, 1.0f),
      80);
}

static void object_space_radius_update(PaintCursorContext &pcontext)
{
  pcontext.radius = object_space_radius_get(
      pcontext.vc, *pcontext.paint, *pcontext.brush, pcontext.location);
}

static void pose_brush_segments_draw(const PaintCursorContext &pcontext)
{
  SculptSession &ss = *pcontext.ss;
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.8f);
  GPU_line_width(2.0f);

  BLI_assert(ss.pose_ik_chain_preview->initial_head_coords.size() ==
             ss.pose_ik_chain_preview->initial_orig_coords.size());

  immBegin(GPU_PRIM_LINES, ss.pose_ik_chain_preview->initial_head_coords.size() * 2);
  for (const int i : ss.pose_ik_chain_preview->initial_head_coords.index_range()) {
    immVertex3fv(pcontext.pos, ss.pose_ik_chain_preview->initial_orig_coords[i]);
    immVertex3fv(pcontext.pos, ss.pose_ik_chain_preview->initial_head_coords[i]);
  }

  immEnd();
}

static void pose_brush_origins_draw(const PaintCursorContext &pcontext)
{

  SculptSession &ss = *pcontext.ss;
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.8f);
  for (const int i : ss.pose_ik_chain_preview->initial_orig_coords.index_range()) {
    screen_space_point_draw(pcontext.pos,
                            pcontext.region,
                            ss.pose_ik_chain_preview->initial_orig_coords[i],
                            pcontext.vc.obact->object_to_world().ptr(),
                            3);
  }
}

static void boundary_preview_pivot_draw(const PaintCursorContext &pcontext)
{
  if (!pcontext.ss->boundary_preview) {
    /* There is no guarantee that a boundary preview exists as there may be no boundaries
     * inside the brush radius. */
    return;
  }
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.8f);
  screen_space_point_draw(pcontext.pos,
                          pcontext.region,
                          pcontext.ss->boundary_preview->pivot_position,
                          pcontext.vc.obact->object_to_world().ptr(),
                          3);
}

static void boundary_preview_update(const PaintCursorContext &pcontext)
{
  SculptSession &ss = *pcontext.ss;
  /* Needed for updating the necessary SculptSession data in order to initialize the
   * boundary data for the preview. */
  BKE_sculpt_update_object_for_edit(pcontext.depsgraph, pcontext.vc.obact, false);

  ss.boundary_preview = boundary::preview_data_init(
      *pcontext.depsgraph, *pcontext.vc.obact, pcontext.brush, pcontext.radius);
}

static void screen_space_overlays_draw(const PaintCursorContext &pcontext)
{
  const Brush &brush = *pcontext.brush;
  Object &active_object = *pcontext.vc.obact;

  float3 active_vertex_co;
  if (brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_GRAB && brush.flag & BRUSH_GRAB_ACTIVE_VERTEX) {
    SculptSession &ss = *pcontext.ss;
    if (bke::object::pbvh_get(active_object)->type() == bke::pbvh::Type::Mesh) {
      const Span<float3> positions = vert_positions_for_grab_active_get(*pcontext.depsgraph,
                                                                        active_object);
      active_vertex_co = positions[std::get<int>(ss.active_vert())];
    }
    else {
      active_vertex_co = pcontext.ss->active_vert_position(*pcontext.depsgraph, active_object);
    }
  }
  else {
    active_vertex_co = pcontext.ss->active_vert_position(*pcontext.depsgraph, active_object);
  }

  /* Cursor location symmetry points. */
  if (math::distance(active_vertex_co, pcontext.location) < pcontext.radius) {
    immUniformColor3fvAlpha(pcontext.outline_col, pcontext.outline_alpha);
    point_with_symmetry_draw(pcontext.pos,
                             pcontext.region,
                             active_vertex_co,
                             *pcontext.sd,
                             active_object,
                             pcontext.radius);
  }

  /* Expand operation origin. */
  if (pcontext.ss->expand_cache) {
    const int vert = pcontext.ss->expand_cache->initial_active_vert;

    float3 position;
    switch (bke::object::pbvh_get(active_object)->type()) {
      case bke::pbvh::Type::Mesh: {
        const Span<float3> positions = bke::pbvh::vert_positions_eval(*pcontext.depsgraph,
                                                                      active_object);
        position = positions[vert];
        break;
      }
      case bke::pbvh::Type::Grids: {
        const SubdivCCG &subdiv_ccg = *pcontext.ss->subdiv_ccg;
        position = subdiv_ccg.positions[vert];
        break;
      }
      case bke::pbvh::Type::BMesh: {
        BMesh &bm = *pcontext.ss->bm;
        position = BM_vert_at_index(&bm, vert)->co;
        break;
      }
    }
    screen_space_point_draw(
        pcontext.pos, pcontext.region, position, active_object.object_to_world().ptr(), 2);
  }

  if (!pcontext.is_brush_active) {
    return;
  }

  switch (brush.sculpt_brush_type) {
    case SCULPT_BRUSH_TYPE_POSE: {
      /* Pose brush updates and rotation origins. */
      /* Just after switching to the Pose Brush, the active vertex can be the same and the
       * cursor won't be tagged to update, so always initialize the preview chain if it is
       * nullptr before drawing it. */
      SculptSession &ss = *pcontext.ss;
      const bool update_previews = pcontext.prev_active_vert_index !=
                                   pcontext.ss->active_vert_index();
      if (update_previews || !ss.pose_ik_chain_preview) {
        BKE_sculpt_update_object_for_edit(pcontext.depsgraph, &active_object, false);

        /* Free the previous pose brush preview. */
        if (ss.pose_ik_chain_preview) {
          ss.pose_ik_chain_preview.reset();
        }

        /* Generate a new pose brush preview from the current cursor location. */
        ss.pose_ik_chain_preview = pose::preview_ik_chain_init(
            *pcontext.depsgraph, active_object, ss, brush, pcontext.location, pcontext.radius);
      }

      /* Draw the pose brush rotation origins. */
      pose_brush_origins_draw(pcontext);
      break;
    }
    case SCULPT_BRUSH_TYPE_BOUNDARY:
      boundary_preview_update(pcontext);
      boundary_preview_pivot_draw(pcontext);
      break;
    default:
      break;
  }
}

static void object_space_overlays_draw(const PaintCursorContext &pcontext)
{
  if (!pcontext.is_brush_active) {
    return;
  }

  const Brush &brush = *pcontext.brush;
  const Object &active_object = *pcontext.vc.obact;

  switch (brush.sculpt_brush_type) {
    case SCULPT_BRUSH_TYPE_GRAB:
      if (brush.flag & BRUSH_GRAB_ACTIVE_VERTEX) {
        geometry_preview_lines_update(
            *pcontext.depsgraph, *pcontext.vc.obact, *pcontext.ss, pcontext.radius);
        geometry_preview_lines_draw(
            *pcontext.depsgraph, pcontext.pos, *pcontext.brush, active_object);
      }
      break;
    case SCULPT_BRUSH_TYPE_POSE:
      pose_brush_segments_draw(pcontext);
      break;
    case SCULPT_BRUSH_TYPE_BOUNDARY:
      boundary::edges_preview_draw(
          pcontext.pos, *pcontext.ss, pcontext.outline_col, pcontext.outline_alpha);
      boundary::pivot_line_preview_draw(pcontext.pos, *pcontext.ss);
      break;
    default:
      break;
  }
}

static void cursor_space_drawing_setup(const PaintCursorContext &pcontext)
{
  const float4x4 cursor_trans = math::translate(pcontext.vc.obact->object_to_world(),
                                                pcontext.location);

  const float3 z_axis = {0.0f, 0.0f, 1.0f};

  const float3 normal = bke::brush::supports_tilt(*pcontext.brush) ?
                            tilt_apply_to_normal(*pcontext.vc.obact,
                                                 float4x4(pcontext.vc.rv3d->viewinv),
                                                 pcontext.normal,
                                                 pcontext.tilt,
                                                 pcontext.brush->tilt_strength_factor) :
                            pcontext.normal;

  const math::AxisAngle between_vecs(z_axis, normal);
  const float4x4 cursor_rot = math::from_rotation<float4x4>(between_vecs);

  GPU_matrix_mul(cursor_trans.ptr());
  GPU_matrix_mul(cursor_rot.ptr());
}

static void main_inactive_cursor_draw(const PaintCursorContext &pcontext)
{
  immUniformColor3fvAlpha(pcontext.outline_col, pcontext.outline_alpha);
  GPU_line_width(2.0f);
  imm_draw_circle_wire_3d(pcontext.pos, 0, 0, pcontext.radius, 80);

  GPU_line_width(1.0f);
  immUniformColor3fvAlpha(pcontext.outline_col, pcontext.outline_alpha * 0.5f);
  imm_draw_circle_wire_3d(
      pcontext.pos,
      0,
      0,
      pcontext.radius * clamp_f(BKE_brush_alpha_get(pcontext.paint, pcontext.brush), 0.0f, 1.0f),
      80);
}

static void layer_brush_height_preview_draw(const uint gpuattr,
                                            const Brush &brush,
                                            const float rds,
                                            const float line_width,
                                            const float3 &outline_col,
                                            const float alpha)
{
  const float4x4 cursor_trans = math::translate(float4x4::identity(),
                                                float3(0.0f, 0.0f, brush.height));
  GPU_matrix_push();
  GPU_matrix_mul(cursor_trans.ptr());

  GPU_line_width(line_width);
  immUniformColor3fvAlpha(outline_col, alpha * 0.5f);
  imm_draw_circle_wire_3d(gpuattr, 0, 0, rds, 80);
  GPU_matrix_pop();
}

static void cursor_space_overlays_draw(const PaintCursorContext &pcontext)
{
  const Brush &brush = *pcontext.brush;
  /* Main inactive cursor. */
  main_inactive_cursor_draw(pcontext);

  if (!pcontext.is_brush_active) {
    return;
  }

  switch (brush.sculpt_brush_type) {
    case SCULPT_BRUSH_TYPE_CLOTH:
      /* Cloth brush local simulation areas. */
      if (brush.cloth_simulation_area_type != BRUSH_CLOTH_SIMULATION_AREA_GLOBAL) {
        const float3 white = {1.0f, 1.0f, 1.0f};
        const float3 zero_v = float3(0.0f);
        /* This functions sets its own drawing space in order to draw the simulation limits when
         * the cursor is active. When used here, this cursor overlay is already in cursor space, so
         * its position and normal should be set to 0. */
        cloth::simulation_limits_draw(
            pcontext.pos, brush, zero_v, zero_v, pcontext.radius, 1.0f, white, 0.25f);
      }
      break;
    case SCULPT_BRUSH_TYPE_LAYER:
      layer_brush_height_preview_draw(pcontext.pos,
                                      brush,
                                      pcontext.radius,
                                      1.0f,
                                      pcontext.outline_col,
                                      pcontext.outline_alpha);
      break;
    default:
      break;
  }
}

void mesh_cursor_inactive_draw(PaintCursorContext &pcontext)
{
  if (!pcontext.is_cursor_over_mesh) {
    inactive_cursor_draw(pcontext);
    return;
  }

  BLI_assert(pcontext.vc.obact);
  Object &active_object = *pcontext.vc.obact;
  object_space_radius_update(pcontext);

  vert_random_access_ensure(active_object);

  /* Setup drawing. */
  wmViewport(&pcontext.region->winrct);

  /* Drawing of Cursor overlays in 2D screen space. */
  screen_space_overlays_draw(pcontext);

  /* Setup 3D perspective drawing. */
  GPU_matrix_push_projection();
  ED_view3d_draw_setup_view(pcontext.wm,
                            pcontext.win,
                            pcontext.depsgraph,
                            pcontext.scene,
                            pcontext.region,
                            pcontext.vc.v3d,
                            nullptr,
                            nullptr,
                            nullptr);

  GPU_matrix_push();
  GPU_matrix_mul(active_object.object_to_world().ptr());

  /* Drawing Cursor overlays in 3D object space. */
  object_space_overlays_draw(pcontext);

  GPU_matrix_pop();

  /* Drawing Cursor overlays in Paint Cursor space (as additional info on top of the brush cursor)
   */
  GPU_matrix_push();
  cursor_space_drawing_setup(pcontext);
  cursor_space_overlays_draw(pcontext);

  GPU_matrix_pop();

  /* Reset drawing. */
  GPU_matrix_pop_projection();
  wmWindowViewport(pcontext.win);
}

}  // namespace blender::ed::sculpt_paint
