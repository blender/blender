/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#include "BKE_colorband.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_view3d.hh"

#include "BKE_paint.hh"

#include "draw_debug.hh"
#include "overlay_instance.hh"

namespace blender::draw::overlay {

void Instance::init()
{
  /* TODO(fclem): Remove DRW global usage. */
  const DRWContext *ctx = DRW_context_get();
  /* Was needed by `object_wire_theme_id()` when doing the port. Not sure if needed nowadays. */
  BKE_view_layer_synced_ensure(ctx->scene, ctx->view_layer);

  clipping_enabled_ = RV3D_CLIPPING_ENABLED(ctx->v3d, ctx->rv3d);

  resources.init(clipping_enabled_);

  state.depsgraph = ctx->depsgraph;
  state.view_layer = ctx->view_layer;
  state.space_data = ctx->space_data;
  state.scene = ctx->scene;
  state.v3d = ctx->v3d;
  state.region = ctx->region;
  state.rv3d = ctx->rv3d;
  state.object_active = BKE_view_layer_active_object_get(ctx->view_layer);
  state.object_mode = ctx->object_mode;
  state.cfra = DEG_get_ctime(state.depsgraph);
  state.is_viewport_image_render = ctx->is_viewport_image_render();
  state.is_image_render = ctx->is_image_render();
  state.is_depth_only_drawing = ctx->is_depth();
  state.skip_particles = ctx->mode == DRWContext::DEPTH_ACTIVE_OBJECT;
  state.is_material_select = ctx->is_material_select();
  state.draw_background = ctx->options.draw_background;
  state.show_text = false;

  /* Note there might be less than 6 planes, but we always compute the 6 of them for simplicity. */
  state.clipping_plane_count = clipping_enabled_ ? 6 : 0;

  state.ctx_mode = CTX_data_mode_enum_ex(ctx->object_edit, ctx->obact, ctx->object_mode);
  state.space_data = ctx->space_data;
  state.space_type = state.v3d != nullptr ? SPACE_VIEW3D : eSpace_Type(ctx->space_data->spacetype);
  if (state.v3d != nullptr) {
    state.clear_in_front = (state.v3d->shading.type != OB_SOLID);
    /* TODO(pragma37): Check with @fclem if this was intentional. */
    // state.use_in_front = (state.v3d->shading.type <= OB_SOLID) ||
    //                      BKE_scene_uses_blender_workbench(state.scene);
    state.use_in_front = true;
    state.is_wireframe_mode = (state.v3d->shading.type == OB_WIRE);
    state.hide_overlays = (state.v3d->flag2 & V3D_HIDE_OVERLAYS) != 0;
    state.xray_enabled = XRAY_ACTIVE(state.v3d) && !state.is_depth_only_drawing;
    state.xray_enabled_and_not_wire = state.xray_enabled && (state.v3d->shading.type > OB_WIRE);
    state.xray_opacity = state.xray_enabled ? XRAY_ALPHA(state.v3d) : 1.0f;
    state.xray_flag_enabled = SHADING_XRAY_FLAG_ENABLED(state.v3d->shading) &&
                              !state.is_depth_only_drawing;
    state.vignette_enabled = ctx->mode == DRWContext::VIEWPORT_XR &&
                             state.v3d->vignette_aperture < M_SQRT1_2;

    const bool viewport_uses_workbench = state.v3d->shading.type <= OB_SOLID ||
                                         BKE_scene_uses_blender_workbench(state.scene);
    const bool viewport_uses_eevee = STREQ(
        ED_view3d_engine_type(state.scene, state.v3d->shading.type)->idname,
        RE_engine_id_BLENDER_EEVEE);
    const bool use_resolution_scaling = BKE_render_preview_pixel_size(&state.scene->r) != 1;
    /* Only workbench ensures the depth buffer is matching overlays.
     * Force depth prepass for other render engines.
     * EEVEE is an exception (if not using mixed resolution) to avoid a significant overhead. */
    state.is_render_depth_available = viewport_uses_workbench ||
                                      (viewport_uses_eevee && !use_resolution_scaling);

    /* For depth only drawing, no other render engine is expected. Except for Grease Pencil which
     * outputs valid depth. Otherwise depth is cleared and is valid. */
    state.is_render_depth_available |= state.is_depth_only_drawing;

    if (!state.hide_overlays) {
      state.overlay = state.v3d->overlay;
      state.v3d_flag = state.v3d->flag;
      state.v3d_gridflag = state.v3d->gridflag;
      state.show_text = !resources.is_selection() && !state.is_depth_only_drawing &&
                        (ctx->v3d->overlay.flag & V3D_OVERLAY_HIDE_TEXT) == 0;
    }
    else {
      memset(&state.overlay, 0, sizeof(state.overlay));
      state.v3d_flag = 0;
      state.v3d_gridflag = 0;
      state.overlay.flag = V3D_OVERLAY_HIDE_TEXT | V3D_OVERLAY_HIDE_MOTION_PATHS |
                           V3D_OVERLAY_HIDE_BONES | V3D_OVERLAY_HIDE_OBJECT_XTRAS |
                           V3D_OVERLAY_HIDE_OBJECT_ORIGINS;
      state.overlay.wireframe_threshold = state.v3d->overlay.wireframe_threshold;
      state.overlay.wireframe_opacity = state.v3d->overlay.wireframe_opacity;
    }

    state.do_pose_xray = state.show_bone_selection();
    state.do_pose_fade_geom = state.do_pose_xray && !(state.object_mode & OB_MODE_WEIGHT_PAINT) &&
                              ctx->object_pose != nullptr;
  }
  else if (state.is_space_image()) {
    SpaceImage *space_image = (SpaceImage *)state.space_data;

    state.clear_in_front = false;
    state.use_in_front = false;
    state.is_wireframe_mode = false;
    state.hide_overlays = (space_image->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS) == 0;
    state.xray_enabled = false;
    /* Avoid triggering the depth prepass. */
    state.is_render_depth_available = true;

    /* During engine initialization phase the `space_image` isn't locked and we are able to
     * retrieve the needed data. During cache_init the image engine locks the `space_image` and
     * makes it impossible to retrieve the data. */
    state.is_image_valid = bool(space_image->image);
    ED_space_image_get_uv_aspect(space_image, &state.image_uv_aspect.x, &state.image_uv_aspect.y);
    ED_space_image_get_size(space_image, &state.image_size.x, &state.image_size.y);
    ED_space_image_get_aspect(space_image, &state.image_aspect.x, &state.image_aspect.y);
  }

  resources.update_theme_settings(ctx, state);
  resources.update_clip_planes(state);

  ensure_weight_ramp_texture();

  {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
    if (resources.dummy_depth_tx.ensure_2d(gpu::TextureFormat::SFLOAT_32_DEPTH, int2(1, 1), usage))
    {
      float data = 1.0f;
      GPU_texture_update_sub(resources.dummy_depth_tx, GPU_DATA_FLOAT, &data, 0, 0, 0, 1, 1, 1);
    }
  }
}

void Instance::ensure_weight_ramp_texture()
{
  /* Weight Painting color ramp texture */
  bool user_weight_ramp = (U.flag & USER_CUSTOM_RANGE) != 0;

  auto is_equal = [](const ColorBand &a, const ColorBand &b) {
    if (a.tot != b.tot || a.cur != b.cur || a.ipotype != b.ipotype ||
        a.ipotype_hue != b.ipotype_hue || a.color_mode != b.color_mode)
    {
      return false;
    }

    auto is_equal_cbd = [](const CBData &a, const CBData &b) {
      return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a && a.pos == b.pos &&
             a.cur == b.cur;
    };

    for (int i = 0; i < ARRAY_SIZE(a.data); i++) {
      if (!is_equal_cbd(a.data[i], b.data[i])) {
        return false;
      }
    }
    return true;
  };

  if (assign_if_different(resources.weight_ramp_custom, user_weight_ramp)) {
    resources.weight_ramp_tx.free();
  }

  if (user_weight_ramp && !is_equal(resources.weight_ramp_copy, U.coba_weight)) {
    resources.weight_ramp_copy = U.coba_weight;
    resources.weight_ramp_tx.free();
  }

  if (resources.weight_ramp_tx.is_valid()) {
    /* Only recreate on updates. */
    return;
  }

  auto evaluate_weight_to_color = [&](const float weight, float result[4]) {
    if (user_weight_ramp) {
      BKE_colorband_evaluate(&U.coba_weight, weight, result);
    }
    else {
      /* Use gamma correction to even out the color bands:
       * increasing widens yellow/cyan vs red/green/blue.
       * Gamma 1.0 produces the original 2.79 color ramp. */
      const float gamma = 1.5f;
      const float hsv[3] = {
          (2.0f / 3.0f) * (1.0f - weight), 1.0f, pow(0.5f + 0.5f * weight, gamma)};

      hsv_to_rgb_v(hsv, result);

      for (int i = 0; i < 3; i++) {
        result[i] = pow(result[i], 1.0f / gamma);
      }
    }
  };

  constexpr int res = 256;

  float pixels[res][4];
  for (int i = 0; i < res; i++) {
    evaluate_weight_to_color(i / 255.0f, pixels[i]);
    pixels[i][3] = 1.0f;
  }

  uchar4 pixels_ubyte[res];
  for (int i = 0; i < res; i++) {
    unit_float_to_uchar_clamp_v4(pixels_ubyte[i], pixels[i]);
  }

  resources.weight_ramp_tx.ensure_1d(
      gpu::TextureFormat::SRGBA_8_8_8_8, res, GPU_TEXTURE_USAGE_SHADER_READ);
  GPU_texture_update(resources.weight_ramp_tx, GPU_DATA_UBYTE, pixels_ubyte);
}

void Resources::update_clip_planes(const State &state)
{
  if (!state.is_space_v3d() || state.clipping_plane_count == 0) {
    /* Unused, do not care about content but still fulfill the bindings. */
    clip_planes_buf.push_update();
    return;
  }

  for (int i : IndexRange(6)) {
    clip_planes_buf[i] = float4(0);
  }

  int plane_len = (RV3D_LOCK_FLAGS(state.rv3d) & RV3D_BOXCLIP) ? 4 : 6;
  for (int i : IndexRange(plane_len)) {
    clip_planes_buf[i] = float4(state.rv3d->clip[i]);
  }

  clip_planes_buf.push_update();
}

void Resources::update_theme_settings(const DRWContext *ctx, const State &state)
{
  using namespace math;
  UniformData &gb = theme;

  auto rgba_uchar_to_float = [](uchar r, uchar g, uchar b, uchar a) {
    return float4(r, g, b, a) / 255.0f;
  };

  UI_GetThemeColor4fv(TH_WIRE, gb.colors.wire);
  UI_GetThemeColor4fv(TH_WIRE_EDIT, gb.colors.wire_edit);
  UI_GetThemeColor4fv(TH_ACTIVE, gb.colors.active_object);
  UI_GetThemeColor4fv(TH_SELECT, gb.colors.object_select);
  gb.colors.library_select = rgba_uchar_to_float(0x88, 0xFF, 0xFF, 155);
  gb.colors.library = rgba_uchar_to_float(0x55, 0xCC, 0xCC, 155);
  UI_GetThemeColor4fv(TH_TRANSFORM, gb.colors.transform);
  UI_GetThemeColor4fv(TH_LIGHT, gb.colors.light);
  UI_GetThemeColor4fv(TH_SPEAKER, gb.colors.speaker);
  UI_GetThemeColor4fv(TH_CAMERA, gb.colors.camera);
  UI_GetThemeColor4fv(TH_CAMERA_PATH, gb.colors.camera_path);
  UI_GetThemeColor4fv(TH_EMPTY, gb.colors.empty);
  UI_GetThemeColor4fv(TH_VERTEX, gb.colors.vert);
  UI_GetThemeColor4fv(TH_VERTEX_SELECT, gb.colors.vert_select);
  UI_GetThemeColor4fv(TH_VERTEX_UNREFERENCED, gb.colors.vert_unreferenced);
  gb.colors.vert_missing_data = rgba_uchar_to_float(0xB0, 0x00, 0xB0, 0xFF);
  UI_GetThemeColor4fv(TH_EDITMESH_ACTIVE, gb.colors.edit_mesh_active);
  UI_GetThemeColor4fv(TH_EDGE_SELECT, gb.colors.edge_select);
  UI_GetThemeColor4fv(TH_EDGE_MODE_SELECT, gb.colors.edge_mode_select);
  UI_GetThemeColor4fv(TH_GP_VERTEX, gb.colors.gpencil_vertex);
  UI_GetThemeColor4fv(TH_GP_VERTEX_SELECT, gb.colors.gpencil_vertex_select);

  UI_GetThemeColor4fv(TH_SEAM, gb.colors.edge_seam);
  UI_GetThemeColor4fv(TH_SHARP, gb.colors.edge_sharp);
  UI_GetThemeColor4fv(TH_CREASE, gb.colors.edge_crease);
  UI_GetThemeColor4fv(TH_BEVEL, gb.colors.edge_bweight);
  UI_GetThemeColor4fv(TH_FACE, gb.colors.face);
  UI_GetThemeColor4fv(TH_FACE_SELECT, gb.colors.face_select);
  UI_GetThemeColor4fv(TH_FACE_MODE_SELECT, gb.colors.face_mode_select);
  UI_GetThemeColor4fv(TH_FACE_RETOPOLOGY, gb.colors.face_retopology);
  UI_GetThemeColor4fv(TH_FACE_BACK, gb.colors.face_back);
  UI_GetThemeColor4fv(TH_FACE_FRONT, gb.colors.face_front);
  UI_GetThemeColor4fv(TH_NORMAL, gb.colors.normal);
  UI_GetThemeColor4fv(TH_VNORMAL, gb.colors.vnormal);
  UI_GetThemeColor4fv(TH_LNORMAL, gb.colors.lnormal);
  UI_GetThemeColor4fv(TH_FACE_SELECT, gb.colors.facedot), gb.colors.facedot[3] = 1.0f;
  UI_GetThemeColor4fv(TH_SKIN_ROOT, gb.colors.skinroot);
  UI_GetThemeColor4fv(TH_BACK, gb.colors.background);
  UI_GetThemeColor4fv(TH_BACK_GRAD, gb.colors.background_gradient);
  UI_GetThemeColor4fv(TH_TRANSPARENT_CHECKER_PRIMARY, gb.colors.checker_primary);
  UI_GetThemeColor4fv(TH_TRANSPARENT_CHECKER_SECONDARY, gb.colors.checker_secondary);
  gb.sizes.checker = UI_GetThemeValuef(TH_TRANSPARENT_CHECKER_SIZE);
  gb.fresnel_mix_edit = ((U.gpu_flag & USER_GPU_FLAG_FRESNEL_EDIT) == 0) ? 0.0f : 1.0f;
  UI_GetThemeColor4fv(TH_V3D_CLIPPING_BORDER, gb.colors.clipping_border);

  /* Custom median color to slightly affect the edit mesh colors. */
  gb.colors.edit_mesh_middle = interpolate(gb.colors.vert_select, gb.colors.wire_edit, 0.35f);
  /* Desaturate. */
  gb.colors.edit_mesh_middle = float4(
      float3(dot(gb.colors.edit_mesh_middle.xyz(), float3(0.3333f))),
      gb.colors.edit_mesh_middle.w);

#ifdef WITH_FREESTYLE
  UI_GetThemeColor4fv(TH_FREESTYLE, gb.colors.edge_freestyle), gb.colors.edge_freestyle[3] = 1.0f;
  UI_GetThemeColor4fv(TH_FREESTYLE, gb.colors.face_freestyle);
#else
  gb.colors.edge_freestyle = float4(0.0f);
  gb.colors.face_freestyle = float4(0.0f);
#endif

  UI_GetThemeColor4fv(TH_TEXT, gb.colors.text);
  UI_GetThemeColor4fv(TH_TEXT_HI, gb.colors.text_hi);

  /* Bone colors */
  UI_GetThemeColor4fv(TH_BONE_POSE, gb.colors.bone_pose);
  UI_GetThemeColor4fv(TH_BONE_POSE_ACTIVE, gb.colors.bone_pose_active);
  UI_GetThemeColorShade4fv(TH_EDGE_SELECT, 60, gb.colors.bone_active);
  UI_GetThemeColorShade4fv(TH_EDGE_SELECT, -20, gb.colors.bone_select);
  UI_GetThemeColorBlendShade4fv(TH_WIRE, TH_BONE_POSE, 0.15f, 0, gb.colors.bone_pose_active_unsel);
  UI_GetThemeColorBlendShade3fv(
      TH_WIRE_EDIT, TH_EDGE_SELECT, 0.15f, 0, gb.colors.bone_active_unsel);
  gb.colors.bone_pose_no_target = rgba_uchar_to_float(255, 150, 0, 80);
  gb.colors.bone_pose_ik = rgba_uchar_to_float(255, 255, 0, 80);
  gb.colors.bone_pose_spline_ik = rgba_uchar_to_float(200, 255, 0, 80);
  gb.colors.bone_pose_constraint = rgba_uchar_to_float(0, 255, 120, 80);
  UI_GetThemeColor4fv(TH_BONE_SOLID, gb.colors.bone_solid);
  UI_GetThemeColor4fv(TH_BONE_LOCKED_WEIGHT, gb.colors.bone_locked);
  gb.colors.bone_ik_line = float4(0.8f, 0.8f, 0.0f, 1.0f);
  gb.colors.bone_ik_line_no_target = float4(0.8f, 0.5f, 0.2f, 1.0f);
  gb.colors.bone_ik_line_spline = float4(0.8f, 0.8f, 0.2f, 1.0f);

  /* Curve */
  UI_GetThemeColor4fv(TH_HANDLE_FREE, gb.colors.handle_free);
  UI_GetThemeColor4fv(TH_HANDLE_AUTO, gb.colors.handle_auto);
  UI_GetThemeColor4fv(TH_HANDLE_VECT, gb.colors.handle_vect);
  UI_GetThemeColor4fv(TH_HANDLE_ALIGN, gb.colors.handle_align);
  UI_GetThemeColor4fv(TH_HANDLE_AUTOCLAMP, gb.colors.handle_autoclamp);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_FREE, gb.colors.handle_sel_free);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_AUTO, gb.colors.handle_sel_auto);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_VECT, gb.colors.handle_sel_vect);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_ALIGN, gb.colors.handle_sel_align);
  UI_GetThemeColor4fv(TH_HANDLE_SEL_AUTOCLAMP, gb.colors.handle_sel_autoclamp);
  UI_GetThemeColor4fv(TH_NURB_ULINE, gb.colors.nurb_uline);
  UI_GetThemeColor4fv(TH_NURB_VLINE, gb.colors.nurb_vline);
  UI_GetThemeColor4fv(TH_NURB_SEL_ULINE, gb.colors.nurb_sel_uline);
  UI_GetThemeColor4fv(TH_NURB_SEL_VLINE, gb.colors.nurb_sel_vline);

  UI_GetThemeColor4fv(TH_CFRAME, gb.colors.current_frame);
  UI_GetThemeColor4fv(TH_FRAME_BEFORE, gb.colors.before_frame);
  UI_GetThemeColor4fv(TH_FRAME_AFTER, gb.colors.after_frame);

  /* Meta-ball. */
  gb.colors.mball_radius = rgba_uchar_to_float(0xA0, 0x30, 0x30, 0xFF);
  gb.colors.mball_radius_select = rgba_uchar_to_float(0xF0, 0xA0, 0xA0, 0xFF);
  gb.colors.mball_stiffness = rgba_uchar_to_float(0x30, 0xA0, 0x30, 0xFF);
  gb.colors.mball_stiffness_select = rgba_uchar_to_float(0xA0, 0xF0, 0xA0, 0xFF);

  /* Grid */
  UI_GetThemeColorShade4fv(TH_GRID, 10, gb.colors.grid);
  /* Emphasize division lines lighter instead of darker, if background is darker than grid. */
  const bool is_bg_darker = reduce_add(gb.colors.grid.xyz()) + 0.12f >
                            reduce_add(gb.colors.background.xyz());
  UI_GetThemeColorShade4fv(TH_GRID, (is_bg_darker) ? 20 : -10, gb.colors.grid_emphasis);
  /* Grid Axis */
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_X, 0.5f, -10, gb.colors.grid_axis_x);
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_Y, 0.5f, -10, gb.colors.grid_axis_y);
  UI_GetThemeColorBlendShade4fv(TH_GRID, TH_AXIS_Z, 0.5f, -10, gb.colors.grid_axis_z);

  UI_GetThemeColorShadeAlpha4fv(TH_TRANSFORM, 0, -80, gb.colors.deselect);
  UI_GetThemeColorShadeAlpha4fv(TH_WIRE, 0, -30, gb.colors.outline);
  UI_GetThemeColorShadeAlpha4fv(TH_LIGHT, 0, 255, gb.colors.light_no_alpha);

  /* UV colors */
  UI_GetThemeColor4fv(TH_UV_SHADOW, gb.colors.uv_shadow);

  /* Color management. */
  {
    float4 *color = reinterpret_cast<float4 *>(&gb.colors);
    float4 *color_end = color + (sizeof(gb.colors) / sizeof(float4));
    do {
      /* TODO: more accurate transform. */
      srgb_to_linearrgb_v4(&color->x, &color->x);
    } while (++color <= color_end);
  }

  gb.sizes.pixel = 1.0f;
  gb.sizes.object_center = UI_GetThemeValuef(TH_OBCENTER_DIA) + 1.0f;
  gb.sizes.light_center = UI_GetThemeValuef(TH_OBCENTER_DIA) + 1.5f;
  gb.sizes.light_circle = 9.0f;
  gb.sizes.light_circle_shadow = (gb.sizes.light_circle + 3.0f);

  /* M_SQRT2 to be at least the same size of the old square */
  gb.sizes.vert = vertex_size_get();
  gb.sizes.vertex_gpencil = UI_GetThemeValuef(TH_GP_VERTEX_SIZE);
  gb.sizes.face_dot = UI_GetThemeValuef(TH_FACEDOT_SIZE);
  gb.sizes.edge = max_ff(1.0f, UI_GetThemeValuef(TH_EDGE_WIDTH)) / 2.0f;

  /* Pixel size. */
  {
    float *size = reinterpret_cast<float *>(&gb.sizes);
    float *size_end = size + (sizeof(gb.sizes) / sizeof(float));
    do {
      *size *= U.pixelsize;
    } while (++size <= size_end);
  }

  gb.pixel_fac = (state.rv3d) ? state.rv3d->pixsize : 1.0f;
  gb.size_viewport = ctx->viewport_size_get();
  gb.size_viewport_inv = 1.0f / gb.size_viewport;

  if (state.v3d) {
    const View3DShading &shading = state.v3d->shading;
    gb.backface_culling = (shading.type == OB_SOLID) &&
                          (shading.flag & V3D_SHADING_BACKFACE_CULLING);

    if (is_selection() || state.is_depth_only_drawing) {
      /* This is bad as this makes a solid mode setting affect material preview / render mode
       * selection and auto-depth. But users are relying on this to work in scene using backface
       * culling in shading (see #136335 and #136418). */
      gb.backface_culling = (shading.flag & V3D_SHADING_BACKFACE_CULLING);
    }
  }
  else {
    gb.backface_culling = false;
  }

  globals_buf.push_update();
}

