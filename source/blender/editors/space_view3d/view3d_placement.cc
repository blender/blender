/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Operator to interactively place data.
 *
 * Currently only adds meshes, but could add other kinds of data
 * including library assets & non-mesh types.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"

#include "ED_gizmo_utils.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "UI_resources.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "view3d_intern.hh"

static const char *view3d_gzgt_placement_id = "VIEW3D_GGT_placement";

/**
 * Dot products below this will be considered view aligned.
 * In this case we can't usefully project the mouse cursor onto the plane,
 * so use a fall-back plane instead.
 */
static const float eps_view_align = 1e-2f;

/* -------------------------------------------------------------------- */
/** \name Local Types
 * \{ */

enum ePlace_PrimType {
  PLACE_PRIMITIVE_TYPE_CUBE = 1,
  PLACE_PRIMITIVE_TYPE_CYLINDER = 2,
  PLACE_PRIMITIVE_TYPE_CONE = 3,
  PLACE_PRIMITIVE_TYPE_SPHERE_UV = 4,
  PLACE_PRIMITIVE_TYPE_SPHERE_ICO = 5,
};

enum ePlace_Origin {
  PLACE_ORIGIN_BASE = 1,
  PLACE_ORIGIN_CENTER = 2,
};

enum ePlace_Aspect {
  PLACE_ASPECT_FREE = 1,
  PLACE_ASPECT_FIXED = 2,
};

enum StepIndex {
  STEP_BASE = 0,
  STEP_DEPTH = 1,
};

struct InteractivePlaceData {
  /* Window manager variables (set these even when waiting for input). */
  Scene *scene;
  ScrArea *area;
  View3D *v3d;
  ARegion *region;

  /** Draw object preview region draw callback. */
  void *draw_handle_view;

  float co_src[3];

  /** Primary & secondary steps. */
  struct {
    /**
     * When centered, drag out the shape from the center.
     * Toggling the setting flips the value from its initial state.
     */
    bool is_centered, is_centered_init;
    /**
     * When fixed, constrain the X/Y aspect for the initial #STEP_BASE drag.
     * For #STEP_DEPTH match the maximum X/Y dimension.
     * Toggling the setting flips the value from its initial state.
     */
    bool is_fixed_aspect, is_fixed_aspect_init;
    float plane[4];
    float co_dst[3];

    /**
     * We can't project the mouse cursor onto `plane`,
     * in this case #view3d_win_to_3d_on_plane_maybe_fallback is used.
     *
     * - For #STEP_BASE we're drawing from the side, where the X/Y axis can't be projected.
     * - For #STEP_DEPTH we're drawing from the top (2D), where the depth can't be projected.
     */
    bool is_degenerate_view_align;
    /**
     * When view aligned, use a diagonal offset (cavalier projection)
     * to give user feedback about the depth being set.
     *
     * Currently this is only used for orthogonal views since perspective views
     * nearly always show some depth, even when view aligned.
     *
     * - Drag to the bottom-left to move away from the view.
     * - Drag to the top-right to move towards the view.
     */
    float degenerate_diagonal[3];
    /**
     * Corrected for display, so what's shown on-screen doesn't loop to be reversed
     * in relation to cursor-motion.
     */
    float degenerate_diagonal_display[3];

    /**
     * Index into `matrix_orient` which is degenerate.
     */
    int degenerate_axis;

  } step[2];

  /** When we can't project onto the real plane, use this in its place. */
  float view_plane[4];

  float matrix_orient[3][3];
  int orient_axis;

  V3DSnapCursorState *snap_state;
  bool use_snap, is_snap_found, is_snap_invert;
  float snap_co[3];

  /** Can index into #InteractivePlaceData.step. */
  StepIndex step_index;

  enum ePlace_PrimType primitive_type;

  /** Activated from the tool-system. */
  bool use_tool;

  /** Event used to start the operator. */
  short launch_event;

  /** When activated without a tool. */
  bool wait_for_input;

  /* WORKAROUND: We need to remove #SCE_SNAP_TO_GRID temporarily. */
  short *snap_to_ptr;
  eSnapMode snap_to_restore;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/**
 * Convenience wrapper to avoid duplicating arguments.
 */
static bool view3d_win_to_3d_on_plane_maybe_fallback(const ARegion *region,
                                                     const float plane[4],
                                                     const float mval[2],
                                                     const float *plane_fallback,
                                                     float r_out[3])
{
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  bool do_clip = rv3d->is_persp;
  if (plane_fallback != nullptr) {
    return ED_view3d_win_to_3d_on_plane_with_fallback(
        region, plane, mval, do_clip, plane_fallback, r_out);
  }
  return ED_view3d_win_to_3d_on_plane(region, plane, mval, do_clip, r_out);
}

/**
 * Return the index of \a dirs with the largest dot product compared to \a dir_test.
 */
static int dot_v3_array_find_max_index(const float dirs[][3],
                                       const int dirs_len,
                                       const float dir_test[3],
                                       bool is_signed)
{
  int index_found = -1;
  float dot_best = -1.0f;
  for (int i = 0; i < dirs_len; i++) {
    float dot_test = dot_v3v3(dirs[i], dir_test);
    if (is_signed == false) {
      dot_test = fabsf(dot_test);
    }
    if ((index_found == -1) || (dot_test > dot_best)) {
      dot_best = dot_test;
      index_found = i;
    }
  }
  return index_found;
}

static UNUSED_FUNCTION_WITH_RETURN_TYPE(wmGizmoGroup *,
                                        idp_gizmogroup_from_region)(ARegion *region)
{
  wmGizmoMap *gzmap = region->runtime->gizmo_map;
  return gzmap ? WM_gizmomap_group_find(gzmap, view3d_gzgt_placement_id) : nullptr;
}

/**
 * Calculate 3D view incremental (grid) snapping.
 *
 * \note This could be moved to a public function.
 */
static bool idp_snap_calc_incremental(
    Scene *scene, View3D *v3d, ARegion *region, const float co_relative[3], float co[3])
{
  const float grid_size = ED_view3d_grid_view_scale(scene, v3d, region, nullptr);
  if (UNLIKELY(grid_size == 0.0f)) {
    return false;
  }

  if (scene->toolsettings->snap_mode & SCE_SNAP_TO_GRID) {
    co_relative = nullptr;
  }

  if (co_relative != nullptr) {
    sub_v3_v3(co, co_relative);
  }
  mul_v3_fl(co, 1.0f / grid_size);
  co[0] = roundf(co[0]);
  co[1] = roundf(co[1]);
  co[2] = roundf(co[2]);
  mul_v3_fl(co, grid_size);
  if (co_relative != nullptr) {
    add_v3_v3(co, co_relative);
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Primitive Drawing (Cube, Cone, Cylinder...)
 * \{ */

static void draw_line_loop(const float coords[][3], int coords_len, const float color[4])
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

  blender::gpu::VertBuf *vert = GPU_vertbuf_create_with_format(*format);
  GPU_vertbuf_data_alloc(*vert, coords_len);

  for (int i = 0; i < coords_len; i++) {
    GPU_vertbuf_attr_set(vert, pos, i, coords[i]);
  }

  GPU_blend(GPU_BLEND_ALPHA);
  blender::gpu::Batch *batch = GPU_batch_create_ex(
      GPU_PRIM_LINE_LOOP, vert, nullptr, GPU_BATCH_OWNS_VBO);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);

  GPU_batch_uniform_4fv(batch, "color", color);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);
  GPU_batch_uniform_2fv(batch, "viewportSize", &viewport[2]);
  GPU_batch_uniform_1f(batch, "lineWidth", U.pixelsize);

  GPU_batch_draw(batch);

  GPU_batch_discard(batch);
  GPU_blend(GPU_BLEND_NONE);
}

static void draw_line_pairs(const float coords_a[][3],
                            float coords_b[][3],
                            int coords_len,
                            const float color[4])
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

