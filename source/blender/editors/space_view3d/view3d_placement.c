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
 * \ingroup spview3d
 *
 * Operator to interactively place data.
 *
 * Currently only adds meshes, but could add other kinds of data
 * including library assets & non-mesh types.
 */

#include "MEM_guardedalloc.h"

#include "BKE_context.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_toolsystem.h"

#include "ED_gizmo_utils.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "GPU_immediate.h"

#include "view3d_intern.h"

#define SNAP_MODE_GEOM \
  (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_FACE | \
   SCE_SNAP_MODE_EDGE_PERPENDICULAR | SCE_SNAP_MODE_EDGE_MIDPOINT)

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

enum ePlace_SnapTo {
  PLACE_SNAP_TO_GEOMETRY = 1,
  PLACE_SNAP_TO_DEFAULT = 2,
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
     * Toggling the setting flips the value from it's initial state.
     */
    bool is_centered, is_centered_init;
    /**
     * When fixed, constrain the X/Y aspect for the initial #STEP_BASE drag.
     * For #STEP_DEPTH match the maximum X/Y dimension.
     * Toggling the setting flips the value from it's initial state.
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

  /** When we can't project onto the real plane, use this in it's place. */
  float view_plane[4];

  float matrix_orient[3][3];
  int orient_axis;

  V3DSnapCursorState *snap_state;
  bool use_snap, is_snap_found, is_snap_invert;
  float snap_co[3];

  /** Can index into #InteractivePlaceData.step. */
  enum {
    STEP_BASE = 0,
    STEP_DEPTH = 1,
  } step_index;

  enum ePlace_PrimType primitive_type;

  /** Activated from the tool-system. */
  bool use_tool;

  /** Event used to start the operator. */
  short launch_event;

  /** When activated without a tool. */
  bool wait_for_input;

  enum ePlace_SnapTo snap_to;
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
  RegionView3D *rv3d = region->regiondata;
  bool do_clip = rv3d->is_persp;
  if (plane_fallback != NULL) {
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
  wmGizmoMap *gzmap = region->gizmo_map;
  return gzmap ? WM_gizmomap_group_find(gzmap, view3d_gzgt_placement_id) : NULL;
}

/**
 * Calculate 3D view incremental (grid) snapping.
 *
 * \note This could be moved to a public function.
 */
static bool idp_snap_calc_incremental(
    Scene *scene, View3D *v3d, ARegion *region, const float co_relative[3], float co[3])
{
  if ((scene->toolsettings->snap_mode & SCE_SNAP_MODE_INCREMENT) == 0) {
    return false;
  }

  const float grid_size = ED_view3d_grid_view_scale(scene, v3d, region, NULL);
  if (UNLIKELY(grid_size == 0.0f)) {
    return false;
  }

  if (scene->toolsettings->snap_flag & SCE_SNAP_ABS_GRID) {
    co_relative = NULL;
  }

  if (co_relative != NULL) {
    sub_v3_v3(co, co_relative);
  }
  mul_v3_fl(co, 1.0f / grid_size);
  co[0] = roundf(co[0]);
  co[1] = roundf(co[1]);
  co[2] = roundf(co[2]);
  mul_v3_fl(co, grid_size);
  if (co_relative != NULL) {
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
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  GPUVertBuf *vert = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(vert, coords_len);

  for (int i = 0; i < coords_len; i++) {
    GPU_vertbuf_attr_set(vert, pos, i, coords[i]);
  }

  GPU_blend(GPU_BLEND_ALPHA);
  GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_LINE_LOOP, vert, NULL, GPU_BATCH_OWNS_VBO);
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
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  GPUVertBuf *vert = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(vert, coords_len * 2);

  for (int i = 0; i < coords_len; i++) {
    GPU_vertbuf_attr_set(vert, pos, i * 2, coords_a[i]);
    GPU_vertbuf_attr_set(vert, pos, (i * 2) + 1, coords_b[i]);
  }

  GPU_blend(GPU_BLEND_ALPHA);
  GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_LINES, vert, NULL, GPU_BATCH_OWNS_VBO);
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
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  const int edges[12][2] = {
      /* First side. */
      {0, 1},
      {1, 2},
      {2, 3},
      {3, 0},
      /* Second side. */
      {4, 5},
      {5, 6},
      {6, 7},
      {7, 4},
      /* Edges between. */
      {0, 4},
      {1, 5},
      {2, 6},
      {3, 7},
  };