void Instance::begin_sync()
{
  /* TODO(fclem): Against design. Should not sync depending on view. */
  View &view = View::default_get();
  state.camera_position = view.viewinv().location();
  state.camera_forward = view.viewinv().z_axis();

  DRW_text_cache_destroy(state.dt);
  state.dt = DRW_text_cache_create();

  resources.begin_sync(state.clipping_plane_count);

  background.begin_sync(resources, state);
  cursor.begin_sync(resources, state);
  image_prepass.begin_sync(resources, state);
  motion_paths.begin_sync(resources, state);
  origins.begin_sync(resources, state);
  outline.begin_sync(resources, state);

  auto begin_sync_layer = [&](OverlayLayer &layer) {
    layer.armatures.begin_sync(resources, state);
    layer.attribute_viewer.begin_sync(resources, state);
    layer.attribute_texts.begin_sync(resources, state);
    layer.axes.begin_sync(resources, state);
    layer.bounds.begin_sync(resources, state);
    layer.cameras.begin_sync(resources, state);
    layer.curves.begin_sync(resources, state);
    layer.text.begin_sync(resources, state);
    layer.empties.begin_sync(resources, state);
    layer.facing.begin_sync(resources, state);
    layer.fade.begin_sync(resources, state);
    layer.force_fields.begin_sync(resources, state);
    layer.fluids.begin_sync(resources, state);
    layer.grease_pencil.begin_sync(resources, state);
    layer.lattices.begin_sync(resources, state);
    layer.lights.begin_sync(resources, state);
    layer.light_probes.begin_sync(resources, state);
    layer.metaballs.begin_sync(resources, state);
    layer.meshes.begin_sync(resources, state);
    layer.mesh_uvs.begin_sync(resources, state);
    layer.mode_transfer.begin_sync(resources, state);
    layer.names.begin_sync(resources, state);
    layer.paints.begin_sync(resources, state);
    layer.particles.begin_sync(resources, state);
    layer.pointclouds.begin_sync(resources, state);
    layer.prepass.begin_sync(resources, state);
    layer.relations.begin_sync(resources, state);
    layer.speakers.begin_sync(resources, state);
    layer.sculpts.begin_sync(resources, state);
    layer.wireframe.begin_sync(resources, state);
  };
  begin_sync_layer(regular);
  begin_sync_layer(infront);

  grid.begin_sync(resources, state);

  anti_aliasing.begin_sync(resources, state);
  xray_fade.begin_sync(resources, state);
}

