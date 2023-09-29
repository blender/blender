/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Window-Manager XR Drawing
 *
 * Implements Blender specific drawing functionality for use with the Ghost-XR API.
 */

#include <string.h>

#include "BKE_context.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "ED_view3d_offscreen.h"

#include "GHOST_C-api.h"
#include "GHOST_Types.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "GPU_viewport.h"

#include "WM_api.h"

#include "wm_surface.h"
#include "wm_xr_intern.h"
typedef struct GHOST_XrPose;

void wm_xr_pose_to_mat(const GHOST_XrPose *pose, float r_mat[4][4])
{
  quat_to_mat4(r_mat, pose->orientation_quat);
  copy_v3_v3(r_mat[3], pose->position);
}

void wm_xr_pose_scale_to_mat(const GHOST_XrPose *pose, float scale, float r_mat[4][4])
{
  wm_xr_pose_to_mat(pose, r_mat);

  BLI_assert(scale > 0.0f);
  mul_v3_fl(r_mat[0], scale);
  mul_v3_fl(r_mat[1], scale);
  mul_v3_fl(r_mat[2], scale);
}

void wm_xr_pose_to_imat(const GHOST_XrPose *pose, float r_imat[4][4])
{
  float iquat[4];
  invert_qt_qt_normalized(iquat, pose->orientation_quat);
  quat_to_mat4(r_imat, iquat);
  translate_m4(r_imat, -pose->position[0], -pose->position[1], -pose->position[2]);
}

void wm_xr_pose_scale_to_imat(const GHOST_XrPose *pose, float scale, float r_imat[4][4])
{
  float iquat[4];
  invert_qt_qt_normalized(iquat, pose->orientation_quat);
  quat_to_mat4(r_imat, iquat);

  BLI_assert(scale > 0.0f);
  scale = 1.0f / scale;
  mul_v3_fl(r_imat[0], scale);
  mul_v3_fl(r_imat[1], scale);
  mul_v3_fl(r_imat[2], scale);

  translate_m4(r_imat, -pose->position[0], -pose->position[1], -pose->position[2]);
}

static void wm_xr_draw_matrices_create(const wmXrDrawData *draw_data,
                                       const GHOST_XrDrawViewInfo *draw_view,
                                       const XrSessionSettings *session_settings,
                                       const wmXrSessionState *session_state,
                                       float r_viewmat[4][4],
                                       float r_projmat[4][4])
{
  GHOST_XrPose eye_pose;
  float eye_inv[4][4], base_inv[4][4], nav_inv[4][4], m[4][4];

  /* Calculate inverse eye matrix. */
  copy_qt_qt(eye_pose.orientation_quat, draw_view->eye_pose.orientation_quat);
  copy_v3_v3(eye_pose.position, draw_view->eye_pose.position);
  if ((session_settings->flag & XR_SESSION_USE_POSITION_TRACKING) == 0) {
    sub_v3_v3(eye_pose.position, draw_view->local_pose.position);
  }
  if ((session_settings->flag & XR_SESSION_USE_ABSOLUTE_TRACKING) == 0) {
    sub_v3_v3(eye_pose.position, draw_data->eye_position_ofs);
  }

  wm_xr_pose_to_imat(&eye_pose, eye_inv);

  /* Apply base pose and navigation. */
  wm_xr_pose_scale_to_imat(&draw_data->base_pose, draw_data->base_scale, base_inv);
  wm_xr_pose_scale_to_imat(&session_state->nav_pose_prev, session_state->nav_scale_prev, nav_inv);

  float viewer_offset[4][4];
  unit_m4(viewer_offset);
  rotate_m4(viewer_offset, 'Y', -session_settings->viewer_angle_offset);
  copy_v3_v3(viewer_offset[3], session_settings->viewer_offset);

  mul_m4_m4m4(m, eye_inv, viewer_offset);
  mul_m4_m4_post(m, base_inv);
  mul_m4_m4m4(r_viewmat, m, nav_inv);

  perspective_m4_fov(r_projmat,
                     draw_view->fov.angle_left,
                     draw_view->fov.angle_right,
                     draw_view->fov.angle_up,
                     draw_view->fov.angle_down,
                     session_settings->clip_start,
                     session_settings->clip_end);
}

static void wm_xr_draw_viewport_buffers_to_active_framebuffer(
    const wmXrRuntimeData *runtime_data,
    const wmXrSurfaceData *surface_data,
    const GHOST_XrDrawViewInfo *draw_view)
{
  const wmXrViewportPair *vp = BLI_findlink(&surface_data->viewports, draw_view->view_idx);
  BLI_assert(vp && vp->viewport);

  const bool is_upside_down = GHOST_XrSessionNeedsUpsideDownDrawing(runtime_data->context);
  rcti rect = {.xmin = 0, .ymin = 0, .xmax = draw_view->width - 1, .ymax = draw_view->height - 1};

  wmViewport(&rect);

  /* For upside down contexts, draw with inverted y-values. */
  if (is_upside_down) {
    SWAP(int, rect.ymin, rect.ymax);
  }
  GPU_viewport_draw_to_screen_ex(vp->viewport, 0, &rect, draw_view->expects_srgb_buffer, true);
}