  blender::gpu::VertBuf *vert = GPU_vertbuf_create_with_format(*format);
  GPU_vertbuf_data_alloc(*vert, coords_len * 2);

  for (int i = 0; i < coords_len; i++) {
    GPU_vertbuf_attr_set(vert, pos, i * 2, coords_a[i]);
    GPU_vertbuf_attr_set(vert, pos, (i * 2) + 1, coords_b[i]);
  }

  GPU_blend(GPU_BLEND_ALPHA);
  blender::gpu::Batch *batch = GPU_batch_create_ex(
      GPU_PRIM_LINES, vert, nullptr, GPU_BATCH_OWNS_VBO);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);

  GPU_batch_uniform_4fv(batch, "color", color);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);
  GPU_batch_uniform_2fv(batch, "viewportSize", &viewport[2]);
  GPU_batch_uniform_1f(batch, "lineWidth", U.pixelsize);

  GPU_batch_draw(batch);

  GPU_batch_discard(batch);
  GPU_blend(GPU_BLEND_NONE);
}

static void draw_line_bounds(const BoundBox *bounds, const float color[4])
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

  const int edges[12][2] = {
      /* First side. */
      {0, 1},
      {1, 2},
      {2, 3},
      {3, 0}, /* Second side. */
      {4, 5},
      {5, 6},
      {6, 7},
      {7, 4}, /* Edges between. */
      {0, 4},
      {1, 5},
      {2, 6},
      {3, 7},
  };

  blender::gpu::VertBuf *vert = GPU_vertbuf_create_with_format(*format);
  GPU_vertbuf_data_alloc(*vert, ARRAY_SIZE(edges) * 2);

  for (int i = 0, j = 0; i < ARRAY_SIZE(edges); i++) {
    GPU_vertbuf_attr_set(vert, pos, j++, bounds->vec[edges[i][0]]);
    GPU_vertbuf_attr_set(vert, pos, j++, bounds->vec[edges[i][1]]);
  }

  GPU_blend(GPU_BLEND_ALPHA);
  blender::gpu::Batch *batch = GPU_batch_create_ex(
      GPU_PRIM_LINES, vert, nullptr, GPU_BATCH_OWNS_VBO);
  GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);

  GPU_batch_uniform_4fv(batch, "color", color);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);
  GPU_batch_uniform_2fv(batch, "viewportSize", &viewport[2]);
  GPU_batch_uniform_1f(batch, "lineWidth", U.pixelsize);

  GPU_batch_draw(batch);

  GPU_batch_discard(batch);
  GPU_blend(GPU_BLEND_NONE);
}

static bool calc_bbox(InteractivePlaceData *ipd, BoundBox *bounds)
{
  *bounds = BoundBox{};

  if (compare_v3v3(ipd->co_src, ipd->step[0].co_dst, FLT_EPSILON)) {
    return false;
  }

  float matrix_orient_inv[3][3];
  invert_m3_m3(matrix_orient_inv, ipd->matrix_orient);

  const int x_axis = (ipd->orient_axis + 1) % 3;
  const int y_axis = (ipd->orient_axis + 2) % 3;

  float quad_base[4][3];
  float quad_secondary[4][3];

  copy_v3_v3(quad_base[0], ipd->co_src);
  copy_v3_v3(quad_base[2], ipd->step[0].co_dst);

  /* Only set when we have a fixed aspect. */
  float fixed_aspect_dimension;

  /* *** Primary *** */

  {
    float delta_local[3];
    float delta_a[3];
    float delta_b[3];

    sub_v3_v3v3(delta_local, ipd->step[0].co_dst, ipd->co_src);
    mul_m3_v3(matrix_orient_inv, delta_local);

    copy_v3_v3(delta_a, delta_local);
    copy_v3_v3(delta_b, delta_local);
    delta_a[ipd->orient_axis] = 0.0f;
    delta_b[ipd->orient_axis] = 0.0f;

    delta_a[x_axis] = 0.0f;
    delta_b[y_axis] = 0.0f;

    /* Assign here in case secondary. */
    fixed_aspect_dimension = max_ff(fabsf(delta_a[y_axis]), fabsf(delta_b[x_axis]));

    if (ipd->step[0].is_fixed_aspect) {
      delta_a[y_axis] = copysignf(fixed_aspect_dimension, delta_a[y_axis]);
      delta_b[x_axis] = copysignf(fixed_aspect_dimension, delta_b[x_axis]);
    }

    mul_m3_v3(ipd->matrix_orient, delta_a);
    mul_m3_v3(ipd->matrix_orient, delta_b);

    if (ipd->step[0].is_fixed_aspect) {
      /* Recalculate the destination point. */
      copy_v3_v3(quad_base[2], ipd->co_src);
      add_v3_v3(quad_base[2], delta_a);
      add_v3_v3(quad_base[2], delta_b);
    }

    add_v3_v3v3(quad_base[1], ipd->co_src, delta_a);
    add_v3_v3v3(quad_base[3], ipd->co_src, delta_b);
  }

  if (ipd->step[0].is_centered) {
    /* Use a copy in case aspect was applied to the quad. */
    float base_co_dst[3];
    copy_v3_v3(base_co_dst, quad_base[2]);
    for (int i = 0; i < ARRAY_SIZE(quad_base); i++) {
      sub_v3_v3(quad_base[i], base_co_dst);
      mul_v3_fl(quad_base[i], 2.0f);
      add_v3_v3(quad_base[i], base_co_dst);
    }
    fixed_aspect_dimension *= 2.0f;
  }

  /* *** Secondary *** */

  float delta_local[3];
  if (ipd->step_index == STEP_DEPTH) {
    sub_v3_v3v3(delta_local, ipd->step[1].co_dst, ipd->step[0].co_dst);
  }
  else {
    zero_v3(delta_local);
  }

  if (ipd->step[1].is_fixed_aspect) {
    if (!is_zero_v3(delta_local)) {
      normalize_v3_length(delta_local, fixed_aspect_dimension);
    }
  }

  if (ipd->step[1].is_centered) {
    float temp_delta[3];
    if (ipd->step[1].is_fixed_aspect) {
      mul_v3_v3fl(temp_delta, delta_local, 0.5f);
    }
    else {
      copy_v3_v3(temp_delta, delta_local);
      mul_v3_fl(delta_local, 2.0f);
    }

    for (int i = 0; i < ARRAY_SIZE(quad_base); i++) {
      sub_v3_v3(quad_base[i], temp_delta);
    }
  }

  if ((ipd->step_index == STEP_DEPTH) &&
      (compare_v3v3(ipd->step[0].co_dst, ipd->step[1].co_dst, FLT_EPSILON) == false))
  {

    for (int i = 0; i < ARRAY_SIZE(quad_base); i++) {
      add_v3_v3v3(quad_secondary[i], quad_base[i], delta_local);
    }
  }
  else {
    copy_v3_v3(quad_secondary[0], quad_base[0]);
    copy_v3_v3(quad_secondary[1], quad_base[1]);
    copy_v3_v3(quad_secondary[2], quad_base[2]);
    copy_v3_v3(quad_secondary[3], quad_base[3]);
  }

  for (int i = 0; i < 4; i++) {
    copy_v3_v3(bounds->vec[i], quad_base[i]);
    copy_v3_v3(bounds->vec[i + 4], quad_secondary[i]);
  }

  return true;
}