void Instance::object_sync(ObjectRef &ob_ref, Manager &manager)
{
  const bool in_object_mode = ob_ref.object->mode == OB_MODE_OBJECT;
  const bool in_edit_mode = ob_ref.object->mode == OB_MODE_EDIT;
  const bool in_paint_mode = object_is_paint_mode(ob_ref.object);
  const bool in_sculpt_mode = object_is_sculpt_mode(ob_ref);
  const bool in_particle_edit_mode = object_is_particle_edit_mode(ob_ref);
  const bool in_edit_paint_mode = object_is_edit_paint_mode(
      ob_ref, in_edit_mode, in_paint_mode, in_sculpt_mode);
  const bool needs_prepass = object_needs_prepass(ob_ref, in_paint_mode);

  OverlayLayer &layer = object_is_in_front(ob_ref.object, state) ? infront : regular;

  layer.mode_transfer.object_sync(manager, ob_ref, resources, state);

  if (needs_prepass) {
    layer.prepass.object_sync(manager, ob_ref, resources, state);
  }

  if (in_particle_edit_mode) {
    layer.particles.edit_object_sync(manager, ob_ref, resources, state);
  }

  /* For 2D UV overlays. */
  if (!state.hide_overlays && state.is_space_image()) {
    switch (ob_ref.object->type) {
      case OB_MESH:
        if (in_edit_paint_mode) {
          /* TODO(fclem): Find a better place / condition. */
          layer.mesh_uvs.edit_object_sync(manager, ob_ref, resources, state);
        }
        else if (in_object_mode) {
          layer.mesh_uvs.object_sync(manager, ob_ref, resources, state);
        }
      default:
        break;
    }
  }

  if (in_paint_mode && !state.hide_overlays) {
    switch (ob_ref.object->type) {
      case OB_MESH:
        /* TODO(fclem): Make it part of a #Meshes. */
        layer.paints.object_sync(manager, ob_ref, resources, state);
        break;
      case OB_GREASE_PENCIL:
        layer.grease_pencil.paint_object_sync(manager, ob_ref, resources, state);
        break;
      default:
        break;
    }
  }

  if (in_sculpt_mode) {
    switch (ob_ref.object->type) {
      case OB_MESH:
      case OB_CURVES:
        /* TODO(fclem): Make it part of a #Meshes. */
        layer.sculpts.object_sync(manager, ob_ref, resources, state);
        break;
      case OB_GREASE_PENCIL:
        layer.grease_pencil.sculpt_object_sync(manager, ob_ref, resources, state);
        break;
      default:
        break;
    }
  }

  if (in_edit_mode && !state.hide_overlays) {
    switch (ob_ref.object->type) {
      case OB_MESH:
        layer.meshes.edit_object_sync(manager, ob_ref, resources, state);
        break;
      case OB_ARMATURE:
        layer.armatures.edit_object_sync(manager, ob_ref, resources, state);
        break;
      case OB_SURF:
      case OB_CURVES_LEGACY:
        layer.curves.edit_object_sync_legacy(manager, ob_ref, resources);
        break;
      case OB_CURVES:
        layer.curves.edit_object_sync(manager, ob_ref, resources, state);
        break;
      case OB_LATTICE:
        layer.lattices.edit_object_sync(manager, ob_ref, resources, state);
        break;
      case OB_MBALL:
        layer.metaballs.edit_object_sync(manager, ob_ref, resources, state);
        break;
      case OB_POINTCLOUD:
        layer.pointclouds.edit_object_sync(manager, ob_ref, resources, state);
        break;
      case OB_FONT:
        layer.text.edit_object_sync(manager, ob_ref, resources, state);
        break;
      case OB_GREASE_PENCIL:
        layer.grease_pencil.edit_object_sync(manager, ob_ref, resources, state);
        break;
    }
  }

  if (state.is_wireframe_mode || !state.hide_overlays) {
    layer.wireframe.object_sync_ex(
        manager, ob_ref, resources, state, in_edit_paint_mode, in_edit_mode);
  }

  if (!state.hide_overlays) {
    switch (ob_ref.object->type) {
      case OB_EMPTY:
        layer.empties.object_sync(manager, ob_ref, resources, state);
        break;
      case OB_CAMERA:
        layer.cameras.object_sync(manager, ob_ref, resources, state);
        break;
      case OB_ARMATURE:
        if (!in_edit_mode) {
          layer.armatures.object_sync(manager, ob_ref, resources, state);
        }
        break;
      case OB_LATTICE:
        if (!in_edit_mode) {
          layer.lattices.object_sync(manager, ob_ref, resources, state);
        }
        break;
      case OB_LAMP:
        layer.lights.object_sync(manager, ob_ref, resources, state);
        break;
      case OB_LIGHTPROBE:
        layer.light_probes.object_sync(manager, ob_ref, resources, state);
        break;
      case OB_MBALL:
        if (!in_edit_mode) {
          layer.metaballs.object_sync(manager, ob_ref, resources, state);
        }
        break;
      case OB_GREASE_PENCIL:
        layer.grease_pencil.object_sync(manager, ob_ref, resources, state);
        break;
      case OB_SPEAKER:
        layer.speakers.object_sync(manager, ob_ref, resources, state);
        break;
    }
    layer.attribute_viewer.object_sync(manager, ob_ref, resources, state);
    layer.attribute_texts.object_sync(manager, ob_ref, resources, state);
    layer.bounds.object_sync(manager, ob_ref, resources, state);
    layer.facing.object_sync(manager, ob_ref, resources, state);
    layer.fade.object_sync(manager, ob_ref, resources, state);
    layer.force_fields.object_sync(manager, ob_ref, resources, state);
    layer.fluids.object_sync(manager, ob_ref, resources, state);
    layer.particles.object_sync(manager, ob_ref, resources, state);
    layer.relations.object_sync(manager, ob_ref, resources, state);
    layer.axes.object_sync(manager, ob_ref, resources, state);
    layer.names.object_sync(manager, ob_ref, resources, state);

    motion_paths.object_sync(manager, ob_ref, resources, state);
    origins.object_sync(manager, ob_ref, resources, state);

    if (object_is_selected(ob_ref) && !in_edit_paint_mode) {
      outline.object_sync(manager, ob_ref, resources, state);
    }
  }
}

