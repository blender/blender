/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Window-Manager XR Drawing
 *
 * Implements Blender specific drawing functionality for use with the Ghost-XR API.
 */

#include <cstring>

#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "ED_view3d_offscreen.hh"

#include "GHOST_C-api.h"

#include "GPU_batch_presets.hh"
#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"
#include "GPU_viewport.hh"

#include "UI_resources.hh"

#include "WM_api.hh"

#include "wm_xr_intern.hh"

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
  mul_m4_m4m4(m, eye_inv, base_inv);
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
  const wmXrViewportPair *vp = static_cast<const wmXrViewportPair *>(
      BLI_findlink(&surface_data->viewports, draw_view->view_idx));
  BLI_assert(vp && vp->viewport);

  const bool is_upside_down = GHOST_XrSessionNeedsUpsideDownDrawing(runtime_data->context);
  rcti rect{};
  rect.xmin = 0;
  rect.ymin = 0;
  rect.xmax = draw_view->width - 1;
  rect.ymax = draw_view->height - 1;

  wmViewport(&rect);

  /* For upside down contexts, draw with inverted y-values. */
  if (is_upside_down) {
    std::swap(rect.ymin, rect.ymax);
  }
  GPU_viewport_draw_to_screen_ex(vp->viewport, 0, &rect, draw_view->expects_srgb_buffer, true);
}

void wm_xr_draw_view(const GHOST_XrDrawViewInfo *draw_view, void *customdata)
{
  wmXrDrawData *draw_data = static_cast<wmXrDrawData *>(customdata);
  wmXrData *xr_data = draw_data->xr_data;
  wmXrSurfaceData *surface_data = draw_data->surface_data;
  wmXrSessionState *session_state = &xr_data->runtime->session_state;
  XrSessionSettings *settings = &xr_data->session_settings;

  const int display_flags = V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS | settings->draw_flags;

  float viewmat[4][4], winmat[4][4];

  BLI_assert(WM_xr_session_is_ready(xr_data));

  wm_xr_session_draw_data_update(session_state, settings, draw_view, draw_data);
  wm_xr_draw_matrices_create(draw_data, draw_view, settings, session_state, viewmat, winmat);
  wm_xr_session_state_update(settings, draw_data, draw_view, session_state);

  if (!wm_xr_session_surface_offscreen_ensure(surface_data, draw_view)) {
    return;
  }

  const wmXrViewportPair *vp = static_cast<const wmXrViewportPair *>(
      BLI_findlink(&surface_data->viewports, draw_view->view_idx));
  BLI_assert(vp && vp->offscreen && vp->viewport);

  /* In case a framebuffer is still bound from drawing the last eye. */
  GPU_framebuffer_restore();
  /* Some systems have drawing glitches without this. */
  GPU_clear_depth(1.0f);

  /* Draws the view into the surface_data->viewport's frame-buffers. */
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
                                  session_state->vignette_data->aperture,
                                  true,
                                  false,
                                  true,
                                  nullptr,
                                  false,
                                  vp->offscreen,
                                  vp->viewport);

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

bool wm_xr_passthrough_enabled(void *customdata)
{
  wmXrDrawData *draw_data = static_cast<wmXrDrawData *>(customdata);
  wmXrData *xr_data = draw_data->xr_data;
  XrSessionSettings *settings = &xr_data->session_settings;

  return (settings->draw_flags & V3D_OFSDRAW_XR_SHOW_PASSTHROUGH) != 0;
}

void wm_xr_disable_passthrough(void *customdata)
{
  wmXrDrawData *draw_data = static_cast<wmXrDrawData *>(customdata);
  wmXrData *xr_data = draw_data->xr_data;
  XrSessionSettings *settings = &xr_data->session_settings;

  settings->draw_flags &= ~V3D_OFSDRAW_XR_SHOW_PASSTHROUGH;
  WM_global_report(RPT_INFO, "Passthrough not available");
}