static void draw_circle_in_quad(const float v1[3],
                                const float v2[3],
                                const float v3[3],
                                const float v4[3],
                                const int resolution,
                                const float color[4])
{
  /* This isn't so efficient. */
  const float quad[4][2] = {
      {-1, -1},
      {+1, -1},
      {+1, +1},
      {-1, +1},
  };

  float (*coords)[3] = static_cast<float (*)[3]>(
      MEM_mallocN(sizeof(float[3]) * (resolution + 1), __func__));
  for (int i = 0; i <= resolution; i++) {
    float theta = ((2.0f * M_PI) * (float(i) / float(resolution))) + 0.01f;
    float x = cosf(theta);
    float y = sinf(theta);
    const float pt[2] = {x, y};
    float w[4];
    barycentric_weights_v2_quad(UNPACK4(quad), pt, w);

    float *co = coords[i];
    zero_v3(co);
    madd_v3_v3fl(co, v1, w[0]);
    madd_v3_v3fl(co, v2, w[1]);
    madd_v3_v3fl(co, v3, w[2]);
    madd_v3_v3fl(co, v4, w[3]);
  }
  draw_line_loop(coords, resolution + 1, color);
  MEM_freeN(coords);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing Callbacks
 * \{ */

static void draw_primitive_view_impl(const bContext *C,
                                     InteractivePlaceData *ipd,
                                     const float color[4],
                                     int flatten_axis)
{
  UNUSED_VARS(C);

  BoundBox bounds;
  calc_bbox(ipd, &bounds);

  /* Use cavalier projection, since it maps the scale usefully to the cursor. */
  if (flatten_axis == STEP_BASE) {
    /* Calculate the plane that would be defined by the side of the cube vertices
     * if the plane had any volume. */

    float no[3];

    cross_v3_v3v3(
        no, ipd->matrix_orient[ipd->orient_axis], ipd->matrix_orient[(ipd->orient_axis + 1) % 3]);

    RegionView3D *rv3d = static_cast<RegionView3D *>(ipd->region->regiondata);
    copy_v3_v3(no, rv3d->viewinv[2]);
    normalize_v3(no);

    float base_plane[4];

    plane_from_point_normal_v3(base_plane, bounds.vec[0], no);

    /* Offset all vertices even though we only need to offset the half of them.
     * This is harmless as `dist` will be zero for the `base_plane` aligned side of the cube. */
    for (int i = 0; i < ARRAY_SIZE(bounds.vec); i++) {
      const float dist = dist_signed_to_plane_v3(bounds.vec[i], base_plane);
      madd_v3_v3fl(bounds.vec[i], base_plane, -dist);
      madd_v3_v3fl(bounds.vec[i], ipd->step[STEP_BASE].degenerate_diagonal_display, dist);
    }
  }

  if (flatten_axis == STEP_DEPTH) {
    const float *base_plane = ipd->step[0].plane;
    for (int i = 0; i < 4; i++) {
      const float dist = dist_signed_to_plane_v3(bounds.vec[i + 4], base_plane);
      madd_v3_v3fl(bounds.vec[i + 4], base_plane, -dist);
      madd_v3_v3fl(bounds.vec[i + 4], ipd->step[STEP_DEPTH].degenerate_diagonal_display, dist);
    }
  }

  draw_line_bounds(&bounds, color);

  if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CUBE) {
    /* pass */
  }
  else if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CYLINDER) {
    draw_circle_in_quad(UNPACK4(bounds.vec), 32, color);
    draw_circle_in_quad(UNPACK4(&bounds.vec[4]), 32, color);
  }
  else if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CONE) {
    draw_circle_in_quad(UNPACK4(bounds.vec), 32, color);

    float center[3];
    mid_v3_v3v3v3v3(center, UNPACK4(&bounds.vec[4]));

    float coords_a[4][3];
    float coords_b[4][3];

    for (int i = 0; i < 4; i++) {
      copy_v3_v3(coords_a[i], center);
      mid_v3_v3v3(coords_b[i], bounds.vec[i], bounds.vec[(i + 1) % 4]);
    }

    draw_line_pairs(coords_a, coords_b, 4, color);
  }
  else if (ELEM(ipd->primitive_type,
                PLACE_PRIMITIVE_TYPE_SPHERE_UV,
                PLACE_PRIMITIVE_TYPE_SPHERE_ICO))
  {
    /* See bound-box diagram for reference. */

    /* Primary Side. */
    float v01[3], v12[3], v23[3], v30[3];
    mid_v3_v3v3(v01, bounds.vec[0], bounds.vec[1]);
    mid_v3_v3v3(v12, bounds.vec[1], bounds.vec[2]);
    mid_v3_v3v3(v23, bounds.vec[2], bounds.vec[3]);
    mid_v3_v3v3(v30, bounds.vec[3], bounds.vec[0]);
    /* Secondary Side. */
    float v45[3], v56[3], v67[3], v74[3];
    mid_v3_v3v3(v45, bounds.vec[4], bounds.vec[5]);
    mid_v3_v3v3(v56, bounds.vec[5], bounds.vec[6]);
    mid_v3_v3v3(v67, bounds.vec[6], bounds.vec[7]);
    mid_v3_v3v3(v74, bounds.vec[7], bounds.vec[4]);
    /* Edges between. */
    float v04[3], v15[3], v26[3], v37[3];
    mid_v3_v3v3(v04, bounds.vec[0], bounds.vec[4]);
    mid_v3_v3v3(v15, bounds.vec[1], bounds.vec[5]);
    mid_v3_v3v3(v26, bounds.vec[2], bounds.vec[6]);
    mid_v3_v3v3(v37, bounds.vec[3], bounds.vec[7]);

    draw_circle_in_quad(v01, v45, v67, v23, 32, color);
    draw_circle_in_quad(v30, v12, v56, v74, 32, color);
    draw_circle_in_quad(v04, v15, v26, v37, 32, color);
  }
}