void Instance::end_sync()
{
  origins.end_sync(resources, state);
  resources.end_sync();

  auto end_sync_layer = [&](OverlayLayer &layer) {
    layer.armatures.end_sync(resources, state);
    layer.axes.end_sync(resources, state);
    layer.bounds.end_sync(resources, state);
    layer.cameras.end_sync(resources, state);
    layer.text.end_sync(resources, state);
    layer.empties.end_sync(resources, state);
    layer.force_fields.end_sync(resources, state);
    layer.lights.end_sync(resources, state);
    layer.light_probes.end_sync(resources, state);
    layer.mesh_uvs.end_sync(resources, state);
    layer.metaballs.end_sync(resources, state);
    layer.relations.end_sync(resources, state);
    layer.fluids.end_sync(resources, state);
    layer.speakers.end_sync(resources, state);
  };
  end_sync_layer(regular);
  end_sync_layer(infront);

  /* WORKAROUND: This prevents bad frame-buffer config inside workbench when xray is enabled.
   * Better find a solution to this chicken-egg problem. */
  {
    const DRWContext *draw_ctx = DRW_context_get();
    /* HACK we allocate the in front depth here to avoid the overhead when if is not needed. */
    DefaultFramebufferList *dfbl = draw_ctx->viewport_framebuffer_list_get();
    DefaultTextureList *dtxl = draw_ctx->viewport_texture_list_get();

    if (dtxl->depth_in_front == nullptr) {
      int2 size = int2(draw_ctx->viewport_size_get());

      dtxl->depth_in_front = GPU_texture_create_2d("txl.depth_in_front",
                                                   size.x,
                                                   size.y,
                                                   1,
                                                   gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8,
                                                   GPU_TEXTURE_USAGE_GENERAL,
                                                   nullptr);
    }

    GPU_framebuffer_ensure_config(
        &dfbl->in_front_fb,
        {GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front), GPU_ATTACHMENT_TEXTURE(dtxl->color)});
  }
}

