/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#include "DEG_depsgraph_query.hh"

#include "ED_view3d.hh"

#include "BKE_paint.hh"

#include "draw_debug.hh"

#include "overlay_next_instance.hh"

namespace blender::draw::overlay {

void Instance::init()
{
  /* TODO(fclem): Remove DRW global usage. */
  const DRWContextState *ctx = DRW_context_state_get();
  /* Was needed by `object_wire_theme_id()` when doing the port. Not sure if needed nowadays. */
  BKE_view_layer_synced_ensure(ctx->scene, ctx->view_layer);

  state.depsgraph = ctx->depsgraph;
  state.view_layer = ctx->view_layer;
  state.scene = ctx->scene;
  state.v3d = ctx->v3d;
  state.region = ctx->region;
  state.rv3d = ctx->rv3d;
  state.active_base = BKE_view_layer_active_base_get(ctx->view_layer);
  state.object_mode = ctx->object_mode;

  /* Note there might be less than 6 planes, but we always compute the 6 of them for simplicity. */
  state.clipping_plane_count = clipping_enabled_ ? 6 : 0;

  state.pixelsize = U.pixelsize;
  state.ctx_mode = CTX_data_mode_enum_ex(ctx->object_edit, ctx->obact, ctx->object_mode);
  state.space_type = state.v3d != nullptr ? SPACE_VIEW3D : eSpace_Type(ctx->space_data->spacetype);
  if (state.v3d != nullptr) {
    state.clear_in_front = (state.v3d->shading.type != OB_SOLID);
    state.use_in_front = (state.v3d->shading.type <= OB_SOLID) ||
                         BKE_scene_uses_blender_workbench(state.scene);
    state.is_wireframe_mode = (state.v3d->shading.type == OB_WIRE);
    state.hide_overlays = (state.v3d->flag2 & V3D_HIDE_OVERLAYS) != 0;
    state.xray_enabled = XRAY_ACTIVE(state.v3d);
    state.xray_enabled_and_not_wire = state.xray_enabled && (state.v3d->shading.type > OB_WIRE);
    state.xray_opacity = XRAY_ALPHA(state.v3d);
    state.cfra = DEG_get_ctime(state.depsgraph);

    if (!state.hide_overlays) {
      state.overlay = state.v3d->overlay;
      state.v3d_flag = state.v3d->flag;
      state.v3d_gridflag = state.v3d->gridflag;
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

    state.do_pose_xray = (state.overlay.flag & V3D_OVERLAY_BONE_SELECT);
    state.do_pose_fade_geom = state.do_pose_xray && !(state.object_mode & OB_MODE_WEIGHT_PAINT) &&
                              ctx->object_pose != nullptr;
  }

  /* TODO(fclem): Remove DRW global usage. */
  resources.globals_buf = G_draw.block_ubo;
  resources.theme_settings = G_draw.block;
  resources.weight_ramp_tx.wrap(G_draw.weight_ramp);
  {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
    if (resources.dummy_depth_tx.ensure_2d(GPU_DEPTH_COMPONENT32F, int2(1, 1), usage)) {
      float data = 1.0f;
      GPU_texture_update_sub(resources.dummy_depth_tx, GPU_DATA_FLOAT, &data, 0, 0, 0, 1, 1, 1);
    }
  }
}

void Instance::begin_sync()
{
  const DRWView *view_legacy = DRW_view_default_get();
  View view("OverlayView", view_legacy);
  state.dt = DRW_text_cache_ensure();
  state.camera_position = view.viewinv().location();
  state.camera_forward = view.viewinv().z_axis();

  resources.begin_sync();

  background.begin_sync(resources, state);
  outline.begin_sync(resources, state);

  auto begin_sync_layer = [&](OverlayLayer &layer) {
    layer.armatures.begin_sync(resources, state);
    layer.bounds.begin_sync();
    layer.cameras.begin_sync(resources, state, view);
    layer.curves.begin_sync(resources, state, view);
    layer.empties.begin_sync(resources, state, view);
    layer.facing.begin_sync(resources, state);
    layer.force_fields.begin_sync();
    layer.fluids.begin_sync(resources, state);
    layer.lattices.begin_sync(resources, state);
    layer.lights.begin_sync();
    layer.light_probes.begin_sync(resources, state);
    layer.metaballs.begin_sync();
    layer.meshes.begin_sync(resources, state, view);
    layer.particles.begin_sync(resources, state);
    layer.prepass.begin_sync(resources, state);
    layer.relations.begin_sync();
    layer.speakers.begin_sync();
    layer.sculpts.begin_sync(resources, state);
    layer.wireframe.begin_sync(resources, state);
  };
  begin_sync_layer(regular);
  begin_sync_layer(infront);

  grid.begin_sync(resources, state, view);

  anti_aliasing.begin_sync(resources);
  xray_fade.begin_sync(resources, state);
}

void Instance::object_sync(ObjectRef &ob_ref, Manager &manager)
{
  const bool in_edit_mode = object_is_edit_mode(ob_ref.object);
  const bool in_paint_mode = object_is_paint_mode(ob_ref.object);
  const bool in_sculpt_mode = object_is_sculpt_mode(ob_ref);
  const bool in_edit_paint_mode = object_is_edit_paint_mode(
      ob_ref, in_edit_mode, in_paint_mode, in_sculpt_mode);
  const bool needs_prepass = !state.xray_enabled; /* TODO */

  OverlayLayer &layer = object_is_in_front(ob_ref.object, state) ? infront : regular;

  if (needs_prepass) {
    layer.prepass.object_sync(manager, ob_ref, resources, state);
  }

  if (in_sculpt_mode) {
    layer.sculpts.object_sync(manager, ob_ref, state);
  }

  if (in_edit_mode && !state.hide_overlays) {
    switch (ob_ref.object->type) {
      case OB_MESH:
        layer.meshes.edit_object_sync(manager, ob_ref, state, resources);
        break;
      case OB_ARMATURE:
        layer.armatures.edit_object_sync(ob_ref, resources, shapes, state);
        break;
      case OB_CURVES_LEGACY:
        layer.curves.edit_object_sync_legacy(manager, ob_ref, resources);
        break;
      case OB_CURVES:
        layer.curves.edit_object_sync(manager, ob_ref, resources);
        break;
      case OB_SURF:
        break;
      case OB_LATTICE:
        layer.lattices.edit_object_sync(manager, ob_ref, resources);
        break;
      case OB_MBALL:
        layer.metaballs.edit_object_sync(ob_ref, resources);
        break;
      case OB_FONT:
        break;
    }
  }

  if (state.is_wireframe_mode || !state.hide_overlays) {
    layer.wireframe.object_sync(manager, ob_ref, resources, in_edit_paint_mode);
  }

  if (!state.hide_overlays) {
    switch (ob_ref.object->type) {
      case OB_EMPTY:
        layer.empties.object_sync(ob_ref, shapes, manager, resources, state);
        break;
      case OB_CAMERA:
        layer.cameras.object_sync(ob_ref, shapes, manager, resources, state);
        break;
      case OB_ARMATURE:
        if (!in_edit_mode) {
          layer.armatures.object_sync(ob_ref, resources, shapes, state);
        }
        break;
      case OB_LATTICE:
        if (!in_edit_mode) {
          layer.lattices.object_sync(manager, ob_ref, resources, state);
        }
        break;
      case OB_LAMP:
        layer.lights.object_sync(ob_ref, resources, state);
        break;
      case OB_LIGHTPROBE:
        layer.light_probes.object_sync(ob_ref, resources, state);
        break;
      case OB_MBALL:
        if (!in_edit_mode) {
          layer.metaballs.object_sync(ob_ref, resources, state);
        }
        break;
      case OB_GPENCIL_LEGACY:
        break;
      case OB_SPEAKER:
        layer.speakers.object_sync(ob_ref, resources, state);
        break;
    }
    layer.bounds.object_sync(ob_ref, resources, state);
    layer.facing.object_sync(manager, ob_ref, state);
    layer.force_fields.object_sync(ob_ref, resources, state);
    layer.fluids.object_sync(manager, ob_ref, resources, state);
    layer.particles.object_sync(manager, ob_ref, resources, state);
    layer.relations.object_sync(ob_ref, resources, state);

    if (object_is_selected(ob_ref) && !in_edit_paint_mode) {
      outline.object_sync(manager, ob_ref, state);
    }
  }
}

void Instance::end_sync()
{
  resources.end_sync();

  auto end_sync_layer = [&](OverlayLayer &layer) {
    layer.armatures.end_sync(resources, shapes, state);
    layer.bounds.end_sync(resources, shapes, state);
    layer.cameras.end_sync(resources, shapes, state);
    layer.empties.end_sync(resources, shapes, state);
    layer.force_fields.end_sync(resources, shapes, state);
    layer.lights.end_sync(resources, shapes, state);
    layer.light_probes.end_sync(resources, shapes, state);
    layer.metaballs.end_sync(resources, shapes, state);
    layer.relations.end_sync(resources, state);
    layer.fluids.end_sync(resources, shapes, state);
    layer.speakers.end_sync(resources, shapes, state);
  };
  end_sync_layer(regular);
  end_sync_layer(infront);

  /* WORKAROUND: This prevents bad frame-buffer config inside workbench when xray is enabled.
   * Better find a solution to this chicken-egg problem. */
  {
    /* HACK we allocate the in front depth here to avoid the overhead when if is not needed. */
    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

    DRW_texture_ensure_fullscreen_2d(
        &dtxl->depth_in_front, GPU_DEPTH24_STENCIL8, DRWTextureFlag(0));

    GPU_framebuffer_ensure_config(
        &dfbl->in_front_fb,
        {GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front), GPU_ATTACHMENT_TEXTURE(dtxl->color)});
  }
}

void Instance::draw(Manager &manager)
{
  resources.depth_tx.wrap(DRW_viewport_texture_list_get()->depth);
  resources.depth_in_front_tx.wrap(DRW_viewport_texture_list_get()->depth_in_front);
  resources.color_overlay_tx.wrap(DRW_viewport_texture_list_get()->color_overlay);
  resources.color_render_tx.wrap(DRW_viewport_texture_list_get()->color);

  resources.render_fb = DRW_viewport_framebuffer_list_get()->default_fb;
  resources.render_in_front_fb = DRW_viewport_framebuffer_list_get()->in_front_fb;

  int2 render_size = int2(resources.depth_tx.size());

  const DRWView *view_legacy = DRW_view_default_get();
  View view("OverlayView", view_legacy);

  /* TODO(fclem): Remove mandatory allocation. */
  if (!resources.depth_in_front_tx.is_valid()) {
    resources.depth_in_front_alloc_tx.acquire(render_size, GPU_DEPTH24_STENCIL8);
    resources.depth_in_front_tx.wrap(resources.depth_in_front_alloc_tx);
  }

  if (state.xray_enabled) {
    /* For X-ray we render the scene to a separate depth buffer. */
    resources.xray_depth_tx.acquire(render_size, GPU_DEPTH24_STENCIL8);
    resources.depth_target_tx.wrap(resources.xray_depth_tx);
    resources.depth_target_in_front_tx.wrap(resources.xray_depth_tx);
  }
  else {
    resources.depth_target_tx.wrap(resources.depth_tx);
    resources.depth_target_in_front_tx.wrap(resources.depth_in_front_tx);
  }

  /* TODO: Better semantics using a switch? */
  if (!resources.color_overlay_tx.is_valid()) {
    /* Likely to be the selection case. Allocate dummy texture and bind only depth buffer. */
    resources.color_overlay_alloc_tx.acquire(int2(1, 1), GPU_SRGB8_A8);
    resources.color_render_alloc_tx.acquire(int2(1, 1), GPU_SRGB8_A8);

    resources.color_overlay_tx.wrap(resources.color_overlay_alloc_tx);
    resources.color_render_tx.wrap(resources.color_render_alloc_tx);

    resources.line_tx.acquire(int2(1, 1), GPU_RGBA8);
    resources.overlay_tx.acquire(int2(1, 1), GPU_SRGB8_A8);

    resources.overlay_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_target_tx));
    resources.overlay_line_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_target_tx));
    resources.overlay_in_front_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_target_tx));
    resources.overlay_line_in_front_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_target_tx));
  }
  else {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                             GPU_TEXTURE_USAGE_ATTACHMENT;
    resources.line_tx.acquire(render_size, GPU_RGBA8, usage);
    resources.overlay_tx.acquire(render_size, GPU_SRGB8_A8, usage);

    resources.overlay_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_target_tx),
                                GPU_ATTACHMENT_TEXTURE(resources.overlay_tx));
    resources.overlay_line_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_target_tx),
                                     GPU_ATTACHMENT_TEXTURE(resources.overlay_tx),
                                     GPU_ATTACHMENT_TEXTURE(resources.line_tx));
    resources.overlay_in_front_fb.ensure(
        GPU_ATTACHMENT_TEXTURE(resources.depth_target_in_front_tx),
        GPU_ATTACHMENT_TEXTURE(resources.overlay_tx));
    resources.overlay_line_in_front_fb.ensure(
        GPU_ATTACHMENT_TEXTURE(resources.depth_target_in_front_tx),
        GPU_ATTACHMENT_TEXTURE(resources.overlay_tx),
        GPU_ATTACHMENT_TEXTURE(resources.line_tx));
  }

  resources.overlay_line_only_fb.ensure(GPU_ATTACHMENT_NONE,
                                        GPU_ATTACHMENT_TEXTURE(resources.overlay_tx),
                                        GPU_ATTACHMENT_TEXTURE(resources.line_tx));
  resources.overlay_color_only_fb.ensure(GPU_ATTACHMENT_NONE,
                                         GPU_ATTACHMENT_TEXTURE(resources.overlay_tx));
  resources.overlay_output_fb.ensure(GPU_ATTACHMENT_NONE,
                                     GPU_ATTACHMENT_TEXTURE(resources.color_overlay_tx));

  regular.sculpts.draw_on_render(resources.render_fb, manager, view);
  infront.sculpts.draw_on_render(resources.render_in_front_fb, manager, view);

  GPU_framebuffer_bind(resources.overlay_line_fb);
  float4 clear_color(0.0f);
  if (state.xray_enabled) {
    /* Rendering to a new depth buffer that needs to be cleared. */
    GPU_framebuffer_clear_color_depth(resources.overlay_line_fb, clear_color, 1.0f);
  }
  else {
    GPU_framebuffer_clear_color(resources.overlay_line_fb, clear_color);
  }

  regular.cameras.draw_scene_background_images(resources.overlay_color_only_fb, manager, view);
  infront.cameras.draw_scene_background_images(resources.overlay_color_only_fb, manager, view);

  regular.empties.draw_background_images(resources.overlay_color_only_fb, manager, view);
  regular.cameras.draw_background_images(resources.overlay_color_only_fb, manager, view);
  infront.cameras.draw_background_images(resources.overlay_color_only_fb, manager, view);

  regular.empties.draw_images(resources.overlay_fb, manager, view);

  regular.prepass.draw(resources.overlay_line_fb, manager, view);
  infront.prepass.draw(resources.overlay_line_in_front_fb, manager, view);

  outline.draw(resources, manager, view);

  auto overlay_fb_draw = [&](OverlayLayer &layer, Framebuffer &framebuffer) {
    layer.facing.draw(framebuffer, manager, view);
  };

  auto draw_layer = [&](OverlayLayer &layer, Framebuffer &framebuffer) {
    layer.bounds.draw(framebuffer, manager, view);
    layer.wireframe.draw(framebuffer, manager, view);
    layer.cameras.draw(framebuffer, manager, view);
    layer.empties.draw(framebuffer, manager, view);
    layer.force_fields.draw(framebuffer, manager, view);
    layer.lights.draw(framebuffer, manager, view);
    layer.light_probes.draw(framebuffer, manager, view);
    layer.speakers.draw(framebuffer, manager, view);
    layer.lattices.draw(framebuffer, manager, view);
    layer.metaballs.draw(framebuffer, manager, view);
    layer.relations.draw(framebuffer, manager, view);
    layer.fluids.draw(framebuffer, manager, view);
    layer.particles.draw(framebuffer, manager, view);
    layer.armatures.draw(framebuffer, manager, view);
    layer.sculpts.draw(framebuffer, manager, view);
    layer.meshes.draw(framebuffer, manager, view);
  };

  auto draw_layer_color_only = [&](OverlayLayer &layer, Framebuffer &framebuffer) {
    layer.light_probes.draw_color_only(framebuffer, manager, view);
    layer.meshes.draw_color_only(framebuffer, manager, view);
    layer.curves.draw_color_only(framebuffer, manager, view);
  };

  overlay_fb_draw(regular, resources.overlay_fb);
  draw_layer(regular, resources.overlay_line_fb);

  overlay_fb_draw(infront, resources.overlay_in_front_fb);
  draw_layer(infront, resources.overlay_line_in_front_fb);

  xray_fade.draw(resources.overlay_color_only_fb, manager, view);
  grid.draw(resources.overlay_color_only_fb, manager, view);

  draw_layer_color_only(regular, resources.overlay_color_only_fb);
  draw_layer_color_only(infront, resources.overlay_color_only_fb);

  infront.empties.draw_in_front_images(resources.overlay_color_only_fb, manager, view);
  regular.cameras.draw_in_front(resources.overlay_color_only_fb, manager, view);
  infront.cameras.draw_in_front(resources.overlay_color_only_fb, manager, view);

  background.draw(resources.overlay_output_fb, manager, view);
  anti_aliasing.draw(resources.overlay_output_fb, manager, view);

  resources.line_tx.release();
  resources.overlay_tx.release();
  resources.xray_depth_tx.release();
  resources.depth_in_front_alloc_tx.release();
  resources.color_overlay_alloc_tx.release();
  resources.color_render_alloc_tx.release();

  resources.read_result();
}

