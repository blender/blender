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
  state.space_data = ctx->space_data;
  state.scene = ctx->scene;
  state.v3d = ctx->v3d;
  state.region = ctx->region;
  state.rv3d = ctx->rv3d;
  state.active_base = BKE_view_layer_active_base_get(ctx->view_layer);
  state.object_active = ctx->obact;
  state.object_mode = ctx->object_mode;
  state.cfra = DEG_get_ctime(state.depsgraph);

  /* Note there might be less than 6 planes, but we always compute the 6 of them for simplicity. */
  state.clipping_plane_count = clipping_enabled_ ? 6 : 0;

  state.pixelsize = U.pixelsize;
  state.ctx_mode = CTX_data_mode_enum_ex(ctx->object_edit, ctx->obact, ctx->object_mode);
  state.space_data = ctx->space_data;
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
  else if (state.space_type == SPACE_IMAGE) {
    SpaceImage *space_image = (SpaceImage *)state.space_data;

    state.clear_in_front = false;
    state.use_in_front = false;
    state.is_wireframe_mode = false;
    state.hide_overlays = (space_image->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS) == 0;
    state.xray_enabled = false;

    /* During engine initialization phase the `space_image` isn't locked and we are able to
     * retrieve the needed data. During cache_init the image engine locks the `space_image` and
     * makes it impossible to retrieve the data. */
    ED_space_image_get_uv_aspect(space_image, &state.image_uv_aspect.x, &state.image_uv_aspect.y);
    ED_space_image_get_size(space_image, &state.image_size.x, &state.image_size.y);
    ED_space_image_get_aspect(space_image, &state.image_aspect.x, &state.image_aspect.y);
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
  motion_paths.begin_sync(resources, state);
  origins.begin_sync(state);
  outline.begin_sync(resources, state);

  auto begin_sync_layer = [&](OverlayLayer &layer) {
    layer.armatures.begin_sync(resources, state);
    layer.attribute_viewer.begin_sync(resources, state);
    layer.axes.begin_sync(resources, state);
    layer.bounds.begin_sync();
    layer.cameras.begin_sync(resources, state, view);
    layer.curves.begin_sync(resources, state, view);
    layer.edit_text.begin_sync(state);
    layer.empties.begin_sync(resources, state, view);
    layer.facing.begin_sync(resources, state);
    layer.fade.begin_sync(resources, state);
    layer.force_fields.begin_sync();
    layer.fluids.begin_sync(resources, state);
    layer.grease_pencil.begin_sync(resources, state, view);
    layer.lattices.begin_sync(resources, state);
    layer.lights.begin_sync(state);
    layer.light_probes.begin_sync(resources, state);
    layer.metaballs.begin_sync();
    layer.meshes.begin_sync(resources, state, view);
    layer.mesh_uvs.begin_sync(resources, state);
    layer.mode_transfer.begin_sync(resources, state);
    layer.names.begin_sync(resources, state);
    layer.paints.begin_sync(resources, state);
    layer.particles.begin_sync(resources, state);
    layer.prepass.begin_sync(resources, state);
    layer.relations.begin_sync(resources, state);
    layer.speakers.begin_sync();
    layer.sculpts.begin_sync(resources, state);
    layer.wireframe.begin_sync(resources, state);
  };
  begin_sync_layer(regular);
  begin_sync_layer(infront);

  grid.begin_sync(resources, shapes, state, view);

  anti_aliasing.begin_sync(resources);
  xray_fade.begin_sync(resources, state);
}

