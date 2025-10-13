/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Shading passes contain draw-calls specific to shading pipelines.
 * They are to be shared across views.
 * This file is only for shading passes. Other passes are declared in their own module.
 */

#include "BLI_bounds.hh"
#include "GPU_capabilities.hh"

#include "eevee_instance.hh"
#include "eevee_pipeline.hh"
#include "eevee_shadow.hh"

#include "GPU_debug.hh"

#include "draw_common.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name World Pipeline
 *
 * Used to draw background.
 * \{ */

void BackgroundPipeline::sync(GPUMaterial *gpumat,
                              const float background_opacity,
                              const float background_blur)
{
  Manager &manager = *inst_.manager;
  RenderBuffers &rbufs = inst_.render_buffers;

  clear_ps_.init();
  clear_ps_.state_set(DRW_STATE_WRITE_COLOR);
  clear_ps_.shader_set(inst_.shaders.static_shader_get(RENDERPASS_CLEAR));
  /* RenderPasses & AOVs. Cleared by background (even if bad practice). */
  clear_ps_.bind_image("rp_color_img", &rbufs.rp_color_tx);
  clear_ps_.bind_image("rp_value_img", &rbufs.rp_value_tx);
  clear_ps_.bind_image("rp_cryptomatte_img", &rbufs.cryptomatte_tx);
  /* Required by validation layers. */
  clear_ps_.bind_resources(inst_.cryptomatte);
  clear_ps_.bind_resources(inst_.uniform_data);
  clear_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  /* To allow opaque pass rendering over it. */
  clear_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);

  world_ps_.init();
  world_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_CLIP_CONTROL_UNIT_RANGE |
                      DRW_STATE_DEPTH_EQUAL);
  world_ps_.material_set(manager, gpumat);
  world_ps_.push_constant("world_opacity_fade", background_opacity);
  world_ps_.push_constant("world_background_blur", square_f(background_blur));
  SphereProbeData &world_data = *static_cast<SphereProbeData *>(&inst_.light_probes.world_sphere_);
  world_ps_.push_constant("world_coord_packed", reinterpret_cast<int4 *>(&world_data.atlas_coord));
  world_ps_.bind_texture("utility_tx", inst_.pipelines.utility_tx);
  /* RenderPasses & AOVs. */
  world_ps_.bind_image("rp_color_img", &rbufs.rp_color_tx);
  world_ps_.bind_image("rp_value_img", &rbufs.rp_value_tx);
  world_ps_.bind_image("rp_cryptomatte_img", &rbufs.cryptomatte_tx);
  /* Required by validation layers. */
  world_ps_.bind_resources(inst_.cryptomatte);
  world_ps_.bind_resources(inst_.uniform_data);
  world_ps_.bind_resources(inst_.sampling);
  world_ps_.bind_resources(inst_.sphere_probes);
  world_ps_.bind_resources(inst_.volume_probes);
  world_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  /* To allow opaque pass rendering over it. */
  world_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

void BackgroundPipeline::clear(View &view)
{
  inst_.manager->submit(clear_ps_, view);
}