  GPUVertBuf *vert = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(vert, ARRAY_SIZE(edges) * 2);

  for (int i = 0, j = 0; i < ARRAY_SIZE(edges); i++) {
    GPU_vertbuf_attr_set(vert, pos, j++, bounds->vec[edges[i][0]]);
    GPU_vertbuf_attr_set(vert, pos, j++, bounds->vec[edges[i][1]]);
  }

  GPU_blend(GPU_BLEND_ALPHA);
  GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_LINES, vert, NULL, GPU_BATCH_OWNS_VBO);
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

static bool calc_bbox(struct InteractivePlaceData *ipd, BoundBox *bounds)
{
  memset(bounds, 0x0, sizeof(*bounds));

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
      (compare_v3v3(ipd->step[0].co_dst, ipd->step[1].co_dst, FLT_EPSILON) == false)) {

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

  float(*coords)[3] = MEM_mallocN(sizeof(float[3]) * (resolution + 1), __func__);
  for (int i = 0; i <= resolution; i++) {
    float theta = ((2.0f * M_PI) * ((float)i / (float)resolution)) + 0.01f;
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

static void draw_primitive_view_impl(const struct bContext *C,
                                     struct InteractivePlaceData *ipd,
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

    RegionView3D *rv3d = ipd->region->regiondata;
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
    draw_circle_in_quad(UNPACK4((&bounds.vec[4])), 32, color);
  }
  else if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CONE) {
    draw_circle_in_quad(UNPACK4(bounds.vec), 32, color);

    float center[3];
    mid_v3_v3v3v3v3(center, UNPACK4((&bounds.vec[4])));

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
                PLACE_PRIMITIVE_TYPE_SPHERE_ICO)) {
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

static void draw_primitive_view(const struct bContext *C, ARegion *UNUSED(region), void *arg)
{
  struct InteractivePlaceData *ipd = arg;
  float color[4];
  UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, color);

  const bool use_depth = !XRAY_ENABLED(ipd->v3d);
  const eGPUDepthTest depth_test_enabled = GPU_depth_test_get();

  if (use_depth) {
    GPU_depth_test(GPU_DEPTH_NONE);
    color[3] = 0.15f;
    draw_primitive_view_impl(C, ipd, color, -1);
  }

  /* Show a flattened projection if the current step is aligned to the view. */
  if (ipd->step[ipd->step_index].is_degenerate_view_align) {
    const RegionView3D *rv3d = ipd->region->regiondata;
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

static bool view3d_interactive_add_calc_snap(bContext *UNUSED(C),
                                             const wmEvent *UNUSED(event),
                                             float r_co_src[3],
                                             float r_matrix_orient[3][3],
                                             bool *r_is_enabled,
                                             bool *r_is_snap_invert)
{
  V3DSnapCursorData *snap_data = ED_view3d_cursor_snap_data_get(NULL, NULL, 0, 0);
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
  return snap_data->snap_elem != 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Object Modal Operator
 * \{ */

static void view3d_interactive_add_begin(bContext *C, wmOperator *op, const wmEvent *event)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();

  const int plane_axis = snap_state->plane_axis;
  const enum ePlace_SnapTo snap_to = RNA_enum_get(op->ptr, "snap_target");

  const enum ePlace_Origin plane_origin[2] = {
      RNA_enum_get(op->ptr, "plane_origin_base"),
      RNA_enum_get(op->ptr, "plane_origin_depth"),
  };
  const enum ePlace_Aspect plane_aspect[2] = {
      RNA_enum_get(op->ptr, "plane_aspect_base"),
      RNA_enum_get(op->ptr, "plane_aspect_depth"),
  };

  struct InteractivePlaceData *ipd = op->customdata;

  ipd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

  ipd->snap_state = ED_view3d_cursor_snap_active();
  ipd->snap_state->draw_point = true;
  ipd->snap_state->draw_plane = true;

  ipd->is_snap_found =
      view3d_interactive_add_calc_snap(
          C, event, ipd->co_src, ipd->matrix_orient, &ipd->use_snap, &ipd->is_snap_invert) != 0;

  ipd->snap_state->draw_plane = false;
  ED_view3d_cursor_snap_prevpoint_set(ipd->snap_state, ipd->co_src);

  ipd->orient_axis = plane_axis;
  for (int i = 0; i < 2; i++) {
    ipd->step[i].is_centered_init = (plane_origin[i] == PLACE_ORIGIN_CENTER);
    ipd->step[i].is_centered = ipd->step[i].is_centered_init;

    ipd->step[i].is_fixed_aspect_init = (plane_aspect[i] == PLACE_ASPECT_FIXED);
    ipd->step[i].is_fixed_aspect = ipd->step[i].is_fixed_aspect_init;
  }

  ipd->step_index = STEP_BASE;
  ipd->snap_to = snap_to;

  plane_from_point_normal_v3(ipd->step[0].plane, ipd->co_src, ipd->matrix_orient[plane_axis]);

  copy_v3_v3(ipd->step[0].co_dst, ipd->co_src);

  {
    RegionView3D *rv3d = ipd->region->regiondata;
    const float view_axis_dot = fabsf(dot_v3v3(rv3d->viewinv[2], ipd->matrix_orient[plane_axis]));
    ipd->step[STEP_BASE].is_degenerate_view_align = view_axis_dot < eps_view_align;
    ipd->step[STEP_DEPTH].is_degenerate_view_align = fabsf(view_axis_dot - 1.0f) < eps_view_align;

    float view_axis[3];
    normalize_v3_v3(view_axis, rv3d->viewinv[2]);
    plane_from_point_normal_v3(ipd->view_plane, ipd->co_src, view_axis);
  }

  if (ipd->step[STEP_BASE].is_degenerate_view_align ||
      ipd->step[STEP_DEPTH].is_degenerate_view_align) {
    RegionView3D *rv3d = ipd->region->regiondata;
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
      ipd->region->type, draw_primitive_view, ipd, REGION_DRAW_POST_VIEW);

  ED_region_tag_redraw(ipd->region);

  /* Setup the primitive type. */
  {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "primitive_type");
    if (RNA_property_is_set(op->ptr, prop)) {
      ipd->primitive_type = RNA_property_enum_get(op->ptr, prop);
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

static int view3d_interactive_add_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool wait_for_input = RNA_boolean_get(op->ptr, "wait_for_input");

  struct InteractivePlaceData *ipd = MEM_callocN(sizeof(*ipd), __func__);
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

  struct InteractivePlaceData *ipd = op->customdata;
  ED_view3d_cursor_snap_deactive(ipd->snap_state);

  ED_region_draw_cb_exit(ipd->region->type, ipd->draw_handle_view);

  ED_region_tag_redraw(ipd->region);

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
      {0, NULL, 0, NULL, NULL},
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

static int view3d_interactive_add_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  UNUSED_VARS(C, op);

  struct InteractivePlaceData *ipd = op->customdata;

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
        view3d_interactive_add_exit(C, op);
        return OPERATOR_CANCELLED;
      }
      case MOUSEMOVE: {
        do_cursor_update = true;
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
        ED_view3d_cursor_snap_prevpoint_set(ipd->snap_state, ipd->co_src);

        /* Set secondary plane. */

        /* Create normal. */
        {
          RegionView3D *rv3d = region->regiondata;
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

        wmOperatorType *ot = NULL;
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

        if (ot != NULL) {
          WM_operator_properties_create_ptr(&op_props, ot);

          if (ipd->use_tool) {
            bToolRef *tref = ipd->area->runtime.tool;
            PointerRNA temp_props;
            WM_toolsystem_ref_properties_init_for_keymap(tref, &temp_props, &op_props, ot);
            SWAP(PointerRNA, temp_props, op_props);
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
                   PLACE_PRIMITIVE_TYPE_SPHERE_ICO)) {
            RNA_float_set(&op_props, "radius", 1.0f);
          }
          if (ELEM(
                  ipd->primitive_type, PLACE_PRIMITIVE_TYPE_CYLINDER, PLACE_PRIMITIVE_TYPE_CONE)) {
            RNA_float_set(&op_props, "depth", 2.0f);
          }
          if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CONE) {
            RNA_float_set(&op_props, "radius1", 1.0f);
            RNA_float_set(&op_props, "radius2", 0.0f);
          }

          WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &op_props);
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
    const float mval_fl[2] = {UNPACK2(event->mval)};

    /* Calculate the snap location on mouse-move or when toggling snap. */
    ipd->is_snap_found = false;
    if (ipd->use_snap) {
      ipd->is_snap_found = view3d_interactive_add_calc_snap(
          C, event, ipd->snap_co, NULL, NULL, NULL);
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
                ipd->step[STEP_BASE].is_degenerate_view_align ? ipd->view_plane : NULL,
                ipd->step[STEP_BASE].co_dst)) {
          /* pass */
        }

        if (ipd->use_snap && (ipd->snap_to == PLACE_SNAP_TO_DEFAULT)) {
          if (idp_snap_calc_incremental(
                  ipd->scene, ipd->v3d, ipd->region, ipd->co_src, ipd->step[STEP_BASE].co_dst)) {
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
                ipd->step[STEP_DEPTH].is_degenerate_view_align ? ipd->view_plane : NULL,
                ipd->step[STEP_DEPTH].co_dst)) {
          /* pass */
        }

        if (ipd->use_snap && (ipd->snap_to == PLACE_SNAP_TO_DEFAULT)) {
          if (idp_snap_calc_incremental(
                  ipd->scene, ipd->v3d, ipd->region, ipd->co_src, ipd->step[STEP_DEPTH].co_dst)) {
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

static int idp_rna_plane_axis_get_fn(struct PointerRNA *UNUSED(ptr),
                                     struct PropertyRNA *UNUSED(prop))
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  return snap_state->plane_axis;
}

static void idp_rna_plane_axis_set_fn(struct PointerRNA *UNUSED(ptr),
                                      struct PropertyRNA *UNUSED(prop),
                                      int value)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  snap_state->plane_axis = (short)value;
  ED_view3d_cursor_snap_state_default_set(snap_state);
}

static int idp_rna_plane_depth_get_fn(struct PointerRNA *UNUSED(ptr),
                                      struct PropertyRNA *UNUSED(prop))
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  return snap_state->plane_depth;
}

static void idp_rna_plane_depth_set_fn(struct PointerRNA *UNUSED(ptr),
                                       struct PropertyRNA *UNUSED(prop),
                                       int value)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  snap_state->plane_depth = value;
  ED_view3d_cursor_snap_state_default_set(snap_state);
}

static int idp_rna_plane_orient_get_fn(struct PointerRNA *UNUSED(ptr),
                                       struct PropertyRNA *UNUSED(prop))
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  return snap_state->plane_orient;
}

