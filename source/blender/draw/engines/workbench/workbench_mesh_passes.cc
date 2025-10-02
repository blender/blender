/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "workbench_private.hh"

namespace blender::workbench {

/* -------------------------------------------------------------------- */
/** \name MeshPass
 * \{ */

MeshPass::MeshPass(const char *name) : PassMain(name) {};

bool MeshPass::is_empty() const
{
  /* TODO: Move to #draw::Pass. */

  return is_empty_;
}

void MeshPass::init_pass(SceneResources &resources, DRWState state, int clip_planes)
{
  use_custom_ids = true;
  is_empty_ = true;
  PassMain::init();
  state_set(state, clip_planes);
  bind_texture(WB_MATCAP_SLOT, resources.matcap_tx);
  bind_texture(WB_TEXTURE_SLOT, resources.dummy_texture_tx);
  bind_texture(WB_TILE_ARRAY_SLOT, resources.dummy_tile_array_tx);
  bind_texture(WB_TILE_DATA_SLOT, resources.dummy_tile_data_tx);
  bind_ssbo(WB_MATERIAL_SLOT, &resources.material_buf);
  bind_ubo(WB_WORLD_SLOT, resources.world_buf);
  if (clip_planes > 0) {
    bind_ubo(DRW_CLIPPING_UBO_SLOT, resources.clip_planes_buf);
  }
}

void MeshPass::init_subpasses(ePipelineType pipeline, eLightingType lighting, bool clip)
{
  texture_subpass_map_.clear();
  pipeline_ = pipeline;
  lighting_ = lighting;
  clip_ = clip;

  for (auto geom : IndexRange(geometry_type_len)) {
    for (auto shader : IndexRange(shader_type_len)) {
      passes_[geom][shader] = nullptr;
    }
  }
}

PassMain::Sub &MeshPass::get_subpass(eGeometryType geometry_type, eShaderType shader_type)
{
  static std::string pass_names[geometry_type_len][shader_type_len] = {};

  PassMain::Sub *&sub_pass = passes_[int(geometry_type)][int(shader_type)];
  if (!sub_pass) {
    std::string &pass_name = pass_names[int(geometry_type)][int(shader_type)];
    if (pass_name.empty()) {
      pass_name = std::string(get_name(geometry_type)) + std::string(get_name(shader_type));
    }
    sub_pass = &sub(pass_name.c_str());
    sub_pass->shader_set(
        ShaderCache::get().prepass_get(geometry_type, pipeline_, lighting_, shader_type, clip_));
  }

  return *sub_pass;
}

PassMain::Sub &MeshPass::get_subpass(eGeometryType geometry_type,
                                     const MaterialTexture *texture /* = nullptr */)
{
  is_empty_ = false;

  if (texture && texture->gpu.texture && *texture->gpu.texture) {
    auto add_cb = [&] {
      PassMain::Sub *sub_pass = &get_subpass(geometry_type, eShaderType::TEXTURE);
      sub_pass = &sub_pass->sub(texture->name);
      if (texture->gpu.tile_mapping) {
        sub_pass->bind_texture(WB_TILE_ARRAY_SLOT, texture->gpu.texture, texture->sampler_state);
        sub_pass->bind_texture(WB_TILE_DATA_SLOT, texture->gpu.tile_mapping);
      }
      else {
        sub_pass->bind_texture(WB_TEXTURE_SLOT, texture->gpu.texture, texture->sampler_state);
      }
      sub_pass->push_constant("is_image_tile", texture->gpu.tile_mapping != nullptr);
      sub_pass->push_constant("image_premult", texture->premultiplied);
      /* TODO(@pragma37): This setting should be exposed on the user side,
       * either as a global parameter (and set it here)
       * or by reading the Material Clipping Threshold (and set it per material) */
      float alpha_cutoff = texture->alpha_cutoff ? 0.1f : -FLT_MAX;
      sub_pass->push_constant("image_transparency_cutoff", alpha_cutoff);
      return sub_pass;
    };

    return *texture_subpass_map_.lookup_or_add_cb(
        {*texture->gpu.texture, texture->sampler_state, geometry_type}, add_cb);
  }

  return get_subpass(geometry_type, eShaderType::MATERIAL);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name OpaquePass
 * \{ */

void OpaquePass::sync(const SceneState &scene_state, SceneResources &resources)
{
  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                   DRW_STATE_WRITE_STENCIL | scene_state.cull_state;

  bool clip = scene_state.clip_planes.size() > 0;

  DRWState in_front_state = state | DRW_STATE_STENCIL_ALWAYS;
  gbuffer_in_front_ps_.init_pass(resources, in_front_state, scene_state.clip_planes.size());
  gbuffer_in_front_ps_.state_stencil(uint8_t(StencilBits::OBJECT_IN_FRONT), 0xFF, 0x00);
  gbuffer_in_front_ps_.init_subpasses(ePipelineType::OPAQUE, scene_state.lighting_type, clip);

  state |= DRW_STATE_STENCIL_NEQUAL;
  gbuffer_ps_.init_pass(resources, state, scene_state.clip_planes.size());
  gbuffer_ps_.state_stencil(
      uint8_t(StencilBits::OBJECT), 0xFF, uint8_t(StencilBits::OBJECT_IN_FRONT));
  gbuffer_ps_.init_subpasses(ePipelineType::OPAQUE, scene_state.lighting_type, clip);

  deferred_ps_.init();
  deferred_ps_.state_set(DRW_STATE_WRITE_COLOR);
  deferred_ps_.shader_set(ShaderCache::get().resolve_get(scene_state.lighting_type,
                                                         scene_state.draw_cavity,
                                                         scene_state.draw_curvature,
                                                         scene_state.draw_shadows));
  deferred_ps_.push_constant("force_shadowing", false);
  deferred_ps_.bind_ubo(WB_WORLD_SLOT, resources.world_buf);
  deferred_ps_.bind_texture(WB_MATCAP_SLOT, resources.matcap_tx);
  deferred_ps_.bind_texture("normal_tx", &gbuffer_normal_tx);
  deferred_ps_.bind_texture("material_tx", &gbuffer_material_tx);
  deferred_ps_.bind_texture("depth_tx", &resources.depth_tx);
  deferred_ps_.bind_texture("stencil_tx", &deferred_ps_stencil_tx);
  resources.cavity.setup_resolve_pass(deferred_ps_, resources);
  deferred_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void OpaquePass::draw(Manager &manager,
                      View &view,
                      SceneResources &resources,
                      int2 resolution,
                      ShadowPass *shadow_pass)
{
  if (is_empty()) {
    return;
  }
  gbuffer_material_tx.acquire(resolution,
                              gpu::TextureFormat::SFLOAT_16_16_16_16,
                              GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);
  gbuffer_normal_tx.acquire(resolution,
                            gpu::TextureFormat::SFLOAT_16_16,
                            GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);

  GPUAttachment object_id_attachment = GPU_ATTACHMENT_NONE;
  if (resources.object_id_tx.is_valid()) {
    object_id_attachment = GPU_ATTACHMENT_TEXTURE(resources.object_id_tx);
  }

  if (!gbuffer_in_front_ps_.is_empty()) {
    gbuffer_in_front_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx),
                               GPU_ATTACHMENT_TEXTURE(gbuffer_material_tx),
                               GPU_ATTACHMENT_TEXTURE(gbuffer_normal_tx),
                               object_id_attachment);
    gbuffer_in_front_fb.bind();

    manager.submit(gbuffer_in_front_ps_, view);

    if (resources.depth_in_front_tx.is_valid()) {
      GPU_texture_copy(resources.depth_in_front_tx, resources.depth_tx);
    }
  }

  if (!gbuffer_ps_.is_empty()) {
    gbuffer_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx),
                      GPU_ATTACHMENT_TEXTURE(gbuffer_material_tx),
                      GPU_ATTACHMENT_TEXTURE(gbuffer_normal_tx),
                      object_id_attachment);
    gbuffer_fb.bind();

    manager.submit(gbuffer_ps_, view);
  }