void Instance::draw(Manager &manager)
{
  /* TODO(fclem): Remove global access. */
  View &view = View::default_get();

  static gpu::DebugScope select_scope = {"Selection"};
  static gpu::DebugScope draw_scope = {"Overlay"};
  static gpu::DebugScope depth_scope = {"DepthOnly"};

  if (state.is_depth_only_drawing) {
    depth_scope.begin_capture();
  }
  else if (resources.is_selection()) {
    select_scope.begin_capture();
  }
  else {
    draw_scope.begin_capture();
  }

  /* TODO(fclem): To be moved to overlay UBO. */
  state.ndc_offset_factor = state.offset_data_get().polygon_offset_factor(view.winmat());

  resources.pre_draw();

  outline.flat_objects_pass_sync(manager, view, resources, state);
  GreasePencil::compute_depth_planes(manager, view, resources, state);

  /* Pre-Draw: Run the compute steps of all passes up-front
   * to avoid constant GPU compute/raster context switching. */
  {
    manager.ensure_visibility(view);

    auto pre_draw = [&](OverlayLayer &layer) {
      layer.attribute_viewer.pre_draw(manager, view);
      layer.cameras.pre_draw(manager, view);
      layer.empties.pre_draw(manager, view);
      layer.facing.pre_draw(manager, view);
      layer.fade.pre_draw(manager, view);
      layer.lattices.pre_draw(manager, view);
      layer.light_probes.pre_draw(manager, view);
      layer.particles.pre_draw(manager, view);
      layer.pointclouds.pre_draw(manager, view);
      layer.prepass.pre_draw(manager, view);
      layer.wireframe.pre_draw(manager, view);
    };

    pre_draw(regular);
    pre_draw(infront);

    outline.pre_draw(manager, view);
  }

  resources.acquire(DRW_context_get(), this->state);

  DRW_submission_start();

  /* TODO(fclem): Would be better to have a v2d overlay class instead of these conditions. */
  switch (state.space_type) {
    case SPACE_NODE:
      draw_node(manager, view);
      break;
    case SPACE_IMAGE:
      draw_v2d(manager, view);
      break;
    case SPACE_VIEW3D:
      draw_v3d(manager, view);
      break;
    default:
      BLI_assert_unreachable();
  }

  DRW_submission_end();

  resources.release();

  resources.read_result();

  if (state.is_depth_only_drawing) {
    depth_scope.end_capture();
  }
  else if (resources.is_selection()) {
    select_scope.end_capture();
  }
  else {
    draw_scope.end_capture();
  }
}