static void draw_primitive_view(const bContext *C, ARegion * /*region*/, void *arg)
{
  InteractivePlaceData *ipd = static_cast<InteractivePlaceData *>(arg);
  float color[4];
  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, color);

  const bool use_depth = !XRAY_ENABLED(ipd->v3d);
  const GPUDepthTest depth_test_enabled = GPU_depth_test_get();

  if (use_depth) {
    GPU_depth_test(GPU_DEPTH_NONE);
    color[3] = 0.15f;
    draw_primitive_view_impl(C, ipd, color, -1);
  }

  /* Show a flattened projection if the current step is aligned to the view. */
  if (ipd->step[ipd->step_index].is_degenerate_view_align) {
    const RegionView3D *rv3d = static_cast<const RegionView3D *>(ipd->region->regiondata);
    if (!rv3d->is_persp) {
      draw_primitive_view_impl(C, ipd, color, ipd->step_index);
    }
  }

  if (use_depth) {
    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  }
  color[3] = 1.0f;
  draw_primitive_view_impl(C, ipd, color, -1);

  if (use_depth) {
    if (depth_test_enabled == false) {
      GPU_depth_test(GPU_DEPTH_NONE);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculate The Initial Placement Plane
 *
 * Use by both the operator and placement cursor.
 * \{ */

static bool view3d_interactive_add_calc_snap(bContext * /*C*/,
                                             const wmEvent * /*event*/,
                                             float r_co_src[3],
                                             float r_matrix_orient[3][3],
                                             bool *r_is_enabled,
                                             bool *r_is_snap_invert)
{
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get();
  copy_v3_v3(r_co_src, snap_data->loc);
  if (r_matrix_orient) {
    copy_m3_m3(r_matrix_orient, snap_data->plane_omat);
  }
  if (r_is_enabled) {
    *r_is_enabled = snap_data->is_enabled;
  }
  if (r_is_snap_invert) {
    *r_is_snap_invert = snap_data->is_snap_invert;
  }
  return snap_data->type_target != SCE_SNAP_TO_NONE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Object Modal Operator
 * \{ */

static void view3d_interactive_add_begin(bContext *C, wmOperator *op, const wmEvent *event)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_active_get();
  ToolSettings *tool_settings = CTX_data_tool_settings(C);

  const int plane_axis = tool_settings->plane_axis;

  const enum ePlace_Origin plane_origin[2] = {
      ePlace_Origin(RNA_enum_get(op->ptr, "plane_origin_base")),
      ePlace_Origin(RNA_enum_get(op->ptr, "plane_origin_depth")),
  };
  const enum ePlace_Aspect plane_aspect[2] = {
      ePlace_Aspect(RNA_enum_get(op->ptr, "plane_aspect_base")),
      ePlace_Aspect(RNA_enum_get(op->ptr, "plane_aspect_depth")),
  };

  InteractivePlaceData *ipd = static_cast<InteractivePlaceData *>(op->customdata);

  ipd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

  V3DSnapCursorState *snap_state_new = ED_view3d_cursor_snap_state_create();
  if (snap_state_new) {
    ipd->snap_state = snap_state = snap_state_new;

    /* For drag events, update the location since it will be set from the drag-start.
     * This is needed as cursor-drawing doesn't deal with drag events and will use
     * the current cursor location instead of the drag-start. */
    if (event->val == KM_PRESS_DRAG) {
      /* Set this flag so snapping always updated. */
      int mval[2];
      WM_event_drag_start_mval(event, ipd->region, mval);

      /* Be sure to also compute the #V3DSnapCursorData.plane_omat. */
      snap_state->draw_plane = true;

      ED_view3d_cursor_snap_data_update(snap_state_new, C, ipd->region, mval);
    }
  }

  snap_state->draw_point = true;
  snap_state->draw_plane = true;
  ipd->is_snap_found =
      view3d_interactive_add_calc_snap(
          C, event, ipd->co_src, ipd->matrix_orient, &ipd->use_snap, &ipd->is_snap_invert) != 0;

  snap_state->draw_plane = false;
  ED_view3d_cursor_snap_state_prevpoint_set(snap_state, ipd->co_src);

  ipd->orient_axis = plane_axis;
  for (int i = 0; i < 2; i++) {
    ipd->step[i].is_centered_init = (plane_origin[i] == PLACE_ORIGIN_CENTER);
    ipd->step[i].is_centered = ipd->step[i].is_centered_init;

    ipd->step[i].is_fixed_aspect_init = (plane_aspect[i] == PLACE_ASPECT_FIXED);
    ipd->step[i].is_fixed_aspect = ipd->step[i].is_fixed_aspect_init;
  }

  ipd->step_index = STEP_BASE;

  ipd->snap_to_ptr = &tool_settings->snap_mode_tools;
  if (eSnapMode(*ipd->snap_to_ptr) == SCE_SNAP_TO_NONE) {
    ipd->snap_to_ptr = &tool_settings->snap_mode;
  }
  ipd->snap_to_restore = eSnapMode(*ipd->snap_to_ptr);

  plane_from_point_normal_v3(ipd->step[0].plane, ipd->co_src, ipd->matrix_orient[plane_axis]);

  copy_v3_v3(ipd->step[0].co_dst, ipd->co_src);

  {
    RegionView3D *rv3d = static_cast<RegionView3D *>(ipd->region->regiondata);
    const float view_axis_dot = fabsf(dot_v3v3(rv3d->viewinv[2], ipd->matrix_orient[plane_axis]));
    ipd->step[STEP_BASE].is_degenerate_view_align = view_axis_dot < eps_view_align;
    ipd->step[STEP_DEPTH].is_degenerate_view_align = fabsf(view_axis_dot - 1.0f) < eps_view_align;

    float view_axis[3];
    normalize_v3_v3(view_axis, rv3d->viewinv[2]);
    plane_from_point_normal_v3(ipd->view_plane, ipd->co_src, view_axis);
  }

  if (ipd->step[STEP_BASE].is_degenerate_view_align ||
      ipd->step[STEP_DEPTH].is_degenerate_view_align)
  {
    RegionView3D *rv3d = static_cast<RegionView3D *>(ipd->region->regiondata);
    float axis_view[3];
    add_v3_v3v3(axis_view, rv3d->viewinv[0], rv3d->viewinv[1]);
    normalize_v3(axis_view);

    /* Setup fallback axes. */
    for (int i = 0; i < 2; i++) {
      if (ipd->step[i].is_degenerate_view_align) {
        const int degenerate_axis =
            (i == STEP_BASE) ?
                /* For #STEP_BASE find the orient axis that align to the view. */
                dot_v3_array_find_max_index(ipd->matrix_orient, 3, rv3d->viewinv[2], false) :
                /* For #STEP_DEPTH the orient axis is always view aligned when degenerate. */
                ipd->orient_axis;

        float axis_fallback[4][3];
        const int x_axis = (degenerate_axis + 1) % 3;
        const int y_axis = (degenerate_axis + 2) % 3;

        /* Assign 4x diagonal axes, find which one is closest to the viewport diagonal
         * bottom left to top right, for a predictable direction from a user perspective. */
        add_v3_v3v3(axis_fallback[0], ipd->matrix_orient[x_axis], ipd->matrix_orient[y_axis]);
        sub_v3_v3v3(axis_fallback[1], ipd->matrix_orient[x_axis], ipd->matrix_orient[y_axis]);
        negate_v3_v3(axis_fallback[2], axis_fallback[0]);
        negate_v3_v3(axis_fallback[3], axis_fallback[1]);

        const int axis_best = dot_v3_array_find_max_index(axis_fallback, 4, axis_view, true);
        normalize_v3_v3(ipd->step[i].degenerate_diagonal, axis_fallback[axis_best]);
        ipd->step[i].degenerate_axis = degenerate_axis;

        /* `degenerate_view_plane_fallback` is used to map cursor motion from a view aligned
         * plane back onto the view aligned plane.
         *
         * The dot product check below ensures cursor motion
         * isn't inverted from a user perspective. */
        const bool degenerate_axis_is_flip = dot_v3v3(ipd->matrix_orient[degenerate_axis],
                                                      ((i == STEP_BASE) ?
                                                           ipd->step[i].degenerate_diagonal :
                                                           rv3d->viewinv[2])) < 0.0f;

        copy_v3_v3(ipd->step[i].degenerate_diagonal_display, ipd->step[i].degenerate_diagonal);
        if (degenerate_axis_is_flip) {
          negate_v3(ipd->step[i].degenerate_diagonal_display);
        }
      }
    }
  }

  ipd->draw_handle_view = ED_region_draw_cb_activate(
      ipd->region->runtime->type, draw_primitive_view, ipd, REGION_DRAW_POST_VIEW);

  ED_region_tag_redraw(ipd->region);

  /* Setup the primitive type. */
  {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "primitive_type");
    if (RNA_property_is_set(op->ptr, prop)) {
      ipd->primitive_type = ePlace_PrimType(RNA_property_enum_get(op->ptr, prop));
      ipd->use_tool = false;
    }
    else {
      ipd->use_tool = true;

      /* Get from the tool, a bit of a non-standard way of operating. */
      const bToolRef *tref = ipd->area->runtime.tool;
      if (tref && STREQ(tref->idname, "builtin.primitive_cube_add")) {
        ipd->primitive_type = PLACE_PRIMITIVE_TYPE_CUBE;
      }
      else if (tref && STREQ(tref->idname, "builtin.primitive_cylinder_add")) {
        ipd->primitive_type = PLACE_PRIMITIVE_TYPE_CYLINDER;
      }
      else if (tref && STREQ(tref->idname, "builtin.primitive_cone_add")) {
        ipd->primitive_type = PLACE_PRIMITIVE_TYPE_CONE;
      }
      else if (tref && STREQ(tref->idname, "builtin.primitive_uv_sphere_add")) {
        ipd->primitive_type = PLACE_PRIMITIVE_TYPE_SPHERE_UV;
      }
      else if (tref && STREQ(tref->idname, "builtin.primitive_ico_sphere_add")) {
        ipd->primitive_type = PLACE_PRIMITIVE_TYPE_SPHERE_ICO;
      }
      else {
        /* If the user runs this as an operator they should set the 'primitive_type',
         * however running from operator search will end up at this point. */
        ipd->primitive_type = PLACE_PRIMITIVE_TYPE_CUBE;
        ipd->use_tool = false;
      }
    }
  }
}

static wmOperatorStatus view3d_interactive_add_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent *event)
{
  const bool wait_for_input = RNA_boolean_get(op->ptr, "wait_for_input");

  InteractivePlaceData *ipd = static_cast<InteractivePlaceData *>(
      MEM_callocN(sizeof(*ipd), __func__));
  op->customdata = ipd;

  ipd->scene = CTX_data_scene(C);
  ipd->area = CTX_wm_area(C);
  ipd->region = CTX_wm_region(C);
  ipd->v3d = CTX_wm_view3d(C);

  if (wait_for_input) {
    ipd->wait_for_input = true;
/* TODO: support snapping when not using with tool. */
#if 0
    WM_gizmo_group_type_ensure(view3d_gzgt_placement_id);
#endif
  }
  else {
    view3d_interactive_add_begin(C, op, event);
  }

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void view3d_interactive_add_exit(bContext *C, wmOperator *op)
{
  UNUSED_VARS(C);

  InteractivePlaceData *ipd = static_cast<InteractivePlaceData *>(op->customdata);
  ED_view3d_cursor_snap_state_free(ipd->snap_state);

  if (ipd->region != nullptr) {
    if (ipd->draw_handle_view != nullptr) {
      ED_region_draw_cb_exit(ipd->region->runtime->type, ipd->draw_handle_view);
    }
    ED_region_tag_redraw(ipd->region);
  }

  MEM_freeN(ipd);
}

static void view3d_interactive_add_cancel(bContext *C, wmOperator *op)
{
  view3d_interactive_add_exit(C, op);
}

enum {
  PLACE_MODAL_SNAP_ON,
  PLACE_MODAL_SNAP_OFF,
  PLACE_MODAL_FIXED_ASPECT_ON,
  PLACE_MODAL_FIXED_ASPECT_OFF,
  PLACE_MODAL_PIVOT_CENTER_ON,
  PLACE_MODAL_PIVOT_CENTER_OFF,
};

void viewplace_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {PLACE_MODAL_SNAP_ON, "SNAP_ON", 0, "Snap On", ""},
      {PLACE_MODAL_SNAP_OFF, "SNAP_OFF", 0, "Snap Off", ""},
      {PLACE_MODAL_FIXED_ASPECT_ON, "FIXED_ASPECT_ON", 0, "Fixed Aspect On", ""},
      {PLACE_MODAL_FIXED_ASPECT_OFF, "FIXED_ASPECT_OFF", 0, "Fixed Aspect Off", ""},
      {PLACE_MODAL_PIVOT_CENTER_ON, "PIVOT_CENTER_ON", 0, "Center Pivot On", ""},
      {PLACE_MODAL_PIVOT_CENTER_OFF, "PIVOT_CENTER_OFF", 0, "Center Pivot Off", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  const char *keymap_name = "View3D Placement Modal";
  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, keymap_name);

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, keymap_name, modal_items);

  WM_modalkeymap_assign(keymap, "VIEW3D_OT_interactive_add");
}

static wmOperatorStatus view3d_interactive_add_modal(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  UNUSED_VARS(C, op);

  InteractivePlaceData *ipd = static_cast<InteractivePlaceData *>(op->customdata);

  ARegion *region = ipd->region;
  bool do_redraw = false;
  bool do_cursor_update = false;

  /* Handle modal key-map. */
  if (event->type == EVT_MODAL_MAP) {
    bool is_fallthrough = false;
    switch (event->val) {
      case PLACE_MODAL_FIXED_ASPECT_ON: {
        is_fallthrough = true;
        ATTR_FALLTHROUGH;
      }
      case PLACE_MODAL_FIXED_ASPECT_OFF: {
        ipd->step[ipd->step_index].is_fixed_aspect =
            is_fallthrough ^ ipd->step[ipd->step_index].is_fixed_aspect_init;
        do_redraw = true;
        break;
      }
      case PLACE_MODAL_PIVOT_CENTER_ON: {
        is_fallthrough = true;
        ATTR_FALLTHROUGH;
      }
      case PLACE_MODAL_PIVOT_CENTER_OFF: {
        ipd->step[ipd->step_index].is_centered = is_fallthrough ^
                                                 ipd->step[ipd->step_index].is_centered_init;
        do_redraw = true;
        break;
      }
      case PLACE_MODAL_SNAP_ON: {
        is_fallthrough = true;
        ATTR_FALLTHROUGH;
      }
      case PLACE_MODAL_SNAP_OFF: {
        const ToolSettings *ts = ipd->scene->toolsettings;
        ipd->is_snap_invert = is_fallthrough;
        ipd->use_snap = (ipd->is_snap_invert == !(ts->snap_flag & SCE_SNAP));
        do_cursor_update = true;
        break;
      }
    }
  }
  else {
    switch (event->type) {
      case EVT_ESCKEY:
      case RIGHTMOUSE: {
        /* Restore snap mode. */
        *ipd->snap_to_ptr = ipd->snap_to_restore;
        view3d_interactive_add_exit(C, op);
        return OPERATOR_CANCELLED;
      }
      case MOUSEMOVE: {
        do_cursor_update = true;
        break;
      }
      default: {
        break;
      }
    }
  }

  if (ipd->wait_for_input) {
    if (ELEM(event->type, LEFTMOUSE)) {
      if (event->val == KM_PRESS) {
        view3d_interactive_add_begin(C, op, event);
        ipd->wait_for_input = false;
        return OPERATOR_RUNNING_MODAL;
      }
    }
    return OPERATOR_RUNNING_MODAL;
  }

  if (ipd->step_index == STEP_BASE) {
    if (ELEM(event->type, ipd->launch_event, LEFTMOUSE)) {
      if (event->val == KM_RELEASE) {
        ED_view3d_cursor_snap_state_prevpoint_set(ipd->snap_state, ipd->co_src);
        if (ipd->snap_to_restore & SCE_SNAP_TO_GRID) {
          /* Don't snap to grid in #STEP_DEPTH. */
          *ipd->snap_to_ptr = ipd->snap_to_restore & ~SCE_SNAP_TO_GRID;
        }

        /* Set secondary plane. */

        /* Create normal. */
        {
          RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
          float no[3], no_temp[3];

          if (ipd->step[STEP_DEPTH].is_degenerate_view_align) {
            cross_v3_v3v3(no_temp, ipd->step[0].plane, ipd->step[STEP_DEPTH].degenerate_diagonal);
            cross_v3_v3v3(no, no_temp, ipd->step[0].plane);
          }
          else {
            cross_v3_v3v3(no_temp, ipd->step[0].plane, rv3d->viewinv[2]);
            cross_v3_v3v3(no, no_temp, ipd->step[0].plane);
          }
          normalize_v3(no);

          plane_from_point_normal_v3(ipd->step[1].plane, ipd->step[0].co_dst, no);
        }

        copy_v3_v3(ipd->step[1].co_dst, ipd->step[0].co_dst);
        ipd->step_index = STEP_DEPTH;

        /* Use the toggle from the previous step. */
        if (ipd->step[0].is_centered != ipd->step[0].is_centered_init) {
          ipd->step[1].is_centered = !ipd->step[1].is_centered;
        }
        if (ipd->step[0].is_fixed_aspect != ipd->step[0].is_fixed_aspect_init) {
          ipd->step[1].is_fixed_aspect = !ipd->step[1].is_fixed_aspect;
        }
      }
    }
  }
  else if (ipd->step_index == STEP_DEPTH) {
    if (ELEM(event->type, ipd->launch_event, LEFTMOUSE)) {
      if (event->val == KM_PRESS) {
        /* Restore snap mode. */
        *ipd->snap_to_ptr = ipd->snap_to_restore;

        /* Confirm. */
        BoundBox bounds;
        calc_bbox(ipd, &bounds);

        float location[3];
        float rotation[3];
        float scale[3];

        float matrix_orient_axis[3][3];
        copy_m3_m3(matrix_orient_axis, ipd->matrix_orient);
        if (ipd->orient_axis != 2) {
          swap_v3_v3(matrix_orient_axis[2], matrix_orient_axis[ipd->orient_axis]);
          swap_v3_v3(matrix_orient_axis[0], matrix_orient_axis[1]);
        }
        /* Needed for shapes where the sign matters (cone for eg). */
        {
          float delta[3];
          sub_v3_v3v3(delta, bounds.vec[0], bounds.vec[4]);
          if (dot_v3v3(ipd->matrix_orient[ipd->orient_axis], delta) > 0.0f) {
            negate_v3(matrix_orient_axis[2]);

            /* Only flip Y so we don't flip a single axis which causes problems. */
            negate_v3(matrix_orient_axis[1]);
          }
        }

        mat3_to_eul(rotation, matrix_orient_axis);

        mid_v3_v3v3(location, bounds.vec[0], bounds.vec[6]);
        const int cube_verts[3] = {3, 1, 4};
        for (int i = 0; i < 3; i++) {
          scale[i] = len_v3v3(bounds.vec[0], bounds.vec[cube_verts[i]]);
          /* Primitives have size 2 by default, compensate for this here. */
          scale[i] /= 2.0f;
        }

        wmOperatorType *ot = nullptr;
        PointerRNA op_props;
        if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CUBE) {
          ot = WM_operatortype_find("MESH_OT_primitive_cube_add", false);
        }
        else if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CYLINDER) {
          ot = WM_operatortype_find("MESH_OT_primitive_cylinder_add", false);
        }
        else if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CONE) {
          ot = WM_operatortype_find("MESH_OT_primitive_cone_add", false);
        }
        else if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_SPHERE_UV) {
          ot = WM_operatortype_find("MESH_OT_primitive_uv_sphere_add", false);
        }
        else if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_SPHERE_ICO) {
          ot = WM_operatortype_find("MESH_OT_primitive_ico_sphere_add", false);
        }

        if (ot != nullptr) {
          WM_operator_properties_create_ptr(&op_props, ot);

          if (ipd->use_tool) {
            bToolRef *tref = ipd->area->runtime.tool;
            PointerRNA temp_props;
            WM_toolsystem_ref_properties_init_for_keymap(tref, &temp_props, &op_props, ot);
            std::swap(temp_props, op_props);
            WM_operator_properties_free(&temp_props);
          }

          RNA_float_set_array(&op_props, "rotation", rotation);
          RNA_float_set_array(&op_props, "location", location);
          RNA_float_set_array(&op_props, "scale", scale);

          /* Always use the defaults here since desired bounds have been set interactively, it does
           * not make sense to use a different values from a previous command. */
          if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CUBE) {
            RNA_float_set(&op_props, "size", 2.0f);
          }
          if (ELEM(ipd->primitive_type,
                   PLACE_PRIMITIVE_TYPE_CYLINDER,
                   PLACE_PRIMITIVE_TYPE_SPHERE_UV,
                   PLACE_PRIMITIVE_TYPE_SPHERE_ICO))
          {
            RNA_float_set(&op_props, "radius", 1.0f);
          }
          if (ELEM(ipd->primitive_type, PLACE_PRIMITIVE_TYPE_CYLINDER, PLACE_PRIMITIVE_TYPE_CONE))
          {
            RNA_float_set(&op_props, "depth", 2.0f);
          }
          if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CONE) {
            RNA_float_set(&op_props, "radius1", 1.0f);
            RNA_float_set(&op_props, "radius2", 0.0f);
          }

          WM_operator_name_call_ptr(
              C, ot, blender::wm::OpCallContext::ExecDefault, &op_props, nullptr);
          WM_operator_properties_free(&op_props);
        }
        else {
          BLI_assert(0);
        }

        view3d_interactive_add_exit(C, op);
        return OPERATOR_FINISHED;
      }
    }
  }
  else {
    BLI_assert(0);
  }

  if (do_cursor_update) {
    float mval_fl[2];
    WM_event_drag_start_mval_fl(event, region, mval_fl);

    /* Calculate the snap location on mouse-move or when toggling snap. */
    ipd->is_snap_found = false;
    if (ipd->use_snap) {
      ipd->is_snap_found = view3d_interactive_add_calc_snap(
          C, event, ipd->snap_co, nullptr, nullptr, nullptr);
    }

    if (ipd->step_index == STEP_BASE) {
      if (ipd->is_snap_found) {
        closest_to_plane_normalized_v3(
            ipd->step[STEP_BASE].co_dst, ipd->step[STEP_BASE].plane, ipd->snap_co);
      }
      else {
        if (view3d_win_to_3d_on_plane_maybe_fallback(
                region,
                ipd->step[STEP_BASE].plane,
                mval_fl,
                ipd->step[STEP_BASE].is_degenerate_view_align ? ipd->view_plane : nullptr,
                ipd->step[STEP_BASE].co_dst))
        {
          /* pass */
        }

        if (ipd->use_snap && (ipd->snap_to_restore & (SCE_SNAP_TO_GRID | SCE_SNAP_TO_INCREMENT))) {
          if (idp_snap_calc_incremental(
                  ipd->scene, ipd->v3d, ipd->region, ipd->co_src, ipd->step[STEP_BASE].co_dst))
          {
          }
        }
      }
    }
    else if (ipd->step_index == STEP_DEPTH) {
      if (ipd->is_snap_found) {
        closest_to_plane_normalized_v3(
            ipd->step[STEP_DEPTH].co_dst, ipd->step[STEP_DEPTH].plane, ipd->snap_co);
      }
      else {
        if (view3d_win_to_3d_on_plane_maybe_fallback(
                region,
                ipd->step[STEP_DEPTH].plane,
                mval_fl,
                ipd->step[STEP_DEPTH].is_degenerate_view_align ? ipd->view_plane : nullptr,
                ipd->step[STEP_DEPTH].co_dst))
        {
          /* pass */
        }

        if (ipd->use_snap && (ipd->snap_to_restore & (SCE_SNAP_TO_GRID | SCE_SNAP_TO_INCREMENT))) {
          if (idp_snap_calc_incremental(
                  ipd->scene, ipd->v3d, ipd->region, ipd->co_src, ipd->step[STEP_DEPTH].co_dst))
          {
          }
        }
      }

      /* Correct the point so it's aligned with the 'ipd->step[0].co_dst'. */
      float close[3], delta[3];
      closest_to_plane_normalized_v3(
          close, ipd->step[STEP_BASE].plane, ipd->step[STEP_DEPTH].co_dst);
      sub_v3_v3v3(delta, close, ipd->step[STEP_BASE].co_dst);
      sub_v3_v3(ipd->step[STEP_DEPTH].co_dst, delta);
    }
    do_redraw = true;
  }

  if (do_redraw) {
    ED_region_tag_redraw(region);
  }

  return OPERATOR_RUNNING_MODAL;
}