void wm_xr_draw_view(const GHOST_XrDrawViewInfo *draw_view, void *customdata)
{
  wmXrDrawData *draw_data = customdata;
  wmXrData *xr_data = draw_data->xr_data;
  wmXrSurfaceData *surface_data = draw_data->surface_data;
  wmXrSessionState *session_state = &xr_data->runtime->session_state;
  XrSessionSettings *settings = &xr_data->session_settings;

  // GG: using 0: shows everything: helper lines (knife cut op), wireframes bones, xform gizmos,
  // etc
  // const int display_flags = ~0;
  const int display_flags = V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS | settings->draw_flags;

  float viewmat[4][4], winmat[4][4];

  BLI_assert(WM_xr_session_is_ready(xr_data));

  wm_xr_session_draw_data_update(session_state, settings, draw_view, draw_data);
  wm_xr_draw_matrices_create(draw_data, draw_view, settings, session_state, viewmat, winmat);
  wm_xr_session_state_update(settings, draw_data, draw_view, session_state);

  if (!wm_xr_session_surface_offscreen_ensure(surface_data, draw_view)) {
    return;
  }

  const wmXrViewportPair *vp = BLI_findlink(&surface_data->viewports, draw_view->view_idx);
  BLI_assert(vp && vp->offscreen && vp->viewport);

  /* In case a framebuffer is still bound from drawing the last eye. */
  GPU_framebuffer_restore();
  /* Some systems have drawing glitches without this. */
  GPU_clear_depth(1.0f);

  // GG: I feel like maybe drawing the BBlender UI can just happen around here and be optional?
  /* Draws the view into the surface_data->viewport's frame-buffers.  Though, perhaps we dont have
   * access to window data?*/
  // GG: maybe we need to set CTX_wm_region_set() to use our fake v3d? maybe thats why tools aren't
  // being drawn in XR? (see editmesh_loopcut.c/search for REGION_DRAW_POST_VIEW They just attach a
  // cb to the active region. Since we don't see it, then maybe the region isn't being set? or,
  // maybe its as simple as this offscreen not calling callbacks)

  ED_view3d_draw_offscreen_simple(draw_data->depsgraph,
                                  draw_data->scene,
                                  &settings->shading,
                                  (eDrawType)settings->shading.type,
                                  settings->object_type_exclude_viewport,
                                  settings->object_type_exclude_select,
                                  draw_view->width,
                                  draw_view->height,
                                  display_flags,
                                  viewmat,
                                  winmat,
                                  settings->clip_start,
                                  settings->clip_end,
                                  true,
                                  false,
                                  true,
                                  NULL,
                                  false,
                                  vp->offscreen,
                                  vp->viewport,
                                  xr_data->evil_C);

  /* The draw-manager uses both GPUOffscreen and GPUViewport to manage frame and texture buffers. A
   * call to GPU_viewport_draw_to_screen() is still needed to get the final result from the
   * viewport buffers composited together and potentially color managed for display on screen.
   * It needs a bound frame-buffer to draw into, for which we simply reuse the GPUOffscreen one.
   *
   * In a next step, Ghost-XR will use the currently bound frame-buffer to retrieve the image
   * to be submitted to the OpenXR swap-chain. So do not un-bind the off-screen yet! */

  GPU_offscreen_bind(vp->offscreen, false);

  wm_xr_draw_viewport_buffers_to_active_framebuffer(xr_data->runtime, surface_data, draw_view);
}

static GPUBatch *wm_xr_controller_model_batch_create(GHOST_XrContextHandle xr_context,
                                                     const char *subaction_path)
{
  GHOST_XrControllerModelData model_data;

  if (!GHOST_XrGetControllerModelData(xr_context, subaction_path, &model_data) ||
      model_data.count_vertices < 1)
  {
    return NULL;
  }

  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, model_data.count_vertices);
  void *vbo_data = GPU_vertbuf_get_data(vbo);
  memcpy(
      vbo_data, model_data.vertices, model_data.count_vertices * sizeof(model_data.vertices[0]));

  GPUIndexBuf *ibo = NULL;
  if (model_data.count_indices > 0 && ((model_data.count_indices % 3) == 0)) {
    GPUIndexBufBuilder ibo_builder;
    const uint prim_len = model_data.count_indices / 3;
    GPU_indexbuf_init(&ibo_builder, GPU_PRIM_TRIS, prim_len, model_data.count_vertices);
    for (uint i = 0; i < prim_len; ++i) {
      const uint32_t *idx = &model_data.indices[i * 3];
      GPU_indexbuf_add_tri_verts(&ibo_builder, idx[0], idx[1], idx[2]);
    }
    ibo = GPU_indexbuf_build(&ibo_builder);
  }

  return GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, ibo, GPU_BATCH_OWNS_VBO | GPU_BATCH_OWNS_INDEX);
}

