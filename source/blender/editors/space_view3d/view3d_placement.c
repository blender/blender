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

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_gizmo_library.h"
#include "ED_gizmo_utils.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_transform.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "GPU_batch.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "view3d_intern.h"

static const char *view3d_gzgt_placement_id = "VIEW3D_GGT_placement";

static void preview_plane_cursor_setup(wmGizmoGroup *gzgroup);
static void preview_plane_cursor_visible_set(wmGizmoGroup *gzgroup, bool do_draw);

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

enum ePlace_Depth {
  PLACE_DEPTH_SURFACE = 1,
  PLACE_DEPTH_CURSOR_PLANE = 2,
  PLACE_DEPTH_CURSOR_VIEW = 3,
};

enum ePlace_Orient {
  PLACE_ORIENT_SURFACE = 1,
  PLACE_ORIENT_DEFAULT = 2,
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

  /** Optional snap gizmo, needed for snapping. */
  wmGizmo *snap_gizmo;

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

/**
 * Re-order \a mat so \a axis_align uses it's own axis which is closest to \a v.
 */
static bool mat3_align_axis_to_v3(float mat[3][3], const int axis_align, const float v[3])
{
  float dot_best = -1.0f;
  int axis_found = axis_align;
  for (int i = 0; i < 3; i++) {
    const float dot_test = fabsf(dot_v3v3(mat[i], v));
    if (dot_test > dot_best) {
      dot_best = dot_test;
      axis_found = i;
    }
  }

  if (axis_align != axis_found) {
    float tmat[3][3];
    copy_m3_m3(tmat, mat);
    const int offset = mod_i(axis_found - axis_align, 3);
    for (int i = 0; i < 3; i++) {
      copy_v3_v3(mat[i], tmat[(i + offset) % 3]);
    }
    return true;
  }
  return false;
}

/* On-screen snap distance. */
#define MVAL_MAX_PX_DIST 12.0f

static bool idp_snap_point_from_gizmo_ex(wmGizmo *gz, const char *prop_id, float r_location[3])
{
  if (gz->state & WM_GIZMO_STATE_HIGHLIGHT) {
    PropertyRNA *prop_location = RNA_struct_find_property(gz->ptr, prop_id);
    RNA_property_float_get_array(gz->ptr, prop_location, r_location);
    return true;
  }
  return false;
}

static bool idp_snap_point_from_gizmo(wmGizmo *gz, float r_location[3])
{
  return idp_snap_point_from_gizmo_ex(gz, "location", r_location);
}

static bool idp_snap_normal_from_gizmo(wmGizmo *gz, float r_normal[3])
{
  return idp_snap_point_from_gizmo_ex(gz, "normal", r_normal);
}

/**
 * Calculate a 3x3 orientation matrix from the surface under the cursor.
 */
static bool idp_poject_surface_normal(SnapObjectContext *snap_context,
                                      struct Depsgraph *depsgraph,
                                      const float mval_fl[2],
                                      const float mat_fallback[3][3],
                                      const float normal_fallback[3],
                                      float r_mat[3][3])
{
  bool success = false;
  float normal[3] = {0.0f};
  float co_dummy[3];
  /* We could use the index to get the orientation from the face. */
  Object *ob_snap;
  float obmat[4][4];

  if (ED_transform_snap_object_project_view3d_ex(snap_context,
                                                 depsgraph,
                                                 SCE_SNAP_MODE_FACE,
                                                 &(const struct SnapObjectParams){
                                                     .snap_select = SNAP_ALL,
                                                     .use_object_edit_cage = true,
                                                 },
                                                 mval_fl,
                                                 NULL,
                                                 NULL,
                                                 co_dummy,
                                                 normal,
                                                 NULL,
                                                 &ob_snap,
                                                 obmat)) {
    /* pass */
  }
  else if (normal_fallback != NULL) {
    copy_m4_m3(obmat, mat_fallback);
    copy_v3_v3(normal, normal_fallback);
  }

  if (!is_zero_v3(normal)) {
    float mat[3][3];
    copy_m3_m4(mat, obmat);
    normalize_m3(mat);

    float dot_best = fabsf(dot_v3v3(mat[0], normal));
    int i_best = 0;
    for (int i = 1; i < 3; i++) {
      float dot_test = fabsf(dot_v3v3(mat[i], normal));
      if (dot_test > dot_best) {
        i_best = i;
        dot_best = dot_test;
      }
    }
    if (dot_v3v3(mat[i_best], normal) < 0.0f) {
      negate_v3(mat[(i_best + 1) % 3]);
      negate_v3(mat[(i_best + 2) % 3]);
    }
    copy_v3_v3(mat[i_best], normal);
    orthogonalize_m3(mat, i_best);
    normalize_m3(mat);

    copy_v3_v3(r_mat[0], mat[(i_best + 1) % 3]);
    copy_v3_v3(r_mat[1], mat[(i_best + 2) % 3]);
    copy_v3_v3(r_mat[2], mat[i_best]);
    success = true;
  }

  return success;
}

static wmGizmoGroup *idp_gizmogroup_from_region(ARegion *region)
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

static void idp_snap_gizmo_update_snap_elements(Scene *scene,
                                                enum ePlace_SnapTo snap_to,
                                                wmGizmo *gizmo)
{
  const int snap_mode =
      (snap_to == PLACE_SNAP_TO_GEOMETRY) ?
          (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_FACE |
           /* SCE_SNAP_MODE_VOLUME | SCE_SNAP_MODE_GRID | SCE_SNAP_MODE_INCREMENT | */
           SCE_SNAP_MODE_EDGE_PERPENDICULAR | SCE_SNAP_MODE_EDGE_MIDPOINT) :
          scene->toolsettings->snap_mode;

  RNA_enum_set(gizmo->ptr, "snap_elements_force", snap_mode);
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

    /* Assign here in case secondary  */
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

static void view3d_interactive_add_calc_plane(bContext *C,
                                              Scene *scene,
                                              View3D *v3d,
                                              ARegion *region,
                                              const float mval_fl[2],
                                              wmGizmo *snap_gizmo,
                                              const enum ePlace_SnapTo snap_to,
                                              const enum ePlace_Depth plane_depth,
                                              const enum ePlace_Orient plane_orient,
                                              const int plane_axis,
                                              const bool plane_axis_auto,
                                              float r_co_src[3],
                                              float r_matrix_orient[3][3])
{
  const RegionView3D *rv3d = region->regiondata;
  ED_transform_calc_orientation_from_type(C, r_matrix_orient);

  /* Non-orthogonal matrices cause the preview and final result not to match.
   *
   * While making orthogonal doesn't always work well (especially with gimbal orientation for e.g.)
   * it's a corner case, without better alternatives as objects don't support shear. */
  orthogonalize_m3(r_matrix_orient, plane_axis);

  SnapObjectContext *snap_context = NULL;
  bool snap_context_free = false;

  /* Set the orientation. */
  if ((plane_orient == PLACE_ORIENT_SURFACE) || (plane_depth == PLACE_DEPTH_SURFACE)) {
    snap_context = (snap_gizmo ?
                        ED_gizmotypes_snap_3d_context_ensure(scene, region, v3d, snap_gizmo) :
                        NULL);
    if (snap_context == NULL) {
      snap_context = ED_transform_snap_object_context_create_view3d(scene, 0, region, v3d);
      snap_context_free = true;
    }
  }

  if (plane_orient == PLACE_ORIENT_SURFACE) {
    bool found_surface_or_normal = false;
    float matrix_orient_surface[3][3];

    /* Use the snap normal as a fallback in case the cursor isn't over a surface
     * but snapping is enabled. */
    float normal_fallback[3];
    bool use_normal_fallback = snap_gizmo ?
                                   idp_snap_normal_from_gizmo(snap_gizmo, normal_fallback) :
                                   false;

    if ((snap_context != NULL) &&
        idp_poject_surface_normal(snap_context,
                                  CTX_data_ensure_evaluated_depsgraph(C),
                                  mval_fl,
                                  use_normal_fallback ? r_matrix_orient : NULL,
                                  use_normal_fallback ? normal_fallback : NULL,
                                  matrix_orient_surface)) {
      copy_m3_m3(r_matrix_orient, matrix_orient_surface);
      found_surface_or_normal = true;
    }

    if (!found_surface_or_normal && plane_axis_auto) {
      /* Drawing into empty space, draw onto the plane most aligned to the view direction. */
      mat3_align_axis_to_v3(r_matrix_orient, plane_axis, rv3d->viewinv[2]);
    }
  }

  const bool is_snap_found = snap_gizmo ? idp_snap_point_from_gizmo(snap_gizmo, r_co_src) : false;

  if (is_snap_found) {
    /* pass */
  }
  else {
    bool use_depth_fallback = true;
    if (plane_depth == PLACE_DEPTH_CURSOR_VIEW) {
      /* View plane. */
      ED_view3d_win_to_3d(v3d, region, scene->cursor.location, mval_fl, r_co_src);
      use_depth_fallback = false;
    }
    else if (plane_depth == PLACE_DEPTH_SURFACE) {
      if ((snap_context != NULL) &&
          ED_transform_snap_object_project_view3d(snap_context,
                                                  CTX_data_ensure_evaluated_depsgraph(C),
                                                  SCE_SNAP_MODE_FACE,
                                                  &(const struct SnapObjectParams){
                                                      .snap_select = SNAP_ALL,
                                                      .use_object_edit_cage = true,
                                                  },
                                                  mval_fl,
                                                  NULL,
                                                  NULL,
                                                  r_co_src,
                                                  NULL)) {
        use_depth_fallback = false;
      }
    }

    /* Use as fallback to surface. */
    if (use_depth_fallback || (plane_depth == PLACE_DEPTH_CURSOR_PLANE)) {
      /* Cursor plane. */
      float plane[4];
      const float *plane_normal = r_matrix_orient[plane_axis];

      const float view_axis_dot = fabsf(dot_v3v3(rv3d->viewinv[2], r_matrix_orient[plane_axis]));
      if (view_axis_dot < eps_view_align) {
        /* In this case, just project onto the view plane as it's important the location
         * is _always_ under the mouse cursor, even if it turns out that wont lie on
         * the original 'plane' that's been calculated for us. */
        plane_normal = rv3d->viewinv[2];
      }

      plane_from_point_normal_v3(plane, scene->cursor.location, plane_normal);

      if (view3d_win_to_3d_on_plane_maybe_fallback(region, plane, mval_fl, NULL, r_co_src)) {
        use_depth_fallback = false;
      }

      /* Even if the calculation works, it's possible the point found is behind the view,
       * or very far away (past the far clipping).
       * In either case creating objects wont be useful. */
      if (rv3d->is_persp) {
        float dir[3];
        sub_v3_v3v3(dir, rv3d->viewinv[3], r_co_src);
        const float dot = dot_v3v3(dir, rv3d->viewinv[2]);
        if (dot < v3d->clip_start || dot > v3d->clip_end) {
          use_depth_fallback = true;
        }
      }
    }

    if (use_depth_fallback) {
      float co_depth[3];
      /* Fallback to view center. */
      negate_v3_v3(co_depth, rv3d->ofs);
      ED_view3d_win_to_3d(v3d, region, co_depth, mval_fl, r_co_src);
    }
  }

  if (!is_snap_found && ((snap_gizmo != NULL) && ED_gizmotypes_snap_3d_is_enabled(snap_gizmo))) {
    if (snap_to == PLACE_SNAP_TO_DEFAULT) {
      idp_snap_calc_incremental(scene, v3d, region, NULL, r_co_src);
    }
  }

  if (snap_context_free) {
    ED_transform_snap_object_context_destroy(snap_context);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Object Modal Operator
 * \{ */

static void view3d_interactive_add_begin(bContext *C, wmOperator *op, const wmEvent *event)
{

  const int plane_axis = RNA_enum_get(op->ptr, "plane_axis");
  const bool plane_axis_auto = RNA_boolean_get(op->ptr, "plane_axis_auto");
  const enum ePlace_SnapTo snap_to = RNA_enum_get(op->ptr, "snap_target");
  const enum ePlace_Depth plane_depth = RNA_enum_get(op->ptr, "plane_depth");
  const enum ePlace_Origin plane_origin[2] = {
      RNA_enum_get(op->ptr, "plane_origin_base"),
      RNA_enum_get(op->ptr, "plane_origin_depth"),
  };
  const enum ePlace_Aspect plane_aspect[2] = {
      RNA_enum_get(op->ptr, "plane_aspect_base"),
      RNA_enum_get(op->ptr, "plane_aspect_depth"),
  };
  const enum ePlace_Orient plane_orient = RNA_enum_get(op->ptr, "plane_orientation");

  const float mval_fl[2] = {UNPACK2(event->mval)};

  struct InteractivePlaceData *ipd = op->customdata;

  /* Assign snap gizmo which is may be used as part of the tool. */
  {
    wmGizmoGroup *gzgroup = idp_gizmogroup_from_region(ipd->region);
    if (gzgroup != NULL) {
      if (gzgroup->gizmos.first) {
        ipd->snap_gizmo = gzgroup->gizmos.first;
      }

      /* Can be NULL when gizmos are disabled. */
      if (gzgroup->customdata != NULL) {
        preview_plane_cursor_visible_set(gzgroup, false);
      }
    }
  }

  /* For tweak events the snap target may have changed since dragging,
   * update the snap target at the cursor location where tweak began.
   *
   * NOTE: we could investigating solving this in a more generic way,
   * so each operator doesn't have to account for it. */
  if (ISTWEAK(event->type)) {
    if (ipd->snap_gizmo != NULL) {
      ED_gizmotypes_snap_3d_update(ipd->snap_gizmo,
                                   CTX_data_ensure_evaluated_depsgraph(C),
                                   ipd->region,
                                   ipd->v3d,
                                   G_MAIN->wm.first,
                                   mval_fl,
                                   NULL,
                                   NULL);
    }
  }

  ipd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);

  view3d_interactive_add_calc_plane(C,
                                    ipd->scene,
                                    ipd->v3d,
                                    ipd->region,
                                    mval_fl,
                                    ipd->snap_gizmo,
                                    snap_to,
                                    plane_depth,
                                    plane_orient,
                                    plane_axis,
                                    plane_axis_auto,
                                    ipd->co_src,
                                    ipd->matrix_orient);

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

  ipd->is_snap_invert = ipd->snap_gizmo ? ED_gizmotypes_snap_3d_invert_snap_get(ipd->snap_gizmo) :
                                          false;
  {
    const ToolSettings *ts = ipd->scene->toolsettings;
    ipd->use_snap = (ipd->is_snap_invert == !(ts->snap_flag & SCE_SNAP));
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
         * however running from operator search will end up at this point.  */
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

  ED_region_draw_cb_exit(ipd->region->type, ipd->draw_handle_view);

  ED_region_tag_redraw(ipd->region);

  {
    wmGizmoGroup *gzgroup = idp_gizmogroup_from_region(ipd->region);
    if (gzgroup != NULL) {
      if (gzgroup->customdata != NULL) {
        preview_plane_cursor_visible_set(gzgroup, true);
      }
    }
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
          /* Always use default size here. */
          if (ipd->primitive_type == PLACE_PRIMITIVE_TYPE_CUBE) {
            RNA_float_set(&op_props, "size", 2.0f);
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
      if (ipd->snap_gizmo != NULL) {
        ED_gizmotypes_snap_3d_toggle_set(ipd->snap_gizmo, ipd->use_snap);
        if (ED_gizmotypes_snap_3d_update(ipd->snap_gizmo,
                                         CTX_data_ensure_evaluated_depsgraph(C),
                                         ipd->region,
                                         ipd->v3d,
                                         G_MAIN->wm.first,
                                         mval_fl,
                                         ipd->snap_co,
                                         NULL)) {
          ipd->is_snap_found = true;
        }
        ED_gizmotypes_snap_3d_toggle_clear(ipd->snap_gizmo);
      }
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

  /* Note, let the operator we call handle undo and registering itself. */
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
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "plane_axis_auto",
                         false,
                         "Auto Axis",
                         "Select the closest axis when placing objects "
                         "(surface overrides)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  static const EnumPropertyItem plane_depth_items[] = {
      {PLACE_DEPTH_SURFACE,
       "SURFACE",
       0,
       "Surface",
       "Start placing on the surface, using the 3D cursor position as a fallback"},
      {PLACE_DEPTH_CURSOR_PLANE,
       "CURSOR_PLANE",
       0,
       "Cursor Plane",
       "Start placement using a point projected onto the orientation axis "
       "at the 3D cursor position"},
      {PLACE_DEPTH_CURSOR_VIEW,
       "CURSOR_VIEW",
       0,
       "Cursor View",
       "Start placement using a point projected onto the view plane at the 3D cursor position"},
      {0, NULL, 0, NULL, NULL},
  };
  prop = RNA_def_property(ot->srna, "plane_depth", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Position", "The initial depth used when placing the cursor");
  RNA_def_property_enum_default(prop, PLACE_DEPTH_SURFACE);
  RNA_def_property_enum_items(prop, plane_depth_items);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  static const EnumPropertyItem plane_orientation_items[] = {
      {PLACE_ORIENT_SURFACE,
       "SURFACE",
       ICON_SNAP_NORMAL,
       "Surface",
       "Use the surface normal (using the transform orientation as a fallback)"},
      {PLACE_ORIENT_DEFAULT,
       "DEFAULT",
       ICON_ORIENTATION_GLOBAL,
       "Default",
       "Use the current transform orientation"},
      {0, NULL, 0, NULL, NULL},
  };
  prop = RNA_def_property(ot->srna, "plane_orientation", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Orientation", "The initial depth used when placing the cursor");
  RNA_def_property_enum_default(prop, PLACE_ORIENT_SURFACE);
  RNA_def_property_enum_items(prop, plane_orientation_items);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  static const EnumPropertyItem snap_to_items[] = {
      {PLACE_SNAP_TO_GEOMETRY, "GEOMETRY", 0, "Geometry", "Snap to all geometry"},
      {PLACE_SNAP_TO_DEFAULT, "DEFAULT", 0, "Default", "Use the current snap settings"},
      {0, NULL, 0, NULL, NULL},
  };
  prop = RNA_def_property(ot->srna, "snap_target", PROP_ENUM, PROP_NONE);
  RNA_def_property_ui_text(prop, "Snap to", "The target to use while snapping");
  RNA_def_property_enum_default(prop, PLACE_SNAP_TO_GEOMETRY);
  RNA_def_property_enum_items(prop, snap_to_items);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

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

static void WIDGETGROUP_placement_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
  wmGizmo *gizmo;

  {
    /* The gizmo snap has to be the first gizmo. */
    const wmGizmoType *gzt_snap;
    gzt_snap = WM_gizmotype_find("GIZMO_GT_snap_3d", true);
    gizmo = WM_gizmo_new_ptr(gzt_snap, gzgroup, NULL);

    WM_gizmo_set_color(gizmo, (float[4]){1.0f, 1.0f, 1.0f, 1.0f});

    /* Don't handle any events, this is for display only. */
    gizmo->flag |= WM_GIZMO_HIDDEN_KEYMAP;
  }

  /* Sets the gizmos custom-data which has it's own free callback. */
  preview_plane_cursor_setup(gzgroup);
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

/* -------------------------------------------------------------------- */
/** \name Placement Preview Plane
 *
 * Preview the plane that will be used for placement.
 *
 * Note that we might want to split this into its own file,
 * for now this is coupled with the 3D view placement gizmo.
 * \{ */

static void gizmo_plane_update_cursor(const bContext *C,
                                      ARegion *region,
                                      const int mval[2],
                                      float r_co[3],
                                      float r_matrix_orient[3][3],
                                      int *r_plane_axis)
{
  wmOperatorType *ot = WM_operatortype_find("VIEW3D_OT_interactive_add", true);
  BLI_assert(ot != NULL);
  PointerRNA ptr;

  ScrArea *area = CTX_wm_area(C);
  BLI_assert(region == CTX_wm_region(C));
  bToolRef *tref = area->runtime.tool;
  WM_toolsystem_ref_properties_ensure_from_operator(tref, ot, &ptr);

  const enum ePlace_SnapTo snap_to = RNA_enum_get(&ptr, "snap_target");
  const int plane_axis = RNA_enum_get(&ptr, "plane_axis");
  const bool plane_axis_auto = RNA_boolean_get(&ptr, "plane_axis_auto");
  const enum ePlace_Depth plane_depth = RNA_enum_get(&ptr, "plane_depth");
  const enum ePlace_Orient plane_orient = RNA_enum_get(&ptr, "plane_orientation");

  const float mval_fl[2] = {UNPACK2(mval)};

  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  /* Assign snap gizmo which is may be used as part of the tool. */
  wmGizmo *snap_gizmo = NULL;
  {
    wmGizmoGroup *gzgroup = idp_gizmogroup_from_region(region);
    if ((gzgroup != NULL) && gzgroup->gizmos.first) {
      snap_gizmo = gzgroup->gizmos.first;
    }
  }

  /* This ensures the snap gizmo has settings from this tool.
   * This function call could be moved a more appropriate place,
   * responding to the setting being changed for example,
   * however setting the value isn't expensive, so do it here.  */
  idp_snap_gizmo_update_snap_elements(scene, snap_to, snap_gizmo);

  view3d_interactive_add_calc_plane((bContext *)C,
                                    scene,
                                    v3d,
                                    region,
                                    mval_fl,
                                    snap_gizmo,
                                    snap_to,
                                    plane_depth,
                                    plane_orient,
                                    plane_axis,
                                    plane_axis_auto,
                                    r_co,
                                    r_matrix_orient);
  *r_plane_axis = plane_axis;
}

static void gizmo_plane_draw_grid(const int resolution,
                                  const float scale,
                                  const float scale_fade,
                                  const float matrix[4][4],
                                  const int plane_axis,
                                  const float color[4])
{
  BLI_assert(scale_fade <= scale);
  const int resolution_min = resolution - 1;
  float color_fade[4] = {UNPACK4(color)};
  const float *center = matrix[3];

  GPU_blend(GPU_BLEND_ADDITIVE);
  GPU_line_smooth(true);
  GPU_line_width(1.0f);

  GPUVertFormat *format = immVertexFormat();
  const uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  const uint col_id = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);

  const size_t coords_len = resolution * resolution;
  float(*coords)[3] = MEM_mallocN(sizeof(*coords) * coords_len, __func__);

  const int axis_x = (plane_axis + 0) % 3;
  const int axis_y = (plane_axis + 1) % 3;
  const int axis_z = (plane_axis + 2) % 3;

  int i;
  const float resolution_div = (float)1.0f / (float)resolution;
  i = 0;
  for (int x = 0; x < resolution; x++) {
    const float x_fl = (x * resolution_div) - 0.5f;
    for (int y = 0; y < resolution; y++) {
      const float y_fl = (y * resolution_div) - 0.5f;
      coords[i][axis_x] = 0.0f;
      coords[i][axis_y] = x_fl * scale;
      coords[i][axis_z] = y_fl * scale;
      mul_m4_v3(matrix, coords[i]);
      i += 1;
    }
  }
  BLI_assert(i == coords_len);
  immBeginAtMost(GPU_PRIM_LINES, coords_len * 4);
  i = 0;
  for (int x = 0; x < resolution_min; x++) {
    for (int y = 0; y < resolution_min; y++) {

      /* Add #resolution_div to ensure we fade-out entirely. */
#define FADE(v) \
  max_ff(0.0f, (1.0f - square_f(((len_v3v3(v, center) / scale_fade) + resolution_div) * 2.0f)))

      const float *v0 = coords[(resolution * x) + y];
      const float *v1 = coords[(resolution * (x + 1)) + y];
      const float *v2 = coords[(resolution * x) + (y + 1)];

      const float f0 = FADE(v0);
      const float f1 = FADE(v1);
      const float f2 = FADE(v2);

      if (f0 > 0.0f || f1 > 0.0f) {
        color_fade[3] = color[3] * f0;
        immAttr4fv(col_id, color_fade);
        immVertex3fv(pos_id, v0);
        color_fade[3] = color[3] * f1;
        immAttr4fv(col_id, color_fade);
        immVertex3fv(pos_id, v1);
      }
      if (f0 > 0.0f || f2 > 0.0f) {
        color_fade[3] = color[3] * f0;
        immAttr4fv(col_id, color_fade);
        immVertex3fv(pos_id, v0);

        color_fade[3] = color[3] * f2;
        immAttr4fv(col_id, color_fade);
        immVertex3fv(pos_id, v2);
      }

#undef FADE

      i++;
    }
  }

  MEM_freeN(coords);

  immEnd();

  immUnbindProgram();

  GPU_line_smooth(false);
  GPU_blend(GPU_BLEND_NONE);
}

/* -------------------------------------------------------------------- */
/** \name Preview Plane Cursor
 * \{ */

struct PlacementCursor {
  /**
   * Back-pointer to the gizmo-group that uses this cursor.
   * Needed so we know that the cursor belongs to the region.
   */
  wmGizmoGroup *gzgroup;

  /**
   * Enable this while the modal operator is running,
   * so the preview-plane doesn't show at the same time time as add-object preview shape
   * since it's distracting & not helpful.
   */
  bool do_draw;

  void *paintcursor;

  int plane_axis;
  float matrix[4][4];

  /* Check if we need to re-calculate the plane matrix. */
  int mval_prev[2];
  float persmat_prev[4][4];
};

static void cursor_plane_draw(bContext *C, int x, int y, void *customdata)
{
  struct PlacementCursor *plc = (struct PlacementCursor *)customdata;
  ARegion *region = CTX_wm_region(C);
  const RegionView3D *rv3d = region->regiondata;

  /* Early exit.
   * Note that we can't do most of these checks in the poll function (besides global checks)
   * so test them here instead.
   *
   * This cursor is only active while the gizmo is being used
   * so it's not so important to have a poll function. */
  if (plc->do_draw == false) {
    return;
  }
  if (G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT)) {
    return;
  }
  if (rv3d->rflag & RV3D_NAVIGATING) {
    return;
  }

  /* Check this gizmo group is in the region. */
  {
    wmGizmoMap *gzmap = region->gizmo_map;
    wmGizmoGroup *gzgroup_test = WM_gizmomap_group_find_ptr(gzmap, plc->gzgroup->type);
    if (gzgroup_test != plc->gzgroup) {
      /* Wrong viewport. */
      return;
    }
  }

  const int mval[2] = {x - region->winrct.xmin, y - region->winrct.ymin};

  /* Update matrix? */
  if ((plc->mval_prev[0] != mval[0]) || (plc->mval_prev[1] != mval[1]) ||
      !equals_m4m4(plc->persmat_prev, rv3d->persmat)) {
    plc->mval_prev[0] = mval[0];
    plc->mval_prev[1] = mval[1];

    float orient_matrix[3][3];
    float co[3];
    gizmo_plane_update_cursor(C, region, mval, co, orient_matrix, &plc->plane_axis);
    copy_m4_m3(plc->matrix, orient_matrix);
    copy_v3_v3(plc->matrix[3], co);

    copy_m4_m4(plc->persmat_prev, rv3d->persmat);
  }

  /* Draw */
  float pixel_size;

  if (rv3d->is_persp) {
    float center[3];
    negate_v3_v3(center, rv3d->ofs);
    pixel_size = ED_view3d_pixel_size(rv3d, center);
  }
  else {
    pixel_size = ED_view3d_pixel_size(rv3d, plc->matrix[3]);
  }

  if (pixel_size > FLT_EPSILON) {

    /* Arbitrary, 1.0 is a little too strong though. */
    float color_alpha = 0.75f;
    if (rv3d->is_persp) {
      /* Scale down the alpha when this is drawn very small,
       * since the add shader causes the small size to show too dense & bright. */
      const float relative_pixel_scale = pixel_size / ED_view3d_pixel_size(rv3d, plc->matrix[3]);
      if (relative_pixel_scale < 1.0f) {
        color_alpha *= max_ff(square_f(relative_pixel_scale), 0.3f);
      }
    }

    {
      /* Extra adjustment when it's near view-aligned as it seems overly bright. */
      float view_vector[3];
      ED_view3d_global_to_vector(rv3d, plc->matrix[3], view_vector);
      float view_dot = fabsf(dot_v3v3(plc->matrix[plc->plane_axis], view_vector));
      color_alpha *= max_ff(0.3f, 1.0f - square_f(square_f(1.0f - view_dot)));
    }

    /* Setup viewport & matrix. */
    wmViewport(&region->winrct);
    GPU_matrix_push_projection();
    GPU_matrix_push();
    GPU_matrix_projection_set(rv3d->winmat);
    GPU_matrix_set(rv3d->viewmat);

    const float scale_mod = U.gizmo_size * 2 * U.dpi_fac;

    float final_scale = (scale_mod * pixel_size);

    const int lines_subdiv = 10;
    int lines = lines_subdiv;

    float final_scale_fade = final_scale;
    final_scale = ceil_power_of_10(final_scale);

    float fac = final_scale_fade / final_scale;

    float color[4] = {1, 1, 1, color_alpha};
    color[3] *= square_f(1.0f - fac);
    if (color[3] > 0.0f) {
      gizmo_plane_draw_grid(lines * lines_subdiv,
                            final_scale,
                            final_scale_fade,
                            plc->matrix,
                            plc->plane_axis,
                            color);
    }

    color[3] = color_alpha;
    /* When the grid is large, we only need the 2x lines in the middle. */
    if (fac < 0.2f) {
      lines = 1;
      final_scale = final_scale_fade;
    }
    gizmo_plane_draw_grid(
        lines, final_scale, final_scale_fade, plc->matrix, plc->plane_axis, color);

    /* Restore matrix. */
    GPU_matrix_pop();
    GPU_matrix_pop_projection();
  }
}

static void preview_plane_cursor_free(void *customdata)
{
  struct PlacementCursor *plc = customdata;

  /* The window manager is freed first on exit. */
  wmWindowManager *wm = G_MAIN->wm.first;
  if (UNLIKELY(wm != NULL)) {
    WM_paint_cursor_end(plc->paintcursor);
  }
  MEM_freeN(plc);
}

static void preview_plane_cursor_setup(wmGizmoGroup *gzgroup)
{
  BLI_assert(gzgroup->customdata == NULL);
  struct PlacementCursor *plc = MEM_callocN(sizeof(*plc), __func__);
  plc->gzgroup = gzgroup;
  plc->paintcursor = WM_paint_cursor_activate(
      SPACE_VIEW3D, RGN_TYPE_WINDOW, NULL, cursor_plane_draw, plc);
  gzgroup->customdata = plc;
  gzgroup->customdata_free = preview_plane_cursor_free;

  preview_plane_cursor_visible_set(gzgroup, true);
}

static void preview_plane_cursor_visible_set(wmGizmoGroup *gzgroup, bool do_draw)
{
  struct PlacementCursor *plc = gzgroup->customdata;
  plc->do_draw = do_draw;
}

/** \} */