void Instance::object_sync(ObjectRef &ob_ref, Manager &manager)
{
  const bool in_edit_mode = ob_ref.object->mode == OB_MODE_EDIT;
  const bool in_paint_mode = object_is_paint_mode(ob_ref.object);
  const bool in_sculpt_mode = object_is_sculpt_mode(ob_ref);
  const bool in_particle_edit_mode = object_is_particle_edit_mode(ob_ref);
  const bool in_edit_paint_mode = object_is_edit_paint_mode(
      ob_ref, in_edit_mode, in_paint_mode, in_sculpt_mode);
  const bool needs_prepass = object_needs_prepass(ob_ref, in_paint_mode);

  OverlayLayer &layer = object_is_in_front(ob_ref.object, state) ? infront : regular;

  layer.mode_transfer.object_sync(manager, ob_ref, state);

  if (needs_prepass) {
    layer.prepass.object_sync(manager, ob_ref, resources, state);
  }

  if (in_particle_edit_mode) {
    layer.particles.edit_object_sync(manager, ob_ref, resources, state);
  }

  if (in_paint_mode) {
    switch (ob_ref.object->type) {
      case OB_MESH:
        /* TODO(fclem): Make it part of a #Meshes. */
        layer.paints.object_sync(manager, ob_ref, state);
        break;
      case OB_GREASE_PENCIL:
        layer.grease_pencil.paint_object_sync(manager, ob_ref, state, resources);
        break;
      default:
        break;
    }
  }

  if (in_sculpt_mode) {
    switch (ob_ref.object->type) {
      case OB_MESH:
        /* TODO(fclem): Make it part of a #Meshes. */
        layer.sculpts.object_sync(manager, ob_ref, state);
        break;
      case OB_GREASE_PENCIL:
        layer.grease_pencil.sculpt_object_sync(manager, ob_ref, state, resources);
        break;
      default:
        break;
    }
  }

  if (in_edit_mode && !state.hide_overlays) {
    switch (ob_ref.object->type) {
      case OB_MESH:
        layer.meshes.edit_object_sync(manager, ob_ref, state, resources);
        /* TODO(fclem): Find a better place / condition. */
        layer.mesh_uvs.edit_object_sync(manager, ob_ref, state);
        break;
      case OB_ARMATURE:
        layer.armatures.edit_object_sync(ob_ref, resources, shapes, state);
        break;
      case OB_SURF:
      case OB_CURVES_LEGACY:
        layer.curves.edit_object_sync_legacy(manager, ob_ref, resources);
        break;
      case OB_CURVES:
        layer.curves.edit_object_sync(manager, ob_ref, resources);
        break;
      case OB_LATTICE:
        layer.lattices.edit_object_sync(manager, ob_ref, resources);
        break;
      case OB_MBALL:
        layer.metaballs.edit_object_sync(ob_ref, resources);
        break;
      case OB_FONT:
        layer.edit_text.edit_object_sync(ob_ref, resources);
        break;
      case OB_GREASE_PENCIL:
        layer.grease_pencil.edit_object_sync(manager, ob_ref, state, resources);
        break;
    }
  }

  if (state.is_wireframe_mode || !state.hide_overlays) {
    layer.wireframe.object_sync(manager, ob_ref, state, resources, in_edit_paint_mode);
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
      case OB_GREASE_PENCIL:
        layer.grease_pencil.object_sync(ob_ref, resources, state);
        break;
      case OB_SPEAKER:
        layer.speakers.object_sync(ob_ref, resources, state);
        break;
    }
    layer.attribute_viewer.object_sync(ob_ref, state, manager);
    layer.bounds.object_sync(ob_ref, resources, state);
    layer.facing.object_sync(manager, ob_ref, state);
    layer.fade.object_sync(manager, ob_ref, state);
    layer.force_fields.object_sync(ob_ref, resources, state);
    layer.fluids.object_sync(manager, ob_ref, resources, state);
    layer.particles.object_sync(manager, ob_ref, resources, state);
    layer.relations.object_sync(ob_ref, resources, state);
    layer.axes.object_sync(ob_ref, resources, state);
    layer.names.object_sync(ob_ref, resources, state);

    motion_paths.object_sync(ob_ref, resources, state);
    origins.object_sync(ob_ref, resources, state);

    if (object_is_selected(ob_ref) && !in_edit_paint_mode) {
      outline.object_sync(manager, ob_ref, state);
    }
  }
}