static void idp_rna_plane_orient_set_fn(struct PointerRNA *UNUSED(ptr),
                                        struct PropertyRNA *UNUSED(prop),
                                        int value)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  snap_state->plane_orient = value;
  ED_view3d_cursor_snap_state_default_set(snap_state);
}

static int idp_rna_snap_target_get_fn(struct PointerRNA *UNUSED(ptr),
                                      struct PropertyRNA *UNUSED(prop))
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  if (!snap_state->snap_elem_force) {
    return PLACE_SNAP_TO_DEFAULT;
  }

  /* Make sure you keep a consistent #snap_mode. */
  snap_state->snap_elem_force = SNAP_MODE_GEOM;
  return PLACE_SNAP_TO_GEOMETRY;
}

static void idp_rna_snap_target_set_fn(struct PointerRNA *UNUSED(ptr),
                                       struct PropertyRNA *UNUSED(prop),
                                       int value)
{
  short snap_mode = 0; /* #toolsettings->snap_mode. */
  const enum ePlace_SnapTo snap_to = value;
  if (snap_to == PLACE_SNAP_TO_GEOMETRY) {
    snap_mode = SNAP_MODE_GEOM;
  }

  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  snap_state->snap_elem_force = snap_mode;
  ED_view3d_cursor_snap_state_default_set(snap_state);
}