void Instance::draw_node(Manager &manager, View &view)
{
  /* Don't clear background for the node editor. The node editor draws the background and we
   * need to mask out the image from the already drawn overlay color buffer. */
  background.draw_output(resources.overlay_output_color_only_fb, manager, view);
}

void Instance::draw_v2d(Manager &manager, View &view)
{
  image_prepass.draw_on_render(resources.render_fb, manager, view);
  regular.mesh_uvs.draw_on_render(resources.render_fb, manager, view);

  GPU_framebuffer_bind(resources.overlay_output_color_only_fb);
  GPU_framebuffer_clear_color(resources.overlay_output_color_only_fb, float4(0.0));

  background.draw_output(resources.overlay_output_color_only_fb, manager, view);
  grid.draw_color_only(resources.overlay_output_color_only_fb, manager, view);
  regular.mesh_uvs.draw(resources.overlay_output_fb, manager, view);

  cursor.draw_output(resources.overlay_output_color_only_fb, manager, view);
}

void Instance::draw_v3d(Manager &manager, View &view)
{
  float4 clear_color(0.0f);

  auto draw = [&](OverlayLayer &layer, Framebuffer &framebuffer) {
    /* TODO(fclem): Depth aware outlines (see #130751). */
    // layer.facing.draw(framebuffer, manager, view);
    layer.fade.draw(framebuffer, manager, view);
    layer.mode_transfer.draw(framebuffer, manager, view);
    layer.text.draw(framebuffer, manager, view);
    layer.paints.draw(framebuffer, manager, view);
    layer.particles.draw(framebuffer, manager, view);
  };

  auto draw_line = [&](OverlayLayer &layer, Framebuffer &framebuffer) {
    layer.bounds.draw_line(framebuffer, manager, view);
    layer.wireframe.draw_line(framebuffer, manager, view);
    layer.cameras.draw_line(framebuffer, manager, view);
    layer.empties.draw_line(framebuffer, manager, view);
    layer.axes.draw_line(framebuffer, manager, view);
    layer.force_fields.draw_line(framebuffer, manager, view);
    layer.lights.draw_line(framebuffer, manager, view);
    layer.light_probes.draw_line(framebuffer, manager, view);
    layer.speakers.draw_line(framebuffer, manager, view);
    layer.lattices.draw_line(framebuffer, manager, view);
    layer.metaballs.draw_line(framebuffer, manager, view);
    layer.pointclouds.draw_line(framebuffer, manager, view);
    layer.relations.draw_line(framebuffer, manager, view);
    layer.fluids.draw_line(framebuffer, manager, view);
    layer.particles.draw_line(framebuffer, manager, view);
    layer.attribute_viewer.draw_line(framebuffer, manager, view);
    layer.armatures.draw_line(framebuffer, manager, view);
    layer.sculpts.draw_line(framebuffer, manager, view);
    layer.grease_pencil.draw_line(framebuffer, manager, view);
    /* NOTE: Temporarily moved after grid drawing (See #136764). */
    // layer.meshes.draw_line(framebuffer, manager, view);
    layer.curves.draw_line(framebuffer, manager, view);
  };

  auto draw_color_only = [&](OverlayLayer &layer, Framebuffer &framebuffer) {
    layer.light_probes.draw_color_only(framebuffer, manager, view);
    layer.meshes.draw_color_only(framebuffer, manager, view);
    layer.curves.draw_color_only(framebuffer, manager, view);
    layer.grease_pencil.draw_color_only(framebuffer, manager, view);
  };

  {
    /* Render pass. Draws directly on render result (instead of overlay result). */
    /* TODO(fclem): Split overlay and rename draw functions. */
    regular.cameras.draw_scene_background_images(resources.render_fb, manager, view);
    infront.cameras.draw_scene_background_images(resources.render_in_front_fb, manager, view);

    regular.sculpts.draw_on_render(resources.render_fb, manager, view);
    infront.sculpts.draw_on_render(resources.render_in_front_fb, manager, view);
  }
  {
    /* Overlay Line prepass. */
    GPU_framebuffer_bind(resources.overlay_line_fb);
    if (state.xray_enabled) {
      /* Rendering to a new depth buffer that needs to be cleared. */
      GPU_framebuffer_clear_color_depth(resources.overlay_line_fb, clear_color, 1.0f);
    }
    else {
      if (!state.is_render_depth_available) {
        /* If the render engine is not outputting correct depth,
         * clear the depth and render a depth prepass. */
        GPU_framebuffer_clear_color_depth(resources.overlay_line_fb, clear_color, 1.0f);
      }
      else {
        GPU_framebuffer_clear_color(resources.overlay_line_fb, clear_color);
      }
    }

    if (BLI_thread_is_main() && !state.hide_overlays) {
      DebugDraw::get().display_to_view(view);
    }

    regular.prepass.draw_line(resources.overlay_line_fb, manager, view);

    /* TODO(fclem): Split overlay and rename draw functions. */
    /* TODO(fclem): Draw on line framebuffer. */
    regular.empties.draw_images(resources.overlay_fb, manager, view);

    if (state.xray_enabled || (state.v3d && state.v3d->shading.type > OB_SOLID)) {
      /* If workbench is not enabled, the infront buffer might contain garbage. */
      GPU_framebuffer_bind(resources.overlay_line_in_front_fb);
      GPU_framebuffer_clear_depth(resources.overlay_line_in_front_fb, 1.0f);
    }

    infront.prepass.draw_line(resources.overlay_line_in_front_fb, manager, view);
  }
  {
    /* Copy depth at the end of the prepass to avoid splitting the main render pass. */
    /* TODO(fclem): Better get rid of it. */
    regular.wireframe.copy_depth(resources.depth_target_tx);
    infront.wireframe.copy_depth(resources.depth_target_in_front_tx);
  }
  {
    /* TODO(fclem): This is really bad for performance as the outline pass will then split the
     * render pass and do a framebuffer switch. This also only fix the issue for non-infront
     * objects.
     * We need to figure a way to merge the outline with correct depth awareness (see #130751). */
    regular.facing.draw(resources.overlay_fb, manager, view);

    /* Line only pass. */
    outline.draw_line_only_ex(resources.overlay_line_only_fb, resources, manager, view);
  }
  {
    /* Overlay (+Line) pass. */
    draw(regular, resources.overlay_fb);
    draw_line(regular, resources.overlay_line_fb);

    /* Here because of custom order of regular.facing. */
    infront.facing.draw(resources.overlay_fb, manager, view);

    draw(infront, resources.overlay_in_front_fb);
    draw_line(infront, resources.overlay_line_in_front_fb);
  }
  {
    /* Color only pass. */
    motion_paths.draw_color_only(resources.overlay_color_only_fb, manager, view);
    xray_fade.draw_color_only(resources.overlay_color_only_fb, manager, view);
    grid.draw_color_only(resources.overlay_color_only_fb, manager, view);

    regular.meshes.draw_line(resources.overlay_line_fb, manager, view);
    infront.meshes.draw_line(resources.overlay_line_in_front_fb, manager, view);

    draw_color_only(regular, resources.overlay_color_only_fb);
    draw_color_only(infront, resources.overlay_color_only_fb);

    /* TODO(fclem): Split overlay and rename draw functions. */
    regular.empties.draw_in_front_images(resources.overlay_color_only_fb, manager, view);
    infront.empties.draw_in_front_images(resources.overlay_color_only_fb, manager, view);
    regular.cameras.draw_in_front(resources.overlay_color_only_fb, manager, view);
    infront.cameras.draw_in_front(resources.overlay_color_only_fb, manager, view);

    origins.draw_color_only(resources.overlay_color_only_fb, manager, view);
  }

  if (state.is_depth_only_drawing == false) {
    /* Output pass. */
    GPU_framebuffer_bind(resources.overlay_output_color_only_fb);
    GPU_framebuffer_clear_color(resources.overlay_output_color_only_fb, clear_color);

    /* TODO(fclem): Split overlay and rename draw functions. */
    regular.cameras.draw_background_images(resources.overlay_output_color_only_fb, manager, view);
    infront.cameras.draw_background_images(resources.overlay_output_color_only_fb, manager, view);
    regular.empties.draw_background_images(resources.overlay_output_color_only_fb, manager, view);

    background.draw_output(resources.overlay_output_color_only_fb, manager, view);
    anti_aliasing.draw_output(resources.overlay_output_color_only_fb, manager, view);
    cursor.draw_output(resources.overlay_output_color_only_fb, manager, view);

    draw_text(resources.overlay_output_color_only_fb);

    if (state.vignette_enabled) {
      background.draw_vignette(resources.overlay_output_color_only_fb, manager, view);
    }
  }
}