  if (shadow_pass) {
    shadow_depth_stencil_tx.ensure_2d(gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8,
                                      resolution,
                                      GPU_TEXTURE_USAGE_SHADER_READ |
                                          GPU_TEXTURE_USAGE_ATTACHMENT |
                                          GPU_TEXTURE_USAGE_FORMAT_VIEW);

    GPU_texture_copy(shadow_depth_stencil_tx, resources.depth_tx);
    clear_fb.ensure(GPU_ATTACHMENT_TEXTURE(shadow_depth_stencil_tx));
    clear_fb.bind();
    GPU_framebuffer_clear_stencil(clear_fb, 0);

    shadow_pass->draw(
        manager, view, resources, **&shadow_depth_stencil_tx, !gbuffer_in_front_ps_.is_empty());
    deferred_ps_stencil_tx = shadow_depth_stencil_tx.stencil_view();
  }
  else {
    shadow_depth_stencil_tx.free();
    deferred_ps_stencil_tx = nullptr;
  }

  if (!shadow_pass || !shadow_pass->is_debug()) {
    /** Don't override the shadow debug output. */
    deferred_fb.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(resources.color_tx));
    deferred_fb.bind();
    manager.submit(deferred_ps_, view);
  }

  gbuffer_normal_tx.release();
  gbuffer_material_tx.release();
}