static blender::gpu::Batch *wm_xr_controller_model_batch_create(GHOST_XrContextHandle xr_context,
                                                                const char *subaction_path)
{
  GHOST_XrControllerModelData model_data;

  if (!GHOST_XrGetControllerModelData(xr_context, subaction_path, &model_data) ||
      model_data.count_vertices < 1)
  {
    return nullptr;
  }

  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
  GPU_vertformat_attr_add(&format, "nor", blender::gpu::VertAttrType::SFLOAT_32_32_32);

  blender::gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*vbo, model_data.count_vertices);
  vbo->data<GHOST_XrControllerModelVertex>().copy_from(
      {model_data.vertices, model_data.count_vertices});

  blender::gpu::IndexBuf *ibo = nullptr;
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
    if (!controller->grip_active) {
      continue;
    }

    blender::gpu::Batch *model = controller->model;
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
      blender::gpu::Batch *sphere = GPU_batch_preset_sphere(2);
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
      draw_ray = !state->is_raycast_shown;
      break;
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);
  uint col = GPU_vertformat_attr_add(
      format, "color", blender::gpu::VertAttrType::SFLOAT_32_32_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_FLAT_COLOR);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);
  immUniform2fv("viewportSize", &viewport[2]);

  immUniform1f("lineWidth", 3.0f * U.pixelsize);

  if (draw_ray) {
    const float color[4] = {0.33f, 0.33f, 1.0f, 0.5f};
    const float scale = settings->clip_end;
    float ray[3];

    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    GPU_blend(GPU_BLEND_ALPHA);

    LISTBASE_FOREACH (wmXrController *, controller, &state->controllers) {
      if (!controller->grip_active) {
        continue;
      }

      immBegin(GPU_PRIM_LINES, 2);

      const float (*mat)[4] = controller->aim_mat;
      madd_v3_v3v3fl(ray, mat[3], mat[2], -scale);

      immAttrSkip(col);
      immVertex3fv(pos, mat[3]);
      immAttr4fv(col, color);
      immVertex3fv(pos, ray);

      immEnd();
    }
  }
  else {
    const float r[4] = {255 / 255.0f, 51 / 255.0f, 82 / 255.0f, 255 / 255.0f};
    const float g[4] = {139 / 255.0f, 220 / 255.0f, 0 / 255.0f, 255 / 255.0f};
    const float b[4] = {40 / 255.0f, 144 / 255.0f, 255 / 255.0f, 255 / 255.0f};
    const float scale = 0.01f;
    float x_axis[3], y_axis[3], z_axis[3];

    GPU_depth_test(GPU_DEPTH_NONE);
    GPU_blend(GPU_BLEND_NONE);

    LISTBASE_FOREACH (wmXrController *, controller, &state->controllers) {
      if (!controller->grip_active) {
        continue;
      }

      immBegin(GPU_PRIM_LINES, 6);

      const float (*mat)[4] = controller->aim_mat;
      madd_v3_v3v3fl(x_axis, mat[3], mat[0], scale);
      madd_v3_v3v3fl(y_axis, mat[3], mat[1], scale);
      madd_v3_v3v3fl(z_axis, mat[3], mat[2], scale);

      immAttrSkip(col);
      immVertex3fv(pos, mat[3]);
      immAttr4fv(col, r);
      immVertex3fv(pos, x_axis);

      immAttrSkip(col);
      immVertex3fv(pos, mat[3]);
      immAttr4fv(col, g);
      immVertex3fv(pos, y_axis);

      immAttrSkip(col);
      immVertex3fv(pos, mat[3]);
      immAttr4fv(col, b);
      immVertex3fv(pos, z_axis);

      immEnd();
    }
  }

  immUnbindProgram();
}

void wm_xr_draw_controllers(const bContext * /*C*/, ARegion * /*region*/, void *customdata)
{
  wmXrData *xr = static_cast<wmXrData *>(customdata);
  const XrSessionSettings *settings = &xr->session_settings;
  GHOST_XrContextHandle xr_context = xr->runtime->context;
  wmXrSessionState *state = &xr->runtime->session_state;

  wm_xr_controller_model_draw(settings, xr_context, state);
  wm_xr_controller_aim_draw(settings, state);
}