void Instance::draw_text(Framebuffer &framebuffer)
{
  if (state.show_text == false) {
    return;
  }
  GPU_framebuffer_bind(framebuffer);

  GPU_depth_test(GPU_DEPTH_NONE);
  DRW_text_cache_draw(state.dt, state.region, state.v3d);
}

bool Instance::object_is_selected(const ObjectRef &ob_ref)
{
  return (ob_ref.object->base_flag & BASE_SELECTED);
}

bool Instance::object_is_paint_mode(const Object *object)
{
  return (object == state.object_active) &&
         (state.object_mode & (OB_MODE_ALL_PAINT | OB_MODE_ALL_PAINT_GPENCIL));
}

bool Instance::object_is_sculpt_mode(const ObjectRef &ob_ref)
{
  if (state.object_mode == OB_MODE_SCULPT_CURVES) {
    const Object *active_object = state.object_active;
    const bool is_active_object = ob_ref.object == active_object;

    bool is_active_geonode_preview = ob_ref.preview_base_geometry() != nullptr &&
                                     ob_ref.is_active(state.object_active);
    return is_active_object || is_active_geonode_preview;
  }

  if (state.object_mode == OB_MODE_SCULPT) {
    const Object *active_object = state.object_active;
    const bool is_active_object = ob_ref.object == active_object;
    return is_active_object;
  }

  return false;
}

