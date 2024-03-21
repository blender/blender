/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "workbench_private.hh"

#include "BKE_camera.h"
#include "BKE_editmesh.hh"
#include "BKE_mesh_types.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_particle.h"
#include "BKE_pbvh_api.hh"
#include "DEG_depsgraph_query.hh"
#include "DNA_fluid_types.h"
#include "ED_paint.hh"
#include "ED_view3d.hh"
#include "GPU_capabilities.h"

namespace blender::workbench {

void SceneState::init(Object *camera_ob /*=nullptr*/)
{
  bool reset_taa = reset_taa_next_sample;
  reset_taa_next_sample = false;

  const DRWContextState *context = DRW_context_state_get();
  View3D *v3d = context->v3d;
  RegionView3D *rv3d = context->rv3d;

  scene = DEG_get_evaluated_scene(context->depsgraph);

  GPUTexture *viewport_tx = DRW_viewport_texture_list_get()->color;
  resolution = int2(GPU_texture_width(viewport_tx), GPU_texture_height(viewport_tx));

  camera_object = camera_ob;
  if (camera_object == nullptr && v3d && rv3d) {
    camera_object = (rv3d->persp == RV3D_CAMOB) ? v3d->camera : nullptr;
  }
  camera = camera_object && camera_object->type == OB_CAMERA ?
               static_cast<Camera *>(camera_object->data) :
               nullptr;

  object_mode = CTX_data_mode_enum_ex(context->object_edit, context->obact, context->object_mode);

  /* TODO(@pragma37):
   * Check why Workbench Next exposes OB_MATERIAL, and Workbench exposes OB_RENDER */
  bool is_render_mode = !v3d || ELEM(v3d->shading.type, OB_RENDER, OB_MATERIAL);

  const View3DShading previous_shading = shading;
  shading = is_render_mode ? scene->display.shading : v3d->shading;

  cull_state = shading.flag & V3D_SHADING_BACKFACE_CULLING ? DRW_STATE_CULL_BACK :
                                                             DRW_STATE_NO_DRAW;

  /* FIXME: This reproduce old behavior when workbench was separated in 2 engines.
   * But this is a workaround for a missing update tagging. */
  DRWState new_clip_state = RV3D_CLIPPING_ENABLED(v3d, rv3d) ? DRW_STATE_CLIP_PLANES :
                                                               DRW_STATE_NO_DRAW;
  DRWState old_clip_state = clip_planes.size() > 0 ? DRW_STATE_CLIP_PLANES : DRW_STATE_NO_DRAW;
  if (new_clip_state != old_clip_state) {
    reset_taa = true;
  }
  clip_planes.clear();
  if (new_clip_state & DRW_STATE_CLIP_PLANES) {
    int plane_len = (RV3D_LOCK_FLAGS(rv3d) & RV3D_BOXCLIP) ? 4 : 6;
    for (auto i : IndexRange(plane_len)) {
      clip_planes.append(rv3d->clip[i]);
    }
  }

  if (shading.type < OB_SOLID) {
    shading.light = V3D_LIGHTING_FLAT;
    shading.color_type = V3D_SHADING_OBJECT_COLOR;
    shading.xray_alpha = 0.0f;
  }
  else if (SHADING_XRAY_ENABLED(shading)) {
    shading.xray_alpha = SHADING_XRAY_ALPHA(shading);
  }
  else {
    shading.xray_alpha = 1.0f;
  }
  xray_mode = shading.xray_alpha != 1.0f;

  if (xray_mode) {
    /* Disable shading options that aren't supported in transparency mode. */
    shading.flag &= ~(V3D_SHADING_SHADOW | V3D_SHADING_CAVITY | V3D_SHADING_DEPTH_OF_FIELD);
  }
  if (SHADING_XRAY_ENABLED(shading) != SHADING_XRAY_ENABLED(previous_shading) ||
      shading.flag != previous_shading.flag)
  {
    reset_taa = true;
  }

  lighting_type = lighting_type_from_v3d_lighting(shading.light);
  material_override = Material(shading.single_color);

  background_color = float4(0.0f);
  if (is_render_mode && scene->r.alphamode != R_ALPHAPREMUL) {
    if (World *w = scene->world) {
      background_color = float4(w->horr, w->horg, w->horb, 1.0f);
    }
  }

  if (rv3d && rv3d->rflag & RV3D_GPULIGHT_UPDATE) {
    reset_taa = true;
    /* FIXME: This reproduce old behavior when workbench was separated in 2 engines.
     * But this is a workaround for a missing update tagging. */
    rv3d->rflag &= ~RV3D_GPULIGHT_UPDATE;
  }

  float4x4 matrix;
  /* TODO(@pragma37): New API? */
  DRW_view_persmat_get(nullptr, matrix.ptr(), false);
  if (matrix != view_projection_matrix) {
    view_projection_matrix = matrix;
    reset_taa = true;
  }

  bool is_playback = DRW_state_is_playback();
  bool is_navigating = DRW_state_is_navigating();

  /* Reset complete drawing when navigating or during viewport playback or when
   * leaving one of those states. In case of multires modifier the navigation
   * mesh differs from the viewport mesh, so we need to be sure to restart. */
  if (is_playback || is_navigating) {
    reset_taa = true;
    reset_taa_next_sample = true;
  }

  int _samples_len = U.viewport_aa;
  if (v3d && ELEM(v3d->shading.type, OB_RENDER, OB_MATERIAL)) {
    _samples_len = scene->display.viewport_aa;
  }
  else if (DRW_state_is_scene_render()) {
    _samples_len = scene->display.render_aa;
  }
  if (is_navigating || is_playback) {
    /* Only draw using SMAA or no AA when navigating. */
    _samples_len = min_ii(_samples_len, 1);
  }
  /* 0 samples means no AA */
  draw_aa = _samples_len > 0;
  _samples_len = max_ii(_samples_len, 1);

  /* Reset the TAA when we have already draw a sample, but the sample count differs from previous
   * time. This removes render artifacts when the viewport anti-aliasing in the user preferences
   * is set to a lower value. */
  if (samples_len != _samples_len) {
    samples_len = _samples_len;
    reset_taa = true;
  }

  bool _overlays_enabled = v3d && !(v3d->flag2 & V3D_HIDE_OVERLAYS);
  /* Depth is always required in Wireframe mode. */
  _overlays_enabled = _overlays_enabled || shading.type < OB_SOLID;
  /* Some overlay passes can be rendered even with overlays disabled (See #116424). */
  _overlays_enabled = _overlays_enabled || new_clip_state & DRW_STATE_CLIP_PLANES;
  if (assign_if_different(overlays_enabled, _overlays_enabled)) {
    /* Reset TAA when enabling overlays, since we won't have valid sample0 depth textures.
     * (See #113741) */
    reset_taa = true;
  }

  if (reset_taa || samples_len <= 1) {
    sample = 0;
  }
  else {
    sample++;
  }
  render_finished = sample >= samples_len && samples_len > 1;

  /* TODO(@pragma37): volumes_do */

  draw_cavity = shading.flag & V3D_SHADING_CAVITY &&
                ELEM(shading.cavity_type, V3D_SHADING_CAVITY_SSAO, V3D_SHADING_CAVITY_BOTH);
  draw_curvature = shading.flag & V3D_SHADING_CAVITY && ELEM(shading.cavity_type,
                                                             V3D_SHADING_CAVITY_CURVATURE,
                                                             V3D_SHADING_CAVITY_BOTH);
  draw_shadows = shading.flag & V3D_SHADING_SHADOW;
  draw_outline = shading.flag & V3D_SHADING_OBJECT_OUTLINE;
  draw_dof = camera && camera->dof.flag & CAM_DOF_ENABLED &&
             shading.flag & V3D_SHADING_DEPTH_OF_FIELD;

  draw_object_id = draw_outline || draw_curvature;
};

static const CustomData *get_loop_custom_data(const Mesh *mesh)
{
  if (mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) {
    BLI_assert(mesh->runtime->edit_mesh != nullptr);
    BLI_assert(mesh->runtime->edit_mesh->bm != nullptr);
    return &mesh->runtime->edit_mesh->bm->ldata;
  }
  return &mesh->corner_data;
}

static const CustomData *get_vert_custom_data(const Mesh *mesh)
{
  if (mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) {
    BLI_assert(mesh->runtime->edit_mesh != nullptr);
    BLI_assert(mesh->runtime->edit_mesh->bm != nullptr);
    return &mesh->runtime->edit_mesh->bm->vdata;
  }
  return &mesh->vert_data;
}

ObjectState::ObjectState(const SceneState &scene_state, Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool is_active = (ob == draw_ctx->obact);

  image_paint_override = nullptr;
  override_sampler_state = GPUSamplerState::default_sampler();
  sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->rv3d) &&
                !DRW_state_is_image_render();
  draw_shadow = scene_state.draw_shadows && (ob->dtx & OB_DRAW_NO_SHADOW_CAST) == 0 &&
                !sculpt_pbvh && !(is_active && DRW_object_use_hide_faces(ob));

