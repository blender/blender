/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#include "DEG_depsgraph_query.h"
#include "ED_view3d.h"
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
  state.rv3d = ctx->rv3d;
  state.active_base = BKE_view_layer_active_base_get(ctx->view_layer);
  state.object_mode = ctx->object_mode;

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
    state.clipping_state = RV3D_CLIPPING_ENABLED(state.v3d, state.rv3d) ? DRW_STATE_CLIP_PLANES :
                                                                          DRWState(0);
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
  }

  /* TODO(fclem): Remove DRW global usage. */
  resources.globals_buf = G_draw.block_ubo;
  resources.theme_settings = G_draw.block;
}

void Instance::begin_sync()
{
  const DRWView *view_legacy = DRW_view_default_get();
  View view("OverlayView", view_legacy);

  resources.begin_sync();

  background.begin_sync(resources, state);
  prepass.begin_sync(resources, state);
  empties.begin_sync();
  metaballs.begin_sync();
  grid.begin_sync(resources, state, view);
}

void Instance::object_sync(ObjectRef &ob_ref, Manager &manager)
{
  const bool in_edit_mode = object_is_edit_mode(ob_ref.object);
  const bool needs_prepass = true; /* TODO */

  if (needs_prepass) {
    switch (ob_ref.object->type) {
      case OB_MESH:
      case OB_SURF:
      case OB_CURVES:
      case OB_FONT:
      case OB_CURVES_LEGACY:
        prepass.object_sync(manager, ob_ref, resources);
        break;
    }
  }

  if (in_edit_mode && !state.hide_overlays) {
    switch (ob_ref.object->type) {
      case OB_MESH:
        break;
      case OB_ARMATURE:
        break;
      case OB_CURVES_LEGACY:
        break;
      case OB_SURF:
        break;
      case OB_LATTICE:
        break;
      case OB_MBALL:
        metaballs.edit_object_sync(ob_ref, resources);
        break;
      case OB_FONT:
        break;
      case OB_CURVES:
        break;
    }
  }

  if (!state.hide_overlays) {
    switch (ob_ref.object->type) {
      case OB_EMPTY:
        empties.object_sync(ob_ref, resources, state);
        break;
      case OB_ARMATURE:
        break;
      case OB_MBALL:
        if (!in_edit_mode) {
          metaballs.object_sync(ob_ref, resources, state);
        }
        break;
      case OB_GPENCIL_LEGACY:
        break;
    }
  }
}

void Instance::end_sync()
{
  resources.end_sync();

  metaballs.end_sync(resources, shapes, state);
  empties.end_sync(resources, shapes, state);
}

void Instance::draw(Manager &manager)
{
  resources.depth_tx.wrap(DRW_viewport_texture_list_get()->depth);
  resources.depth_in_front_tx.wrap(DRW_viewport_texture_list_get()->depth_in_front);
  resources.color_overlay_tx.wrap(DRW_viewport_texture_list_get()->color_overlay);
  resources.color_render_tx.wrap(DRW_viewport_texture_list_get()->color);

  int2 render_size = int2(resources.depth_tx.size());

  const DRWView *view_legacy = DRW_view_default_get();
  View view("OverlayView", view_legacy);

  /* TODO: Better semantics using a switch? */
  if (!resources.color_overlay_tx.is_valid()) {
    /* Likely to be the selection case. Allocate dummy texture and bind only depth buffer. */
    resources.line_tx.acquire(int2(1, 1), GPU_RGBA8);
    resources.color_overlay_alloc_tx.acquire(int2(1, 1), GPU_SRGB8_A8);
    resources.color_render_alloc_tx.acquire(int2(1, 1), GPU_SRGB8_A8);

    resources.color_overlay_tx.wrap(resources.color_overlay_alloc_tx);
    resources.color_render_tx.wrap(resources.color_render_alloc_tx);

    resources.overlay_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx));
    resources.overlay_line_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx));
    /* Create it but shouldn't even be used. */
    resources.overlay_color_only_fb.ensure(GPU_ATTACHMENT_NONE,
                                           GPU_ATTACHMENT_TEXTURE(resources.color_overlay_tx));
  }
  else {
    resources.line_tx.acquire(render_size, GPU_RGBA8);

    resources.overlay_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx),
                                GPU_ATTACHMENT_TEXTURE(resources.color_overlay_tx));
    resources.overlay_line_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx),
                                     GPU_ATTACHMENT_TEXTURE(resources.color_overlay_tx),
                                     GPU_ATTACHMENT_TEXTURE(resources.line_tx));
    resources.overlay_color_only_fb.ensure(GPU_ATTACHMENT_NONE,
                                           GPU_ATTACHMENT_TEXTURE(resources.color_overlay_tx));
  }

  /* TODO(fclem): Remove mandatory allocation. */
  if (!resources.depth_in_front_tx.is_valid()) {
    resources.depth_in_front_alloc_tx.acquire(render_size, GPU_DEPTH_COMPONENT24);
    resources.depth_in_front_tx.wrap(resources.depth_in_front_alloc_tx);
  }

  resources.overlay_in_front_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_in_front_tx),
                                       GPU_ATTACHMENT_TEXTURE(resources.color_overlay_tx));
  resources.overlay_line_in_front_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_in_front_tx),
                                            GPU_ATTACHMENT_TEXTURE(resources.color_overlay_tx),
                                            GPU_ATTACHMENT_TEXTURE(resources.line_tx));

  GPU_framebuffer_bind(resources.overlay_color_only_fb);

  float4 clear_color(0.0f);
  GPU_framebuffer_clear_color(resources.overlay_color_only_fb, clear_color);

  prepass.draw(resources, manager, view);
  prepass.draw_in_front(resources, manager, view);

  background.draw(resources, manager);

  empties.draw(resources, manager, view);
  metaballs.draw(resources, manager, view);

  grid.draw(resources, manager, view);

  empties.draw_in_front(resources, manager, view);
  metaballs.draw_in_front(resources, manager, view);

  // anti_aliasing.draw(resources, manager, view);

  resources.line_tx.release();
  resources.depth_in_front_alloc_tx.release();
  resources.color_overlay_alloc_tx.release();
  resources.color_render_alloc_tx.release();

  resources.read_result();
}

bool Instance::object_is_edit_mode(const Object *ob)
{
  if (DRW_object_is_in_edit_mode(ob)) {
    /* Also check for context mode as the object mode is not 100% reliable. (see T72490) */
    switch (ob->type) {
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

}  // namespace blender::draw::overlay

#include "overlay_private.hh"

/* TODO(fclem): Move elsewhere. */
BoneInstanceData::BoneInstanceData(Object *ob,
                                   const float *pos,
                                   const float radius,
                                   const float color[4])
{
  /* TODO(fclem): Use C++ math API. */
  mul_v3_v3fl(this->mat[0], ob->object_to_world[0], radius);
  mul_v3_v3fl(this->mat[1], ob->object_to_world[1], radius);
  mul_v3_v3fl(this->mat[2], ob->object_to_world[2], radius);
  mul_v3_m4v3(this->mat[3], ob->object_to_world, pos);
  /* WATCH: Reminder, alpha is wire-size. */
  OVERLAY_bone_instance_data_set_color(this, color);
}
