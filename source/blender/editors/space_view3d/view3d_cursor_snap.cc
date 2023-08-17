/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \brief Snap cursor.
 */

#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "ED_screen.hh"
#include "ED_transform.hh"
#include "ED_transform_snap_object_context.hh"
#include "ED_view3d.hh"

#include "UI_resources.hh"

#include "RNA_access.hh"

#include "DEG_depsgraph_query.h"

#include "WM_api.hh"

#define STATE_INTERN_GET(state) \
\
  (SnapStateIntern *)((char *)state - offsetof(SnapStateIntern, snap_state))

struct SnapStateIntern {
  SnapStateIntern *next, *prev;
  V3DSnapCursorState snap_state;
};

struct SnapCursorDataIntern {
  V3DSnapCursorState state_default;
  ListBase state_intern;
  V3DSnapCursorData snap_data;

  SnapObjectContext *snap_context_v3d;
  const Scene *scene;
  eSnapMode snap_elem_hidden;

  float prevpoint_stack[3];

  /* Copy of the parameters of the last event state in order to detect updates. */
  struct {
    int x;
    int y;
#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
    uint8_t modifier;
#endif
  } last_eventstate;

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
  wmKeyMap *keymap;
  int snap_on;
#endif

  wmPaintCursor *handle;

  bool is_initiated;
};

static SnapCursorDataIntern g_data_intern = []() {
  SnapCursorDataIntern data{};
  data.state_default.flag = V3D_SNAPCURSOR_SNAP_EDIT_GEOM_FINAL;
  copy_v4_v4_uchar(data.state_default.target_color, blender::uchar4{255, 255, 255, 255});
  copy_v4_v4_uchar(data.state_default.source_color, blender::uchar4{255, 255, 255, 128});
  copy_v4_v4_uchar(data.state_default.color_box, blender::uchar4{255, 255, 255, 128});
  copy_v3_fl(data.state_default.box_dimensions, 1.0f);
  data.state_default.draw_point = true;
  return data;
}();

/**
 * Dot products below this will be considered view aligned.
 * In this case we can't usefully project the mouse cursor onto the plane.
 */
static const float eps_view_align = 1e-2f;

/**
 * Calculate a 3x3 orientation matrix from the surface under the cursor.
 */
static void v3d_cursor_poject_surface_normal(const float normal[3],
                                             const float obmat[4][4],
                                             float r_mat[3][3])
{
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
}

/**
 * Calculate 3D view incremental (grid) snapping.
 *
 * \note This could be moved to a public function.
 */