void BackgroundPipeline::render(View &view, Framebuffer &combined_fb)
{
  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(world_ps_, view);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name World Probe Pipeline
 * \{ */

void WorldPipeline::sync(GPUMaterial *gpumat)
{
  const int2 extent(1);
  constexpr eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_WRITE |
                                     GPU_TEXTURE_USAGE_SHADER_READ;
  dummy_cryptomatte_tx_.ensure_2d(gpu::TextureFormat::SFLOAT_32_32_32_32, extent, usage);
  dummy_renderpass_tx_.ensure_2d(gpu::TextureFormat::SFLOAT_16_16_16_16, extent, usage);
  dummy_aov_color_tx_.ensure_2d_array(gpu::TextureFormat::SFLOAT_16_16_16_16, extent, 1, usage);
  dummy_aov_value_tx_.ensure_2d_array(gpu::TextureFormat::SFLOAT_16, extent, 1, usage);

  PassSimple &pass = cubemap_face_ps_;
  pass.init();
  pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS);

  Manager &manager = *inst_.manager;
  pass.material_set(manager, gpumat);
  pass.push_constant("world_opacity_fade", 1.0f);
  pass.push_constant("world_background_blur", 0.0f);
  pass.push_constant("world_coord_packed", int4(0.0f));
  pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  pass.bind_image("rp_normal_img", dummy_renderpass_tx_);
  pass.bind_image("rp_light_img", dummy_renderpass_tx_);
  pass.bind_image("rp_diffuse_color_img", dummy_renderpass_tx_);
  pass.bind_image("rp_specular_color_img", dummy_renderpass_tx_);
  pass.bind_image("rp_emission_img", dummy_renderpass_tx_);
  pass.bind_image("rp_cryptomatte_img", dummy_cryptomatte_tx_);
  pass.bind_image("rp_color_img", dummy_aov_color_tx_);
  pass.bind_image("rp_value_img", dummy_aov_value_tx_);
  pass.bind_image("aov_color_img", dummy_aov_color_tx_);
  pass.bind_image("aov_value_img", dummy_aov_value_tx_);
  pass.bind_ssbo("aov_buf", &inst_.film.aovs_info);
  /* Required by validation layers. */
  pass.bind_resources(inst_.cryptomatte);
  pass.bind_resources(inst_.uniform_data);
  pass.bind_resources(inst_.sampling);
  pass.bind_resources(inst_.sphere_probes);
  pass.bind_resources(inst_.volume_probes);
  pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void WorldPipeline::render(View &view)
{
  /* TODO(Miguel Pozo): All world probes are rendered as RAY_TYPE_GLOSSY. */
  inst_.pipelines.data.is_sphere_probe = true;
  inst_.uniform_data.push_update();

  inst_.manager->submit(cubemap_face_ps_, view);

  inst_.pipelines.data.is_sphere_probe = false;
  inst_.uniform_data.push_update();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name World Volume Pipeline
 *
 * \{ */

void WorldVolumePipeline::sync(GPUMaterial *gpumat)
{
  is_valid_ = (gpumat != nullptr) && (GPU_material_status(gpumat) == GPU_MAT_SUCCESS) &&
              GPU_material_has_volume_output(gpumat);
  if (!is_valid_) {
    /* Skip if the material has not compiled yet. */
    return;
  }

  world_ps_.init();
  world_ps_.state_set(DRW_STATE_WRITE_COLOR);
  world_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  world_ps_.bind_resources(inst_.uniform_data);
  world_ps_.bind_resources(inst_.volume.properties);
  world_ps_.bind_resources(inst_.sampling);

  world_ps_.material_set(*inst_.manager, gpumat);
  /* Bind correct dummy texture for attributes defaults. */
  PassSimple::Sub *sub = volume_sub_pass(world_ps_, nullptr, nullptr, gpumat);

  is_valid_ = (sub != nullptr);
  if (is_valid_) {
    world_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    /* Sync with object property pass. */
    world_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
}

void WorldVolumePipeline::render(View &view)
{
  if (!is_valid_) {
    /* Clear the properties buffer instead of rendering if there is no valid shader. */
    inst_.volume.prop_scattering_tx_.clear(float4(0.0f));
    inst_.volume.prop_extinction_tx_.clear(float4(0.0f));
    inst_.volume.prop_emission_tx_.clear(float4(0.0f));
    inst_.volume.prop_phase_tx_.clear(float4(0.0f));
    inst_.volume.prop_phase_weight_tx_.clear(float4(0.0f));
    return;
  }

  inst_.manager->submit(world_ps_, view);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadow Pipeline
 *
 * \{ */

void ShadowPipeline::sync()
{
  render_ps_.init();

  /* NOTE: TILE_COPY technique perform a three-pass implementation. First performing the clear
   * directly on tile, followed by a fast depth-only pass, then storing the on-tile results into
   * the shadow atlas during a final storage pass. This takes advantage of TBDR architecture,
   * reducing overdraw and additional per-fragment calculations. */
  bool shadow_update_tbdr = (ShadowModule::shadow_technique == ShadowTechnique::TILE_COPY);
  if (shadow_update_tbdr) {
    draw::PassMain::Sub &pass = render_ps_.sub("Shadow.TilePageClear");
    pass.subpass_transition(GPU_ATTACHMENT_WRITE, {GPU_ATTACHMENT_WRITE});
    pass.shader_set(inst_.shaders.static_shader_get(SHADOW_PAGE_TILE_CLEAR));
    /* Only manually clear depth of the updated tiles.
     * This is because the depth is initialized to near depth using attachments for fast clear and
     * color is cleared to far depth. This way we can save a bit of bandwidth by only clearing
     * the updated tiles depth to far depth and not touch the color attachment. */
    pass.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
    pass.bind_ssbo("src_coord_buf", inst_.shadows.src_coord_buf_);
    pass.draw_procedural_indirect(GPU_PRIM_TRIS, inst_.shadows.tile_draw_buf_);
  }

  {
    /* Metal writes depth value in local tile memory, which is considered a color attachment. */
    DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_WRITE_COLOR;

    draw::PassMain::Sub &pass = render_ps_.sub("Shadow.Surface");
    pass.state_set(state);
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    pass.bind_ssbo(SHADOW_RENDER_VIEW_BUF_SLOT, &inst_.shadows.render_view_buf_);
    if (!shadow_update_tbdr) {
      /* We do not need all of the shadow information when using the TBDR-optimized approach. */
      pass.bind_image(SHADOW_ATLAS_IMG_SLOT, inst_.shadows.atlas_tx_);
      pass.bind_ssbo(SHADOW_RENDER_MAP_BUF_SLOT, &inst_.shadows.render_map_buf_);
      pass.bind_ssbo(SHADOW_PAGE_INFO_SLOT, &inst_.shadows.pages_infos_data_);
    }
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.sampling);
    surface_double_sided_ps_ = &pass.sub("Shadow.Surface.Double-Sided");
    surface_single_sided_ps_ = &pass.sub("Shadow.Surface.Single-Sided");
    surface_single_sided_ps_->state_set(state | DRW_STATE_CULL_BACK);
  }

  if (shadow_update_tbdr) {
    draw::PassMain::Sub &pass = render_ps_.sub("Shadow.TilePageStore");
    pass.shader_set(inst_.shaders.static_shader_get(SHADOW_PAGE_TILE_STORE));
    /* The most optimal way would be to only store pixels that have been rendered to (depth > 0).
     * But that requires that the destination pages in the atlas would have been already cleared
     * using compute. Experiments showed that it is faster to just copy the whole tiles back.
     *
     * For relative performance, raster-based clear within tile update adds around 0.1ms vs 0.25ms
     * for compute based clear for a simple test case. */
    pass.state_set(DRW_STATE_DEPTH_ALWAYS);
    /* Metal have implicit sync with Raster Order Groups. Other backend need to have manual
     * sub-pass transition to allow reading the frame-buffer. This is a no-op on Metal. */
    pass.subpass_transition(GPU_ATTACHMENT_WRITE, {GPU_ATTACHMENT_READ});
    pass.bind_image(SHADOW_ATLAS_IMG_SLOT, inst_.shadows.atlas_tx_);
    pass.bind_ssbo("dst_coord_buf", inst_.shadows.dst_coord_buf_);
    pass.bind_ssbo("src_coord_buf", inst_.shadows.src_coord_buf_);
    pass.draw_procedural_indirect(GPU_PRIM_TRIS, inst_.shadows.tile_draw_buf_);
  }
}

PassMain::Sub *ShadowPipeline::surface_material_add(::Material *material, GPUMaterial *gpumat)
{
  PassMain::Sub *pass = (material->blend_flag & MA_BL_CULL_BACKFACE_SHADOW) ?
                            surface_single_sided_ps_ :
                            surface_double_sided_ps_;
  return &pass->sub(GPU_material_get_name(gpumat));
}

void ShadowPipeline::render(View &view)
{
  inst_.manager->submit(render_ps_, view);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Forward Pass
 *
 * NPR materials (using Closure to RGBA) or material using ALPHA_BLEND.
 * \{ */

void ForwardPipeline::sync()
{
  camera_forward_ = inst_.camera.forward();
  has_opaque_ = false;
  has_transparent_ = false;

  DRWState state_depth_only = DRW_STATE_WRITE_DEPTH | DRW_STATE_CLIP_CONTROL_UNIT_RANGE |
                              inst_.film.depth.test_state;
  DRWState state_depth_color = state_depth_only | DRW_STATE_WRITE_COLOR;
  {
    prepass_ps_.init();

    {
      /* Common resources. */
      prepass_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
      prepass_ps_.bind_resources(inst_.uniform_data);
      prepass_ps_.bind_resources(inst_.velocity);
      prepass_ps_.bind_resources(inst_.sampling);
    }

    prepass_double_sided_static_ps_ = &prepass_ps_.sub("DoubleSided.Static");
    prepass_double_sided_static_ps_->state_set(state_depth_only);

    prepass_single_sided_static_ps_ = &prepass_ps_.sub("SingleSided.Static");
    prepass_single_sided_static_ps_->state_set(state_depth_only | DRW_STATE_CULL_BACK);

    prepass_double_sided_moving_ps_ = &prepass_ps_.sub("DoubleSided.Moving");
    prepass_double_sided_moving_ps_->state_set(state_depth_color);

    prepass_single_sided_moving_ps_ = &prepass_ps_.sub("SingleSided.Moving");
    prepass_single_sided_moving_ps_->state_set(state_depth_color | DRW_STATE_CULL_BACK);
  }
  {
    opaque_ps_.init();

    {
      /* Common resources. */
      opaque_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
      opaque_ps_.bind_resources(inst_.uniform_data);
      opaque_ps_.bind_resources(inst_.lights);
      opaque_ps_.bind_resources(inst_.shadows);
      opaque_ps_.bind_resources(inst_.volume.result);
      opaque_ps_.bind_resources(inst_.sampling);
      opaque_ps_.bind_resources(inst_.hiz_buffer.front);
      opaque_ps_.bind_resources(inst_.volume_probes);
      opaque_ps_.bind_resources(inst_.sphere_probes);
    }

    opaque_single_sided_ps_ = &opaque_ps_.sub("SingleSided");
    opaque_single_sided_ps_->state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_CLIP_CONTROL_UNIT_RANGE |
                                       DRW_STATE_DEPTH_EQUAL | DRW_STATE_CULL_BACK);

    opaque_double_sided_ps_ = &opaque_ps_.sub("DoubleSided");
    opaque_double_sided_ps_->state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_CLIP_CONTROL_UNIT_RANGE |
                                       DRW_STATE_DEPTH_EQUAL);
  }
  {
    transparent_ps_.init();
    /* Workaround limitation of PassSortable. Use dummy pass that will be sorted first in all
     * circumstances. */
    PassMain::Sub &sub = transparent_ps_.sub("ResourceBind", -FLT_MAX);

    /* Common resources. */

    /* Textures. */
    sub.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);

    sub.bind_resources(inst_.uniform_data);
    sub.bind_resources(inst_.lights);
    sub.bind_resources(inst_.shadows);
    sub.bind_resources(inst_.volume.result);
    sub.bind_resources(inst_.sampling);
    sub.bind_resources(inst_.hiz_buffer.front);
    sub.bind_resources(inst_.volume_probes);
    sub.bind_resources(inst_.sphere_probes);
  }
}

PassMain::Sub *ForwardPipeline::prepass_opaque_add(::Material *blender_mat,
                                                   GPUMaterial *gpumat,
                                                   bool has_motion)
{
  BLI_assert_msg(GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT) == false,
                 "Forward Transparent should be registered directly without calling "
                 "PipelineModule::material_add()");
  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                            (has_motion ? prepass_single_sided_moving_ps_ :
                                          prepass_single_sided_static_ps_) :
                            (has_motion ? prepass_double_sided_moving_ps_ :
                                          prepass_double_sided_static_ps_);

  /* If material is fully additive or transparent, we can skip the opaque prepass. */
  /* TODO(fclem): To skip it, we need to know if the transparent BSDF is fully white AND if there
   * is no mix shader (could do better constant folding but that's expensive). */

  has_opaque_ = true;
  return &pass->sub(GPU_material_get_name(gpumat));
}

PassMain::Sub *ForwardPipeline::material_opaque_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  BLI_assert_msg(GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT) == false,
                 "Forward Transparent should be registered directly without calling "
                 "PipelineModule::material_add()");
  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ? opaque_single_sided_ps_ :
                                                                          opaque_double_sided_ps_;
  has_opaque_ = true;
  return &pass->sub(GPU_material_get_name(gpumat));
}

PassMain::Sub *ForwardPipeline::prepass_transparent_add(const Object *ob,
                                                        ::Material *blender_mat,
                                                        GPUMaterial *gpumat)
{
  if ((blender_mat->blend_flag & MA_BL_HIDE_BACKFACE) == 0) {
    return nullptr;
  }
  DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_CLIP_CONTROL_UNIT_RANGE |
                   inst_.film.depth.test_state;
  if (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) {
    state |= DRW_STATE_CULL_BACK;
  }
  has_transparent_ = true;
  float sorting_value = math::dot(float3(ob->object_to_world().location()), camera_forward_);
  PassMain::Sub *pass = &transparent_ps_.sub(GPU_material_get_name(gpumat), sorting_value);
  pass->state_set(state);
  pass->material_set(*inst_.manager, gpumat, true);
  return pass;
}

PassMain::Sub *ForwardPipeline::material_transparent_add(const Object *ob,
                                                         ::Material *blender_mat,
                                                         GPUMaterial *gpumat)
{
  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM |
                   DRW_STATE_CLIP_CONTROL_UNIT_RANGE | inst_.film.depth.test_state;
  if (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) {
    state |= DRW_STATE_CULL_BACK;
  }
  has_transparent_ = true;
  float sorting_value = math::dot(float3(ob->object_to_world().location()), camera_forward_);
  PassMain::Sub *pass = &transparent_ps_.sub(GPU_material_get_name(gpumat), sorting_value);
  pass->state_set(state);
  pass->material_set(*inst_.manager, gpumat, true);
  return pass;
}

void ForwardPipeline::render(View &view,
                             Framebuffer &prepass_fb,
                             Framebuffer &combined_fb,
                             int2 extent)
{
  if (!has_transparent_ && !has_opaque_) {
    inst_.volume.draw_resolve(view);
    return;
  }

  GPU_debug_group_begin("Forward.Opaque");

  prepass_fb.bind();
  inst_.manager->submit(prepass_ps_, view);

  inst_.hiz_buffer.set_dirty();

  inst_.shadows.set_view(view, extent);
  inst_.volume_probes.set_view(view);
  inst_.sphere_probes.set_view(view);

  if (has_opaque_) {
    combined_fb.bind();
    inst_.manager->submit(opaque_ps_, view);
  }

  GPU_debug_group_end();

  inst_.volume.draw_resolve(view);

  if (has_transparent_) {
    combined_fb.bind();
    inst_.manager->submit(transparent_ps_, view);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Layer
 * \{ */

void DeferredLayerBase::gbuffer_pass_sync(Instance &inst)
{
  gbuffer_ps_.init();
  gbuffer_ps_.subpass_transition(GPU_ATTACHMENT_WRITE,
                                 {GPU_ATTACHMENT_WRITE,
                                  GPU_ATTACHMENT_WRITE,
                                  GPU_ATTACHMENT_WRITE,
                                  GPU_ATTACHMENT_WRITE,
                                  GPU_ATTACHMENT_WRITE});
  /* G-buffer. */
  inst.gbuffer.bind_optional_layers(gbuffer_ps_);
  /* RenderPasses & AOVs. */
  gbuffer_ps_.bind_image(RBUFS_COLOR_SLOT, &inst.render_buffers.rp_color_tx);
  gbuffer_ps_.bind_image(RBUFS_VALUE_SLOT, &inst.render_buffers.rp_value_tx);
  /* Cryptomatte. */
  gbuffer_ps_.bind_image(RBUFS_CRYPTOMATTE_SLOT, &inst.render_buffers.cryptomatte_tx);
  /* Storage Buffer. */
  /* Textures. */
  gbuffer_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst.pipelines.utility_tx);

  gbuffer_ps_.bind_resources(inst.uniform_data);
  gbuffer_ps_.bind_resources(inst.sampling);
  gbuffer_ps_.bind_resources(inst.hiz_buffer.front);
  gbuffer_ps_.bind_resources(inst.cryptomatte);

  /* Bind light resources for the NPR materials that gets rendered first.
   * Non-NPR shaders will override these resource bindings. */
  gbuffer_ps_.bind_resources(inst.lights);
  gbuffer_ps_.bind_resources(inst.shadows);
  gbuffer_ps_.bind_resources(inst.sphere_probes);
  gbuffer_ps_.bind_resources(inst.volume_probes);

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_WRITE_STENCIL |
                   DRW_STATE_CLIP_CONTROL_UNIT_RANGE | DRW_STATE_STENCIL_ALWAYS;

  gbuffer_single_sided_hybrid_ps_ = &gbuffer_ps_.sub("DoubleSided");
  gbuffer_single_sided_hybrid_ps_->state_set(state | DRW_STATE_CULL_BACK);

  gbuffer_double_sided_hybrid_ps_ = &gbuffer_ps_.sub("SingleSided");
  gbuffer_double_sided_hybrid_ps_->state_set(state);

  gbuffer_double_sided_ps_ = &gbuffer_ps_.sub("DoubleSided");
  gbuffer_double_sided_ps_->state_set(state);

  gbuffer_single_sided_ps_ = &gbuffer_ps_.sub("SingleSided");
  gbuffer_single_sided_ps_->state_set(state | DRW_STATE_CULL_BACK);

  closure_bits_ = CLOSURE_NONE;
  closure_count_ = 0;
}

void DeferredLayer::begin_sync()
{
  {
    prepass_ps_.init();
    /* Textures. */
    prepass_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);

    /* Make alpha hash scale sub-pixel so that it converges to a noise free image.
     * If there is motion, use pixel scale for stability. */
    bool alpha_hash_subpixel_scale = !inst_.is_viewport() || !inst_.velocity.camera_has_motion();
    inst_.pipelines.data.alpha_hash_scale = alpha_hash_subpixel_scale ? 0.1f : 1.0f;

    prepass_ps_.bind_resources(inst_.uniform_data);
    prepass_ps_.bind_resources(inst_.velocity);
    prepass_ps_.bind_resources(inst_.sampling);

    /* Clear stencil buffer so that prepass can tag it. Then draw a fullscreen triangle that will
     * clear AOVs for all the pixels touched by this layer. */
    prepass_ps_.clear_stencil(0xFFu);
    prepass_ps_.state_stencil(0xFFu, 0u, 0xFFu);

    DRWState state_depth_only = DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_ALWAYS |
                                DRW_STATE_WRITE_DEPTH | DRW_STATE_CLIP_CONTROL_UNIT_RANGE |
                                inst_.film.depth.test_state;
    DRWState state_depth_color = state_depth_only | DRW_STATE_WRITE_COLOR;

    prepass_double_sided_static_ps_ = &prepass_ps_.sub("DoubleSided.Static");
    prepass_double_sided_static_ps_->state_set(state_depth_only);

    prepass_single_sided_static_ps_ = &prepass_ps_.sub("SingleSided.Static");
    prepass_single_sided_static_ps_->state_set(state_depth_only | DRW_STATE_CULL_BACK);

    prepass_double_sided_moving_ps_ = &prepass_ps_.sub("DoubleSided.Moving");
    prepass_double_sided_moving_ps_->state_set(state_depth_color);

    prepass_single_sided_moving_ps_ = &prepass_ps_.sub("SingleSided.Moving");
    prepass_single_sided_moving_ps_->state_set(state_depth_color | DRW_STATE_CULL_BACK);
  }

  this->gbuffer_pass_sync(inst_);
}

bool DeferredLayer::do_merge_direct_indirect_eval(const Instance &inst)
{
  return !inst.raytracing.use_raytracing();
}

bool DeferredLayer::do_split_direct_indirect_radiance(const Instance &inst)
{
  return do_merge_direct_indirect_eval(inst) &&
         (inst.sampling.use_clamp_direct() || inst.sampling.use_clamp_indirect());
}

void DeferredLayer::end_sync(bool is_first_pass,
                             bool is_last_pass,
                             bool next_layer_has_transmission)
{
  const bool has_any_closure = closure_bits_ != 0;
  /* We need the feedback output in case of refraction in the next pass (see #126455). */
  const bool is_layer_refracted = (next_layer_has_transmission && has_any_closure);
  const bool has_transmit_closure = (closure_bits_ & (CLOSURE_REFRACTION | CLOSURE_TRANSLUCENT));
  const bool has_reflect_closure = (closure_bits_ & (CLOSURE_REFLECTION | CLOSURE_DIFFUSE));
  use_raytracing_ = (has_transmit_closure || has_reflect_closure) &&
                    inst_.raytracing.use_raytracing();
  use_clamp_direct_ = inst_.sampling.use_clamp_direct();
  use_clamp_indirect_ = inst_.sampling.use_clamp_indirect();
  /* Is the radiance split for the combined pass. */
  use_split_radiance_ = use_raytracing_ || (use_clamp_direct_ || use_clamp_indirect_);

  /* The first pass will never have any surfaces behind it. Nothing is refracted except the
   * environment. So in this case, disable tracing and fallback to probe. */
  use_screen_transmission_ = use_raytracing_ && has_transmit_closure && !is_first_pass;
  use_screen_reflection_ = use_raytracing_ && has_reflect_closure;

  use_feedback_output_ = (use_raytracing_ || is_layer_refracted) &&
                         (!is_last_pass || use_screen_reflection_);

  /* Clear AOVs in case previous layers wrote to them. First pass always get clear buffer because
   * of #BackgroundPipeline::clear(). */
  if (inst_.film.aovs_info.color_len > 0 && !is_first_pass) {
    gpu::Shader *sh = inst_.shaders.static_shader_get(DEFERRED_AOV_CLEAR);
    PassMain::Sub &sub = prepass_ps_.sub("AOVsClear");
    sub.shader_set(sh);
    sub.state_set(DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_EQUAL);
    sub.bind_image("rp_color_img", &inst_.render_buffers.rp_color_tx);
    sub.bind_image("rp_value_img", &inst_.render_buffers.rp_value_tx);
    sub.bind_image("rp_cryptomatte_img", &inst_.render_buffers.cryptomatte_tx);
    sub.bind_resources(inst_.cryptomatte);
    sub.bind_resources(inst_.uniform_data);
    sub.state_stencil(0xFFu, 0x0u, 0xFFu);
    sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  {
    RenderBuffersInfoData &rbuf_data = inst_.render_buffers.data;

    /* Add the stencil classification step at the end of the GBuffer pass. */
    {
      gpu::Shader *sh = inst_.shaders.static_shader_get(DEFERRED_TILE_CLASSIFY);
      PassMain::Sub &sub = gbuffer_ps_.sub("StencilClassify");
      sub.subpass_transition(GPU_ATTACHMENT_WRITE, /* Needed for depth test. */
                             {GPU_ATTACHMENT_IGNORE,
                              GPU_ATTACHMENT_READ, /* Header. */
                              GPU_ATTACHMENT_IGNORE,
                              GPU_ATTACHMENT_IGNORE,
                              GPU_ATTACHMENT_IGNORE});
      sub.shader_set(sh);
      if (GPU_stencil_clasify_buffer_workaround()) {
        /* Binding any buffer to satisfy the binding. The buffer is not actually used. */
        sub.bind_ssbo("dummy_workaround_buf", &inst_.film.aovs_info);
      }
      sub.state_set(DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_ALWAYS);
      if (GPU_stencil_export_support()) {
        /* The shader sets the stencil directly in one full-screen pass. */
        sub.state_stencil(uint8_t(StencilBits::HEADER_BITS), /* Set by shader */ 0x0u, 0xFFu);
        sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
      }
      else {
        /* The shader cannot set the stencil directly. So we do one full-screen pass for each
         * stencil bit we need to set and accumulate the result. */
        auto set_bit = [&](StencilBits bit) {
          sub.push_constant("current_bit", int(bit));
          sub.state_stencil(uint8_t(bit), 0xFFu, 0xFFu);
          sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
        };

        if (closure_count_ > 0) {
          set_bit(StencilBits::CLOSURE_COUNT_0);
        }
        if (closure_count_ > 1) {
          set_bit(StencilBits::CLOSURE_COUNT_1);
        }
        if (closure_bits_ & CLOSURE_TRANSMISSION) {
          set_bit(StencilBits::TRANSMISSION);
        }
      }
    }

    {
      PassSimple &pass = eval_light_ps_;
      pass.init();

      /* TODO(fclem): Could also skip if no material uses thickness from shadow. */
      if (closure_bits_ & CLOSURE_TRANSMISSION) {
        PassSimple::Sub &sub = pass.sub("Eval.ThicknessFromShadow");
        sub.shader_set(inst_.shaders.static_shader_get(DEFERRED_THICKNESS_AMEND));
        sub.bind_resources(inst_.lights);
        sub.bind_resources(inst_.shadows);
        sub.bind_resources(inst_.hiz_buffer.front);
        sub.bind_resources(inst_.uniform_data);
        sub.bind_resources(inst_.sampling);
        sub.bind_texture("gbuf_header_tx", &inst_.gbuffer.header_tx);
        sub.bind_image("gbuf_normal_img", &inst_.gbuffer.normal_tx);
        sub.state_set(DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_EQUAL);
        /* Render where there is transmission and the thickness from shadow bit is set. */
        uint8_t stencil_bits = uint8_t(StencilBits::TRANSMISSION) |
                               uint8_t(StencilBits::THICKNESS_FROM_SHADOW);
        sub.state_stencil(0x0u, stencil_bits, stencil_bits);
        sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
        sub.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
      }
      {
        const bool use_transmission = (closure_bits_ & CLOSURE_TRANSMISSION) != 0;
        const bool use_split_indirect = do_split_direct_indirect_radiance(inst_);
        const bool use_lightprobe_eval = do_merge_direct_indirect_eval(inst_);
        PassSimple::Sub &sub = pass.sub("Eval.Light");
        /* Use depth test to reject background pixels which have not been stencil cleared. */
        /* WORKAROUND: Avoid rasterizer discard by enabling stencil write, but the shaders actually
         * use no fragment output. */
        sub.state_set(DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_EQUAL | DRW_STATE_DEPTH_GREATER);
        sub.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
        sub.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
        sub.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
        const ShadowSceneData &shadow_scene = inst_.shadows.get_data();
        auto set_specialization_constants =
            [&](PassSimple::Sub &sub, gpu::Shader *sh, bool use_transmission) {
              sub.specialize_constant(sh, "render_pass_shadow_id", rbuf_data.shadow_id);
              sub.specialize_constant(sh, "use_split_indirect", use_split_indirect);
              sub.specialize_constant(sh, "use_lightprobe_eval", use_lightprobe_eval);
              sub.specialize_constant(sh, "use_transmission", use_transmission);
              sub.specialize_constant(sh, "shadow_ray_count", &shadow_scene.ray_count);
              sub.specialize_constant(sh, "shadow_ray_step_count", &shadow_scene.step_count);
            };
        /* Submit the more costly ones first to avoid long tail in occupancy.
         * See page 78 of "SIGGRAPH 2023: Unreal Engine Substrate" by Hillaire & de Rousiers. */

        for (int i = min_ii(3, closure_count_) - 1; i >= 0; i--) {
          gpu::Shader *sh = inst_.shaders.static_shader_get(
              eShaderType(DEFERRED_LIGHT_SINGLE + i));
          set_specialization_constants(sub, sh, false);
          sub.shader_set(sh);
          sub.bind_image("direct_radiance_1_img", &direct_radiance_txs_[0]);
          sub.bind_image("direct_radiance_2_img", &direct_radiance_txs_[1]);
          sub.bind_image("direct_radiance_3_img", &direct_radiance_txs_[2]);
          sub.bind_image("indirect_radiance_1_img", &indirect_result_.closures[0]);
          sub.bind_image("indirect_radiance_2_img", &indirect_result_.closures[1]);
          sub.bind_image("indirect_radiance_3_img", &indirect_result_.closures[2]);
          sub.bind_resources(inst_.uniform_data);
          sub.bind_resources(inst_.gbuffer);
          sub.bind_resources(inst_.lights);
          sub.bind_resources(inst_.shadows);
          sub.bind_resources(inst_.sampling);
          sub.bind_resources(inst_.hiz_buffer.front);
          sub.bind_resources(inst_.sphere_probes);
          sub.bind_resources(inst_.volume_probes);
          uint8_t compare_mask = uint8_t(StencilBits::CLOSURE_COUNT_0) |
                                 uint8_t(StencilBits::CLOSURE_COUNT_1) |
                                 uint8_t(StencilBits::TRANSMISSION);
          sub.state_stencil(0x0u, i + 1, compare_mask);
          sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
          if (use_transmission) {
            /* Separate pass for transmission BSDF as their evaluation is quite costly. */
            set_specialization_constants(sub, sh, true);
            sub.shader_set(sh);
            sub.state_stencil(0x0u, (i + 1) | uint8_t(StencilBits::TRANSMISSION), compare_mask);
            sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
          }
        }
      }
    }
    {
      PassSimple &pass = combine_ps_;
      pass.init();
      gpu::Shader *sh = inst_.shaders.static_shader_get(DEFERRED_COMBINE);
      /* TODO(fclem): Could specialize directly with the pass index but this would break it for
       * OpenGL and Vulkan implementation which aren't fully supporting the specialize
       * constant. */
      pass.specialize_constant(sh,
                               "render_pass_diffuse_light_enabled",
                               (rbuf_data.diffuse_light_id != -1) ||
                                   (rbuf_data.diffuse_color_id != -1));
      pass.specialize_constant(sh,
                               "render_pass_specular_light_enabled",
                               (rbuf_data.specular_light_id != -1) ||
                                   (rbuf_data.specular_color_id != -1));
      pass.specialize_constant(sh, "use_split_radiance", use_split_radiance_);
      pass.specialize_constant(
          sh, "use_radiance_feedback", use_feedback_output_ && use_clamp_direct_);
      pass.specialize_constant(sh, "render_pass_normal_enabled", rbuf_data.normal_id != -1);
      pass.specialize_constant(sh, "render_pass_position_enabled", rbuf_data.position_id != -1);
      pass.shader_set(sh);
      /* Use stencil test to reject pixels not written by this layer. */
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL | DRW_STATE_STENCIL_NEQUAL);
      /* Render where stencil is not 0. */
      pass.state_stencil(0x0u, 0x0u, uint8_t(StencilBits::HEADER_BITS));
      pass.bind_texture("direct_radiance_1_tx", &direct_radiance_txs_[0]);
      pass.bind_texture("direct_radiance_2_tx", &direct_radiance_txs_[1]);
      pass.bind_texture("direct_radiance_3_tx", &direct_radiance_txs_[2]);
      pass.bind_texture("indirect_radiance_1_tx", &indirect_result_.closures[0]);
      pass.bind_texture("indirect_radiance_2_tx", &indirect_result_.closures[1]);
      pass.bind_texture("indirect_radiance_3_tx", &indirect_result_.closures[2]);
      pass.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
      pass.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
      pass.bind_image("radiance_feedback_img", &radiance_feedback_tx_);
      pass.bind_resources(inst_.gbuffer);
      pass.bind_resources(inst_.uniform_data);
      pass.bind_resources(inst_.hiz_buffer.front);
      pass.barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
      pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
  }
}

PassMain::Sub *DeferredLayer::prepass_add(::Material *blender_mat,
                                          GPUMaterial *gpumat,
                                          bool has_motion)
{
  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                            (has_motion ? prepass_single_sided_moving_ps_ :
                                          prepass_single_sided_static_ps_) :
                            (has_motion ? prepass_double_sided_moving_ps_ :
                                          prepass_double_sided_static_ps_);

  return &pass->sub(GPU_material_get_name(gpumat));
}

PassMain::Sub *DeferredLayer::material_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  eClosureBits closure_bits = shader_closure_bits_from_flag(gpumat);
  if (closure_bits == eClosureBits(0)) {
    /* Fix the case where there is no active closure in the shader.
     * In this case we force the evaluation of emission to avoid disabling the entire layer by
     * accident, see #126459. */
    closure_bits |= CLOSURE_EMISSION;
  }
  closure_bits_ |= closure_bits;
  closure_count_ = max_ii(closure_count_, count_bits_i(closure_bits));

  bool has_shader_to_rgba = (closure_bits & CLOSURE_SHADER_TO_RGBA) != 0;
  bool backface_culling = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) != 0;
  bool use_thickness_from_shadow = (blender_mat->blend_flag & MA_BL_THICKNESS_FROM_SHADOW) != 0;

  PassMain::Sub *pass = (has_shader_to_rgba) ?
                            ((backface_culling) ? gbuffer_single_sided_hybrid_ps_ :
                                                  gbuffer_double_sided_hybrid_ps_) :
                            ((backface_culling) ? gbuffer_single_sided_ps_ :
                                                  gbuffer_double_sided_ps_);

  PassMain::Sub *material_pass = &pass->sub(GPU_material_get_name(gpumat));
  /* Set stencil for some deferred specialized shaders. */
  uint8_t material_stencil_bits = 0u;
  if (use_thickness_from_shadow) {
    material_stencil_bits |= uint8_t(StencilBits::THICKNESS_FROM_SHADOW);
  }
  /* We use this opportunity to clear the stencil bits. The undefined areas are discarded using the
   * gbuf header value. */
  material_pass->state_stencil(0xFFu, material_stencil_bits, 0xFFu);

  return material_pass;
}

gpu::Texture *DeferredLayer::render(View &main_view,
                                    View &render_view,
                                    Framebuffer &prepass_fb,
                                    Framebuffer &combined_fb,
                                    Framebuffer &gbuffer_fb,
                                    int2 extent,
                                    RayTraceBuffer &rt_buffer,
                                    gpu::Texture *radiance_behind_tx)
{
  if (closure_count_ == 0) {
    return nullptr;
  }

  RenderBuffers &rb = inst_.render_buffers;

  constexpr eGPUTextureUsage usage_read = GPU_TEXTURE_USAGE_SHADER_READ;
  constexpr eGPUTextureUsage usage_write = GPU_TEXTURE_USAGE_SHADER_WRITE;
  constexpr eGPUTextureUsage usage_rw = usage_read | usage_write;

  if (use_screen_transmission_) {
    /* Update for refraction. */
    inst_.hiz_buffer.update();
  }

  GPU_framebuffer_bind(prepass_fb);
  inst_.manager->submit(prepass_ps_, render_view);

  inst_.hiz_buffer.swap_layer();
  /* Update for lighting pass or AO node. */
  inst_.hiz_buffer.update();

  inst_.volume_probes.set_view(render_view);
  inst_.sphere_probes.set_view(render_view);
  inst_.shadows.set_view(render_view, extent);

  inst_.gbuffer.bind(gbuffer_fb);
  inst_.manager->submit(gbuffer_ps_, render_view);

  for (int i = 0; i < ARRAY_SIZE(direct_radiance_txs_); i++) {
    direct_radiance_txs_[i].acquire((closure_count_ > i) ? extent : int2(1),
                                    gpu::TextureFormat::DEFERRED_RADIANCE_FORMAT,
                                    usage_rw);
  }

  if (use_raytracing_) {
    indirect_result_ = inst_.raytracing.render(
        rt_buffer, radiance_behind_tx, closure_bits_, main_view, render_view);
  }
  else if (use_split_radiance_) {
    indirect_result_ = inst_.raytracing.alloc_only(rt_buffer);
  }
  else {
    indirect_result_ = inst_.raytracing.alloc_dummy(rt_buffer);
  }

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(eval_light_ps_, render_view);

  inst_.subsurface.render(
      direct_radiance_txs_[0], indirect_result_.closures[0], closure_bits_, render_view);

  radiance_feedback_tx_ = rt_buffer.feedback_ensure(!use_feedback_output_, extent);

  if (use_feedback_output_ && use_clamp_direct_) {
    /* We need to do a copy before the combine pass (otherwise we have a dependency issue) to save
     * the emission and the previous layer's radiance. */
    GPU_texture_copy(radiance_feedback_tx_, rb.combined_tx);
  }

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(combine_ps_, render_view);

  if (use_feedback_output_ && !use_clamp_direct_) {
    /* We skip writing the radiance during the combine pass. Do a simple fast copy. */
    GPU_texture_copy(radiance_feedback_tx_, rb.combined_tx);
  }

  indirect_result_.release();

  for (int i = 0; i < ARRAY_SIZE(direct_radiance_txs_); i++) {
    direct_radiance_txs_[i].release();
  }

  inst_.pipelines.deferred.debug_draw(render_view, combined_fb);

  return use_feedback_output_ ? radiance_feedback_tx_ : nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Pipeline
 *
 * Closure data are written to intermediate buffer allowing screen space processing.
 * \{ */

void DeferredPipeline::begin_sync()
{
  Instance &inst = opaque_layer_.inst_;

  const bool use_raytracing = (inst.scene->eevee.flag & SCE_EEVEE_SSR_ENABLED) != 0;
  use_combined_lightprobe_eval = !use_raytracing;

  opaque_layer_.begin_sync();
  refraction_layer_.begin_sync();
}

void DeferredPipeline::end_sync()
{
  Instance &inst = opaque_layer_.inst_;

  opaque_layer_.end_sync(true, refraction_layer_.is_empty(), refraction_layer_.has_transmission());
  refraction_layer_.end_sync(opaque_layer_.is_empty(), true, false);

  inst.pipelines.data.gbuffer_additional_data_layer_id = this->normal_layer_count() - 1;

  debug_pass_sync();
}

void DeferredPipeline::debug_pass_sync()
{
  Instance &inst = opaque_layer_.inst_;
  if (!ELEM(inst.debug_mode,
            eDebugMode::DEBUG_GBUFFER_EVALUATION,
            eDebugMode::DEBUG_GBUFFER_STORAGE))
  {
    return;
  }

  PassSimple &pass = debug_draw_ps_;
  pass.init();
  pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
  pass.shader_set(inst.shaders.static_shader_get(DEBUG_GBUFFER));
  pass.push_constant("debug_mode", int(inst.debug_mode));
  pass.bind_resources(inst.gbuffer);
  pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void DeferredPipeline::debug_draw(draw::View &view, gpu::FrameBuffer *combined_fb)
{
  Instance &inst = opaque_layer_.inst_;
  if (!ELEM(inst.debug_mode,
            eDebugMode::DEBUG_GBUFFER_EVALUATION,
            eDebugMode::DEBUG_GBUFFER_STORAGE))
  {
    return;
  }

  switch (inst.debug_mode) {
    case eDebugMode::DEBUG_GBUFFER_EVALUATION:
      inst.info_append("Debug Mode: Deferred Lighting Cost");
      break;
    case eDebugMode::DEBUG_GBUFFER_STORAGE:
      inst.info_append("Debug Mode: Gbuffer Storage Cost");
      break;
    default:
      /* Nothing to display. */
      return;
  }

  GPU_framebuffer_bind(combined_fb);
  inst.manager->submit(debug_draw_ps_, view);
}

PassMain::Sub *DeferredPipeline::prepass_add(::Material *blender_mat,
                                             GPUMaterial *gpumat,
                                             bool has_motion)
{
  if (!use_combined_lightprobe_eval && (blender_mat->blend_flag & MA_BL_SS_REFRACTION)) {
    return refraction_layer_.prepass_add(blender_mat, gpumat, has_motion);
  }
  return opaque_layer_.prepass_add(blender_mat, gpumat, has_motion);
}

PassMain::Sub *DeferredPipeline::material_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  if (!use_combined_lightprobe_eval && (blender_mat->blend_flag & MA_BL_SS_REFRACTION)) {
    return refraction_layer_.material_add(blender_mat, gpumat);
  }
  return opaque_layer_.material_add(blender_mat, gpumat);
}

void DeferredPipeline::render(View &main_view,
                              View &render_view,
                              Framebuffer &prepass_fb,
                              Framebuffer &combined_fb,
                              Framebuffer &gbuffer_fb,
                              int2 extent,
                              RayTraceBuffer &rt_buffer_opaque_layer,
                              RayTraceBuffer &rt_buffer_refract_layer)
{
  gpu::Texture *feedback_tx = nullptr;

  GPU_debug_group_begin("Deferred.Opaque");
  feedback_tx = opaque_layer_.render(main_view,
                                     render_view,
                                     prepass_fb,
                                     combined_fb,
                                     gbuffer_fb,
                                     extent,
                                     rt_buffer_opaque_layer,
                                     feedback_tx);
  GPU_debug_group_end();

  GPU_debug_group_begin("Deferred.Refract");
  feedback_tx = refraction_layer_.render(main_view,
                                         render_view,
                                         prepass_fb,
                                         combined_fb,
                                         gbuffer_fb,
                                         extent,
                                         rt_buffer_refract_layer,
                                         feedback_tx);
  GPU_debug_group_end();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Layer
 *
 * \{ */

void VolumeLayer::sync()
{
  object_bounds_.clear();
  combined_screen_bounds_ = std::nullopt;
  use_hit_list = false;
  is_empty = true;
  finalized = false;
  has_scatter = false;
  has_absorption = false;

  draw::PassMain &layer_pass = volume_layer_ps_;
  layer_pass.init();
  layer_pass.clear_stencil(0x0u);
  {
    PassMain::Sub &pass = layer_pass.sub("occupancy_ps");
    /* Always double sided to let all fragments be invoked. */
    pass.state_set(DRW_STATE_WRITE_DEPTH);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.volume.occupancy);
    pass.bind_resources(inst_.sampling);
    occupancy_ps_ = &pass;
  }
  {
    PassMain::Sub &pass = layer_pass.sub("material_ps");
    /* Double sided with stencil equal to ensure only one fragment is invoked per pixel. */
    pass.state_set(DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_NEQUAL);
    pass.state_stencil(0x1u, 0x1u, 0x1u);
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.volume.properties);
    pass.bind_resources(inst_.sampling);
    material_ps_ = &pass;
  }
}

PassMain::Sub *VolumeLayer::occupancy_add(const Object *ob,
                                          const ::Material *blender_mat,
                                          GPUMaterial *gpumat)
{
  BLI_assert_msg((ob->type == OB_VOLUME) || GPU_material_has_volume_output(gpumat),
                 "Only volume material should be added here");
  bool use_fast_occupancy = (ob->type == OB_VOLUME) ||
                            (blender_mat->volume_intersection_method == MA_VOLUME_ISECT_FAST);
  use_hit_list |= !use_fast_occupancy;
  is_empty = false;

  PassMain::Sub *pass = &occupancy_ps_->sub(GPU_material_get_name(gpumat));
  pass->material_set(*inst_.manager, gpumat, true);
  pass->push_constant("use_fast_method", use_fast_occupancy);
  return pass;
}

PassMain::Sub *VolumeLayer::material_add(const Object *ob,
                                         const ::Material * /*blender_mat*/,
                                         GPUMaterial *gpumat)
{
  BLI_assert_msg((ob->type == OB_VOLUME) || GPU_material_has_volume_output(gpumat),
                 "Only volume material should be added here");
  UNUSED_VARS_NDEBUG(ob);

  PassMain::Sub *pass = &material_ps_->sub(GPU_material_get_name(gpumat));
  pass->material_set(*inst_.manager, gpumat, true);
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_VOLUME_SCATTER)) {
    has_scatter = true;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_VOLUME_ABSORPTION)) {
    has_absorption = true;
  }
  return pass;
}

bool VolumeLayer::bounds_overlaps(const VolumeObjectBounds &object_bounds) const
{
  /* First check the biggest area. */
  if (bounds::intersect(object_bounds.screen_bounds, combined_screen_bounds_)) {
    return true;
  }
  /* Check against individual bounds to try to squeeze the new object between them. */
  for (const std::optional<Bounds<float2>> &other_aabb : object_bounds_) {
    if (bounds::intersect(object_bounds.screen_bounds, other_aabb)) {
      return true;
    }
  }
  return false;
}

void VolumeLayer::add_object_bound(const VolumeObjectBounds &object_bounds)
{
  object_bounds_.append(object_bounds.screen_bounds);
  combined_screen_bounds_ = bounds::merge(combined_screen_bounds_, object_bounds.screen_bounds);
}

void VolumeLayer::render(View &view, Texture &occupancy_tx)
{
  if (is_empty) {
    return;
  }
  if (finalized == false) {
    finalized = true;
    if (use_hit_list) {
      /* Add resolve pass only when needed. Insert after occupancy, before material pass. */
      occupancy_ps_->shader_set(inst_.shaders.static_shader_get(VOLUME_OCCUPANCY_CONVERT));
      occupancy_ps_->barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
      occupancy_ps_->draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
  }
  /* TODO(fclem): Move this clear inside the render pass. */
  occupancy_tx.clear(uint4(0u));
  inst_.manager->submit(volume_layer_ps_, view);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Pipeline
 * \{ */

void VolumePipeline::sync()
{
  object_integration_range_ = std::nullopt;
  has_scatter_ = false;
  has_absorption_ = false;
  for (auto &layer : layers_) {
    layer->sync();
  }
}

void VolumePipeline::render(View &view, Texture &occupancy_tx)
{
  for (auto &layer : layers_) {
    layer->render(view, occupancy_tx);
  }
}

VolumeObjectBounds::VolumeObjectBounds(const Camera &camera, Object *ob)
{
  /* TODO(fclem): For panoramic camera, we will have to do this check for each cube-face. */
  const float4x4 &view_matrix = camera.data_get().viewmat;
  /* Note in practice we only care about the projection type since we only care about 2D overlap,
   * and this is independent of FOV. */
  const float4x4 &projection_matrix = camera.data_get().winmat;

  const Bounds<float3> bounds = BKE_object_boundbox_get(ob).value_or(Bounds(float3(0.0f)));

  const std::array<float3, 8> corners = bounds::corners(bounds);

  screen_bounds = std::nullopt;
  z_range = std::nullopt;

  for (const float3 &l_corner : corners) {
    float3 ws_corner = math::transform_point(ob->object_to_world(), l_corner);
    /* Split view and projection for precision. */
    float3 vs_corner = math::transform_point(view_matrix, ws_corner);
    float3 ss_corner = math::project_point(projection_matrix, vs_corner);

    z_range = bounds::min_max(z_range, vs_corner.z);
    if (camera.is_perspective() && vs_corner.z >= 1.0e-8f) {
      /* If the object is crossing the z=0 plane, we can't determine its 2D bounds easily.
       * In this case, consider the object covering the whole screen.
       * Still continue the loop for the Z range. */
      screen_bounds = Bounds<float2>(float2(-1.0f), float2(1.0f));
    }
    else {
      screen_bounds = bounds::min_max(screen_bounds, ss_corner.xy());
    }
  }
}

VolumeLayer *VolumePipeline::register_and_get_layer(Object *ob)
{
  /* TODO(fclem): This is against design. Sync shouldn't depend on view properties (camera). */
  VolumeObjectBounds object_bounds(inst_.camera, ob);
  if (math::reduce_max(object_bounds.screen_bounds->size()) < 1e-5) {
    /* WORKAROUND(fclem): Fixes an issue with 0 scaled object (see #132889).
     * Is likely to be an issue somewhere else in the pipeline but it is hard to find. */
    return nullptr;
  }

  object_integration_range_ = bounds::merge(object_integration_range_, object_bounds.z_range);

  /* Do linear search in all layers in order. This can be optimized. */
  for (auto &layer : layers_) {
    if (!layer->bounds_overlaps(object_bounds)) {
      layer->add_object_bound(object_bounds);
      return layer.get();
    }
  }
  /* No non-overlapping layer found. Create new one. */
  int64_t index = layers_.append_and_get_index(std::make_unique<VolumeLayer>(inst_));
  (*layers_[index]).add_object_bound(object_bounds);
  return layers_[index].get();
}

std::optional<Bounds<float>> VolumePipeline::object_integration_range() const
{
  return object_integration_range_;
}

bool VolumePipeline::use_hit_list() const
{
  for (const auto &layer : layers_) {
    if (layer->use_hit_list) {
      return true;
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Probe Pipeline
 *
 * Closure data are written to intermediate buffer allowing screen space processing.
 * \{ */

void DeferredProbePipeline::begin_sync()
{
  draw::PassMain &pass = opaque_layer_.prepass_ps_;
  pass.init();
  {
    /* Common resources. */

    /* Textures. */
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);

    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.velocity);
    pass.bind_resources(inst_.sampling);
  }

  DRWState state_depth_only = DRW_STATE_WRITE_DEPTH | DRW_STATE_CLIP_CONTROL_UNIT_RANGE |
                              inst_.film.depth.test_state;
  /* Only setting up static pass because we don't use motion vectors for light-probes. */
  opaque_layer_.prepass_double_sided_static_ps_ = &pass.sub("DoubleSided");
  opaque_layer_.prepass_double_sided_static_ps_->state_set(state_depth_only);
  opaque_layer_.prepass_single_sided_static_ps_ = &pass.sub("SingleSided");
  opaque_layer_.prepass_single_sided_static_ps_->state_set(state_depth_only | DRW_STATE_CULL_BACK);

  opaque_layer_.gbuffer_pass_sync(inst_);
}

void DeferredProbePipeline::end_sync()
{
  if (!opaque_layer_.prepass_ps_.is_empty()) {
    PassSimple &pass = eval_light_ps_;
    pass.init();
    /* Use depth test to reject background pixels. */
    pass.state_set(DRW_STATE_DEPTH_GREATER | DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
    pass.shader_set(inst_.shaders.static_shader_get(DEFERRED_CAPTURE_EVAL));
    pass.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
    pass.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.gbuffer);
    pass.bind_resources(inst_.lights);
    pass.bind_resources(inst_.shadows);
    pass.bind_resources(inst_.sampling);
    pass.bind_resources(inst_.hiz_buffer.front);
    pass.bind_resources(inst_.volume_probes);
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

PassMain::Sub *DeferredProbePipeline::prepass_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                            opaque_layer_.prepass_single_sided_static_ps_ :
                            opaque_layer_.prepass_double_sided_static_ps_;

  return &pass->sub(GPU_material_get_name(gpumat));
}

PassMain::Sub *DeferredProbePipeline::material_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  eClosureBits closure_bits = shader_closure_bits_from_flag(gpumat);
  if (closure_bits == eClosureBits(0)) {
    /* Fix the case where there is no active closure in the shader.
     * In this case we force the evaluation of emission to avoid disabling the entire layer by
     * accident, see #126459. */
    closure_bits |= CLOSURE_EMISSION;
  }
  opaque_layer_.closure_bits_ |= closure_bits;
  opaque_layer_.closure_count_ = max_ii(opaque_layer_.closure_count_, count_bits_i(closure_bits));

  bool has_shader_to_rgba = (closure_bits & CLOSURE_SHADER_TO_RGBA) != 0;
  bool backface_culling = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) != 0;

  PassMain::Sub *pass = (has_shader_to_rgba) ?
                            ((backface_culling) ? opaque_layer_.gbuffer_single_sided_hybrid_ps_ :
                                                  opaque_layer_.gbuffer_double_sided_hybrid_ps_) :
                            ((backface_culling) ? opaque_layer_.gbuffer_single_sided_ps_ :
                                                  opaque_layer_.gbuffer_double_sided_ps_);

  return &pass->sub(GPU_material_get_name(gpumat));
}

void DeferredProbePipeline::render(View &view,
                                   Framebuffer &prepass_fb,
                                   Framebuffer &combined_fb,
                                   Framebuffer &gbuffer_fb,
                                   int2 extent)
{
  GPU_debug_group_begin("Probe.Render");

  GPU_framebuffer_bind(prepass_fb);
  inst_.manager->submit(opaque_layer_.prepass_ps_, view);

  inst_.hiz_buffer.set_source(&inst_.render_buffers.depth_tx);
  inst_.hiz_buffer.update();

  inst_.lights.set_view(view, extent);
  inst_.shadows.set_view(view, extent);
  inst_.volume_probes.set_view(view);
  inst_.sphere_probes.set_view(view);

  /* Update for lighting pass. */
  inst_.hiz_buffer.update();

  inst_.gbuffer.bind(gbuffer_fb);
  inst_.manager->submit(opaque_layer_.gbuffer_ps_, view);

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(eval_light_ps_, view);

  GPU_debug_group_end();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Planar Probe Pipeline
 *
 * \{ */

void PlanarProbePipeline::begin_sync()
{
  {
    prepass_ps_.init();
    prepass_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    prepass_ps_.bind_ubo(CLIP_PLANE_BUF, inst_.planar_probes.world_clip_buf_);
    prepass_ps_.bind_resources(inst_.uniform_data);
    prepass_ps_.bind_resources(inst_.sampling);

    DRWState state_depth_only = DRW_STATE_WRITE_DEPTH | DRW_STATE_CLIP_CONTROL_UNIT_RANGE |
                                inst_.film.depth.test_state;

    prepass_double_sided_static_ps_ = &prepass_ps_.sub("DoubleSided.Static");
    prepass_double_sided_static_ps_->state_set(state_depth_only);

    prepass_single_sided_static_ps_ = &prepass_ps_.sub("SingleSided.Static");
    prepass_single_sided_static_ps_->state_set(state_depth_only | DRW_STATE_CULL_BACK);
  }

  this->gbuffer_pass_sync(inst_);

  closure_bits_ = CLOSURE_NONE;
  closure_count_ = 0;
}

void PlanarProbePipeline::end_sync()
{
  if (!prepass_ps_.is_empty()) {
    PassSimple &pass = eval_light_ps_;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
    pass.shader_set(inst_.shaders.static_shader_get(DEFERRED_PLANAR_EVAL));
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    pass.bind_resources(inst_.uniform_data);
    pass.bind_resources(inst_.gbuffer);
    pass.bind_resources(inst_.lights);
    pass.bind_resources(inst_.shadows);
    pass.bind_resources(inst_.sampling);
    pass.bind_resources(inst_.hiz_buffer.front);
    pass.bind_resources(inst_.sphere_probes);
    pass.bind_resources(inst_.volume_probes);
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

PassMain::Sub *PlanarProbePipeline::prepass_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                            prepass_single_sided_static_ps_ :
                            prepass_double_sided_static_ps_;
  return &pass->sub(GPU_material_get_name(gpumat));
}

PassMain::Sub *PlanarProbePipeline::material_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  eClosureBits closure_bits = shader_closure_bits_from_flag(gpumat);
  if (closure_bits == eClosureBits(0)) {
    /* Fix the case where there is no active closure in the shader.
     * In this case we force the evaluation of emission to avoid disabling the entire layer by
     * accident, see #126459. */
    closure_bits |= CLOSURE_EMISSION;
  }
  closure_bits_ |= closure_bits;
  closure_count_ = max_ii(closure_count_, count_bits_i(closure_bits));

  bool has_shader_to_rgba = (closure_bits & CLOSURE_SHADER_TO_RGBA) != 0;
  bool backface_culling = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) != 0;

  PassMain::Sub *pass = (has_shader_to_rgba) ?
                            ((backface_culling) ? gbuffer_single_sided_hybrid_ps_ :
                                                  gbuffer_double_sided_hybrid_ps_) :
                            ((backface_culling) ? gbuffer_single_sided_ps_ :
                                                  gbuffer_double_sided_ps_);

  return &pass->sub(GPU_material_get_name(gpumat));
}

void PlanarProbePipeline::render(View &view,
                                 gpu::Texture *depth_layer_tx,
                                 Framebuffer &gbuffer_fb,
                                 Framebuffer &combined_fb,
                                 int2 extent)
{
  GPU_debug_group_begin("Planar.Capture");

  inst_.pipelines.data.is_sphere_probe = true;
  inst_.uniform_data.push_update();

  GPU_framebuffer_bind(gbuffer_fb);
  GPU_framebuffer_clear_depth(gbuffer_fb, inst_.film.depth.clear_value);
  inst_.manager->submit(prepass_ps_, view);

  /* TODO(fclem): This is the only place where we use the layer source to HiZ.
   * This is because the texture layer view is still a layer texture. */
  inst_.hiz_buffer.set_source(&depth_layer_tx, 0);
  inst_.hiz_buffer.update();

  inst_.lights.set_view(view, extent);
  inst_.shadows.set_view(view, extent);
  inst_.volume_probes.set_view(view);
  inst_.sphere_probes.set_view(view);

  inst_.gbuffer.bind(gbuffer_fb);
  inst_.manager->submit(gbuffer_ps_, view);

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(eval_light_ps_, view);

  inst_.pipelines.data.is_sphere_probe = false;
  inst_.uniform_data.push_update();

  GPU_debug_group_end();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Capture Pipeline
 *
 * \{ */

void CapturePipeline::sync()
{
  surface_ps_.init();
  /* Surfel output is done using a SSBO, so no need for a fragment shader output color or depth. */
  /* WORKAROUND: Avoid rasterizer discard, but the shaders actually use no fragment output. */
  surface_ps_.state_set(DRW_STATE_WRITE_STENCIL);
  surface_ps_.framebuffer_set(&inst_.volume_probes.bake.empty_raster_fb_);

  surface_ps_.bind_ssbo(SURFEL_BUF_SLOT, &inst_.volume_probes.bake.surfels_buf_);
  surface_ps_.bind_ssbo(CAPTURE_BUF_SLOT, &inst_.volume_probes.bake.capture_info_buf_);

  surface_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  /* TODO(fclem): Remove. Bind to get the camera data,
   * but there should be no view dependent behavior during capture. */
  surface_ps_.bind_resources(inst_.uniform_data);
}

PassMain::Sub *CapturePipeline::surface_material_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  PassMain::Sub &sub_pass = surface_ps_.sub(GPU_material_get_name(gpumat));
  GPUPass *gpupass = GPU_material_get_pass(gpumat);
  sub_pass.shader_set(GPU_pass_shader_get(gpupass));
  sub_pass.push_constant("is_double_sided",
                         bool(blender_mat->blend_flag & MA_BL_LIGHTPROBE_VOLUME_DOUBLE_SIDED));
  return &sub_pass;
}

void CapturePipeline::render(View &view)
{
  inst_.manager->submit(surface_ps_, view);
}

/** \} */

}  // namespace blender::eevee