bool Instance::object_is_particle_edit_mode(const ObjectRef &ob_ref)
{
  return (ob_ref.object->mode == OB_MODE_PARTICLE_EDIT) && (state.ctx_mode == CTX_MODE_PARTICLE);
}

bool Instance::object_is_sculpt_mode(const Object *object)
{
  if (object->sculpt && (object->sculpt->mode_type == OB_MODE_SCULPT)) {
    return object == state.object_active;
  }
  return false;
}

bool Instance::object_is_edit_paint_mode(const ObjectRef &ob_ref,
                                         bool in_edit_mode,
                                         bool in_paint_mode,
                                         bool in_sculpt_mode)
{
  bool in_edit_paint_mode = in_edit_mode || in_paint_mode || in_sculpt_mode;
  /* Disable outlines for objects instanced by an object in sculpt, paint or edit mode. */
  return in_edit_paint_mode || ob_ref.parent_is_in_edit_paint_mode(
                                   state.object_active, state.object_mode, state.ctx_mode);
}

bool Instance::object_is_edit_mode(const Object *object)
{
  if (DRW_object_is_in_edit_mode(object)) {
    /* Also check for context mode as the object mode is not 100% reliable. (see T72490) */
    switch (object->type) {
      case OB_MESH:
        return state.ctx_mode == CTX_MODE_EDIT_MESH;
      case OB_ARMATURE:
        return state.ctx_mode == CTX_MODE_EDIT_ARMATURE;
      case OB_CURVES_LEGACY:
        return state.ctx_mode == CTX_MODE_EDIT_CURVE;
      case OB_SURF:
        return state.ctx_mode == CTX_MODE_EDIT_SURFACE;
      case OB_LATTICE:
        return state.ctx_mode == CTX_MODE_EDIT_LATTICE;
      case OB_MBALL:
        return state.ctx_mode == CTX_MODE_EDIT_METABALL;
      case OB_FONT:
        return state.ctx_mode == CTX_MODE_EDIT_TEXT;
      case OB_CURVES:
        return state.ctx_mode == CTX_MODE_EDIT_CURVES;
      case OB_POINTCLOUD:
        return state.ctx_mode == CTX_MODE_EDIT_POINTCLOUD;
      case OB_GREASE_PENCIL:
        return state.ctx_mode == CTX_MODE_EDIT_GREASE_PENCIL;
      case OB_VOLUME:
        /* No edit mode yet. */
        return false;
    }
  }
  return false;
}

bool Instance::object_is_in_front(const Object *object, const State &state)
{
  switch (object->type) {
    case OB_ARMATURE:
      return (object->dtx & OB_DRAW_IN_FRONT) ||
             (state.do_pose_xray && Armatures::is_pose_mode(object, state));
    default:
      return state.use_in_front && (object->dtx & OB_DRAW_IN_FRONT);
  }
}

bool Instance::object_needs_prepass(const ObjectRef &ob_ref, bool in_paint_mode)
{
  if (resources.is_selection() && state.is_wireframe_mode && !state.is_solid()) {
    /* Selection in wireframe mode only use wires unless xray opacity is 1. */
    return false;
  }

  if (resources.is_selection() || state.is_depth_only_drawing) {
    if (ob_ref.object->visibility_flag & OB_HIDE_SURFACE_PICK) {
      /* Special flag to avoid surfaces to contribute to depth picking and selection. */
      return false;
    }
    /* Selection and depth picking always need a prepass.
     * Note that depth writing and depth test might be disable for certain selection mode. */
    return true;
  }

  if (in_paint_mode) {
    /* Allow paint overlays to draw with depth equal test. */
    if (object_is_rendered_transparent(ob_ref.object, state) ||
        object_is_in_front(ob_ref.object, state))
    {
      return true;
    }
  }

  if (!state.xray_enabled) {
    /* Force depth prepass if depth buffer form render engine is not available. */
    if (!state.is_render_depth_available && (ob_ref.object->dt >= OB_SOLID)) {
      return true;
    }
  }

  return false;
}

bool Instance::object_is_rendered_transparent(const Object *object, const State &state)
{
  if (state.v3d == nullptr) {
    return false;
  }

  if (!state.is_solid()) {
    return true;
  }

  if (ELEM(object->dt, OB_WIRE, OB_BOUNDBOX)) {
    return true;
  }

  const View3DShading &shading = state.v3d->shading;

  if (shading.type == OB_WIRE) {
    return true;
  }

  if (shading.type > OB_SOLID) {
    return false;
  }

  if (shading.color_type == V3D_SHADING_OBJECT_COLOR) {
    return object->color[3] < 1.0f;
  }

  if (shading.color_type == V3D_SHADING_MATERIAL_COLOR) {
    if (object->type == OB_MESH) {
      const int materials_num = BKE_object_material_used_with_fallback_eval(*object);
      for (int i = 0; i < materials_num; i++) {
        Material *mat = BKE_object_material_get_eval(const_cast<Object *>(object), i + 1);
        if (mat && mat->a < 1.0f) {
          return true;
        }
      }
    }
  }

  return false;
}

}  // namespace blender::draw::overlay
