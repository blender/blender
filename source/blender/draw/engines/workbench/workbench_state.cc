/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "workbench_private.hh"

#include "DNA_userdef_types.h"

#include "BKE_camera.h"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_types.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_world_types.h"

#include "ED_paint.hh"
#include "ED_view3d.hh"

#include "GPU_capabilities.hh"

namespace blender::workbench {

/* Used for update detection on the render settings. */
static bool operator!=(const View3DShading &a, const View3DShading &b)
{
  /* Only checks the properties that are actually used by workbench. */
  if (a.type != b.type) {
    return true;
  }
  if (a.color_type != b.color_type) {
    return true;
  }
  if (a.flag != b.flag) {
    return true;
  }
  if (a.light != b.light) {
    return true;
  }
  if (a.background_type != b.background_type) {
    return true;
  }
  if (a.cavity_type != b.cavity_type) {
    return true;
  }
  if (a.wire_color_type != b.wire_color_type) {
    return true;
  }
  if (StringRefNull(a.studio_light) != StringRefNull(b.studio_light)) {
    return true;
  }
  if (StringRefNull(a.matcap) != StringRefNull(b.matcap)) {
    return true;
  }
  if (a.shadow_intensity != b.shadow_intensity) {
    return true;
  }
  if (float3(a.single_color) != float3(b.single_color)) {
    return true;
  }
  if (a.studiolight_rot_z != b.studiolight_rot_z) {
    return true;
  }
  if (float3(a.object_outline_color) != float3(b.object_outline_color)) {
    return true;
  }
  if (a.xray_alpha != b.xray_alpha) {
    return true;
  }
  if (a.xray_alpha_wire != b.xray_alpha_wire) {
    return true;
  }
  if (a.cavity_valley_factor != b.cavity_valley_factor) {
    return true;
  }
  if (a.cavity_ridge_factor != b.cavity_ridge_factor) {
    return true;
  }
  if (float3(a.background_color) != float3(b.background_color)) {
    return true;
  }
  if (a.curvature_ridge_factor != b.curvature_ridge_factor) {
    return true;
  }
  if (a.curvature_valley_factor != b.curvature_valley_factor) {
    return true;
  }
  return false;
}

void SceneState::init(const DRWContext *context,
                      bool scene_updated,
                      Object *camera_ob /*=nullptr*/)
{
  bool reset_taa = reset_taa_next_sample || scene_updated;
  reset_taa_next_sample = false;

  View3D *v3d = context->v3d;
  RegionView3D *rv3d = context->rv3d;

  scene = DEG_get_evaluated_scene(context->depsgraph);

  if (assign_if_different(resolution, int2(context->viewport_size_get()))) {
    /* In some cases, the viewport can change resolution without a call to `workbench_view_update`.
     * This is the case when dragging a window between two screen with different DPI settings.
     * (See #128712) */
    reset_taa = true;
  }

  camera_object = camera_ob;
  if (camera_object == nullptr && v3d && rv3d) {
    camera_object = (rv3d->persp == RV3D_CAMOB) ? v3d->camera : nullptr;
  }
  camera = camera_object && camera_object->type == OB_CAMERA ?
               &DRW_object_get_data_for_drawing<Camera>(*camera_object) :
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

  if (shading != previous_shading) {
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

  float4x4 matrix = View::default_get().persmat();
  if (matrix != view_projection_matrix) {
    view_projection_matrix = matrix;
    reset_taa = true;
  }

  bool is_playback = context->is_playback();
  bool is_navigating = context->is_navigating();

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
  else if (context->is_scene_render()) {
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

  draw_object_id = (draw_outline || draw_curvature);
};

static bool mesh_has_color_attribute(const Mesh &mesh)
{
  if (mesh.runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) {
    const BMesh &bm = *mesh.runtime->edit_mesh->bm;
    const BMDataLayerLookup attr = BM_data_layer_lookup(bm, mesh.active_color_attribute);
    return attr && bke::mesh::is_color_attribute(bke::AttributeMetaData{attr.domain, attr.type});
  }
  const bke::AttributeAccessor attributes = mesh.attributes();
  return bke::mesh::is_color_attribute(attributes.lookup_meta_data(mesh.active_color_attribute));
}

static bool mesh_has_uv_map_attribute(const Mesh &mesh)
{
  if (mesh.runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) {
    const BMesh &bm = *mesh.runtime->edit_mesh->bm;
    const BMDataLayerLookup attr = BM_data_layer_lookup(bm, mesh.active_uv_map_name());
    return attr && bke::mesh::is_uv_map(bke::AttributeMetaData{attr.domain, attr.type});
  }
  const bke::AttributeAccessor attributes = mesh.attributes();
  return bke::mesh::is_uv_map(attributes.lookup_meta_data(mesh.active_uv_map_name()));
}

ObjectState::ObjectState(const DRWContext *draw_ctx,
                         const SceneState &scene_state,
                         const SceneResources &resources,
                         Object *ob)
{
  const bool is_active = (ob == draw_ctx->obact);

  sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->rv3d) &&
                !draw_ctx->is_image_render();
  draw_shadow = scene_state.draw_shadows && (ob->dtx & OB_DRAW_NO_SHADOW_CAST) == 0 &&
                !sculpt_pbvh && !(is_active && DRW_object_use_hide_faces(ob));

  color_type = (eV3DShadingColorType)scene_state.shading.color_type;

  /* Don't perform CustomData lookup unless it's really necessary, since it's quite expensive. */
  const auto has_color = [&]() {
    if (ob->type != OB_MESH) {
      return false;
    }
    const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(*ob);
    return mesh_has_color_attribute(mesh);
  };

  const auto has_uv = [&]() {
    if (ob->type != OB_MESH) {
      return false;
    }
    const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(*ob);
    return mesh_has_uv_map_attribute(mesh);
  };

  if (color_type == V3D_SHADING_TEXTURE_COLOR && (!has_uv() || ob->dt < OB_TEXTURE)) {
    color_type = V3D_SHADING_MATERIAL_COLOR;
  }
  else if (color_type == V3D_SHADING_VERTEX_COLOR && !has_color()) {
    color_type = V3D_SHADING_MATERIAL_COLOR;
  }

  if (sculpt_pbvh) {
    if (color_type == V3D_SHADING_TEXTURE_COLOR &&
        bke::object::pbvh_get(*ob)->type() != bke::pbvh::Type::Mesh)
    {
      /* Force use of material color for sculpt. */
      color_type = V3D_SHADING_MATERIAL_COLOR;
    }

    /* Bad call C is required to access the tool system that is context aware. Cast to non-const
     * due to current API. */
    bContext *C = (bContext *)draw_ctx->evil_C;
    if (C != nullptr) {
      color_type = ED_paint_shading_color_override(
          C, &scene_state.scene->toolsettings->paint_mode, *ob, color_type);
    }
  }
  else if (ob->type == OB_MESH && !draw_ctx->is_scene_render()) {
    /* Force texture or vertex mode if object is in paint mode. */
    const bool is_vertpaint_mode = is_active && (scene_state.object_mode == CTX_MODE_PAINT_VERTEX);
    const bool is_texpaint_mode = is_active && (scene_state.object_mode == CTX_MODE_PAINT_TEXTURE);
    if (is_vertpaint_mode && has_color()) {
      color_type = V3D_SHADING_VERTEX_COLOR;
    }
    else if (is_texpaint_mode && has_uv()) {
      color_type = V3D_SHADING_TEXTURE_COLOR;
      show_missing_texture = true;
      const ImagePaintSettings *imapaint = &scene_state.scene->toolsettings->imapaint;
      if (imapaint->mode == IMAGEPAINT_MODE_IMAGE) {
        if (imapaint->canvas) {
          image_paint_override = MaterialTexture(imapaint->canvas);
          image_paint_override.sampler_state.extend_x = GPU_SAMPLER_EXTEND_MODE_REPEAT;
          image_paint_override.sampler_state.extend_yz = GPU_SAMPLER_EXTEND_MODE_REPEAT;
          const bool use_linear_filter = imapaint->interp == IMAGEPAINT_INTERP_LINEAR;
          image_paint_override.sampler_state.set_filtering_flag_from_test(
              GPU_SAMPLER_FILTERING_LINEAR, use_linear_filter);
        }
        else {
          image_paint_override = resources.missing_texture;
        }
      }
    }
  }

  use_per_material_batches = image_paint_override.gpu.texture == nullptr &&
                             ELEM(color_type,
                                  V3D_SHADING_TEXTURE_COLOR,
                                  V3D_SHADING_MATERIAL_COLOR);
}

}  // namespace blender::workbench