void Instance::end_sync()
{
  origins.end_sync(resources, state);
  resources.end_sync();

  auto end_sync_layer = [&](OverlayLayer &layer) {
    layer.armatures.end_sync(resources, shapes, state);
    layer.axes.end_sync(resources, shapes, state);
    layer.bounds.end_sync(resources, shapes, state);
    layer.cameras.end_sync(resources, shapes, state);
    layer.edit_text.end_sync(resources, shapes, state);
    layer.empties.end_sync(resources, shapes, state);
    layer.force_fields.end_sync(resources, shapes, state);
    layer.lights.end_sync(resources, shapes, state);
    layer.light_probes.end_sync(resources, shapes, state);
    layer.mesh_uvs.end_sync(resources, shapes, state);
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
  const DRWView *view_legacy = DRW_view_default_get();
  View view("OverlayView", view_legacy);

  /* Pre-Draw: Run the compute steps of all passes up-front
   * to avoid constant GPU compute/raster context switching. */
  {
    manager.compute_visibility(view);

    auto pre_draw = [&](OverlayLayer &layer) {
      layer.attribute_viewer.pre_draw(manager, view);
      layer.cameras.pre_draw(manager, view);
      layer.empties.pre_draw(manager, view);
      layer.facing.pre_draw(manager, view);
      layer.fade.pre_draw(manager, view);
      layer.lattices.pre_draw(manager, view);
      layer.light_probes.pre_draw(manager, view);
      layer.particles.pre_draw(manager, view);
      layer.prepass.pre_draw(manager, view);
      layer.wireframe.pre_draw(manager, view);
    };

    pre_draw(regular);
    pre_draw(infront);
    outline.pre_draw(manager, view);
  }

  resources.depth_tx.wrap(DRW_viewport_texture_list_get()->depth);
  resources.depth_in_front_tx.wrap(DRW_viewport_texture_list_get()->depth_in_front);
  resources.color_overlay_tx.wrap(DRW_viewport_texture_list_get()->color_overlay);
  resources.color_render_tx.wrap(DRW_viewport_texture_list_get()->color);

  resources.render_fb = DRW_viewport_framebuffer_list_get()->default_fb;
  resources.render_in_front_fb = DRW_viewport_framebuffer_list_get()->in_front_fb;

  int2 render_size = int2(resources.depth_tx.size());

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

  static gpu::DebugScope select_scope = {"Selection"};
  static gpu::DebugScope draw_scope = {"Overlay"};

  if (resources.selection_type != SelectionType::DISABLED) {
    select_scope.begin_capture();
  }
  else {
    draw_scope.begin_capture();
  }

  regular.cameras.draw_scene_background_images(resources.render_fb, manager, view);
  infront.cameras.draw_scene_background_images(resources.render_fb, manager, view);

  regular.sculpts.draw_on_render(resources.render_fb, manager, view);
  regular.mesh_uvs.draw_on_render(resources.render_fb, manager, view);
  infront.sculpts.draw_on_render(resources.render_in_front_fb, manager, view);
  regular.mesh_uvs.draw_on_render(resources.render_in_front_fb, manager, view);

  GPU_framebuffer_bind(resources.overlay_line_fb);
  float4 clear_color(0.0f);
  if (state.xray_enabled) {
    /* Rendering to a new depth buffer that needs to be cleared. */
    GPU_framebuffer_clear_color_depth(resources.overlay_line_fb, clear_color, 1.0f);
  }
  else {
    GPU_framebuffer_clear_color(resources.overlay_line_fb, clear_color);
  }

  /* TODO(fclem): Would be better to have a v2d overlay class instead of this condition. */
  if (state.space_type == SPACE_IMAGE) {
    grid.draw(resources.overlay_color_only_fb, manager, view);
  }

  regular.empties.draw_images(resources.overlay_fb, manager, view);

  regular.prepass.draw(resources.overlay_line_fb, manager, view);
  infront.prepass.draw(resources.overlay_line_in_front_fb, manager, view);

  outline.draw(resources, manager, view);

  auto overlay_fb_draw = [&](OverlayLayer &layer, Framebuffer &framebuffer) {
    layer.facing.draw(framebuffer, manager, view);
    layer.fade.draw(framebuffer, manager, view);
    layer.mode_transfer.draw(framebuffer, manager, view);
    layer.edit_text.draw(framebuffer, manager, view);
    layer.paints.draw(framebuffer, manager, view);
    layer.particles.draw_no_line(framebuffer, manager, view);
  };

  auto draw_layer = [&](OverlayLayer &layer, Framebuffer &framebuffer) {
    layer.bounds.draw(framebuffer, manager, view);
    layer.wireframe.draw(framebuffer, resources, manager, view);
    layer.cameras.draw(framebuffer, manager, view);
    layer.empties.draw(framebuffer, manager, view);
    layer.axes.draw(framebuffer, manager, view);
    layer.force_fields.draw(framebuffer, manager, view);
    layer.lights.draw(framebuffer, manager, view);
    layer.light_probes.draw(framebuffer, manager, view);
    layer.speakers.draw(framebuffer, manager, view);
    layer.lattices.draw(framebuffer, manager, view);
    layer.metaballs.draw(framebuffer, manager, view);
    layer.relations.draw(framebuffer, manager, view);
    layer.fluids.draw(framebuffer, manager, view);
    layer.particles.draw(framebuffer, manager, view);
    layer.attribute_viewer.draw(framebuffer, manager, view);
    layer.armatures.draw(framebuffer, manager, view);
    layer.sculpts.draw(framebuffer, manager, view);
    layer.grease_pencil.draw(framebuffer, manager, view);
    layer.meshes.draw(framebuffer, manager, view);
    layer.mesh_uvs.draw(framebuffer, manager, view);
    layer.curves.draw(framebuffer, manager, view);
  };

  auto draw_layer_color_only = [&](OverlayLayer &layer, Framebuffer &framebuffer) {
    layer.light_probes.draw_color_only(framebuffer, manager, view);
    layer.meshes.draw_color_only(framebuffer, manager, view);
    layer.curves.draw_color_only(framebuffer, manager, view);
    layer.grease_pencil.draw_color_only(framebuffer, manager, view);
  };

  overlay_fb_draw(regular, resources.overlay_fb);
  draw_layer(regular, resources.overlay_line_fb);

  overlay_fb_draw(infront, resources.overlay_in_front_fb);
  draw_layer(infront, resources.overlay_line_in_front_fb);

  motion_paths.draw_color_only(resources.overlay_color_only_fb, manager, view);
  xray_fade.draw(resources.overlay_color_only_fb, manager, view);
  if (state.space_type != SPACE_IMAGE) {
    grid.draw(resources.overlay_color_only_fb, manager, view);
  }

  draw_layer_color_only(regular, resources.overlay_color_only_fb);
  draw_layer_color_only(infront, resources.overlay_color_only_fb);

  regular.empties.draw_in_front_images(resources.overlay_color_only_fb, manager, view);
  infront.empties.draw_in_front_images(resources.overlay_color_only_fb, manager, view);
  regular.cameras.draw_in_front(resources.overlay_color_only_fb, manager, view);
  infront.cameras.draw_in_front(resources.overlay_color_only_fb, manager, view);

  origins.draw(resources.overlay_color_only_fb, manager, view);

  /* Don't clear background for the node editor. The node editor draws the background and we
   * need to mask out the image from the already drawn overlay color buffer. */
  if (state.space_type != SPACE_NODE) {
    GPU_framebuffer_bind(resources.overlay_output_fb);
    GPU_framebuffer_clear_color(resources.overlay_output_fb, clear_color);
  }

  regular.cameras.draw_background_images(resources.overlay_output_fb, manager, view);
  infront.cameras.draw_background_images(resources.overlay_output_fb, manager, view);
  regular.empties.draw_background_images(resources.overlay_output_fb, manager, view);

  background.draw(resources.overlay_output_fb, manager, view);
  anti_aliasing.draw(resources.overlay_output_fb, manager, view);

  resources.line_tx.release();
  resources.overlay_tx.release();
  resources.xray_depth_tx.release();
  resources.depth_in_front_alloc_tx.release();
  resources.color_overlay_alloc_tx.release();
  resources.color_render_alloc_tx.release();

  resources.read_result();

  if (resources.selection_type != SelectionType::DISABLED) {
    select_scope.end_capture();
  }
  else {
    draw_scope.end_capture();
  }
}

bool Instance::object_is_selected(const ObjectRef &ob_ref)
{
  return (ob_ref.object->base_flag & BASE_SELECTED);
}

bool Instance::object_is_paint_mode(const Object *object)
{
  if (object->type == OB_GREASE_PENCIL && (state.object_mode & OB_MODE_ALL_PAINT_GPENCIL)) {
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

bool Instance::object_is_particle_edit_mode(const ObjectRef &ob_ref)
{
  return (ob_ref.object->mode == OB_MODE_PARTICLE_EDIT) && (state.ctx_mode == CTX_MODE_PARTICLE);
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
    in_edit_paint_mode |= ob_ref.dupli_parent && (object_is_edit_mode(ob_ref.dupli_parent) ||
                                                  object_is_sculpt_mode(ob_ref.dupli_parent) ||
                                                  object_is_paint_mode(ob_ref.dupli_parent));
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
  if (selection_type_ != SelectionType::DISABLED) {
    /* Selection always need a prepass. Except if it is in xray mode. */
    return !state.xray_enabled;
  }

  if (in_paint_mode) {
    /* Allow paint overlays to draw with depth equal test. */
    return object_is_rendered_transparent(ob_ref.object, state);
  }

  if (!state.xray_enabled) {
    /* Only workbench ensures the depth buffer is matching overlays.
     * Force depth prepass for other render engines. */
    /* TODO(fclem): Make an exception for EEVEE if not using mixed resolution. */
    return state.v3d && (state.v3d->shading.type > OB_SOLID) && (ob_ref.object->dt >= OB_SOLID);
  }

  return false;
}

bool Instance::object_is_rendered_transparent(const Object *object, const State &state)
{
  if (state.v3d == nullptr) {
    return false;
  }

  if (state.xray_enabled) {
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
      Mesh *mesh = static_cast<Mesh *>(object->data);
      for (int i = 0; i < mesh->totcol; i++) {
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
