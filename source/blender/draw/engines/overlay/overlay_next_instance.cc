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
  state.object_active = BKE_view_layer_active_object_get(ctx->view_layer);
  state.object_mode = ctx->object_mode;
  state.cfra = DEG_get_ctime(state.depsgraph);
  state.is_viewport_image_render = DRW_state_is_viewport_image_render();
  state.is_image_render = DRW_state_is_image_render();
  state.is_depth_only_drawing = DRW_state_is_depth();
  state.is_material_select = DRW_state_is_material_select();
  state.draw_background = DRW_state_draw_background();
  state.show_text = DRW_state_show_text();

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
    layer.edit_text.begin_sync(resources, state);
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

  if (in_paint_mode && !state.hide_overlays) {
    switch (ob_ref.object->type) {
      case OB_MESH:
        /* TODO(fclem): Make it part of a #Meshes. */
        layer.paints.object_sync(manager, ob_ref, resources, state);
        /* For wire-frames. */
        layer.mesh_uvs.edit_object_sync(manager, ob_ref, resources, state);
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
        /* TODO(fclem): Find a better place / condition. */
        layer.mesh_uvs.edit_object_sync(manager, ob_ref, resources, state);
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
      case OB_FONT:
        layer.edit_text.edit_object_sync(manager, ob_ref, resources, state);
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
    layer.edit_text.end_sync(resources, state);
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
  /* TODO(fclem): Remove global access. */
  view.sync(DRW_view_default_get());

  static gpu::DebugScope select_scope = {"Selection"};
  static gpu::DebugScope draw_scope = {"Overlay"};

  if (resources.is_selection()) {
    select_scope.begin_capture();
  }
  else {
    draw_scope.begin_capture();
  }

  outline.flat_objects_pass_sync(manager, view, resources, state);
  GreasePencil::compute_depth_planes(manager, view, resources, state);

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

  resources.acquire(this->state, *DRW_viewport_texture_list_get());

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

  resources.release();

  resources.read_result();

  if (resources.is_selection()) {
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
  background.draw_output(resources.overlay_output_fb, manager, view);
}

void Instance::draw_v2d(Manager &manager, View &view)
{
  regular.mesh_uvs.draw_on_render(resources.render_fb, manager, view);

  GPU_framebuffer_bind(resources.overlay_output_fb);
  GPU_framebuffer_clear_color(resources.overlay_output_fb, float4(0.0));

  background.draw_output(resources.overlay_output_fb, manager, view);
  grid.draw_color_only(resources.overlay_output_fb, manager, view);
  regular.mesh_uvs.draw(resources.overlay_output_fb, manager, view);
}

void Instance::draw_v3d(Manager &manager, View &view)
{
  float4 clear_color(0.0f);

  auto draw = [&](OverlayLayer &layer, Framebuffer &framebuffer) {
    /* TODO(fclem): Depth aware outlines (see #130751). */
    // layer.facing.draw(framebuffer, manager, view);
    layer.fade.draw(framebuffer, manager, view);
    layer.mode_transfer.draw(framebuffer, manager, view);
    layer.edit_text.draw(framebuffer, manager, view);
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
    layer.relations.draw_line(framebuffer, manager, view);
    layer.fluids.draw_line(framebuffer, manager, view);
    layer.particles.draw_line(framebuffer, manager, view);
    layer.attribute_viewer.draw_line(framebuffer, manager, view);
    layer.armatures.draw_line(framebuffer, manager, view);
    layer.sculpts.draw_line(framebuffer, manager, view);
    layer.grease_pencil.draw_line(framebuffer, manager, view);
    layer.meshes.draw_line(framebuffer, manager, view);
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
      GPU_framebuffer_clear_color(resources.overlay_line_fb, clear_color);
    }

    /* TODO(fclem): Split overlay and rename draw functions. */
    /* TODO(fclem): Draw on line framebuffer. */
    regular.empties.draw_images(resources.overlay_fb, manager, view);

    regular.prepass.draw_line(resources.overlay_line_fb, manager, view);

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

    draw_color_only(regular, resources.overlay_color_only_fb);
    draw_color_only(infront, resources.overlay_color_only_fb);

    /* TODO(fclem): Split overlay and rename draw functions. */
    regular.empties.draw_in_front_images(resources.overlay_color_only_fb, manager, view);
    infront.empties.draw_in_front_images(resources.overlay_color_only_fb, manager, view);
    regular.cameras.draw_in_front(resources.overlay_color_only_fb, manager, view);
    infront.cameras.draw_in_front(resources.overlay_color_only_fb, manager, view);

    origins.draw_color_only(resources.overlay_color_only_fb, manager, view);
  }
  {
    /* Output pass. */
    GPU_framebuffer_bind(resources.overlay_output_fb);
    GPU_framebuffer_clear_color(resources.overlay_output_fb, clear_color);

    /* TODO(fclem): Split overlay and rename draw functions. */
    regular.cameras.draw_background_images(resources.overlay_output_fb, manager, view);
    infront.cameras.draw_background_images(resources.overlay_output_fb, manager, view);
    regular.empties.draw_background_images(resources.overlay_output_fb, manager, view);

    background.draw_output(resources.overlay_output_fb, manager, view);
    anti_aliasing.draw_output(resources.overlay_output_fb, manager, view);
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
  return (object == state.object_active) && (state.object_mode & OB_MODE_ALL_PAINT);
}

bool Instance::object_is_sculpt_mode(const ObjectRef &ob_ref)
{
  if (state.object_mode == OB_MODE_SCULPT_CURVES) {
    const Object *active_object = state.object_active;
    const bool is_active_object = ob_ref.object == active_object;

    bool is_geonode_preview = ob_ref.dupli_object && ob_ref.dupli_object->preview_base_geometry;
    bool is_active_dupli_parent = ob_ref.dupli_parent == active_object;
    return is_active_object || (is_active_dupli_parent && is_geonode_preview);
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
    /* Selection always need a prepass.
     * Note that depth writing and depth test might be disable for certain selection mode. */
    return true;
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