  color_type = (eV3DShadingColorType)scene_state.shading.color_type;

  bool has_color = false;
  bool has_uv = false;

  if (ob->type == OB_MESH) {
    const Mesh *mesh = static_cast<Mesh *>(ob->data);
    const CustomData *cd_vdata = get_vert_custom_data(mesh);
    const CustomData *cd_ldata = get_loop_custom_data(mesh);

    has_color = (CustomData_has_layer(cd_vdata, CD_PROP_COLOR) ||
                 CustomData_has_layer(cd_vdata, CD_PROP_BYTE_COLOR) ||
                 CustomData_has_layer(cd_ldata, CD_PROP_COLOR) ||
                 CustomData_has_layer(cd_ldata, CD_PROP_BYTE_COLOR));

    has_uv = CustomData_has_layer(cd_ldata, CD_PROP_FLOAT2);
  }

  if (color_type == V3D_SHADING_TEXTURE_COLOR && (!has_uv || ob->dt < OB_TEXTURE)) {
    color_type = V3D_SHADING_MATERIAL_COLOR;
  }
  else if (color_type == V3D_SHADING_VERTEX_COLOR && !has_color) {
    color_type = V3D_SHADING_OBJECT_COLOR;
  }

  if (sculpt_pbvh) {
    if (color_type == V3D_SHADING_TEXTURE_COLOR && BKE_pbvh_type(ob->sculpt->pbvh) != PBVH_FACES) {
      /* Force use of material color for sculpt. */
      color_type = V3D_SHADING_MATERIAL_COLOR;
    }

    /* Bad call C is required to access the tool system that is context aware. Cast to non-const
     * due to current API. */
    bContext *C = (bContext *)DRW_context_state_get()->evil_C;
    if (C != nullptr) {
      color_type = ED_paint_shading_color_override(
          C, &scene_state.scene->toolsettings->paint_mode, ob, color_type);
    }
  }
  else if (ob->type == OB_MESH && !DRW_state_is_scene_render()) {
    /* Force texture or vertex mode if object is in paint mode. */
    const bool is_vertpaint_mode = is_active && (scene_state.object_mode == CTX_MODE_PAINT_VERTEX);
    const bool is_texpaint_mode = is_active && (scene_state.object_mode == CTX_MODE_PAINT_TEXTURE);
    if (is_vertpaint_mode && has_color) {
      color_type = V3D_SHADING_VERTEX_COLOR;
    }
    else if (is_texpaint_mode && has_uv) {
      color_type = V3D_SHADING_TEXTURE_COLOR;
      const ImagePaintSettings *imapaint = &scene_state.scene->toolsettings->imapaint;
      if (imapaint->mode == IMAGEPAINT_MODE_IMAGE) {
        image_paint_override = imapaint->canvas;
        override_sampler_state.extend_x = GPU_SAMPLER_EXTEND_MODE_REPEAT;
        override_sampler_state.extend_yz = GPU_SAMPLER_EXTEND_MODE_REPEAT;
        const bool use_linear_filter = imapaint->interp == IMAGEPAINT_INTERP_LINEAR;
        override_sampler_state.set_filtering_flag_from_test(GPU_SAMPLER_FILTERING_LINEAR,
                                                            use_linear_filter);
      }
    }
  }

  use_per_material_batches = image_paint_override == nullptr && ELEM(color_type,
                                                                     V3D_SHADING_TEXTURE_COLOR,
                                                                     V3D_SHADING_MATERIAL_COLOR);
}

}  // namespace blender::workbench