static bool idp_rna_use_plane_axis_auto_get_fn(struct PointerRNA *UNUSED(ptr),
                                               struct PropertyRNA *UNUSED(prop))
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  return snap_state->use_plane_axis_auto;
}

static void idp_rna_use_plane_axis_auto_set_fn(struct PointerRNA *UNUSED(ptr),
                                               struct PropertyRNA *UNUSED(prop),
                                               bool value)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_state_get();
  snap_state->use_plane_axis_auto = value;
  ED_view3d_cursor_snap_state_default_set(snap_state);
}

void VIEW3D_OT_interactive_add(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Primitive Object";
  ot->description = "Interactively add an object";
  ot->idname = "VIEW3D_OT_interactive_add";

  /* api callbacks */
  ot->invoke = view3d_interactive_add_invoke;
  ot->modal = view3d_interactive_add_modal;
  ot->cancel = view3d_interactive_add_cancel;
  ot->poll = view3d_interactive_add_poll;

  /* NOTE: let the operator we call handle undo and registering itself. */
  /* flags */
  ot->flag = 0;

  /* properties */
  PropertyRNA *prop;

  /* WORKAROUND: properties with `_funcs_runtime` should not be saved in keymaps.
   *             So reassign the #PROP_IDPROPERTY flag to trick the property as not being set.
   *             (See #RNA_property_is_set). */
  PropertyFlag unsalvageable = PROP_SKIP_SAVE | PROP_HIDDEN | PROP_PTR_NO_OWNERSHIP |
                               PROP_IDPROPERTY;

  /* Normally not accessed directly, leave unset and check the active tool. */
  static const EnumPropertyItem primitive_type[] = {
      {PLACE_PRIMITIVE_TYPE_CUBE, "CUBE", 0, "Cube", ""},
      {PLACE_PRIMITIVE_TYPE_CYLINDER, "CYLINDER", 0, "Cylinder", ""},
      {PLACE_PRIMITIVE_TYPE_CONE, "CONE", 0, "Cone", ""},
      {PLACE_PRIMITIVE_TYPE_SPHERE_UV, "SPHERE_UV", 0, "UV Sphere", ""},
      {PLACE_PRIMITIVE_TYPE_SPHERE_ICO, "SPHERE_ICO", 0, "ICO Sphere", ""},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(ot->srna, "primitive_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Primitive", "");
  RNA_def_property_enum_items(prop, primitive_type);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_property(ot->srna, "plane_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Plane Axis", "The axis used for placing the base region");
  RNA_def_property_enum_default(prop, 2);
  RNA_def_property_enum_items(prop, rna_enum_axis_xyz_items);
  RNA_def_property_enum_funcs_runtime(
      prop, idp_rna_plane_axis_get_fn, idp_rna_plane_axis_set_fn, NULL);
  RNA_def_property_flag(prop, unsalvageable);

  prop = RNA_def_boolean(ot->srna,
                         "plane_axis_auto",
                         false,
                         "Auto Axis",
                         "Select the closest axis when placing objects "
                         "(surface overrides)");
  RNA_def_property_boolean_funcs_runtime(
      prop, idp_rna_use_plane_axis_auto_get_fn, idp_rna_use_plane_axis_auto_set_fn);
  RNA_def_property_flag(prop, unsalvageable);

  static const EnumPropertyItem plane_depth_items[] = {
      {V3D_PLACE_DEPTH_SURFACE,
       "SURFACE",
       0,
       "Surface",
       "Start placing on the surface, using the 3D cursor position as a fallback"},
      {V3D_PLACE_DEPTH_CURSOR_PLANE,
       "CURSOR_PLANE",
       0,
       "Cursor Plane",
       "Start placement using a point projected onto the orientation axis "
       "at the 3D cursor position"},
      {V3D_PLACE_DEPTH_CURSOR_VIEW,
       "CURSOR_VIEW",
       0,
       "Cursor View",
       "Start placement using a point projected onto the view plane at the 3D cursor position"},
      {0, NULL, 0, NULL, NULL},
  };
  prop = RNA_def_property(ot->srna, "plane_depth", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Position", "The initial depth used when placing the cursor");
  RNA_def_property_enum_default(prop, V3D_PLACE_DEPTH_SURFACE);
  RNA_def_property_enum_items(prop, plane_depth_items);
  RNA_def_property_enum_funcs_runtime(
      prop, idp_rna_plane_depth_get_fn, idp_rna_plane_depth_set_fn, NULL);
  RNA_def_property_flag(prop, unsalvageable);

  static const EnumPropertyItem plane_orientation_items[] = {
      {V3D_PLACE_ORIENT_SURFACE,
       "SURFACE",
       ICON_SNAP_NORMAL,
       "Surface",
       "Use the surface normal (using the transform orientation as a fallback)"},
      {V3D_PLACE_ORIENT_DEFAULT,
       "DEFAULT",
       ICON_ORIENTATION_GLOBAL,
       "Default",
       "Use the current transform orientation"},
      {0, NULL, 0, NULL, NULL},
  };
  prop = RNA_def_property(ot->srna, "plane_orientation", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Orientation", "The initial depth used when placing the cursor");
  RNA_def_property_enum_default(prop, V3D_PLACE_ORIENT_SURFACE);
  RNA_def_property_enum_items(prop, plane_orientation_items);
  RNA_def_property_enum_funcs_runtime(
      prop, idp_rna_plane_orient_get_fn, idp_rna_plane_orient_set_fn, NULL);
  RNA_def_property_flag(prop, unsalvageable);

  static const EnumPropertyItem snap_to_items[] = {
      {PLACE_SNAP_TO_GEOMETRY, "GEOMETRY", 0, "Geometry", "Snap to all geometry"},
      {PLACE_SNAP_TO_DEFAULT, "DEFAULT", 0, "Default", "Use the current snap settings"},
      {0, NULL, 0, NULL, NULL},
  };
  prop = RNA_def_property(ot->srna, "snap_target", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Snap to", "The target to use while snapping");
  RNA_def_property_enum_default(prop, PLACE_SNAP_TO_GEOMETRY);
  RNA_def_property_enum_items(prop, snap_to_items);
  RNA_def_property_enum_funcs_runtime(
      prop, idp_rna_snap_target_get_fn, idp_rna_snap_target_set_fn, NULL);
  RNA_def_property_flag(prop, unsalvageable);

  { /* Plane Origin. */
    static const EnumPropertyItem items[] = {
        {PLACE_ORIGIN_BASE, "EDGE", 0, "Edge", "Start placing the edge position"},
        {PLACE_ORIGIN_CENTER, "CENTER", 0, "Center", "Start placing the center position"},
        {0, NULL, 0, NULL, NULL},
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
        {0, NULL, 0, NULL, NULL},
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
  V3DSnapCursorState *snap_state = customdata;
  ED_view3d_cursor_snap_deactive(snap_state);
}

static void WIDGETGROUP_placement_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
  V3DSnapCursorState *snap_state = ED_view3d_cursor_snap_active();
  snap_state->draw_plane = true;

  gzgroup->customdata = snap_state;
  gzgroup->customdata_free = preview_plane_free_fn;
}

void VIEW3D_GGT_placement(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Placement Widget";
  gzgt->idname = view3d_gzgt_placement_id;

  gzgt->flag |= WM_GIZMOGROUPTYPE_3D | WM_GIZMOGROUPTYPE_SCALE | WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL;

  gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = ED_gizmo_poll_or_unlink_delayed_from_tool;
  gzgt->setup = WIDGETGROUP_placement_setup;
}

/** \} */