static void wm_xr_controller_model_draw(const XrSessionSettings *settings,
                                        GHOST_XrContextHandle xr_context,
                                        wmXrSessionState *state)
{

  GHOST_XrControllerModelData model_data;

  float color[4];
  switch (settings->controller_draw_style) {
    case XR_CONTROLLER_DRAW_DARK:
    case XR_CONTROLLER_DRAW_DARK_RAY:
      color[0] = color[1] = color[2] = 0.0f;
      color[3] = 0.4f;
      break;
    case XR_CONTROLLER_DRAW_LIGHT:
    case XR_CONTROLLER_DRAW_LIGHT_RAY:
      color[0] = 0.422f;
      color[1] = 0.438f;
      color[2] = 0.446f;
      color[3] = 0.4f;
      break;
  }

  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_blend(GPU_BLEND_ALPHA);

  LISTBASE_FOREACH (wmXrController *, controller, &state->controllers) {
    GPUBatch *model = controller->model;
    if (!model) {
      model = controller->model = wm_xr_controller_model_batch_create(xr_context,
                                                                      controller->subaction_path);
    }

    if (model &&
        GHOST_XrGetControllerModelData(xr_context, controller->subaction_path, &model_data) &&
        model_data.count_components > 0)
    {
      GPU_batch_program_set_builtin(model, GPU_SHADER_3D_UNIFORM_COLOR);
      GPU_batch_uniform_4fv(model, "color", color);

      GPU_matrix_push();
      GPU_matrix_mul(controller->grip_mat);
      for (uint component_idx = 0; component_idx < model_data.count_components; ++component_idx) {
        const GHOST_XrControllerModelComponent *component = &model_data.components[component_idx];
        GPU_matrix_push();
        GPU_matrix_mul(component->transform);
        GPU_batch_draw_range(model,
                             model->elem ? component->index_offset : component->vertex_offset,
                             model->elem ? component->index_count : component->vertex_count);
        GPU_matrix_pop();
      }
      GPU_matrix_pop();
    }
    else {
      /* Fallback. */
      const float scale = 0.05f;
      GPUBatch *sphere = GPU_batch_preset_sphere(2);
      GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
      GPU_batch_uniform_4fv(sphere, "color", color);

      GPU_matrix_push();
      GPU_matrix_mul(controller->grip_mat);
      GPU_matrix_scale_1f(scale);
      GPU_batch_draw(sphere);
      GPU_matrix_pop();
    }
  }
}

static void wm_xr_controller_aim_draw(const XrSessionSettings *settings, wmXrSessionState *state)
{
  bool draw_ray;
  switch (settings->controller_draw_style) {
    case XR_CONTROLLER_DRAW_DARK:
    case XR_CONTROLLER_DRAW_LIGHT:
      draw_ray = false;
      break;
    case XR_CONTROLLER_DRAW_DARK_RAY:
    case XR_CONTROLLER_DRAW_LIGHT_RAY:
      draw_ray = true;
      break;
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_FLAT_COLOR);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);
  immUniform2fv("viewportSize", &viewport[2]);

  immUniform1f("lineWidth", 3.0f * U.pixelsize);

  if (draw_ray) {
    const uchar color[4] = {89, 89, 255, 127};
    const float scale = settings->clip_end;
    float ray[3];

    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    GPU_blend(GPU_BLEND_ALPHA);

    immBegin(GPU_PRIM_LINES, (uint)BLI_listbase_count(&state->controllers) * 2);

    LISTBASE_FOREACH (wmXrController *, controller, &state->controllers) {
      const float(*mat)[4] = controller->aim_mat;
      madd_v3_v3v3fl(ray, mat[3], mat[2], -scale);

      immAttrSkip(col);
      // immVertex3fv(pos, mat[3]);
      // immVertex3f(pos, mat[3][0] + 1, mat[3][1], mat[3][2]);
      immAttr4ubv(col, color);
      immVertex3fv(pos, ray);

      // immAttrSkip(col);
      // immVertex3f(pos, mat[3][0] + 1, mat[3][1], mat[3][2]);
      // immAttr4ubv(col, color);
      // immVertex3f(pos, ray[0] + 1, ray[1], ray[2]);
    }

    immEnd();
  }
  else {
    const uchar r[4] = {255, 51, 82, 255};
    const uchar g[4] = {139, 220, 0, 255};
    const uchar b[4] = {40, 144, 255, 255};
    const float scale = 0.01f;
    float x_axis[3], y_axis[3], z_axis[3];

    GPU_depth_test(GPU_DEPTH_NONE);
    GPU_blend(GPU_BLEND_NONE);

    immBegin(GPU_PRIM_LINES, (uint)BLI_listbase_count(&state->controllers) * 6);

    LISTBASE_FOREACH (wmXrController *, controller, &state->controllers) {
      const float(*mat)[4] = controller->aim_mat;
      madd_v3_v3v3fl(x_axis, mat[3], mat[0], scale);
      madd_v3_v3v3fl(y_axis, mat[3], mat[1], scale);
      madd_v3_v3v3fl(z_axis, mat[3], mat[2], scale * 5);

      immAttrSkip(col);
      immVertex3fv(pos, mat[3]);
      immAttr4ubv(col, r);
      immVertex3fv(pos, x_axis);

      immAttrSkip(col);
      immVertex3fv(pos, mat[3]);
      immAttr4ubv(col, g);
      immVertex3fv(pos, y_axis);

      immAttrSkip(col);
      immVertex3fv(pos, mat[3]);
      immAttr4ubv(col, b);
      immVertex3fv(pos, z_axis);
    }

    immEnd();
  }

  immUnbindProgram();
}

void wm_xr_draw_controllers(const bContext *UNUSED(C), ARegion *UNUSED(region), void *customdata)
{
  wmXrData *xr = customdata;
  const XrSessionSettings *settings = &xr->session_settings;

  if ((settings->flag & XR_SESSION_SHOW_CONTROLLERS) == 0) {
    return;
  }

  GHOST_XrContextHandle xr_context = xr->runtime->context;
  wmXrSessionState *state = &xr->runtime->session_state;

  wm_xr_controller_model_draw(settings, xr_context, state);
  wm_xr_controller_aim_draw(settings, state);
}