bool OpaquePass::is_empty() const
{
  return gbuffer_ps_.is_empty() && gbuffer_in_front_ps_.is_empty();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name TransparentPass
 * \{ */

void TransparentPass::sync(const SceneState &scene_state, SceneResources &resources)
{
  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_OIT |
                   scene_state.cull_state;

  bool clip = scene_state.clip_planes.size() > 0;

  accumulation_ps_.init_pass(
      resources, state | DRW_STATE_STENCIL_NEQUAL, scene_state.clip_planes.size());
  accumulation_ps_.state_stencil(
      uint8_t(StencilBits::OBJECT), 0xFF, uint8_t(StencilBits::OBJECT_IN_FRONT));
  accumulation_ps_.clear_color(float4(0.0f, 0.0f, 0.0f, 1.0f));
  accumulation_ps_.init_subpasses(ePipelineType::TRANSPARENT, scene_state.lighting_type, clip);

  accumulation_in_front_ps_.init_pass(resources, state, scene_state.clip_planes.size());
  accumulation_in_front_ps_.clear_color(float4(0.0f, 0.0f, 0.0f, 1.0f));
  accumulation_in_front_ps_.init_subpasses(
      ePipelineType::TRANSPARENT, scene_state.lighting_type, clip);

  resolve_ps_.init();
  resolve_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA);
  resolve_ps_.shader_set(ShaderCache::get().transparent_resolve.get());
  resolve_ps_.bind_texture("transparent_accum", &accumulation_tx);
  resolve_ps_.bind_texture("transparent_revealage", &reveal_tx);
  resolve_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void TransparentPass::draw(Manager &manager,
                           View &view,
                           SceneResources &resources,
                           int2 resolution)
{
  if (is_empty()) {
    return;
  }
  accumulation_tx.acquire(resolution,
                          gpu::TextureFormat::SFLOAT_16_16_16_16,
                          GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);
  reveal_tx.acquire(resolution,
                    gpu::TextureFormat::SFLOAT_16,
                    GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);

  resolve_fb.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(resources.color_tx));

  if (!accumulation_ps_.is_empty()) {
    transparent_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx),
                          GPU_ATTACHMENT_TEXTURE(accumulation_tx),
                          GPU_ATTACHMENT_TEXTURE(reveal_tx));
    transparent_fb.bind();
    manager.submit(accumulation_ps_, view);
    resolve_fb.bind();
    manager.submit(resolve_ps_, view);
  }
  if (!accumulation_in_front_ps_.is_empty()) {
    transparent_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_in_front_tx),
                          GPU_ATTACHMENT_TEXTURE(accumulation_tx),
                          GPU_ATTACHMENT_TEXTURE(reveal_tx));
    transparent_fb.bind();
    manager.submit(accumulation_in_front_ps_, view);
    resolve_fb.bind();
    manager.submit(resolve_ps_, view);
  }

  accumulation_tx.release();
  reveal_tx.release();
}

bool TransparentPass::is_empty() const
{
  return accumulation_ps_.is_empty() && accumulation_in_front_ps_.is_empty();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name TransparentDepthPass
 * \{ */

void TransparentDepthPass::sync(const SceneState &scene_state, SceneResources &resources)
{
  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                   DRW_STATE_WRITE_STENCIL | scene_state.cull_state;

  bool clip = scene_state.clip_planes.size() > 0;

  DRWState in_front_state = state | DRW_STATE_STENCIL_ALWAYS;
  in_front_ps_.init_pass(resources, in_front_state, scene_state.clip_planes.size());
  in_front_ps_.state_stencil(uint8_t(StencilBits::OBJECT_IN_FRONT), 0xFF, 0x00);
  in_front_ps_.init_subpasses(ePipelineType::OPAQUE, eLightingType::FLAT, clip);

  merge_ps_.init();
  merge_ps_.shader_set(ShaderCache::get().merge_depth.get());
  merge_ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_WRITE_STENCIL |
                      DRW_STATE_STENCIL_EQUAL);
  merge_ps_.state_stencil(
      uint8_t(StencilBits::OBJECT_IN_FRONT), 0xFF, uint8_t(StencilBits::OBJECT_IN_FRONT));
  merge_ps_.bind_texture("depth_tx", &resources.depth_in_front_tx);
  merge_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  state |= DRW_STATE_STENCIL_NEQUAL;
  main_ps_.init_pass(resources, state, scene_state.clip_planes.size());
  main_ps_.state_stencil(
      uint8_t(StencilBits::OBJECT), 0xFF, uint8_t(StencilBits::OBJECT_IN_FRONT));
  main_ps_.init_subpasses(ePipelineType::OPAQUE, eLightingType::FLAT, clip);
}

void TransparentDepthPass::draw(Manager &manager, View &view, SceneResources &resources)
{
  if (is_empty()) {
    return;
  }

  GPUAttachment object_id_attachment = GPU_ATTACHMENT_NONE;
  if (resources.object_id_tx.is_valid()) {
    object_id_attachment = GPU_ATTACHMENT_TEXTURE(resources.object_id_tx);
  }

  if (!in_front_ps_.is_empty()) {
    in_front_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_in_front_tx),
                       GPU_ATTACHMENT_NONE,
                       GPU_ATTACHMENT_NONE,
                       object_id_attachment);
    in_front_fb.bind();
    manager.submit(in_front_ps_, view);

    merge_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx));
    merge_fb.bind();
    manager.submit(merge_ps_, view);
  }

  if (!main_ps_.is_empty()) {
    main_fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx),
                   GPU_ATTACHMENT_NONE,
                   GPU_ATTACHMENT_NONE,
                   object_id_attachment);
    main_fb.bind();
    manager.submit(main_ps_, view);
  }
}

bool TransparentDepthPass::is_empty() const
{
  return main_ps_.is_empty() && in_front_ps_.is_empty();
}

/** \} */

}  // namespace blender::workbench
