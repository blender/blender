/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Window-Manager XR Location Scouting
 *
 * Implements XR Location Scouting drawing and feature logic.
 */

#include "BKE_camera.h"
#include "BKE_context.hh"
#include "BKE_lib_id.hh"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_time.h"

#include "DNA_camera_types.h"

#include "ED_view3d_offscreen.hh"

#include "GHOST_Types.hh"

#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"
#include "GPU_viewport.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BLF_api.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "interface_intern.hh"

#include "WM_api.hh"

#include "wm_xr_intern.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Location Scouting Captures
 * \{ */

bool wm_xr_location_scouting_is_captures_empty(Scene *scene)
{
  PointerRNA scene_ptr = RNA_id_pointer_create(&scene->id);
  PropertyRNA *captures_prop = RNA_struct_find_property(&scene_ptr, "vr_captures");

  if (captures_prop == nullptr) {
    return true;
  }

  if (RNA_property_collection_is_empty(&scene_ptr, captures_prop)) {
    /* Empty capture collection. */
    return true;
  }

  return false;
}

std::optional<XrLocationScoutingCapture> wm_xr_location_scouting_get_active_capture(Scene *scene)
{
  /* Workaround: To allow for conditionally registering the location scouting capture collection on
   *             the Scene while not polluting the main DNA Scene struct, and generally keep the
   *             definition simple and contained, the VRCapture collection is defined on the Python
   *             add-on side. Values are then obtained back here via RNA introspection.
   *
   * NOTE: This functions thus *needs* be kept in sync with the VRCapture Python class. */

  if (wm_xr_location_scouting_is_captures_empty(scene)) {
    return std::nullopt;
  }

  PointerRNA scene_ptr = RNA_id_pointer_create(&scene->id);
  PropertyRNA *captures_prop = RNA_struct_find_property(&scene_ptr, "vr_captures");
  BLI_assert(captures_prop != nullptr);

  PropertyRNA *idx_prop = RNA_struct_find_property(&scene_ptr, "vr_captures_selected");
  const int capture_idx = RNA_property_int_get(&scene_ptr, idx_prop);

  PointerRNA current_capture;
  RNA_property_collection_lookup_int(&scene_ptr, captures_prop, capture_idx, &current_capture);

  /* Captured pose (location / orientation). */
  PropertyRNA *location_prop = RNA_struct_find_property(&current_capture, "location");
  PropertyRNA *orientation_prop = RNA_struct_find_property(&current_capture, "orientation");

  float3 capture_location;
  float4 capture_orientation;
  RNA_property_float_get_array(&current_capture, location_prop, capture_location);
  RNA_property_float_get_array(&current_capture, orientation_prop, capture_orientation);

  GHOST_XrPose capture_pose;
  capture_pose.is_active = true;
  copy_v3_v3(capture_pose.position, capture_location);
  copy_qt_qt(capture_pose.orientation_quat, capture_orientation);

  /* Captured view settings (lens / DoF). */
  PropertyRNA *lens_focal_prop = RNA_struct_find_property(&current_capture, "lens_focal");
  PropertyRNA *dof_enabled_prop = RNA_struct_find_property(&current_capture, "dof_enabled");
  PropertyRNA *dof_distance_prop = RNA_struct_find_property(&current_capture, "dof_distance");
  PropertyRNA *dof_fstop_prop = RNA_struct_find_property(&current_capture, "dof_fstop");

  XrLocationScoutingCapture capture = {
      .position = capture_location,
      .orientation_quat = capture_orientation,
      .lens_focal = RNA_property_float_get(&current_capture, lens_focal_prop),
      .dof_enabled = RNA_property_boolean_get(&current_capture, dof_enabled_prop),
      .dof_distance = RNA_property_float_get(&current_capture, dof_distance_prop),
      .dof_fstop = RNA_property_float_get(&current_capture, dof_fstop_prop)};

  return std::make_optional(capture);
}

static GHOST_XrPose wm_xr_location_scouting_capture_to_ghost_pose(
    const XrLocationScoutingCapture &capture)
{
  /* Create a GHOST_XrPose from a XrLocationScoutingCapture. Used to prevent storing GHOST types
   * inside XrLocationScoutingCapture while still being able to use wm_xr_pose_* functions. */
  GHOST_XrPose pose;

  pose.is_active = true;
  copy_v3_v3(pose.position, capture.position);
  copy_qt_qt(pose.orientation_quat, capture.orientation_quat);

  return pose;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Location Scouting Viewfinder Logic
 * \{ */

/* Factor used to size UI widgets in XR world space. Going from scene to UI units. */
static constexpr float xr_ui_unit_fac = 0.05f;

static const char *wm_xr_viewfinder_get_hand_user_path(const XrSessionSettings *settings)
{
  switch (settings->viewfinder_hand) {
    case XR_VIEWFINDER_HAND_LEFT:
      return "/user/hand/left";
    case XR_VIEWFINDER_HAND_RIGHT:
      return "/user/hand/right";
    default:
      BLI_assert_unreachable();
      return nullptr;
  }
}

static wmXrController *wm_xr_viewfinder_get_controller(const XrSessionSettings *settings,
                                                       const wmXrSessionState *state)
{
  const char *user_path = wm_xr_viewfinder_get_hand_user_path(settings);

  for (wmXrController &controller : state->controllers) {
    if (STREQ(controller.subaction_path, user_path) && controller.grip_active) {
      return &controller;
    }
  }

  return nullptr;
}

static StringRefNull wm_xr_viewfinder_get_active_mode_str(const wmXrSessionState *state)
{
  switch (state->viewfinder.active_mode) {
    case XR_VIEWFINDER_MODE_LIVE:
      return "active_action_live";
    case XR_VIEWFINDER_MODE_PLAYBACK:
      return "active_action_playback";
    case XR_VIEWFINDER_MODE_CONFIRM:
      return "active_action_confirm";
  }

  return "";
}

static int wm_xr_viewfinder_get_active_action_idx(const wmXrSessionState *state)
{
  switch (state->viewfinder.active_mode) {
    case XR_VIEWFINDER_MODE_LIVE:
      return int(state->viewfinder.active_action_live);
    case XR_VIEWFINDER_MODE_PLAYBACK:
      return int(state->viewfinder.active_action_playback);
    case XR_VIEWFINDER_MODE_CONFIRM:
      return int(state->viewfinder.active_action_confirm);
  }

  return 0;
}

bool wm_xr_viewfinder_operator_event_match_hand(bContext *C, const wmEvent *event)
{
  XrSessionSettings *settings = &CTX_wm_manager(C)->xr.session_settings;
  wmXrActionData *actiondata = static_cast<wmXrActionData *>(event->customdata);

  if (!settings->viewfinder_enabled) {
    return false;
  }

  return STREQ(actiondata->user_path, wm_xr_viewfinder_get_hand_user_path(settings));
}

static rctf wm_xr_viewfinder_get_rect(const bContext *C, const XrSessionSettings *settings)
{
  const RenderData *scene_render_settings = &CTX_data_scene(C)->r;

  /* Use scene render aspect ratio. */
  const float render_x = scene_render_settings->xsch * scene_render_settings->xasp;
  const float render_y = scene_render_settings->ysch * scene_render_settings->yasp;
  const float render_aspect_ratio = render_y / render_x;

  constexpr float minimum_width = 4.3f;
  const float viewfinder_width = minimum_width + settings->viewfinder_scale;
  const float viewfinder_height = viewfinder_width * render_aspect_ratio;

  rctf viewfinder_rect = {};
  BLI_rctf_resize(&viewfinder_rect, viewfinder_width, viewfinder_height);

  return viewfinder_rect;
}

static float wm_xr_viewfinder_get_pp_overscan(const XrSessionSettings *settings)
{
  /* Turn the user-facing 0 -> 1 factor into a 1 -> 1.5 factor. */
  return (settings->viewfinder_passepartout_overscan * 0.5) + 1.0f;
}

static bool wm_xr_viewfinder_get_capture_mat(const XrSessionSettings *settings,
                                             const wmXrSessionState *state,
                                             const float viewfinder_height,
                                             float r_mat[4][4])
{
  const wmXrController *viewfinder_controller = wm_xr_viewfinder_get_controller(settings, state);
  if (!viewfinder_controller) {
    return false;
  }

  /* Compute vertical offset. */
  constexpr float base_controller_offset = -0.1f;
  const float height_offset = (viewfinder_height / 2) * xr_ui_unit_fac * -1;
  const float viewfinder_vertical_offset = base_controller_offset + height_offset;

  /* Obtain viewfinder capture mat from the choosen controller grip mat. */
  float viewfinder_mat[4][4];
  copy_m4_m4(viewfinder_mat, viewfinder_controller->grip_mat);
  translate_m4(viewfinder_mat, 0.0f, 0.0f, viewfinder_vertical_offset);
  rotate_m4(viewfinder_mat, 'X', -M_PI_2);

  copy_m4_m4(r_mat, viewfinder_mat);
  return true;
}

static void wm_xr_viewfinder_transform_update_smoothed(wmXrSessionState *state,
                                                       float raw_capture_mat[4][4],
                                                       float r_smoothed_mat[4][4])
{
  /* Take the raw capture matrix, apply movement smoothing from stored state position/orientation
   * while also updating them, and return the resulting smoothed viewfinder transform matrix. */
  float raw_capture_position[3];
  float raw_capture_orientation_quat[4];
  mat4_to_loc_quat(raw_capture_position, raw_capture_orientation_quat, raw_capture_mat);

  const double current_time = BLI_time_now_seconds();
  const double delta_t = current_time - state->viewfinder.smoothing_delta_t;

  /* Delta-T threshold used to reset smoothing when switching between Playback/Live mode. */
  constexpr double delta_t_threshold = 0.25;

  if (state->viewfinder.smoothing_delta_t != 0.0 && delta_t < delta_t_threshold) {
    /* Apply exponential movement smoothing. */
    constexpr float movement_smoothing_speed = 25.0f;
    const float clamped_delta = min_ff(delta_t, 0.1f);
    const float factor = 1.0f - exp(-clamped_delta * movement_smoothing_speed);

    interp_v3_v3v3(state->viewfinder.capture_position,
                   state->viewfinder.capture_position,
                   raw_capture_position,
                   factor);
    interp_qt_qtqt(state->viewfinder.capture_orientation_quat,
                   state->viewfinder.capture_orientation_quat,
                   raw_capture_orientation_quat,
                   factor);
  }
  else {
    /* Initialize smoothing, or reset if delta_t threshold was exceeded. */
    copy_v3_v3(state->viewfinder.capture_position, raw_capture_position);
    copy_qt_qt(state->viewfinder.capture_orientation_quat, raw_capture_orientation_quat);
  }

  state->viewfinder.smoothing_delta_t = current_time;

  quat_to_mat4(r_smoothed_mat, state->viewfinder.capture_orientation_quat);
  copy_v3_v3(r_smoothed_mat[3], state->viewfinder.capture_position);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Location Scouting Viewfinder Live/Playback View Rendering
 * \{ */

void wm_xr_viewfinder_render_view(wmXrData *xr_data)
{
  wmXrSessionState *state = &xr_data->runtime->session_state;
  XrSessionSettings *settings = &xr_data->session_settings;

  if (!settings->viewfinder_enabled) {
    return;
  }

  float viewfinder_render_viewmat[4][4] = {};

  Camera *cam_render_data = state->viewfinder.render_cam_data_id; /* Allocated ID. */
  CameraParams cam_render_params;
  BKE_camera_params_init(&cam_render_params);

  bContext *xr_context = WM_xr_session_context_get(xr_data);
  Scene *scene = CTX_data_scene(xr_context);

  switch (state->viewfinder.active_mode) {
    case XR_VIEWFINDER_MODE_LIVE: {
      const rctf viewfinder_rect = wm_xr_viewfinder_get_rect(xr_context, settings);
      const float viewfinder_height = BLI_rctf_size_y(&viewfinder_rect);

      float raw_capture_mat[4][4];
      if (!wm_xr_viewfinder_get_capture_mat(settings, state, viewfinder_height, raw_capture_mat)) {
        /* Invalid viewfinder capture matrix, cannot draw, early return. */
        return;
      }

      /* Build final capture camera matrix for rendering, apply movement smoothing over time to
       * stabilize the Viewfinder Live capture view. */
      float viewfinder_capture_mat[4][4];
      wm_xr_viewfinder_transform_update_smoothed(state, raw_capture_mat, viewfinder_capture_mat);

      invert_m4_m4(viewfinder_render_viewmat, viewfinder_capture_mat);

      cam_render_params.lens = state->viewfinder.capture_lens_focal;
      SET_FLAG_FROM_TEST(
          cam_render_data->dof.flag, state->viewfinder.capture_dof_enabled, CAM_DOF_ENABLED);
      cam_render_data->dof.aperture_fstop = state->viewfinder.capture_dof_fstop;
      cam_render_data->dof.focus_distance = state->viewfinder.capture_dof_distance;

      break;
    }
    case XR_VIEWFINDER_MODE_PLAYBACK:
    case XR_VIEWFINDER_MODE_CONFIRM: {
      auto capture = wm_xr_location_scouting_get_active_capture(scene);
      if (!capture.has_value()) {
        /* Nothing to draw, early return. */
        return;
      }

      const GHOST_XrPose capture_pose = wm_xr_location_scouting_capture_to_ghost_pose(*capture);
      wm_xr_pose_to_imat(&capture_pose, viewfinder_render_viewmat);
      cam_render_params.lens = capture->lens_focal;

      SET_FLAG_FROM_TEST(cam_render_data->dof.flag, capture->dof_enabled, CAM_DOF_ENABLED);
      cam_render_data->dof.focus_distance = capture->dof_distance;
      cam_render_data->dof.aperture_fstop = capture->dof_fstop;

      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }

  /* Compute obtained camera parameter from Live / Playback for render, using scene render
   * aspect ratio. */
  const RenderData *render_settings = &scene->r;
  BKE_camera_params_compute_viewplane(&cam_render_params,
                                      render_settings->xsch,
                                      render_settings->ysch,
                                      render_settings->xasp,
                                      render_settings->yasp);
  /* In Live mode, scale viewplane by passepartout overscan. */
  if (state->viewfinder.active_mode == XR_VIEWFINDER_MODE_LIVE) {
    BLI_rctf_mul(&cam_render_params.viewplane, wm_xr_viewfinder_get_pp_overscan(settings));
  }

  BKE_camera_params_compute_matrix(&cam_render_params);

  float viewfinder_winmat[4][4];
  copy_m4_m4(viewfinder_winmat, cam_render_params.winmat);

  /* Set viewfinder View3D draw flags, using base XR draw flags with some overriden exceptions. */
  int viewfinder_draw_flags = V3D_OFSDRAW_OVERRIDE_SCENE_SETTINGS | settings->draw_flags;
  viewfinder_draw_flags &= ~V3D_OFSDRAW_SHOW_SELECTION;      /* Always hide selection outlines. */
  viewfinder_draw_flags &= ~V3D_OFSDRAW_XR_SHOW_CONTROLLERS; /* Always hide other XR controller. */
  viewfinder_draw_flags &= ~V3D_OFSDRAW_XR_SHOW_CUSTOM_OVERLAYS; /* Always hide XR overlays. */

  /* Always enable DoF in the View3D settings used by in the viewfinder rendered view
   * for Workbench. */
  View3DShading viewfinder_shading_settings = settings->shading;
  viewfinder_shading_settings.flag |= V3D_SHADING_DEPTH_OF_FIELD;

  /* Shim Viewfinder Camera Object to override the View3D with for rendering and pass it our
   * cam_render_data ID. */
  Object viewfinder_cam_ob = {};
  viewfinder_cam_ob.type = OB_CAMERA;
  viewfinder_cam_ob.data = id_cast<ID *>(cam_render_data);

  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(xr_context);
  ED_view3d_draw_offscreen_simple(depsgraph,
                                  scene,
                                  &viewfinder_shading_settings,
                                  xr_context,
                                  settings->shading.type,
                                  settings->object_type_exclude_viewport,
                                  settings->object_type_exclude_select,
                                  blender::wmXrViewfinderState::view_resolution,
                                  blender::wmXrViewfinderState::view_resolution,
                                  viewfinder_draw_flags,
                                  viewfinder_render_viewmat,
                                  viewfinder_winmat,
                                  settings->clip_start,
                                  settings->clip_end,
                                  1.0f,
                                  true,
                                  false,
                                  true,
                                  nullptr,
                                  true,
                                  &viewfinder_cam_ob,
                                  state->viewfinder.offscreen,
                                  state->viewfinder.viewport);
}

/* -------------------------------------------------------------------- */
/** \name Location Scouting Viewfinder UI Widgets
 *
 * \note For the most part, this code tricks our UI widget library into drawing in world space
 *       using fake window/region sizes and a custom `ui::block_draw` replacement.
 *       As such, this should be considered a PoC more than a working and sustainable way to draw
 *       UI in XR, and long-term should be replaced with a proper XR UI Toolkit.
 * \{ */

static ui::Block *wm_xr_viewfinder_ui_block_begin(const bContext *C, ui::EmbossType emboss)
{
  ui::Block *block = ui::block_begin(C, nullptr, __func__, emboss);

  /* Since there are no windows in XR, use a dummy constant window size as aspect. */
  const blender::int2 win_size = {1600 * 2, 900 * 2};
  const rcti winrct = {0, win_size[0] - 1, 0, win_size[1] - 1};

  wmGetProjectionMatrix(block->winmat, &winrct);
  block->aspect = 2.0f / fabsf(win_size[0] * block->winmat[0][0]);

  ui::block_flag_enable(block, ui::BLOCK_LOOP | ui::BLOCK_KEEP_OPEN | ui::BLOCK_NO_WIN_CLIP);
  ui::block_theme_style_set(block, ui::BLOCK_THEME_STYLE_POPUP); /* Can also use REGULAR here. */

  return block;
}

static void wm_xr_viewfinder_ui_block_draw(const bContext *C, ui::Block *block)
{
  /* This is a stripped down version of #ui::block_draw without background drawing (which relies
   * on window coordinates), while also keeping the transform matrix from the outside GPU context
   * for the block to be positioned in VR space. */

  /* Fake fixed region winrct size values, for drawing to not depend on the window size. Values
   * obtained from the window region used during development.
   * TODO(@brainzman): Switch to a uniform size, and resize existing XR UI widgets accordingly.
   */
  ARegion region = {};
  BLI_rcti_init(&region.winrct, 0, 1680, 0, 1760);

  GPU_blend(GPU_BLEND_ALPHA);
  ui::widgetbase_draw_cache_begin();

  uiStyle style = *ui::style_get_dpi();
  for (ui::Button &but : block->buttons()) {
    rcti rect;
    button_to_pixelrect(&rect, &region, block, &but);

    if (rect.xmin < rect.xmax && rect.ymin < rect.ymax) {
      draw_button(C, &region, &style, &but, &rect);
    }
  }

  ui::widgetbase_draw_cache_end();
  GPU_blend(GPU_BLEND_NONE);
}

static ui::Layout &wm_xr_viewfinder_ui_layout(ui::Block *block)
{
  const uiStyle *style = ui::style_get_dpi();
  const int viewfinder_width = style->widget.points * 50 * UI_SCALE_FAC;

  using namespace blender;
  return ui::block_layout(block,
                          ui::LayoutDirection::Vertical,
                          ui::LayoutType::Panel,
                          0,
                          0,
                          viewfinder_width,
                          0,
                          0,
                          style);
}

static ui::Block *wm_xr_viewfinder_ui_mode_tabs_block(const bContext *C,
                                                      const wmXrSessionState *state)
{
  ui::Block *block = wm_xr_viewfinder_ui_block_begin(C, ui::EmbossType::Emboss);

  const float tab_width = UI_UNIT_X * 10.5f;

  ui::Button *but = uiDefBut(block,
                             ui::ButtonType::Tab,
                             "Live Camera View",
                             0,
                             0,
                             tab_width,
                             UI_UNIT_Y,
                             nullptr,
                             0,
                             0,
                             "");
  button_func_pushed_state_set(but, [&state](const ui::Button &) -> bool {
    return state->viewfinder.active_mode == XR_VIEWFINDER_MODE_LIVE;
  });

  but = uiDefBut(block,
                 ui::ButtonType::Tab,
                 "Image Playback",
                 tab_width,
                 0,
                 tab_width,
                 UI_UNIT_Y,
                 nullptr,
                 0,
                 0,
                 "");
  button_func_pushed_state_set(but, [&state](const ui::Button &) -> bool {
    return state->viewfinder.active_mode == XR_VIEWFINDER_MODE_PLAYBACK;
  });

  ui::block_end(C, block);

  return block;
}

static ui::Block *wm_xr_viewfinder_ui_settings_left_label_block(const bContext *C,
                                                                const wmXrSessionState *state)
{

  ui::Block *block = wm_xr_viewfinder_ui_block_begin(C, ui::EmbossType::Emboss);
  ui::Layout &layout = wm_xr_viewfinder_ui_layout(block);

  if (state->viewfinder.active_mode == XR_VIEWFINDER_MODE_PLAYBACK) {
    const std::string settings_left_side_label = fmt::format(
        "Show active capture in space: {}",
        state->viewfinder.playback_show_active_capture_in_space_enabled ? "on" : "off");
    layout.label(settings_left_side_label.c_str(), ICON_NONE);
  }
  else if (state->viewfinder.active_mode == XR_VIEWFINDER_MODE_CONFIRM) {
    layout.label("Confirm: Delete this shot?", ICON_NONE);
  }

  ui::block_end(C, block);

  return block;
}

static ui::Block *wm_xr_viewfinder_ui_settings_right_label_block(const bContext *C,
                                                                 const wmXrSessionState *state)
{

  ui::Block *block = wm_xr_viewfinder_ui_block_begin(C, ui::EmbossType::Emboss);
  ui::Layout &layout = wm_xr_viewfinder_ui_layout(block);

  Scene *scene = CTX_data_scene(C);
  PointerRNA scene_ptr = RNA_id_pointer_create(&scene->id);

  /* Note: unsafe, relies on the VR add-on being loaded. */
  PropertyRNA *captures_len_prop = RNA_struct_find_property(&scene_ptr, "vr_captures");
  PropertyRNA *captures_idx_prop = RNA_struct_find_property(&scene_ptr, "vr_captures_selected");
  const int captures_len = RNA_property_collection_length(&scene_ptr, captures_len_prop);
  const int captures_idx = RNA_property_int_get(&scene_ptr, captures_idx_prop);

  std::string settings_right_side_label;
  switch (state->viewfinder.active_mode) {
    /* \xe2\x80\x87 corresponds to a Unicode Figure Space (BLI_STR_UTF8_FIGURE_SPACE). */
    case XR_VIEWFINDER_MODE_LIVE:
      settings_right_side_label = fmt::format(
          "{:\xe2\x80\x87>3}mm   DoF: {}   d: {:\xe2\x80\x87<4.1f}   f {:\xe2\x80\x87<3.1f}",
          state->viewfinder.capture_lens_focal,
          state->viewfinder.capture_dof_enabled ? "on " : "off",
          state->viewfinder.capture_dof_distance,
          state->viewfinder.capture_dof_fstop);
      break;
    case XR_VIEWFINDER_MODE_PLAYBACK:
    case XR_VIEWFINDER_MODE_CONFIRM:
      /* Current capture indicator (`current capture idx / all captures`). */
      if (captures_len > 0) {
        const int width = captures_len >= 10 ? 2 : 1;
        const char *pad_prefix = captures_len < 10 ? "     " : "";
        settings_right_side_label = fmt::format("{}{:\xe2\x80\x87>{}} / {:\xe2\x80\x87>{}}",
                                                pad_prefix,
                                                captures_idx + 1,
                                                width,
                                                captures_len,
                                                width);
      }
      break;
    default:
      BLI_assert_unreachable();
      return nullptr;
  }

  layout.label(settings_right_side_label.c_str(), ICON_NONE);

  ui::block_end(C, block);

  return block;
}

static ui::Block *wm_xr_viewfinder_ui_action_label_block(const bContext *C,
                                                         const wmXrSessionState *state)
{
  ui::Block *block = wm_xr_viewfinder_ui_block_begin(C, ui::EmbossType::None);
  ui::Layout &layout = wm_xr_viewfinder_ui_layout(block);

  const StringRefNull active_action_prop = wm_xr_viewfinder_get_active_mode_str(state);
  int active_action_idx = wm_xr_viewfinder_get_active_action_idx(state);

  PointerRNA ptr = RNA_pointer_create_discrete(
      &CTX_wm_manager(C)->id, RNA_XrViewfinderState, (void *)&state->viewfinder);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, active_action_prop.c_str());

  const char *active_action_label;
  RNA_property_enum_name(nullptr, &ptr, prop, active_action_idx, &active_action_label);
  layout.label(active_action_label, ICON_NONE);

  ui::block_end(C, block);

  return block;
}

static ui::Block *wm_xr_viewfinder_ui_action_enum_block(const bContext *C,
                                                        const wmXrSessionState *state)
{
  ui::Block *block = wm_xr_viewfinder_ui_block_begin(C, ui::EmbossType::Emboss);
  ui::Layout &layout = wm_xr_viewfinder_ui_layout(block);
  ui::Layout &row = layout.row(true);

  layout.scale_y_set(1.1f);

  PointerRNA ptr = RNA_pointer_create_discrete(
      &CTX_wm_manager(C)->id, RNA_XrViewfinderState, (void *)&state->viewfinder);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "active_action_live");

  if (state->viewfinder.active_mode == XR_VIEWFINDER_MODE_LIVE) {
    /* Live mode, display each property enum separately for the DoF controls to be marked
     * as disabled when DoF is disabled. */
    layout.ui_units_x_set(8.0f); /* Width hack. */

    ui::Layout &sub1 = row.row(true);
    sub1.prop_enum(&ptr, prop, XR_VIEWFINDER_ACTION_LIVE_LENS, "", ICON_NONE);
    sub1.prop_enum(&ptr, prop, XR_VIEWFINDER_ACTION_LIVE_DOF, "", ICON_NONE);

    ui::Layout &sub2 = row.row(true);
    /* Show these controls greyed-out if DoF is disabled. */
    sub2.enabled_set(state->viewfinder.capture_dof_enabled);
    sub2.prop_enum(&ptr, prop, XR_VIEWFINDER_ACTION_LIVE_FOCUS, "", ICON_NONE);
    sub2.prop_enum(&ptr, prop, XR_VIEWFINDER_ACTION_LIVE_APERTURE, "", ICON_NONE);
  }
  else {
    /* Playback and Confirm mode, directly draw the full enum prop. */
    layout.scale_x_set(15.0f); /* Width hack. */

    const StringRefNull active_action_prop = wm_xr_viewfinder_get_active_mode_str(state);
    row.prop(
        &ptr, active_action_prop.c_str(), ui::ITEM_R_EXPAND | ui::ITEM_R_ICON_ONLY, "", ICON_NONE);
  }

  ui::block_end(C, block);

  return block;
}

static ui::Block *wm_xr_viewfinder_ui_missing_captures_label_block(const bContext *C,
                                                                   const wmXrSessionState *state)
{
  const bool empty_captures = wm_xr_location_scouting_is_captures_empty(CTX_data_scene(C));

  ui::Block *block = wm_xr_viewfinder_ui_block_begin(C, ui::EmbossType::Emboss);
  ui::Layout &layout = wm_xr_viewfinder_ui_layout(block);

  if (state->viewfinder.active_mode == XR_VIEWFINDER_MODE_PLAYBACK && empty_captures) {
    layout.label("No shots captured yet.", ICON_NONE);
  }

  ui::block_end(C, block);

  return block;
}

static void wm_xr_viewfinder_ui_draw_widgets(const bContext *C,
                                             const wmXrSessionState *state,
                                             const rctf &viewfinder_rect)
{
  using BlockFuncPtr = decltype(&wm_xr_viewfinder_ui_mode_tabs_block);
  const auto draw_block = [&](BlockFuncPtr block_func, float x_off, float y_off) {
    GPU_matrix_push();
    GPU_matrix_translate_3f(x_off, y_off, 0.0f);
    GPU_matrix_scale_1f(0.02f);

    ui::Block *block = block_func(C, state);
    wm_xr_viewfinder_ui_block_draw(C, block);

    GPU_matrix_pop();
  };

  const float mode_tabs_x = viewfinder_rect.xmin - 0.15f;
  const float mode_tabs_y = viewfinder_rect.ymax + 0.45f;

  draw_block(wm_xr_viewfinder_ui_mode_tabs_block, mode_tabs_x, mode_tabs_y);

  const float settings_left_label_x = viewfinder_rect.xmin + 0.05f;
  const float settings_right_label_x = state->viewfinder.active_mode == XR_VIEWFINDER_MODE_LIVE ?
                                           viewfinder_rect.xmax - 3.7f :
                                           viewfinder_rect.xmax - 0.8f;
  const float settings_label_y = viewfinder_rect.ymax + 0.47f;

  draw_block(
      wm_xr_viewfinder_ui_settings_left_label_block, settings_left_label_x, settings_label_y);
  draw_block(
      wm_xr_viewfinder_ui_settings_right_label_block, settings_right_label_x, settings_label_y);

  const float action_label_x = viewfinder_rect.xmin + 0.05f;
  const float action_label_y = viewfinder_rect.ymin - 0.15f;

  float action_enum_x = 0.0f;
  switch (state->viewfinder.active_mode) {
    case XR_VIEWFINDER_MODE_LIVE:
      action_enum_x = viewfinder_rect.xmax - 1.6f;
      break;
    case XR_VIEWFINDER_MODE_PLAYBACK:
      action_enum_x = viewfinder_rect.xmax - 1.2f;
      break;
    case XR_VIEWFINDER_MODE_CONFIRM:
      action_enum_x = viewfinder_rect.xmax - 0.8f;
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  const float action_enum_y = viewfinder_rect.ymin - 0.15f;

  draw_block(wm_xr_viewfinder_ui_action_label_block, action_label_x, action_label_y);
  draw_block(wm_xr_viewfinder_ui_action_enum_block, action_enum_x, action_enum_y);

  constexpr float missing_label_x = -1.1f;
  constexpr float missing_label_y = 0.1f;

  draw_block(wm_xr_viewfinder_ui_missing_captures_label_block, missing_label_x, missing_label_y);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Location Scouting world-space gizmos drawing
 * \{ */

static void wm_xr_viewfinder_gizmo_draw_capture_camera(const bContext *C, wmXrSessionState *state)
{
  /* NOTE: This duplicates logic from the Python add-on VIEW3D_GGT_vr_captures gizmo, not ideal. */
  if (state->viewfinder.active_mode != XR_VIEWFINDER_MODE_PLAYBACK ||
      state->viewfinder.playback_show_active_capture_in_space_enabled == false)
  {
    return;
  }

  const auto capture = wm_xr_location_scouting_get_active_capture(CTX_data_scene(C));
  if (!capture.has_value()) {
    return;
  }

  float capture_mat[4][4];

  const GHOST_XrPose capture_pose = wm_xr_location_scouting_capture_to_ghost_pose(*capture);
  wm_xr_pose_to_mat(&capture_pose, capture_mat);

  /* Compute focal. */
  constexpr float sensor_fit_fac = 36 * 2; /* Twice the default Camera sensor fit (36mm). */
  const float focal = capture->lens_focal / sensor_fit_fac;

  /* Compute aspect. */
  const RenderData *render_settings = &CTX_data_scene(C)->r;
  const float render_x = render_settings->xsch * render_settings->xasp;
  const float render_y = render_settings->ysch * render_settings->yasp;
  const float aspect_x = render_x < render_y ? render_x / render_y : 1;
  const float aspect_y = render_x > render_y ? render_y / render_x : 1;
  /* Base aspect to match native Blender Camera Gizmo (using Auto Sensor Fit). */
  constexpr float base_aspect = 1.0f / 4.0f;
  const float aspect[2] = {aspect_x * base_aspect, aspect_y * base_aspect};

  const float corners[4][3] = {
      {-aspect[0], -aspect[1], -focal},
      {aspect[0], -aspect[1], -focal},
      {aspect[0], aspect[1], -focal},
      {-aspect[0], aspect[1], -focal},
  };

  constexpr float color[4] = {0.25f, 0.81f, 1.0f, 1.0f};

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32_32);
  uint col = GPU_vertformat_attr_add(format, "color", gpu::VertAttrType::SFLOAT_32_32_32_32);

  GPU_matrix_push();
  GPU_matrix_mul(capture_mat);

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_FLAT_COLOR);

  float viewport[4];
  GPU_viewport_size_get_f(viewport);
  immUniform2fv("viewportSize", &viewport[2]);
  immUniform1f("lineWidth", 2.0f * U.pixelsize);

  /* Camera frame. */
  immBegin(GPU_PRIM_LINE_STRIP, 5);
  for (int i = 0; i < 4; i++) {
    immAttr4fv(col, color);
    immVertex3fv(pos, corners[i]);
  }
  immAttr4fv(col, color);
  immVertex3fv(pos, corners[0]);
  immEnd();

  /* Camera frustum lines from origin to each corner. */
  immBegin(GPU_PRIM_LINES, 8);
  for (int i = 0; i < 4; i++) {
    immAttr4fv(col, color);
    immVertex3f(pos, 0.0f, 0.0f, 0.0f);
    immAttr4fv(col, color);
    immVertex3fv(pos, corners[i]);
  }
  immEnd();

  immUnbindProgram();
  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_blend(GPU_BLEND_NONE);

  GPU_matrix_pop();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Location Scouting Viewfinder Interface Drawing
 * \{ */

static constexpr float viewfinder_alpha_accent_color[4] = {0.26f, 0.26f, 0.26f, 0.2f};

static void wm_xr_viewfinder_ui_draw_background(const rctf &viewfinder_rect)
{
  float background_col[3];
  ui::theme::get_color_3fv(TH_TAB_ACTIVE, background_col);

  rctf background_rect = viewfinder_rect;
  BLI_rctf_pad(&background_rect, 0.2f, 0.6f);
  BLI_rctf_translate(&background_rect, 0.0f, -0.1f);

  rctf outline_rect = viewfinder_rect;
  BLI_rctf_pad(&outline_rect, 0.08f, 0.08f);

  rctf tabs_bg_rect = {.xmin = background_rect.xmin,
                       .xmax = background_rect.xmin + 4.55f,
                       .ymin = background_rect.ymax - 0.3f,
                       .ymax = background_rect.ymax + 0.45f};

  GPU_matrix_push();
  /* Workaround: regain precision on the rect side by a factor of 100. */
  GPU_matrix_scale_1f(0.01f);
  BLI_rctf_mul(&background_rect, 100);
  BLI_rctf_mul(&tabs_bg_rect, 100);
  BLI_rctf_mul(&outline_rect, 100);

  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);

  GPU_matrix_translate_3f(0.0f, 0.0f, -1.5f);

  ui::draw_roundbox_3fv_alpha(&tabs_bg_rect, true, 16, background_col, 1.0f);
  GPU_matrix_translate_3f(0.0f, 0.0f, 0.5f);
  ui::draw_roundbox_3fv_alpha(&background_rect, true, 16, background_col, 1.0f);
  GPU_matrix_translate_3f(0.0f, 0.0f, 0.5f);
  ui::draw_roundbox_4fv(&outline_rect, true, 0, viewfinder_alpha_accent_color);

  GPU_matrix_pop();
}

static void wm_xr_viewfinder_ui_draw_texture(gpu::Texture *texture,
                                             const rctf &rect,
                                             const rctf &uv,
                                             const float color[4])
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);
  uint texco = GPU_vertformat_attr_add(format, "texCoord", gpu::VertAttrType::SFLOAT_32_32);

  GPU_blend(GPU_BLEND_ALPHA_PREMULT);
  immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);

  immUniformColor4fv(color);

  GPUSamplerExtendMode extend = GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER;
  immBindTextureSampler("image", texture, {GPU_SAMPLER_FILTERING_LINEAR, extend, extend});

  immRectf_with_texco(pos, texco, rect, uv);

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
}

static void wm_xr_viewfinder_ui_draw_view_texture(const bContext *C,
                                                  const wmXrSessionState *state,
                                                  const rctf &viewfinder_rect)
{
  PointerRNA scene_ptr = RNA_id_pointer_create(&CTX_data_scene(C)->id);
  const bool empty_captures = wm_xr_location_scouting_is_captures_empty(CTX_data_scene(C));

  if (state->viewfinder.active_mode == XR_VIEWFINDER_MODE_PLAYBACK && empty_captures) {
    return;
  }

  /* Obtain the Viewfinder view texture we computed in #wm_xr_draw_view. */
  gpu::Texture *view_tex = GPU_offscreen_color_texture(state->viewfinder.offscreen);

  constexpr rctf tex_uv = {0.0f, 1.0f, 0.0f, 1.0f};
  constexpr float tex_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  wm_xr_viewfinder_ui_draw_texture(view_tex, viewfinder_rect, tex_uv, tex_color);
}

static void wm_xr_viewfinder_ui_draw_backside_logo_texture(const wmXrSessionState *state,
                                                           const rctf &viewfinder_rect)
{
  gpu::Texture *logo_tex = state->viewfinder.backside_logo_texture;

  /* Fit logo within viewfinder while maintaining aspect ratio. */
  const float logo_aspect = GPU_texture_width(logo_tex) / GPU_texture_height(logo_tex);
  const float logo_width = BLI_rctf_size_x(&viewfinder_rect);
  const float logo_height = BLI_rctf_size_y(&viewfinder_rect);
  const float width = min_ff(logo_width, logo_height * logo_aspect);

  rctf logo_rect = viewfinder_rect;
  BLI_rctf_resize(&logo_rect, width, width / logo_aspect);

  /* Flip UV coords for horizontal mirror to draw on the backside of the viewfinder. */
  const rctf tex_uv = {1.0f, 0.0f, 0.0f, 1.0f};
  GPU_matrix_translate_3f(0.0f, 0.0f, -0.05f);
  wm_xr_viewfinder_ui_draw_texture(logo_tex, logo_rect, tex_uv, viewfinder_alpha_accent_color);
}

static void wm_xr_viewfinder_ui_draw_capture_overlays(const XrSessionSettings *settings,
                                                      const wmXrSessionState *state,
                                                      const rctf &vf_rect)
{
  /* Only draw overlays in Live capture mode, early return if there's no overlay to be drawn. */
  if (state->viewfinder.active_mode != XR_VIEWFINDER_MODE_LIVE) {
    return;
  }
  rctf capture_rect = vf_rect;
  BLI_rctf_resize(&capture_rect,
                  BLI_rctf_size_x(&vf_rect) / wm_xr_viewfinder_get_pp_overscan(settings),
                  BLI_rctf_size_y(&vf_rect) / wm_xr_viewfinder_get_pp_overscan(settings));

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", gpu::VertAttrType::SFLOAT_32_32);

  GPU_blend(GPU_BLEND_ALPHA);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* Passepartout. */
  immUniformColor4f(0.0f, 0.0f, 0.0f, settings->viewfinder_passepartout_opacity);

  /* Main passepartout dark border strips. */
  immRectf(pos, vf_rect.xmin, capture_rect.ymax, vf_rect.xmax, vf_rect.ymax);
  immRectf(pos, vf_rect.xmin, vf_rect.ymin, vf_rect.xmax, capture_rect.ymin);
  immRectf(pos, vf_rect.xmin, capture_rect.ymin, capture_rect.xmin, capture_rect.ymax);
  immRectf(pos, capture_rect.xmax, capture_rect.ymin, vf_rect.xmax, capture_rect.ymax);

  /* White wire frame around capture area for the passepartout to be visible at 0 opacity. */
  immUniformColor4f(1.0f, 1.0f, 1.0f, 0.2f);
  immBegin(GPU_PRIM_LINE_LOOP, 4);
  immVertex2f(pos, capture_rect.xmin, capture_rect.ymin);
  immVertex2f(pos, capture_rect.xmax, capture_rect.ymin);
  immVertex2f(pos, capture_rect.xmax, capture_rect.ymax);
  immVertex2f(pos, capture_rect.xmin, capture_rect.ymax);
  immEnd();

  /* Crosshair. */
  if (settings->viewfinder_crosshair_enabled) {
    const float center_x = BLI_rctf_cent_x(&capture_rect);
    const float center_y = BLI_rctf_cent_y(&capture_rect);
    const float crosshair_size = BLI_rctf_size_y(&capture_rect) * 0.1f;

    constexpr float base_col[4] = {1.0f, 1.0f, 0.0f, 0.8f};
    constexpr float focus_hit_col[4] = {0.0f, 1.0f, 1.0f, 0.8f};
    constexpr float focus_miss_col[4] = {1.0f, 0.0f, 0.0f, 0.8f};

    /* Interval during which the cursor changes color to indicate focus hit status. */
    constexpr double focus_hit_time = 0.15;
    const double last_focus_hit_delta = BLI_time_now_seconds() -
                                        state->viewfinder.last_focus_hit_time;

    const float *crosshair_col = base_col;

    /* Focus hit indicator color. */
    if (last_focus_hit_delta < focus_hit_time) {
      crosshair_col = state->viewfinder.last_focus_hit_success ? focus_hit_col : focus_miss_col;
    }

    immUniformColor4fv(crosshair_col);
    immBegin(GPU_PRIM_LINES, 4);
    /* Horizontal line. */
    immVertex2f(pos, center_x - crosshair_size, center_y);
    immVertex2f(pos, center_x + crosshair_size, center_y);
    /* Vertical line. */
    immVertex2f(pos, center_x, center_y - crosshair_size);
    immVertex2f(pos, center_x, center_y + crosshair_size);
    immEnd();
  }

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
}

static void wm_xr_viewfinder_ui_draw_capture_flash(wmXrSessionState *state,
                                                   const rctf &viewfinder_rect)
{
  /* Do not apply the flash effect if we're in playback mode. */
  if (state->viewfinder.active_mode == XR_VIEWFINDER_MODE_PLAYBACK) {
    state->viewfinder.last_flash_trigger_time = 0.0;
    return;
  }

  constexpr double flash_duration_sec = 0.4;
  constexpr float full_flash_alpha = 0.3f;

  const double last_flash_delta = BLI_time_now_seconds() -
                                  state->viewfinder.last_flash_trigger_time;

  if (last_flash_delta < flash_duration_sec) {
    const float flash_progress = last_flash_delta / flash_duration_sec;
    const float flash_alpha = interpf(0.0f, full_flash_alpha, flash_progress);

    GPUVertFormat *flash_format = immVertexFormat();
    uint flash_pos = GPU_vertformat_attr_add(flash_format, "pos", gpu::VertAttrType::SFLOAT_32_32);

    GPU_blend(GPU_BLEND_ALPHA);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4f(1.0f, 1.0f, 1.0f, flash_alpha);
    immRectf(flash_pos,
             viewfinder_rect.xmin,
             viewfinder_rect.ymin,
             viewfinder_rect.xmax,
             viewfinder_rect.ymax);
    immUnbindProgram();
    GPU_blend(GPU_BLEND_NONE);
  }
}

void wm_xr_viewfinder_draw(const bContext *C,
                           const XrSessionSettings *settings,
                           wmXrSessionState *state)
{
  if (!settings->viewfinder_enabled) {
    return;
  }

  const rctf viewfinder_rect = wm_xr_viewfinder_get_rect(C, settings);

  /* Initial transform setup. */
  float viewfinder_mat[4][4];
  const float viewfinder_height = BLI_rctf_size_y(&viewfinder_rect);
  if (!wm_xr_viewfinder_get_capture_mat(settings, state, viewfinder_height, viewfinder_mat)) {
    return;
  }

  GPU_matrix_push();
  GPU_matrix_mul(viewfinder_mat);
  GPU_matrix_scale_1f(xr_ui_unit_fac);

  /* Main background elements. */
  wm_xr_viewfinder_ui_draw_background(viewfinder_rect);

  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  GPU_depth_mask(false);

  /* Viewfinder view, capture flash and overlays. */
  wm_xr_viewfinder_ui_draw_view_texture(C, state, viewfinder_rect);
  wm_xr_viewfinder_ui_draw_capture_overlays(settings, state, viewfinder_rect);
  wm_xr_viewfinder_ui_draw_capture_flash(state, viewfinder_rect);

  /* UI Widgets. */
  wm_xr_viewfinder_ui_draw_widgets(C, state, viewfinder_rect);

  /* Logo on the back side of the viewfinder. */
  wm_xr_viewfinder_ui_draw_backside_logo_texture(state, viewfinder_rect);

  GPU_depth_mask(true);
  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_matrix_pop();

  /* Selected playback capture camera (drawn in world space). */
  wm_xr_viewfinder_gizmo_draw_capture_camera(C, state);
}

/** \} */

}  // namespace blender