// GG: draw blender UI, need to have access to all regions
void draw_mirror_blender_ui_to_xr(const bContext *UNUSED(C),
                                  ARegion *UNUSED(region),
                                  void *customdata)
{
  wmXrData *xr = customdata;
  const XrSessionSettings *settings = &xr->session_settings;

  if ((settings->flag & XR_SESSION_SHOW_BLENDER_UI_MIRROR) == 0) {
    return;
  }
  // GG: TIP: Setting UI Resolution in prefs to 1.5+ makes the mirrored UI extrmely readable, along
  // w/ setting text rendering to full and using AA. You can also plain make the window itself
  // larger and span across multiple monitors before starting the XR session (not as effective
  // though).

  // GG: TIP: Placing raycast origin aligned with nose or lower isn't jarring, probably least
  // jarring
  //  of all possible placements (probably because we've learned to ignore our nose). Next least
  //  jarring is to palce the raycast on our shoulder but then rotating objects is slightly off,
  //  enough to notice but not enough to make it unusable. Best place is under the nose.

  // GG: TIP:  TO get more wokring space, just use a single window thats wide and extends mupltiple
  //  monitors. Its better than using multiple window since cursor capture is slow when moving
  //  between
  // windows.
  // GG: TODO: use camera origin + forward projected onto quad sphere as the ui origin (suhc that
  // its
  //   where the center of the screenspace v3D is in viewspace.
  //    (base_pose.location + base_pose.forawrd * radius)
  //    This also makes it the scale origin so that changing scale in the ui props doesn't affect
  //    the v3D center point in vr space, which is what the user calibrates their UI pos/scale/rads
  //    to.

  // GG: TODO: default ui offset should be centered on raycast origin. Currently its higher up than
  // expected by 0.5 * ui_height
  GPUVertFormat *format;
  uint texcoord;
  uint pos;
  uint col;

  float ui_width = 1920;
  float ui_height = 1080;
  if (!BLI_listbase_is_empty(&xr->full_blender_ui_screens)) {
    GPUOffScreen *any_screen = ((LinkData *)xr->full_blender_ui_screens.first)->data;
    if (any_screen) {
      ui_width = GPU_offscreen_width(any_screen);
      // / 4.0f;                                               // xr->ui_res_scaling_factor;
      ui_height = GPU_offscreen_height(any_screen);
      // / 4.0f;  // xr->ui_res_scaling_factor;
    }
  }

  // TODO: really should alpha clip v3D pixels instead of just doing alpha blend. (maybe try
  // colormask?)
  //   since non-clip still writes to depth buffer.
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  /**
   * OK, so I think that the entire blenderUI should
   * just be fully rendered on XR again instead of just
   * being blitted. This allows python users to draw
   * overlays specifically for the XR device for non
   * view3D's, or do things specifically when seen through XR.
   * ...Q:... if XR is already only drawn for XR view3D,
   * then shouldnt some caller high up be able to draw
   * to the same space and even have access to the entire
   * blender UI?
   */

  float base_pose_mat[4][4];
  GHOST_XrPose *base_pose = &xr->runtime->session_state.prev_base_pose;
  wm_xr_pose_to_mat(base_pose, base_pose_mat);
  float resx = 2.0f;
  const float resy_per_x = (ui_height / ui_width);
  float resy = resy_per_x * resx;
  float halfresx = resx * 0.5f;
  float halfresy = resy * 0.5f;
  // GG: window iter from L1534
  // GG: TODO: really, only need one window, the main one that user is using.

  // float fmouse_xyz[3] = {xr->runtime->mouse_xy[0], xr->runtime->mouse_xy[1], ui_distance};
  // // Normalize mouse coords to UI resolution.
  // fmouse_xyz[0] /= ui_width;
  // fmouse_xyz[1] /= ui_height;
  // fmouse_xyz[0] *= resx;
  // fmouse_xyz[1] *= resy;
  // fmouse_xyz[0] -= halfresx;
  // fmouse_xyz[1] -= halfresy;
  // float fmouse_xyz_screenspace[3];
  // copy_v3_v3(fmouse_xyz_screenspace, fmouse_xyz);
  // mul_m4_v3(base_pose_mat, fmouse_xyz);
  // float fmouse_mat[4][4];
  // unit_m4(fmouse_mat);
  // copy_v3_v3(fmouse_mat[3], fmouse_xyz);
  // copy_m3_m3(fmouse_mat, base_pose_mat);
  // mul_m4_m4m4(fmouse_mat, base_pose_mat, fmouse_mat);

  // unit_m4(fmouse_mat);
  // float color[4] = {0, 0, 1, 1};
  // GPUBatch *sphere = GPU_batch_preset_sphere(0);
  // GPU_batch_program_set_builtin(sphere, GPU_SHADER_3D_UNIFORM_COLOR);
  // GPU_batch_uniform_4fv(sphere, "color", color);

  // GPU_matrix_push();
  // GPU_matrix_set(base_pose_mat);
  ////GPU_matrix_set(fmouse_mat);
  // GPU_matrix_scale_1f(0.1f);
  // GPU_batch_draw(sphere);
  // GPU_matrix_pop();

  bool is_mouse_over_v3d = (xr->runtime->flags & XR_RUNTIME_IS_MOUSE_OVER_V3D) != 0;

  if (is_mouse_over_v3d) {
    format = immVertexFormat();
    pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    col = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR);
    float viewport[4];
    GPU_viewport_size_get_f(viewport);
    immUniform2fv("viewportSize", &viewport[2]);

    // immUniform1f("lineWidth", 1);  // 3.0f * U.pixelsize);//thinner is easier on eyes
    immUniform1f("lineWidth", 10.0f * U.pixelsize);  // thinner is easier on eyes

    // GG:TODO: replace with drawing blender UI to a quad (then eventually sphere quad)
    // currently just ensuring XR arbitrary drawing.

    immBeginAtMost(GPU_PRIM_LINE_STRIP, 16);

    // consider starting ray at mouse? or atleast screen? (since mouse may not match w/ ray)
    immAttr3f(col, 0, 0, 1);
    float raycast_startpoint[3];
    copy_v3_v3(raycast_startpoint, xr->runtime->v3d_ray_dir);
    // starting depth of drawn ray, dont want it to be too closer to user's eyes
    mul_v3_fl(raycast_startpoint, xr->session_settings.cursor_raycast_distance0);
    add_v3_v3(raycast_startpoint, xr->runtime->v3d_origin);
    immVertex3fv(pos, raycast_startpoint);

    immAttr3f(col, 0, 1, 0);
    float raycast_endpoint[3];
    copy_v3_v3(raycast_endpoint, xr->runtime->v3d_ray_dir);
    mul_v3_fl(raycast_endpoint, xr->session_settings.cursor_raycast_distance1);
    add_v3_v3(raycast_endpoint, xr->runtime->v3d_origin);
    immVertex3fv(pos, raycast_endpoint);

    immAttr3f(col, 1, 0, 0);
    copy_v3_v3(raycast_endpoint, xr->runtime->v3d_ray_dir);
    mul_v3_fl(raycast_endpoint, xr->session_settings.cursor_raycast_distance2);
    add_v3_v3(raycast_endpoint, xr->runtime->v3d_origin);
    immVertex3fv(pos, raycast_endpoint);

    immEnd();

    immUnbindProgram();
  }

  float viewspace_from_worldspace[4][4];
  float worldspace_from_viewspace[4][4];
  copy_m4_m4(worldspace_from_viewspace, base_pose_mat);
  invert_m4_m4(viewspace_from_worldspace, worldspace_from_viewspace);

  float rads_per_viewspace_unit = 1;
  float total_rads_per_ui_width = xr->session_settings.mirrored_ui_rads_span;
  const float rads_per_pixel = total_rads_per_ui_width / ui_width;
  float viewspace_ui_height = 0;
  float window_origin_viewspace[3] = {0, 0, 0};
  {
    mul_m4_v3(xr->runtime->worldspace_from_windowspace, window_origin_viewspace);
    mul_m4_v3(viewspace_from_worldspace, window_origin_viewspace);
  }
  {
    float viewspace_unit_per_rad_vec[3] = {1.0 / rads_per_pixel, 0, 0};
    mul_m4_v3(xr->runtime->worldspace_from_windowspace, viewspace_unit_per_rad_vec);
    mul_m4_v3(viewspace_from_worldspace, viewspace_unit_per_rad_vec);
    sub_v3_v3(viewspace_unit_per_rad_vec, window_origin_viewspace);
    rads_per_viewspace_unit = 1.0 / len_v3(viewspace_unit_per_rad_vec);

    float window_top_left_viewspace[3] = {0, ui_height, 0};
    mul_m4_v3(xr->runtime->worldspace_from_windowspace, window_top_left_viewspace);
    mul_m4_v3(viewspace_from_worldspace, window_top_left_viewspace);
    sub_v3_v3(window_top_left_viewspace, window_origin_viewspace);
    viewspace_ui_height = len_v3(window_top_left_viewspace);
  }

  float center_viewspace[3] = {ui_width / 2.0f, ui_height / 2.0f, 0};
  mul_m4_v3(xr->runtime->worldspace_from_windowspace, center_viewspace);
  // curve the UI so its easier to read (for now its at an arbitrary distance)
  mul_m4_v3(viewspace_from_worldspace, center_viewspace);
  float radius = len_v3(center_viewspace) * 2.0f *
                 xr->session_settings.mirrored_ui_distance_factor;
  float ui_width_arclength = total_rads_per_ui_width * radius;

  float pre_ui_width = ui_width;
  float pre_ui_height = ui_height;

  int windex;

  bool hide_ui_on_mouse_over_v3d = (xr->session_settings.flag &
                                    XR_SESSION_HIDE_UI_MIRROR_ON_MOUSE_OVER_V3D) != 0;

  float mirror_ui_camera_center_viewspace[3] = {0, 0, radius};
  if (!hide_ui_on_mouse_over_v3d || !is_mouse_over_v3d) {
    LISTBASE_FOREACH_INDEX (LinkData *, ld, &xr->full_blender_ui_screens, windex) {
      GPUOffScreen *offscreen = ld->data;
      if (offscreen == NULL)
        continue;
      ui_width = GPU_offscreen_width(offscreen);
      ui_height = GPU_offscreen_height(offscreen);
      short window_pos_x = 0;
      short window_pos_y = 0;
      if (windex < 32) {
        window_pos_x = xr->window_positions_xy[windex * 2 + 0];
        window_pos_y = xr->window_positions_xy[windex * 2 + 1];
      }

      /* Setup offscreen color texture for drawing. */
      GPUTexture *texture = GPU_offscreen_color_texture(offscreen);

      GPU_texture_bind(texture, 0);
      /* No mipmaps or filtering. */
      // GPU_texture_mipmap_mode(texture, false, false);
      GPU_texture_mipmap_mode(texture, true, true);
      // wmWindowViewport(win);  // not necessary?

      // float vp_mat[4][4];
      // GPU_matrix_model_view_projection_get();
      // immUniformMatrix4fv("ModelViewProjectionMatrix",);
      // GPU_matrix_push()

      // float vert_positions[3 * 4] = {
      //     -halfresx,
      //     -halfresy,
      //     ui_distance,
      //     //
      //     halfresx,
      //     -halfresy,
      //     ui_distance,
      //     //
      //     halfresx,
      //     halfresy,
      //     ui_distance,
      //     //
      //     -halfresx,
      //     halfresy,
      //     ui_distance,
      // };

      // window space vert positoins (basically pixel coords)
      int total_verts = 4;
      // float vert_positions[3 * 4] = {
      //     0,
      //     0,
      //     0,
      //     //
      //     ui_width,
      //     0,
      //     0,
      //     //
      //     ui_width,
      //     ui_height,
      //     0,
      //     //
      //     0,
      //     ui_height,
      //     0,
      // };
      // float vert_texcoords[2 * 4] = {0,
      //                                0,
      //                                //
      //                                1,
      //                                0,
      //                                //
      //                                1,
      //                                1,
      //                                //
      //                                0,
      //                                1};

#define vertices_per_width 32
      total_verts = (vertices_per_width + 0) * (vertices_per_width + 0);
      float vert_positions[3 * (vertices_per_width + 0) * (vertices_per_width + 0)];
      float vert_texcoords[2 * (vertices_per_width + 0) * (vertices_per_width + 0)];
      for (int y = 0; y < vertices_per_width; y++) {
        for (int x = 0; x < vertices_per_width; x++) {
          int vert_index = (x + y * vertices_per_width);
          float mu_x = ((float)x / (vertices_per_width - 1));
          float mu_y = ((float)y / (vertices_per_width - 1));
          vert_positions[vert_index * 3 + 0] = mu_x * ui_width;
          vert_positions[vert_index * 3 + 1] = mu_y * ui_height;
          vert_positions[vert_index * 3 + 2] = 0;
          // float z = fabsf((mu_x - 0.5f) * 2.0f * ui_width);
          // vert_positions[vert_index * 3 + 2] = .2 * z * z;
          /*
         z = fabs((mu_x - 0.5f) * 2.0f * ui_width);
         vert_positions[vert_index * 3 + 2] = .001f * z * z;*/
          /*  z = mu_x;
            vert_positions[vert_index * 3 + 2] = 5 * ui_width * -sin(z * 3.14);
            vert_positions[vert_index * 3 + 0] = -.4 * ui_width * sin((mu_x + 0.5) * 3.14) +
                                                 .5 * ui_width;*/
          // vert_positions[vert_index * 3 + 2] = mu_x * 10;

          // float base_length = 500;
          // float center_xz[2] = {(0.5f * ui_width), 10*base_length};
          // float vec[2] = {vert_positions[vert_index * 3 + 0], vert_positions[vert_index * 3 +
          // 2]}; sub_v2_v2(vec, center_xz); normalize_v2_length(vec, base_length); add_v2_v2(vec,
          // center_xz); vert_positions[vert_index * 3 + 0] = vec[0]; vert_positions[vert_index * 3
          // + 2] = vec[1];
          /*
      float center[3] = {0.5f * ui_width, 0.5f * ui_height, -1000};
      sub_v3_v3(&vert_positions[vert_index * 3], center);
      normalize_v3_length(&vert_positions[vert_index * 3], 1000);
      add_v3_v3(&vert_positions[vert_index * 3], center);*/

          vert_texcoords[vert_index * 2 + 0] = mu_x;
          vert_texcoords[vert_index * 2 + 1] = mu_y;
        }
      }

#define tri_buffer_len (2 * 3 * (vertices_per_width + -1) * (vertices_per_width + -1))
      int tri_index_buffer[tri_buffer_len];
      for (int y = 0; y < vertices_per_width - 1; y++) {
        for (int x = 0; x < vertices_per_width - 1; x++) {
          int vert_index = (x + y * vertices_per_width);
          int tri_index = (x + y * (vertices_per_width - 1));
          tri_index_buffer[tri_index * 6 + 0] = vert_index;
          tri_index_buffer[tri_index * 6 + 1] = ((x + 0) + (y + 1) * vertices_per_width);
          tri_index_buffer[tri_index * 6 + 2] = ((x + 1) + (y + 1) * vertices_per_width);

          tri_index_buffer[tri_index * 6 + 3] = vert_index;
          tri_index_buffer[tri_index * 6 + 4] = ((x + 1) + (y + 1) * vertices_per_width);
          tri_index_buffer[tri_index * 6 + 5] = ((x + 1) + (y + 0) * vertices_per_width);
        }
      }

      // int soffx = 0;
      // const int sizex = WM_window_pixels_x(win);
      // const int sizey = WM_window_pixels_y(win);

      // /* wmOrtho for the screen has this same offset */
      // const float halfx = GLA_PIXEL_OFS / sizex;
      // const float halfy = GLA_PIXEL_OFS / sizex;

      /* Texture is already bound to GL_TEXTURE0 unit. */
      // DONE: so the rads origin (vert pos X) and the screen space to viewpsace origin need to
      // match
      //  w/ camera forward as zero.
      float vert_pos_viewspace[3];
      for (int i = 0; i < total_verts; i++) {
        float *vert_position = &vert_positions[i * 3];
        // vert_position[1] = (vert_position[1] / ui_height);
        // GG: TODO: scale from center of view3D instead of bottom left corner since the user
        // sets up their mirror UI offsets based on the raycast range/area. Currently the scaling
        // moves the entire UI/v3d area so it no longer matches with the raycast effective
        // range/area thus the user will have to re-offset everything again.
        // mul_v3_fl(vert_position, xr->session_settings.mirrored_ui_scale);
        // angle span and ui_scale counter eachother, so need yscale for really wide windows to see
        // better
        vert_position[1] *= xr->session_settings.mirrored_ui_scale_y;

        add_v2_v2(vert_position, xr->session_settings.mirrored_ui_offset);
        // better Y default. than zero
        vert_position[1] -= ui_height * 0.5 * xr->session_settings.mirrored_ui_scale_y;
        vert_position[0] += window_pos_x;
        vert_position[1] += window_pos_y;

        mul_m4_v3(xr->runtime->worldspace_from_windowspace, vert_position);

        mul_m4_v3(viewspace_from_worldspace, vert_position);

        float offset_from_origin_viewspace[3];
        copy_v3_v3(offset_from_origin_viewspace, vert_position);
        sub_v3_v3(offset_from_origin_viewspace, window_origin_viewspace);

        // I'm assuming that the UI is planar, which it is, and that viewspace up is aligned with
        // UI up, which I think is true.
        vert_position[1] = (offset_from_origin_viewspace[1] / viewspace_ui_height) *
                           (ui_width_arclength * resy_per_x);

        sub_v3_v3(vert_position, mirror_ui_camera_center_viewspace);

        float radsxz = -vert_position[0] * rads_per_viewspace_unit;
        radsxz += 3.14;
        float xz_rotation_forward[3] = {sinf(radsxz), 0, cosf(radsxz)};
        vert_position[0] = xz_rotation_forward[0] * radius;
        vert_position[2] = xz_rotation_forward[2] * radius;

        // float viewspace_up[3] = {0, 1, 0};
        // float secondary_rotation_axis[3];
        // cross_v3_v3v3(secondary_rotation_axis, viewspace_up, xz_rotation_forward);
        //
        // float radsyz = -vert_position[1] * rads_per_viewspace_unit;

        ////radsyz += 3.14;
        // rotate_v3_v3v3fl(vert_position, xz_rotation_forward, secondary_rotation_axis, radsyz);
        // normalize_v3_length(vert_position, radius);

        mul_m4_v3(worldspace_from_viewspace, vert_position);
      }
      //// curve the UI so its easier to read (for now its at an arbitrary distance)
      // mul_m4_v3(viewspace_from_worldspace, &vert_positions[i * 3]);
      //// float v2[2] = {vert_positions[i * 3 + 0], vert_positions[i * 3 + 2]};
      // normalize_v3_length(&vert_positions[i * 3], radius);
      // mul_m4_v3(worldspace_from_viewspace, &vert_positions[i * 3]);
      format = immVertexFormat();
      texcoord = GPU_vertformat_attr_add(format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

      // draw UI Xrayed through objects. (a bit annoying for animating, but nice in general..?)
      if ((xr->session_settings.flag & XR_SESSION_USE_UI_MIRROR_XRAY) != 0) {

        immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);
        float gray = 0.75f;
        immUniformColor4f(0, gray, 0, 0.5f);
        GPU_depth_test(GPU_DEPTH_NONE);
        GPU_depth_mask(false);
        immBegin(GPU_PRIM_TRIS, tri_buffer_len);

        // for (int i = 0; i < total_verts; i++) {
        //   immAttr2fv(texcoord, &vert_texcoords[i * 2]);
        //   immVertex3fv(pos, &vert_positions[i * 3]);
        // }

        for (int i = 0; i < tri_buffer_len; i++) {
          int vertex_index = tri_index_buffer[i];
          immAttr2fv(texcoord, &vert_texcoords[vertex_index * 2]);
          immVertex3fv(pos, &vert_positions[vertex_index * 3]);
        }

        immEnd();

        immUnbindProgram();
      }

      immBindBuiltinProgram(GPU_SHADER_3D_IMAGE);
      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
      GPU_depth_mask(true);
      immBegin(GPU_PRIM_TRIS, tri_buffer_len);
      // immBegin(GPU_PRIM_TRI_FAN, 4);

      // for (int i = 0; i < 4; i++) {
      //   immAttr2fv(texcoord, &vert_texcoords[i * 2]);
      //   immVertex3fv(pos, &vert_positions[i * 3]);
      // }

      for (int i = 0; i < tri_buffer_len; i++) {
        int vertex_index = tri_index_buffer[i];
        immAttr2fv(texcoord, &vert_texcoords[vertex_index * 2]);
        immVertex3fv(pos, &vert_positions[vertex_index * 3]);
      }
      immEnd();
      immUnbindProgram();

      GPU_texture_unbind(texture);
#undef vertices_per_width
#undef tri_buffer_len
    }
  }
  ui_width = pre_ui_width;
  ui_height = pre_ui_height;
  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_depth_mask(false);

  format = immVertexFormat();
  pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  col = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);

  // GG:TODO: replace with drawing blender UI to a quad (then eventually sphere quad)
  // currently just ensuring XR arbitrary drawing.
  immBeginAtMost(GPU_PRIM_TRI_FAN, 4);
  // float size = 0.005f;
  float size = 6;
  float mverts[3 * 4] = {
      -size,
      -size,
      0,  // size * .5f,
      //
      size,
      -size,
      0,  // size * .5f,
      //
      size,
      size,
      0,  // size * .5f,
      //
      -size,
      size,
      0,  // size * .5f,
  };
  float windowspace_from_mouse[4][4];
  unit_m4(windowspace_from_mouse);

  // windowspace_from_mouse[3][0] = xr->runtime->mouse_xy[0];
  // windowspace_from_mouse[3][1] = xr->runtime->mouse_xy[1];
  windowspace_from_mouse[3][0] = xr->mouse_position_global_xy[0];
  windowspace_from_mouse[3][1] = xr->mouse_position_global_xy[1];

  for (int i = 0; i < 4; i++) {
    // mul_m4_v3(fmouse_mat, &mverts[i * 3]);
    float *vert_position = &mverts[i * 3];

    mul_m4_v3(windowspace_from_mouse, &mverts[i * 3]);
    // GG: TODO: scale from center of view3D instead of bottom left corner since the user
    // sets up their mirror UI offsets based on the raycast range/area. Currently the scaling
    // moves the entire UI/v3d area so it no longer matches with the raycast effective range/area
    // thus the user will have to re-offset everything again.
    // mul_v3_fl(vert_position, xr->session_settings.mirrored_ui_scale);
    // angle span and ui_scale counter eachother, so need yscale for really wide windows to see
    // better
    vert_position[1] *= xr->session_settings.mirrored_ui_scale_y;
    add_v2_v2(vert_position, xr->session_settings.mirrored_ui_offset);
    // better Y default. than zero
    vert_position[1] -= ui_height * 0.5 * xr->session_settings.mirrored_ui_scale_y;

    mul_m4_v3(xr->runtime->worldspace_from_windowspace, &mverts[i * 3]);

    mul_m4_v3(viewspace_from_worldspace, vert_position);

    float offset_from_origin_viewspace[3];
    copy_v3_v3(offset_from_origin_viewspace, vert_position);
    sub_v3_v3(offset_from_origin_viewspace, window_origin_viewspace);

    // I'm assuming that the UI is planar, which it is, and that viewspace up is aligned with UI
    // up, which I think is true.
    vert_position[1] = (offset_from_origin_viewspace[1] / viewspace_ui_height) *
                       (ui_width_arclength * resy_per_x);

    sub_v3_v3(vert_position, mirror_ui_camera_center_viewspace);

    float radsxz = -vert_position[0] * rads_per_viewspace_unit;
    float width_arclength = radsxz * radius;
    radsxz += 3.14;
    float xz_rotation_forward[3] = {sinf(radsxz), 0, cosf(radsxz)};
    vert_position[0] = xz_rotation_forward[0] * radius * 0.999f;
    vert_position[2] = xz_rotation_forward[2] * radius * 0.999f;  // NOTE: only use 0.99f on final
    // vert_position[1] = (vert_position[1] / ui_width) * (width_arclength * resy_per_x);
    //

    //
    //  float viewspace_up[3] = {0, 1, 0};
    // float secondary_rotation_axis[3];
    // cross_v3_v3v3(secondary_rotation_axis, viewspace_up, xz_rotation_forward);

    // float radsyz = -vert_position[1] * rads_per_viewspace_unit;

    //// radsyz += 3.14;
    // rotate_v3_v3v3fl(vert_position, xz_rotation_forward, secondary_rotation_axis, radsyz);
    // normalize_v3_length(vert_position, radius * 0.99f);

    mul_m4_v3(worldspace_from_viewspace, vert_position);
  }
  // top left
  immAttr4f(col, 1, 1, 1, 0.5f);
  immVertex3fv(pos, &mverts[0]);

  // top right
  immAttr4f(col, 1, 1, 1, 0.5f);
  immVertex3fv(pos, &mverts[3]);

  // bottom left
  immAttr4f(col, 1, 1, 1, 0.5f);
  immVertex3fv(pos, &mverts[6]);

  // bottom right
  immAttr4f(col, 1, 1, 1, 0.5f);
  immVertex3fv(pos, &mverts[9]);

  immEnd();

  const int floats_per_vert = 3;
  const int verts_per_line = 2;
  const int total_lines = 3;
  float line_lengths = 0.1;
  const int total_verts = 6;
  float scene_cursor_verts[3 * 2 * 3] = {
      // X-axis
      -1,
      0,
      0,
      //
      1,
      0,
      0,
      // Y-axis
      0,
      -1,
      0,
      //
      0,
      1,
      0,
      // Z-axis
      0,
      0,
      -1,
      //
      0,
      0,
      1,
  };

  float *scene_cursor_location = xr->runtime->cursor_location_worldspace;

  for (int i = 0; i < total_verts; i++) {
    float *vert_position = &scene_cursor_verts[i * 3];
    mul_v3_fl(vert_position, line_lengths);
    add_v3_v3(vert_position, scene_cursor_location);
  }

  immBegin(GPU_PRIM_LINES, total_verts);

  for (int i = 0; i < total_verts; i++) {
    float *vert_position = &scene_cursor_verts[i * 3];
    immAttr4f(col, 1, 1, 0, 0.5f);
    immVertex3fv(pos, vert_position);
  }
  immEnd();

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
  GPU_depth_test(GPU_DEPTH_NONE);
}