static bool v3d_cursor_snap_calc_incremental(
    Scene *scene, View3D *v3d, ARegion *region, const float co_relative[3], float co[3])
{
  const float grid_size = ED_view3d_grid_view_scale(scene, v3d, region, nullptr);
  if (UNLIKELY(grid_size == 0.0f)) {
    return false;
  }

  if (scene->toolsettings->snap_flag & SCE_SNAP_ABS_GRID) {
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

/**
 * Re-order \a mat so \a axis_align uses its own axis which is closest to \a v.
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

/* -------------------------------------------------------------------- */
/** \name Drawings
 * \{ */

static void v3d_cursor_plane_draw_grid(const int resolution,
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
  float(*coords)[3] = static_cast<float(*)[3]>(
      MEM_mallocN(sizeof(*coords) * coords_len, __func__));

  const int axis_x = (plane_axis + 0) % 3;
  const int axis_y = (plane_axis + 1) % 3;
  const int axis_z = (plane_axis + 2) % 3;

  int i;
  const float resolution_div = float(1.0f) / float(resolution);
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
  BLI_assert(i == int(coords_len));
  immBeginAtMost(GPU_PRIM_LINES, coords_len * 4);
  i = 0;
  for (int x = 0; x < resolution_min; x++) {
    for (int y = 0; y < resolution_min; y++) {

/* Add #resolution_div to ensure we fade-out entirely. */
#define FADE(v) \
\
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

static void v3d_cursor_plane_draw(const RegionView3D *rv3d,
                                  const int plane_axis,
                                  const float matrix[4][4])
{
  /* Draw */
  float pixel_size;

  if (rv3d->is_persp) {
    float center[3];
    negate_v3_v3(center, rv3d->ofs);
    pixel_size = ED_view3d_pixel_size(rv3d, center);
  }
  else {
    pixel_size = ED_view3d_pixel_size(rv3d, matrix[3]);
  }

  if (pixel_size > FLT_EPSILON) {

    /* Arbitrary, 1.0 is a little too strong though. */
    float color_alpha = 0.75f;
    if (rv3d->is_persp) {
      /* Scale down the alpha when this is drawn very small,
       * since the add shader causes the small size to show too dense & bright. */
      const float relative_pixel_scale = pixel_size / ED_view3d_pixel_size(rv3d, matrix[3]);
      if (relative_pixel_scale < 1.0f) {
        color_alpha *= max_ff(square_f(relative_pixel_scale), 0.3f);
      }
    }

    {
      /* Extra adjustment when it's near view-aligned as it seems overly bright. */
      float view_vector[3];
      ED_view3d_global_to_vector(rv3d, matrix[3], view_vector);
      float view_dot = fabsf(dot_v3v3(matrix[plane_axis], view_vector));
      color_alpha *= max_ff(0.3f, 1.0f - square_f(square_f(1.0f - view_dot)));
    }

    const float scale_mod = U.gizmo_size * 2 * UI_SCALE_FAC / U.pixelsize;

    float final_scale = (scale_mod * pixel_size);

    const int lines_subdiv = 10;
    int lines = lines_subdiv;

    float final_scale_fade = final_scale;
    final_scale = ceil_power_of_10(final_scale);

    float fac = final_scale_fade / final_scale;

    float color[4] = {1, 1, 1, color_alpha};
    color[3] *= square_f(1.0f - fac);
    if (color[3] > 0.0f) {
      v3d_cursor_plane_draw_grid(
          lines * lines_subdiv, final_scale, final_scale_fade, matrix, plane_axis, color);
    }

    color[3] = color_alpha;
    /* When the grid is large, we only need the 2x lines in the middle. */
    if (fac < 0.2f) {
      lines = 1;
      final_scale = final_scale_fade;
    }
    v3d_cursor_plane_draw_grid(lines, final_scale, final_scale_fade, matrix, plane_axis, color);
  }
}

static void cursor_box_draw(const float dimensions[3], uchar color[4])
{
  GPUVertFormat *format = immVertexFormat();
  const uint pos_id = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);
  GPU_line_width(1.0f);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4ubv(color);
  imm_draw_cube_corners_3d(pos_id, blender::float3{0.0f, 0.0f, dimensions[2]}, dimensions, 0.15f);
  immUnbindProgram();

  GPU_line_smooth(false);
  GPU_blend(GPU_BLEND_NONE);
}

void ED_view3d_cursor_snap_draw_util(RegionView3D *rv3d,
                                     const float source_loc[3],
                                     const float target_loc[3],
                                     const float target_normal[3],
                                     const uchar source_color[4],
                                     const uchar target_color[4],
                                     const eSnapMode target_type)
{
  if (!source_loc && !target_loc) {
    return;
  }

  float view_inv[4][4];
  copy_m4_m4(view_inv, rv3d->viewinv);

  /* The size of the circle is larger than the vertex size.
   * This prevents a drawing overlaps the other. */
  float radius = 2.5f * UI_GetThemeValuef(TH_VERTEX_SIZE);
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  if (target_loc) {
    immUniformColor4ubv(target_color);
    imm_drawcircball(target_loc, ED_view3d_pixel_size(rv3d, target_loc) * radius, view_inv, pos);

    /* draw normal if needed */
    if (target_normal) {
      immBegin(GPU_PRIM_LINES, 2);
      immVertex3fv(pos, target_loc);
      immVertex3f(pos,
                  target_loc[0] + target_normal[0],
                  target_loc[1] + target_normal[1],
                  target_loc[2] + target_normal[2]);
      immEnd();
    }
  }

  if (source_loc) {
    /* Draw an "X" indicating where the previous snap point is.
     * This is useful for indicating perpendicular snap. */

    /* v1, v2, v3 and v4 indicate the coordinates of the ends of the "X". */
    float vx[3], vy[3], v1[3], v2[3], v3[3], v4[4];

    /* Multiply by 0.75f so that the final size of the "X" is close to that of
     * the circle.
     * (A closer value is 0.7071f, but we don't need to be exact here). */
    float x_size = 0.75f * radius * ED_view3d_pixel_size(rv3d, source_loc);

    mul_v3_v3fl(vx, view_inv[0], x_size);
    mul_v3_v3fl(vy, view_inv[1], x_size);

    add_v3_v3v3(v1, vx, vy);
    sub_v3_v3v3(v2, vx, vy);
    negate_v3_v3(v3, v1);
    negate_v3_v3(v4, v2);

    add_v3_v3(v1, source_loc);
    add_v3_v3(v2, source_loc);
    add_v3_v3(v3, source_loc);
    add_v3_v3(v4, source_loc);

    immUniformColor4ubv(source_color);
    immBegin(GPU_PRIM_LINES, 4);
    immVertex3fv(pos, v3);
    immVertex3fv(pos, v1);
    immVertex3fv(pos, v4);
    immVertex3fv(pos, v2);
    immEnd();

    if (target_loc && (target_type & SCE_SNAP_TO_EDGE_PERPENDICULAR)) {
      /* Dashed line. */
      immUnbindProgram();

      immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);
      float viewport_size[4];
      GPU_viewport_size_get_f(viewport_size);
      immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);
      immUniform1f("dash_width", 6.0f * U.pixelsize);
      immUniform1f("udash_factor", 1.0f / 4.0f);
      immUniformColor4ubv(source_color);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex3fv(pos, source_loc);
      immVertex3fv(pos, target_loc);
      immEnd();
    }
  }

  immUnbindProgram();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Event State
 * \{ */

/* Checks if the current event is different from the one captured in the last update. */
static bool v3d_cursor_eventstate_has_changed(SnapCursorDataIntern *data_intern,
                                              V3DSnapCursorState *state,
                                              const wmWindowManager *wm,
                                              const int x,
                                              const int y)
{
  if (wm && wm->winactive) {
    const wmEvent *event = wm->winactive->eventstate;
    if ((x != data_intern->last_eventstate.x) || (y != data_intern->last_eventstate.y)) {
      return true;
    }
#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
    if (!(state && (state->flag & V3D_SNAPCURSOR_TOGGLE_ALWAYS_TRUE))) {
      if (event->modifier != data_intern->last_eventstate.modifier) {
        return true;
      }
    }
#endif
  }
  return false;
}

/* Copies the current eventstate. */
static void v3d_cursor_eventstate_save_xy(SnapCursorDataIntern *cursor_snap,
                                          const int x,
                                          const int y)
{
  cursor_snap->last_eventstate.x = x;
  cursor_snap->last_eventstate.y = y;
}

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
static void v3d_cursor_eventstate_save_modifier(SnapCursorDataIntern *data_intern,
                                                const wmWindowManager *wm)
{
  if (!wm || !wm->winactive) {
    return;
  }
  const wmEvent *event = wm->winactive->eventstate;
  data_intern->last_eventstate.modifier = event->modifier;
}

static bool v3d_cursor_is_snap_invert(SnapCursorDataIntern *data_intern, const wmWindowManager *wm)
{
  if (!wm || !wm->winactive) {
    return false;
  }

  const wmEvent *event = wm->winactive->eventstate;
  if (event->modifier == data_intern->last_eventstate.modifier) {
    /* Nothing has changed. */
    return data_intern->snap_data.is_snap_invert;
  }

  /* Save new eventstate. */
  data_intern->last_eventstate.modifier = event->modifier;

  const int snap_on = data_intern->snap_on;

  wmKeyMap *keymap = WM_keymap_active(wm, data_intern->keymap);
  LISTBASE_FOREACH (const wmKeyMapItem *, kmi, &keymap->items) {
    if (kmi->flag & KMI_INACTIVE) {
      continue;
    }

    if (kmi->propvalue == snap_on) {
      if ((ELEM(kmi->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY) && (event->modifier & KM_CTRL)) ||
          (ELEM(kmi->type, EVT_LEFTSHIFTKEY, EVT_RIGHTSHIFTKEY) && (event->modifier & KM_SHIFT)) ||
          (ELEM(kmi->type, EVT_LEFTALTKEY, EVT_RIGHTALTKEY) && (event->modifier & KM_ALT)) ||
          ((kmi->type == EVT_OSKEY) && (event->modifier & KM_OSKEY)))
      {
        return true;
      }
    }
  }
  return false;
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update
 * \{ */

static eSnapMode v3d_cursor_snap_elements(ToolSettings *tool_settings)
{
  return eSnapMode(tool_settings->snap_mode_tools == SCE_SNAP_TO_NONE ?
                       tool_settings->snap_mode :
                       tool_settings->snap_mode_tools);
}

static void v3d_cursor_snap_context_ensure(Scene *scene)
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  if (data_intern->snap_context_v3d && (data_intern->scene != scene)) {
    ED_transform_snap_object_context_destroy(data_intern->snap_context_v3d);
    data_intern->snap_context_v3d = nullptr;
  }
  if (data_intern->snap_context_v3d == nullptr) {
    data_intern->snap_context_v3d = ED_transform_snap_object_context_create(scene, 0);
    data_intern->scene = scene;
  }
}

static bool v3d_cursor_snap_calc_plane()
{
  /* If any of the states require the plane, calculate the `plane_omat`. */
  LISTBASE_FOREACH (SnapStateIntern *, state, &g_data_intern.state_intern) {
    if (state->snap_state.draw_plane || state->snap_state.draw_box) {
      return true;
    }
  }
  return false;
}

static void v3d_cursor_snap_update(V3DSnapCursorState *state,
                                   const bContext *C,
                                   wmWindowManager *wm,
                                   Depsgraph *depsgraph,
                                   Scene *scene,
                                   ARegion *region,
                                   View3D *v3d,
                                   int x,
                                   int y)
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  V3DSnapCursorData *snap_data = &data_intern->snap_data;
  ToolSettings *tool_settings = scene->toolsettings;

  eSnapMode snap_elements = v3d_cursor_snap_elements(tool_settings);
  const bool calc_plane_omat = v3d_cursor_snap_calc_plane();

  snap_data->is_enabled = true;
  if (!(state->flag & V3D_SNAPCURSOR_TOGGLE_ALWAYS_TRUE)) {
#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
    snap_data->is_snap_invert = v3d_cursor_is_snap_invert(data_intern, wm);
#endif

    if (snap_data->is_snap_invert != !(tool_settings->snap_flag & SCE_SNAP)) {
      snap_data->is_enabled = false;
      if (!calc_plane_omat) {
        snap_data->snap_elem = SCE_SNAP_TO_NONE;
        return;
      }
      snap_elements = SCE_SNAP_TO_NONE;
    }
  }

  const bool use_surface_nor = tool_settings->plane_orient == V3D_PLACE_ORIENT_SURFACE;
  const bool use_surface_co = snap_data->is_enabled || tool_settings->plane_depth == V3D_PLACE_DEPTH_SURFACE;

  float co[3], no[3], face_nor[3], obmat[4][4], omat[3][3];
  eSnapMode snap_elem = SCE_SNAP_TO_NONE;
  int snap_elem_index[3] = {-1, -1, -1};
  int index = -1;

  const float mval_fl[2] = {float(x), float(y)};
  zero_v3(no);
  zero_v3(face_nor);
  unit_m3(omat);

  if (use_surface_nor || use_surface_co) {
    v3d_cursor_snap_context_ensure(scene);

    data_intern->snap_elem_hidden = SCE_SNAP_TO_NONE;
    if (calc_plane_omat && !(snap_elements & SCE_SNAP_TO_FACE)) {
      data_intern->snap_elem_hidden = SCE_SNAP_TO_FACE;
      snap_elements |= SCE_SNAP_TO_FACE;
    }

    if (snap_elements & SCE_SNAP_TO_GEOM) {
      float prev_co[3] = {0.0f};
      if (state->prevpoint) {
        copy_v3_v3(prev_co, state->prevpoint);
      }
      else {
        snap_elements &= ~SCE_SNAP_TO_EDGE_PERPENDICULAR;
      }

      eSnapEditType edit_mode_type = (state->flag & V3D_SNAPCURSOR_SNAP_EDIT_GEOM_FINAL) ?
                                         SNAP_GEOM_FINAL :
                                     (state->flag & V3D_SNAPCURSOR_SNAP_EDIT_GEOM_CAGE) ?
                                         SNAP_GEOM_CAGE :
                                         SNAP_GEOM_EDIT;

      bool use_occlusion_test = (state->flag & V3D_SNAPCURSOR_OCCLUSION_ALWAYS_TRUE) ? false :
                                                                                       true;

      float dist_px = 12.0f * U.pixelsize;

      SnapObjectParams params{};
      params.snap_target_select = SCE_SNAP_TARGET_ALL;
      params.edit_mode_type = edit_mode_type;
      params.use_occlusion_test = use_occlusion_test;
      snap_elem = ED_transform_snap_object_project_view3d_ex(data_intern->snap_context_v3d,
                                                             depsgraph,
                                                             region,
                                                             v3d,
                                                             snap_elements,
                                                             &params,
                                                             nullptr,
                                                             mval_fl,
                                                             prev_co,
                                                             &dist_px,
                                                             co,
                                                             no,
                                                             &index,
                                                             nullptr,
                                                             obmat,
                                                             face_nor);
    }
  }
#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
  else {
    v3d_cursor_eventstate_save_modifier(data_intern, wm);
  }
#endif

  if (calc_plane_omat) {
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
    bool orient_surface = use_surface_nor && (snap_elem != SCE_SNAP_TO_NONE);
    if (orient_surface) {
      copy_m3_m4(omat, obmat);
    }
    else {
      ViewLayer *view_layer = CTX_data_view_layer(C);
      BKE_view_layer_synced_ensure(CTX_data_scene(C), view_layer);
      Object *ob = BKE_view_layer_active_object_get(view_layer);
      const int orient_index = BKE_scene_orientation_get_index(scene, SCE_ORIENT_DEFAULT);
      const int pivot_point = scene->toolsettings->transform_pivot_point;
      ED_transform_calc_orientation_from_type_ex(
          scene, view_layer, v3d, rv3d, ob, nullptr, orient_index, pivot_point, omat);

      if (tool_settings->use_plane_axis_auto) {
        mat3_align_axis_to_v3(omat, tool_settings->plane_axis, rv3d->viewinv[2]);
      }
    }

    /* Non-orthogonal matrices cause the preview and final result not to match.
     *
     * While making orthogonal doesn't always work well (especially with gimbal orientation for
     * e.g.) it's a corner case, without better alternatives as objects don't support shear. */
    orthogonalize_m3(omat, tool_settings->plane_axis);

    if (orient_surface) {
      if (!is_zero_v3(face_nor)) {
        /* Negate the face normal according to the view. */
        float ray_dir[3];
        if (rv3d->is_persp) {
          BLI_assert_msg(snap_elem != SCE_SNAP_TO_NONE,
                         "Use of variable `co` without it being computed");

          sub_v3_v3v3(ray_dir, co, rv3d->viewinv[3]); /* No need to normalize. */
        }
        else {
          negate_v3_v3(ray_dir, rv3d->viewinv[2]);
        }

        if (dot_v3v3(ray_dir, face_nor) >= 0.0f) {
          negate_v3(face_nor);
        }
      }
      else if (!is_zero_v3(no)) {
        copy_v3_v3(face_nor, no);
      }
      else {
        face_nor[tool_settings->plane_axis] = 1.0f;
      }
      v3d_cursor_poject_surface_normal(face_nor, obmat, omat);
    }
  }

  if (!use_surface_co) {
    snap_elem = SCE_SNAP_TO_NONE;
  }

  float *co_depth = (snap_elem != SCE_SNAP_TO_NONE) ? co : scene->cursor.location;
  snap_elem &= ~data_intern->snap_elem_hidden;
  if (snap_elem == SCE_SNAP_TO_NONE) {
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
    const float *plane_normal = omat[tool_settings->plane_axis];
    bool do_plane_isect = (tool_settings->plane_depth != V3D_PLACE_DEPTH_CURSOR_VIEW) &&
                          (rv3d->is_persp ||
                           (fabsf(dot_v3v3(plane_normal, rv3d->viewinv[2])) > eps_view_align));

    if (do_plane_isect) {
      float plane[4];
      plane_from_point_normal_v3(plane, co_depth, plane_normal);
      do_plane_isect = ED_view3d_win_to_3d_on_plane(region, plane, mval_fl, rv3d->is_persp, co);
    }

    if (!do_plane_isect) {
      ED_view3d_win_to_3d(v3d, region, co_depth, mval_fl, co);
    }

    if (snap_data->is_enabled && (snap_elements & SCE_SNAP_TO_INCREMENT)) {
      v3d_cursor_snap_calc_incremental(scene, v3d, region, state->prevpoint, co);
    }
  }
  else if (snap_elem == SCE_SNAP_TO_VERTEX) {
    snap_elem_index[0] = index;
  }
  else if (snap_elem &
           (SCE_SNAP_TO_EDGE | SCE_SNAP_TO_EDGE_MIDPOINT | SCE_SNAP_TO_EDGE_PERPENDICULAR))
  {
    snap_elem_index[1] = index;
  }
  else if (snap_elem == SCE_SNAP_TO_FACE) {
    snap_elem_index[2] = index;
  }

  snap_data->snap_elem = snap_elem;
  copy_v3_v3(snap_data->loc, co);
  copy_v3_v3(snap_data->nor, no);
  copy_m4_m4(snap_data->obmat, obmat);
  copy_v3_v3_int(snap_data->elem_index, snap_elem_index);

  copy_m3_m3(snap_data->plane_omat, omat);

  v3d_cursor_eventstate_save_xy(data_intern, x, y);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks
 * \{ */

static bool v3d_cursor_snap_poll_fn(bContext *C)
{
  if (G.moving) {
    return false;
  }

  ScrArea *area = CTX_wm_area(C);
  if (area->spacetype != SPACE_VIEW3D) {
    return false;
  }

  ARegion *region = CTX_wm_region(C);
  if (region->regiontype != RGN_TYPE_WINDOW) {
    if (!region->overlap) {
      return false;
    }
    /* Sometimes the cursor may be on an invisible part of an overlapping region. */
    const wmWindowManager *wm = CTX_wm_manager(C);
    const wmEvent *event = wm->winactive->eventstate;
    if (ED_region_overlap_isect_xy(region, event->xy)) {
      return false;
    }
    /* Find the visible region under the cursor.
     * TODO(Germano): Shouldn't this be the region in context? */
    region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  }

  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  if (rv3d->rflag & RV3D_NAVIGATING) {
    /* Don't draw the cursor while navigating. It can be distracting. */
    return false;
  };

  /* Call this callback last and don't reuse the `state` as the caller can free the cursor. */
  V3DSnapCursorState *state = ED_view3d_cursor_snap_state_active_get();
  if (state->poll && !state->poll(region, state->poll_data)) {
    return false;
  }

  return true;
}

static void v3d_cursor_snap_draw_fn(bContext *C, int x, int y, void * /*customdata*/)
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  V3DSnapCursorState *state = ED_view3d_cursor_snap_state_active_get();
  V3DSnapCursorData *snap_data = &data_intern->snap_data;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = DEG_get_input_scene(depsgraph);

  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  x -= region->winrct.xmin;
  y -= region->winrct.ymin;
  if (v3d_cursor_eventstate_has_changed(data_intern, state, wm, x, y)) {
    View3D *v3d = CTX_wm_view3d(C);
    v3d_cursor_snap_update(state, C, wm, depsgraph, scene, region, v3d, x, y);
  }

  const bool draw_plane = state->draw_plane || state->draw_box;
  if (snap_data->snap_elem == SCE_SNAP_TO_NONE && !draw_plane) {
    return;
  }

  /* Setup viewport & matrix. */
  RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
  wmViewport(&region->winrct);
  GPU_matrix_projection_set(rv3d->winmat);
  GPU_matrix_set(rv3d->viewmat);

  float matrix[4][4];
  if (draw_plane) {
    copy_m4_m3(matrix, snap_data->plane_omat);
    copy_v3_v3(matrix[3], snap_data->loc);

    v3d_cursor_plane_draw(rv3d, scene->toolsettings->plane_axis, matrix);
  }

  if (snap_data->snap_elem != SCE_SNAP_TO_NONE && (state->draw_point || state->draw_box)) {
    const float *prev_point = (snap_data->snap_elem & SCE_SNAP_TO_EDGE_PERPENDICULAR) ?
                                  state->prevpoint :
                                  nullptr;

    GPU_line_smooth(false);
    GPU_line_width(1.0f);

    ED_view3d_cursor_snap_draw_util(rv3d,
                                    prev_point,
                                    snap_data->loc,
                                    nullptr,
                                    state->source_color,
                                    state->target_color,
                                    snap_data->snap_elem);
  }

  if (state->draw_box) {
    GPU_matrix_mul(matrix);
    cursor_box_draw(state->box_dimensions, state->color_box);
  }

  /* Restore matrix. */
  wmWindowViewport(CTX_wm_window(C));
}

/** \} */

V3DSnapCursorState *ED_view3d_cursor_snap_state_active_get()
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  if (BLI_listbase_is_empty(&data_intern->state_intern)) {
    return &g_data_intern.state_default;
  }
  return &((SnapStateIntern *)data_intern->state_intern.last)->snap_state;
}

void ED_view3d_cursor_snap_state_active_set(V3DSnapCursorState *state)
{
  if (state == &g_data_intern.state_default) {
    BLI_assert_unreachable();
    return;
  }

  SnapStateIntern *state_intern = STATE_INTERN_GET(state);
  if (state_intern == (SnapStateIntern *)g_data_intern.state_intern.last) {
    return;
  }

  if (!BLI_remlink_safe(&g_data_intern.state_intern, state_intern)) {
    BLI_assert_unreachable();
    return;
  }

  BLI_addtail(&g_data_intern.state_intern, state_intern);
}

static void v3d_cursor_snap_activate()
{
  SnapCursorDataIntern *data_intern = &g_data_intern;

  if (!data_intern->handle) {
    if (!data_intern->is_initiated) {
      /* Only initiate intern data once.
       * TODO: ED_view3d_cursor_snap_init */

#ifdef USE_SNAP_DETECT_FROM_KEYMAP_HACK
      wmKeyConfig *keyconf = ((wmWindowManager *)G.main->wm.first)->defaultconf;

      data_intern->keymap = WM_modalkeymap_find(keyconf, "Generic Gizmo Tweak Modal Map");
      RNA_enum_value_from_id(
          static_cast<const EnumPropertyItem *>(data_intern->keymap->modal_items),
          "SNAP_ON",
          &data_intern->snap_on);
#endif
      data_intern->is_initiated = true;
    }

    wmPaintCursor *pc = WM_paint_cursor_activate(
        SPACE_VIEW3D, RGN_TYPE_WINDOW, v3d_cursor_snap_poll_fn, v3d_cursor_snap_draw_fn, nullptr);
    data_intern->handle = pc;
  }
}

static void v3d_cursor_snap_free()
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  if (data_intern->handle) {
    if (G_MAIN->wm.first) {
      WM_paint_cursor_end(data_intern->handle);
    }
    data_intern->handle = nullptr;
  }
  if (data_intern->snap_context_v3d) {
    ED_transform_snap_object_context_destroy(data_intern->snap_context_v3d);
    data_intern->snap_context_v3d = nullptr;
  }
}

void ED_view3d_cursor_snap_state_default_set(V3DSnapCursorState *state)
{
  g_data_intern.state_default = *state;

  /* These values are temporarily set by the tool.
   * They are not convenient as default values.
   * So reset to null. */
  g_data_intern.state_default.prevpoint = nullptr;
  g_data_intern.state_default.draw_plane = false;
  g_data_intern.state_default.draw_box = false;
  g_data_intern.state_default.poll = nullptr;
  g_data_intern.state_default.poll_data = nullptr;
}

V3DSnapCursorState *ED_view3d_cursor_snap_state_create()
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  if (!data_intern->handle) {
    v3d_cursor_snap_activate();
  }

  SnapStateIntern *state_intern = static_cast<SnapStateIntern *>(
      MEM_mallocN(sizeof(*state_intern), __func__));
  state_intern->snap_state = g_data_intern.state_default;
  BLI_addtail(&g_data_intern.state_intern, state_intern);

  return (V3DSnapCursorState *)&state_intern->snap_state;
}

void ED_view3d_cursor_snap_state_free(V3DSnapCursorState *state)
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  if (BLI_listbase_is_empty(&data_intern->state_intern)) {
    return;
  }

  SnapStateIntern *state_intern = STATE_INTERN_GET(state);
  BLI_remlink(&data_intern->state_intern, state_intern);
  MEM_freeN(state_intern);
  if (BLI_listbase_is_empty(&data_intern->state_intern)) {
    v3d_cursor_snap_free();
  }
}

void ED_view3d_cursor_snap_state_prevpoint_set(V3DSnapCursorState *state,
                                               const float prev_point[3])
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  if (!state) {
    state = ED_view3d_cursor_snap_state_active_get();
  }
  if (prev_point) {
    copy_v3_v3(data_intern->prevpoint_stack, prev_point);
    state->prevpoint = data_intern->prevpoint_stack;
  }
  else {
    state->prevpoint = nullptr;
  }
}

void ED_view3d_cursor_snap_data_update(V3DSnapCursorState *state,
                                       const bContext *C,
                                       const int x,
                                       const int y)
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  wmWindowManager *wm = CTX_wm_manager(C);
  if (v3d_cursor_eventstate_has_changed(data_intern, state, wm, x, y)) {
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    Scene *scene = DEG_get_input_scene(depsgraph);
    ScrArea *area = CTX_wm_area(C);
    ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
    View3D *v3d = CTX_wm_view3d(C);

    if (!state) {
      state = ED_view3d_cursor_snap_state_active_get();
    }
    v3d_cursor_snap_update(state, C, wm, depsgraph, scene, region, v3d, x, y);
  }
}

V3DSnapCursorData *ED_view3d_cursor_snap_data_get()
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  return &data_intern->snap_data;
}

SnapObjectContext *ED_view3d_cursor_snap_context_ensure(Scene *scene)
{
  SnapCursorDataIntern *data_intern = &g_data_intern;
  v3d_cursor_snap_context_ensure(scene);
  return data_intern->snap_context_v3d;
}