static bool view3d_interactive_add_poll(bContext *C)
{
  const enum eContextObjectMode mode = CTX_data_mode_enum(C);
  return ELEM(mode, CTX_MODE_OBJECT, CTX_MODE_EDIT_MESH);
}

void VIEW3D_OT_interactive_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Primitive Object";
  ot->description = "Interactively add an object";
  ot->idname = "VIEW3D_OT_interactive_add";

  /* API callbacks. */
  ot->invoke = view3d_interactive_add_invoke;
  ot->modal = view3d_interactive_add_modal;
  ot->cancel = view3d_interactive_add_cancel;
  ot->poll = view3d_interactive_add_poll;

  /* NOTE: let the operator we call handle undo and registering itself. */
  /* flags */
  ot->flag = 0;

  /* properties */
  PropertyRNA *prop;

  /* Normally not accessed directly, leave unset and check the active tool. */
  static const EnumPropertyItem primitive_type[] = {
      {PLACE_PRIMITIVE_TYPE_CUBE, "CUBE", 0, "Cube", ""},
      {PLACE_PRIMITIVE_TYPE_CYLINDER, "CYLINDER", 0, "Cylinder", ""},
      {PLACE_PRIMITIVE_TYPE_CONE, "CONE", 0, "Cone", ""},
      {PLACE_PRIMITIVE_TYPE_SPHERE_UV, "SPHERE_UV", 0, "UV Sphere", ""},
      {PLACE_PRIMITIVE_TYPE_SPHERE_ICO, "SPHERE_ICO", 0, "ICO Sphere", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(ot->srna, "primitive_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Primitive", "");
  RNA_def_property_enum_items(prop, primitive_type);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  { /* Plane Origin. */
    static const EnumPropertyItem items[] = {
        {PLACE_ORIGIN_BASE, "EDGE", 0, "Edge", "Start placing the edge position"},
        {PLACE_ORIGIN_CENTER, "CENTER", 0, "Center", "Start placing the center position"},
        {0, nullptr, 0, nullptr, nullptr},
    };
    const char *identifiers[2] = {"plane_origin_base", "plane_origin_depth"};
    for (int i = 0; i < 2; i++) {
      prop = RNA_def_property(ot->srna, identifiers[i], PROP_ENUM, PROP_NONE);
      RNA_def_property_ui_text(prop, "Origin", "The initial position for placement");
      RNA_def_property_enum_default(prop, PLACE_ORIGIN_BASE);
      RNA_def_property_enum_items(prop, items);
      RNA_def_property_flag(prop, PROP_SKIP_SAVE);
    }
  }

  { /* Plane Aspect. */
    static const EnumPropertyItem items[] = {
        {PLACE_ASPECT_FREE, "FREE", 0, "Free", "Use an unconstrained aspect"},
        {PLACE_ASPECT_FIXED, "FIXED", 0, "Fixed", "Use a fixed 1:1 aspect"},
        {0, nullptr, 0, nullptr, nullptr},
    };
    const char *identifiers[2] = {"plane_aspect_base", "plane_aspect_depth"};
    for (int i = 0; i < 2; i++) {
      prop = RNA_def_property(ot->srna, identifiers[i], PROP_ENUM, PROP_NONE);
      RNA_def_property_ui_text(prop, "Aspect", "The initial aspect setting");
      RNA_def_property_enum_default(prop, PLACE_ASPECT_FREE);
      RNA_def_property_enum_items(prop, items);
      RNA_def_property_flag(prop, PROP_SKIP_SAVE);
    }
  }

  /* When not accessed via a tool. */
  prop = RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Placement Gizmo Group
 *
 * This is currently only used for snapping before the tool is initialized,
 * we could show a placement plane here.
 * \{ */

static void preview_plane_free_fn(void *customdata)
{
  V3DSnapCursorState *snap_state = static_cast<V3DSnapCursorState *>(customdata);
  ED_view3d_cursor_snap_state_free(snap_state);
}

static bool snap_cursor_poll(ARegion *region, void *data)
{
  if (WM_gizmomap_group_find_ptr(region->runtime->gizmo_map, (wmGizmoGroupType *)data) == nullptr)
  {
    /* Wrong viewport. */
    return false;
  }
  return true;
}

static void WIDGETGROUP_placement_setup(const bContext * /*C*/, wmGizmoGroup *gzgroup)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_create();
  if (snap_state) {
    snap_state->poll = snap_cursor_poll;
    snap_state->poll_data = gzgroup->type;
    snap_state->draw_plane = true;

    gzgroup->customdata = snap_state;
    gzgroup->customdata_free = preview_plane_free_fn;
  }
}

static bool WIDGETGROUP_placement_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
  if (ED_gizmo_poll_or_unlink_delayed_from_tool(C, gzgt)) {
    const Scene *scene = CTX_data_scene(C);
    if (BKE_id_is_editable(CTX_data_main(C), &scene->id)) {
      return true;
    }
  }
  return false;
}

void VIEW3D_GGT_placement(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Placement Widget";
  gzgt->idname = view3d_gzgt_placement_id;

  gzgt->flag |= WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_SCALE | WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = WIDGETGROUP_placement_poll;
  gzgt->setup = WIDGETGROUP_placement_setup;
}

/** \} */