bool Instance::object_is_selected(const ObjectRef &ob_ref)
{
  return (ob_ref.object->base_flag & BASE_SELECTED);
}

bool Instance::object_is_paint_mode(const Object *object)
{
  if (object->type == OB_GREASE_PENCIL && (state.object_mode & OB_MODE_WEIGHT_GPENCIL_LEGACY)) {
    return true;
  }
  return state.active_base && (object == state.active_base->object) &&
         (state.object_mode & OB_MODE_ALL_PAINT);
}

bool Instance::object_is_sculpt_mode(const ObjectRef &ob_ref)
{
  if (state.object_mode == OB_MODE_SCULPT_CURVES) {
    const Object *active_object = state.active_base->object;
    const bool is_active_object = ob_ref.object == active_object;

    bool is_geonode_preview = ob_ref.dupli_object && ob_ref.dupli_object->preview_base_geometry;
    bool is_active_dupli_parent = ob_ref.dupli_parent == active_object;
    return is_active_object || (is_active_dupli_parent && is_geonode_preview);
  }

  if (state.object_mode == OB_MODE_SCULPT) {
    const Object *active_object = state.active_base->object;
    const bool is_active_object = ob_ref.object == active_object;
    return is_active_object;
  }

  return false;
}

bool Instance::object_is_sculpt_mode(const Object *object)
{
  if (object->sculpt && (object->sculpt->mode_type == OB_MODE_SCULPT)) {
    return object == state.active_base->object;
  }
  return false;
}

bool Instance::object_is_edit_paint_mode(const ObjectRef &ob_ref,
                                         bool in_edit_mode,
                                         bool in_paint_mode,
                                         bool in_sculpt_mode)
{
  bool in_edit_paint_mode = in_edit_mode || in_paint_mode || in_sculpt_mode;
  if (ob_ref.object->base_flag & BASE_FROM_DUPLI) {
    /* Disable outlines for objects instanced by an object in sculpt, paint or edit mode. */
    in_edit_paint_mode |= object_is_edit_mode(ob_ref.dupli_parent) ||
                          object_is_sculpt_mode(ob_ref.dupli_parent) ||
                          object_is_paint_mode(ob_ref.dupli_parent);
  }
  return in_edit_paint_mode;
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
        return state.ctx_mode == CTX_MODE_EDIT_POINT_CLOUD;
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
    case OB_MESH:
    case OB_CURVES_LEGACY:
    case OB_SURF:
    case OB_LATTICE:
    case OB_MBALL:
    case OB_FONT:
    case OB_CURVES:
    case OB_POINTCLOUD:
    case OB_VOLUME:
      return state.use_in_front && (object->dtx & OB_DRAW_IN_FRONT);
  }
  return false;
}

}  // namespace blender::draw::overlay
