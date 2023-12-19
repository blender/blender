/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Shading passes contain drawcalls specific to shading pipelines.
 * They are to be shared across views.
 * This file is only for shading passes. Other passes are declared in their own module.
 */

#include "eevee_instance.hh"

#include "eevee_pipeline.hh"

#include "eevee_shadow.hh"

#include "draw_common.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name World Pipeline
 *
 * Used to draw background.
 * \{ */

void BackgroundPipeline::sync(GPUMaterial *gpumat, const float background_opacity)
{
  Manager &manager = *inst_.manager;
  RenderBuffers &rbufs = inst_.render_buffers;

  world_ps_.init();
  world_ps_.state_set(DRW_STATE_WRITE_COLOR);
  world_ps_.material_set(manager, gpumat);
  world_ps_.push_constant("world_opacity_fade", background_opacity);
  world_ps_.bind_texture("utility_tx", inst_.pipelines.utility_tx);
  /* RenderPasses & AOVs. Cleared by background (even if bad practice). */
  world_ps_.bind_image("rp_color_img", &rbufs.rp_color_tx);
  world_ps_.bind_image("rp_value_img", &rbufs.rp_value_tx);
  world_ps_.bind_image("rp_cryptomatte_img", &rbufs.cryptomatte_tx);
  /* Required by validation layers. */
  inst_.cryptomatte.bind_resources(world_ps_);
  inst_.bind_uniform_data(&world_ps_);
  world_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  /* To allow opaque pass rendering over it. */
  world_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

void BackgroundPipeline::render(View &view)
{
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
  dummy_cryptomatte_tx_.ensure_2d(GPU_RGBA32F, extent, usage);
  dummy_renderpass_tx_.ensure_2d(GPU_RGBA16F, extent, usage);
  dummy_aov_color_tx_.ensure_2d_array(GPU_RGBA16F, extent, 1, usage);
  dummy_aov_value_tx_.ensure_2d_array(GPU_R16F, extent, 1, usage);

  PassSimple &pass = cubemap_face_ps_;
  pass.init();
  pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_ALWAYS);

  Manager &manager = *inst_.manager;
  pass.material_set(manager, gpumat);
  pass.push_constant("world_opacity_fade", 1.0f);
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
  inst_.cryptomatte.bind_resources(pass);
  inst_.bind_uniform_data(&pass);
  pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void WorldPipeline::render(View &view)
{
  inst_.manager->submit(cubemap_face_ps_, view);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name World Volume Pipeline
 *
 * \{ */

void WorldVolumePipeline::sync(GPUMaterial *gpumat)
{
  is_valid_ = (gpumat != nullptr) && (GPU_material_status(gpumat) == GPU_MAT_SUCCESS);
  if (!is_valid_) {
    /* Skip if the material has not compiled yet. */
    return;
  }

  world_ps_.init();
  world_ps_.state_set(DRW_STATE_WRITE_COLOR);
  world_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  inst_.bind_uniform_data(&world_ps_);
  inst_.volume.bind_properties_buffers(world_ps_);
  inst_.sampling.bind_resources(world_ps_);

  world_ps_.material_set(*inst_.manager, gpumat);
  volume_sub_pass(world_ps_, nullptr, nullptr, gpumat);

  world_ps_.dispatch(math::divide_ceil(inst_.volume.grid_size(), int3(VOLUME_GROUP_SIZE)));
  /* Sync with object property pass. */
  world_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

void WorldVolumePipeline::render(View &view)
{
  if (!is_valid_) {
    /* Clear the properties buffer instead of rendering if there is no valid shader. */
    inst_.volume.prop_scattering_tx_.clear(float4(0.0f));
    inst_.volume.prop_extinction_tx_.clear(float4(0.0f));
    inst_.volume.prop_emission_tx_.clear(float4(0.0f));
    inst_.volume.prop_phase_tx_.clear(float4(0.0f));
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
    pass.subpass_transition(GPU_ATTACHEMENT_WRITE, {GPU_ATTACHEMENT_WRITE});
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
    pass.bind_ssbo(SHADOW_VIEWPORT_INDEX_BUF_SLOT, &inst_.shadows.viewport_index_buf_);
    if (!shadow_update_tbdr) {
      /* We do not need all of the shadow information when using the TBDR-optimized approach. */
      pass.bind_image(SHADOW_ATLAS_IMG_SLOT, inst_.shadows.atlas_tx_);
      pass.bind_ssbo(SHADOW_RENDER_MAP_BUF_SLOT, &inst_.shadows.render_map_buf_);
      pass.bind_ssbo(SHADOW_PAGE_INFO_SLOT, &inst_.shadows.pages_infos_data_);
    }
    inst_.bind_uniform_data(&pass);
    inst_.sampling.bind_resources(pass);
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
    pass.subpass_transition(GPU_ATTACHEMENT_WRITE, {GPU_ATTACHEMENT_READ});
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

  DRWState state_depth_only = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
  DRWState state_depth_color = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS |
                               DRW_STATE_WRITE_COLOR;
  {
    prepass_ps_.init();

    {
      /* Common resources. */

      /* Textures. */
      prepass_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);

      inst_.bind_uniform_data(&prepass_ps_);
      inst_.velocity.bind_resources(prepass_ps_);
      inst_.sampling.bind_resources(prepass_ps_);
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
      /* RenderPasses & AOVs. */
      opaque_ps_.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
      opaque_ps_.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
      /* Cryptomatte. */
      opaque_ps_.bind_image(RBUFS_CRYPTOMATTE_SLOT, &inst_.render_buffers.cryptomatte_tx);
      /* Textures. */
      opaque_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);

      inst_.bind_uniform_data(&opaque_ps_);
      inst_.lights.bind_resources(opaque_ps_);
      inst_.shadows.bind_resources(opaque_ps_);
      inst_.volume.bind_resources(opaque_ps_);
      inst_.sampling.bind_resources(opaque_ps_);
      inst_.hiz_buffer.bind_resources(opaque_ps_);
      inst_.irradiance_cache.bind_resources(opaque_ps_);
      inst_.reflection_probes.bind_resources(opaque_ps_);
    }

    opaque_single_sided_ps_ = &opaque_ps_.sub("SingleSided");
    opaque_single_sided_ps_->state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL |
                                       DRW_STATE_CULL_BACK);

    opaque_double_sided_ps_ = &opaque_ps_.sub("DoubleSided");
    opaque_double_sided_ps_->state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL);
  }
  {
    transparent_ps_.init();
    /* Workaround limitation of PassSortable. Use dummy pass that will be sorted first in all
     * circumstances. */
    PassMain::Sub &sub = transparent_ps_.sub("ResourceBind", -FLT_MAX);

    /* Common resources. */

    /* Textures. */
    sub.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);

    inst_.bind_uniform_data(&sub);
    inst_.lights.bind_resources(sub);
    inst_.shadows.bind_resources(sub);
    inst_.volume.bind_resources(sub);
    inst_.sampling.bind_resources(sub);
    inst_.hiz_buffer.bind_resources(sub);
    inst_.irradiance_cache.bind_resources(sub);
    inst_.reflection_probes.bind_resources(sub);
  }
}

PassMain::Sub *ForwardPipeline::prepass_opaque_add(::Material *blender_mat,
                                                   GPUMaterial *gpumat,
                                                   bool has_motion)
{
  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                            (has_motion ? prepass_single_sided_moving_ps_ :
                                          prepass_single_sided_static_ps_) :
                            (has_motion ? prepass_double_sided_moving_ps_ :
                                          prepass_double_sided_static_ps_);

  /* If material is fully additive or transparent, we can skip the opaque prepass. */
  /* TODO(fclem): To skip it, we need to know if the transparent BSDF is fully white AND if there
   * is no mix shader (could do better constant folding but that's expensive). */

  return &pass->sub(GPU_material_get_name(gpumat));
}

PassMain::Sub *ForwardPipeline::material_opaque_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  BLI_assert_msg(GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT) == false,
                 "Forward Transparent should be registered directly without calling "
                 "PipelineModule::material_add()");
  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ? opaque_single_sided_ps_ :
                                                                          opaque_double_sided_ps_;
  return &pass->sub(GPU_material_get_name(gpumat));
}

PassMain::Sub *ForwardPipeline::prepass_transparent_add(const Object *ob,
                                                        ::Material *blender_mat,
                                                        GPUMaterial *gpumat)
{
  if ((blender_mat->blend_flag & MA_BL_HIDE_BACKFACE) == 0) {
    return nullptr;
  }
  DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
  if (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) {
    state |= DRW_STATE_CULL_BACK;
  }
  float sorting_value = math::dot(float3(ob->object_to_world[3]), camera_forward_);
  PassMain::Sub *pass = &transparent_ps_.sub(GPU_material_get_name(gpumat), sorting_value);
  pass->state_set(state);
  pass->material_set(*inst_.manager, gpumat);
  return pass;
}

PassMain::Sub *ForwardPipeline::material_transparent_add(const Object *ob,
                                                         ::Material *blender_mat,
                                                         GPUMaterial *gpumat)
{
  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM | DRW_STATE_DEPTH_LESS_EQUAL;
  if (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) {
    state |= DRW_STATE_CULL_BACK;
  }
  float sorting_value = math::dot(float3(ob->object_to_world[3]), camera_forward_);
  PassMain::Sub *pass = &transparent_ps_.sub(GPU_material_get_name(gpumat), sorting_value);
  pass->state_set(state);
  pass->material_set(*inst_.manager, gpumat);
  return pass;
}

void ForwardPipeline::render(View &view, Framebuffer &prepass_fb, Framebuffer &combined_fb)
{
  DRW_stats_group_start("Forward.Opaque");

  prepass_fb.bind();
  inst_.manager->submit(prepass_ps_, view);

  inst_.hiz_buffer.set_dirty();

  inst_.shadows.set_view(view, inst_.render_buffers.depth_tx);
  inst_.irradiance_cache.set_view(view);

  combined_fb.bind();
  inst_.manager->submit(opaque_ps_, view);

  DRW_stats_group_end();

  inst_.volume.draw_resolve(view);

  combined_fb.bind();
  inst_.manager->submit(transparent_ps_, view);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Layer
 * \{ */

void DeferredLayerBase::gbuffer_pass_sync(Instance &inst)
{
  gbuffer_ps_.init();
  gbuffer_ps_.subpass_transition(GPU_ATTACHEMENT_WRITE,
                                 {GPU_ATTACHEMENT_WRITE,
                                  GPU_ATTACHEMENT_WRITE,
                                  GPU_ATTACHEMENT_WRITE,
                                  GPU_ATTACHEMENT_WRITE});
  /* G-buffer. */
  gbuffer_ps_.bind_image(GBUF_CLOSURE_SLOT, &inst.gbuffer.closure_img_tx);
  gbuffer_ps_.bind_image(GBUF_COLOR_SLOT, &inst.gbuffer.color_img_tx);
  /* RenderPasses & AOVs. */
  gbuffer_ps_.bind_image(RBUFS_COLOR_SLOT, &inst.render_buffers.rp_color_tx);
  gbuffer_ps_.bind_image(RBUFS_VALUE_SLOT, &inst.render_buffers.rp_value_tx);
  /* Cryptomatte. */
  gbuffer_ps_.bind_image(RBUFS_CRYPTOMATTE_SLOT, &inst.render_buffers.cryptomatte_tx);
  /* Storage Buffer. */
  /* Textures. */
  gbuffer_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst.pipelines.utility_tx);

  inst.bind_uniform_data(&gbuffer_ps_);
  inst.sampling.bind_resources(gbuffer_ps_);
  inst.hiz_buffer.bind_resources(gbuffer_ps_);
  inst.cryptomatte.bind_resources(gbuffer_ps_);

  /* Bind light resources for the NPR materials that gets rendered first.
   * Non-NPR shaders will override these resource bindings. */
  inst.lights.bind_resources(gbuffer_ps_);
  inst.shadows.bind_resources(gbuffer_ps_);
  inst.reflection_probes.bind_resources(gbuffer_ps_);
  inst.irradiance_cache.bind_resources(gbuffer_ps_);

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;

  gbuffer_single_sided_hybrid_ps_ = &gbuffer_ps_.sub("DoubleSided");
  gbuffer_single_sided_hybrid_ps_->state_set(state | DRW_STATE_CULL_BACK);

  gbuffer_double_sided_hybrid_ps_ = &gbuffer_ps_.sub("SingleSided");
  gbuffer_double_sided_hybrid_ps_->state_set(state);

  gbuffer_double_sided_ps_ = &gbuffer_ps_.sub("DoubleSided");
  gbuffer_double_sided_ps_->state_set(state);

  gbuffer_single_sided_ps_ = &gbuffer_ps_.sub("SingleSided");
  gbuffer_single_sided_ps_->state_set(state | DRW_STATE_CULL_BACK);

  closure_bits_ = CLOSURE_NONE;
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

    inst_.bind_uniform_data(&prepass_ps_);
    inst_.velocity.bind_resources(prepass_ps_);
    inst_.sampling.bind_resources(prepass_ps_);

    DRWState state_depth_only = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
    DRWState state_depth_color = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS |
                                 DRW_STATE_WRITE_COLOR;

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

void DeferredLayer::end_sync()
{
  eClosureBits evaluated_closures = CLOSURE_DIFFUSE | CLOSURE_TRANSLUCENT | CLOSURE_REFLECTION |
                                    CLOSURE_REFRACTION;
  if (closure_bits_ & evaluated_closures) {
    /* Add the tile classification step at the end of the GBuffer pass. */
    {
      /* Fill tile mask texture with the collected closure present in a tile. */
      PassMain::Sub &sub = gbuffer_ps_.sub("TileClassify");
      sub.subpass_transition(GPU_ATTACHEMENT_WRITE, /* Needed for depth test. */
                             {GPU_ATTACHEMENT_IGNORE,
                              GPU_ATTACHEMENT_READ, /* Header. */
                              GPU_ATTACHEMENT_IGNORE,
                              GPU_ATTACHEMENT_IGNORE});
      /* Use depth test to reject background pixels. */
      /* WORKAROUND: Avoid rasterizer discard, but the shaders actually use no fragment output. */
      sub.state_set(DRW_STATE_WRITE_STENCIL | DRW_STATE_DEPTH_GREATER);
      sub.shader_set(inst_.shaders.static_shader_get(DEFERRED_TILE_CLASSIFY));
      sub.bind_image("tile_mask_img", &tile_mask_tx_);
      sub.push_constant("closure_tile_size_shift", &closure_tile_size_shift_);
      sub.barrier(GPU_BARRIER_TEXTURE_FETCH);
      sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
    {
      PassMain::Sub &sub = gbuffer_ps_.sub("TileCompaction");
      /* Use rasterizer discard. This processes the tile data to create tile command lists. */
      sub.state_set(DRW_STATE_NO_DRAW);
      sub.shader_set(inst_.shaders.static_shader_get(DEFERRED_TILE_COMPACT));
      sub.bind_texture("tile_mask_tx", &tile_mask_tx_);
      sub.bind_ssbo("closure_single_tile_buf", &closure_bufs_[0].tile_buf_);
      sub.bind_ssbo("closure_single_draw_buf", &closure_bufs_[0].draw_buf_);
      sub.bind_ssbo("closure_double_tile_buf", &closure_bufs_[1].tile_buf_);
      sub.bind_ssbo("closure_double_draw_buf", &closure_bufs_[1].draw_buf_);
      sub.bind_ssbo("closure_triple_tile_buf", &closure_bufs_[2].tile_buf_);
      sub.bind_ssbo("closure_triple_draw_buf", &closure_bufs_[2].draw_buf_);
      sub.barrier(GPU_BARRIER_TEXTURE_FETCH);
      sub.draw_procedural(GPU_PRIM_POINTS, 1, max_lighting_tile_count_);
    }

    {
      PassSimple &pass = eval_light_ps_;
      pass.init();

      {
        PassSimple::Sub &sub = pass.sub("StencilSet");
        sub.state_set(DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_ALWAYS |
                      DRW_STATE_DEPTH_GREATER);
        sub.shader_set(inst_.shaders.static_shader_get(DEFERRED_TILE_STENCIL));
        sub.push_constant("closure_tile_size_shift", &closure_tile_size_shift_);
        sub.bind_texture("direct_radiance_tx", &direct_radiance_txs_[0]);
        /* Set stencil value for each tile complexity level. */
        for (int i = 0; i < ARRAY_SIZE(closure_bufs_); i++) {
          sub.bind_ssbo("closure_tile_buf", &closure_bufs_[i].tile_buf_);
          sub.state_stencil(0xFFu, 1u << i, 0xFFu);
          sub.draw_procedural_indirect(GPU_PRIM_TRIS, closure_bufs_[i].draw_buf_);
        }
      }
      {
        PassSimple::Sub &sub = pass.sub("Eval");
        /* Use depth test to reject background pixels which have not been stencil cleared. */
        /* WORKAROUND: Avoid rasterizer discard by enabling stencil write, but the shaders actually
         * use no fragment output. */
        sub.state_set(DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_EQUAL | DRW_STATE_DEPTH_GREATER);
        sub.barrier(GPU_BARRIER_SHADER_STORAGE);
        sub.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
        sub.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
        sub.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
        /* Submit the more costly ones first to avoid long tail in occupancy.
         * See page 78 of "SIGGRAPH 2023: Unreal Engine Substrate" by Hillaire & de Rousiers. */
        for (int i = ARRAY_SIZE(closure_bufs_) - 1; i >= 0; i--) {
          sub.shader_set(inst_.shaders.static_shader_get(eShaderType(DEFERRED_LIGHT_SINGLE + i)));
          sub.bind_image("direct_radiance_1_img", &direct_radiance_txs_[0]);
          sub.bind_image("direct_radiance_2_img", &direct_radiance_txs_[1]);
          sub.bind_image("direct_radiance_3_img", &direct_radiance_txs_[2]);
          inst_.bind_uniform_data(&sub);
          inst_.gbuffer.bind_resources(sub);
          inst_.lights.bind_resources(sub);
          inst_.shadows.bind_resources(sub);
          inst_.sampling.bind_resources(sub);
          inst_.hiz_buffer.bind_resources(sub);
          sub.state_stencil(0xFFu, 1u << i, 0xFFu);
          sub.draw_procedural(GPU_PRIM_TRIS, 1, 3);
        }
      }
    }
    {
      PassSimple &pass = combine_ps_;
      pass.init();
      /* Use depth test to reject background pixels. */
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_GREATER | DRW_STATE_BLEND_ADD_FULL);
      pass.shader_set(inst_.shaders.static_shader_get(DEFERRED_COMBINE));
      pass.bind_image("direct_radiance_1_img", &direct_radiance_txs_[0]);
      pass.bind_image("direct_radiance_2_img", &direct_radiance_txs_[1]);
      pass.bind_image("direct_radiance_3_img", &direct_radiance_txs_[2]);
      pass.bind_image("indirect_diffuse_img", &indirect_diffuse_tx_);
      pass.bind_image("indirect_reflect_img", &indirect_reflect_tx_);
      pass.bind_image("indirect_refract_img", &indirect_refract_tx_);
      pass.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
      pass.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
      inst_.gbuffer.bind_resources(pass);
      inst_.bind_uniform_data(&pass);
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
  closure_bits_ |= closure_bits;

  bool has_shader_to_rgba = (closure_bits & CLOSURE_SHADER_TO_RGBA) != 0;
  bool backface_culling = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) != 0;

  PassMain::Sub *pass = (has_shader_to_rgba) ?
                            ((backface_culling) ? gbuffer_single_sided_hybrid_ps_ :
                                                  gbuffer_double_sided_hybrid_ps_) :
                            ((backface_culling) ? gbuffer_single_sided_ps_ :
                                                  gbuffer_double_sided_ps_);

  return &pass->sub(GPU_material_get_name(gpumat));
}

void DeferredLayer::render(View &main_view,
                           View &render_view,
                           Framebuffer &prepass_fb,
                           Framebuffer &combined_fb,
                           Framebuffer &gbuffer_fb,
                           int2 extent,
                           RayTraceBuffer &rt_buffer,
                           bool is_first_pass)
{
  RenderBuffers &rb = inst_.render_buffers;

  /* The first pass will never have any surfaces behind it. Nothing is refracted except the
   * environment. So in this case, disable tracing and fallback to probe. */
  bool do_screen_space_refraction = !is_first_pass && (closure_bits_ & CLOSURE_REFRACTION);
  bool do_screen_space_reflection = (closure_bits_ & (CLOSURE_REFLECTION | CLOSURE_DIFFUSE));
  eGPUTextureUsage usage_rw = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE;

  if (do_screen_space_reflection) {
    /* TODO(fclem): Verify if GPU_TEXTURE_USAGE_ATTACHMENT is needed for the copy and the clear. */
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
    if (radiance_feedback_tx_.ensure_2d(rb.color_format, extent, usage)) {
      radiance_feedback_tx_.clear(float4(0.0));
      radiance_feedback_persmat_ = render_view.persmat();
    }
  }
  else {
    /* Dummy texture. Will not be used. */
    radiance_feedback_tx_.ensure_2d(rb.color_format, int2(1), GPU_TEXTURE_USAGE_SHADER_READ);
  }

  if (do_screen_space_refraction) {
    /* Update for refraction. */
    inst_.hiz_buffer.update();
    /* TODO(fclem): Verify if GPU_TEXTURE_USAGE_ATTACHMENT is needed for the copy. */
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
    radiance_behind_tx_.ensure_2d(rb.color_format, extent, usage);
    GPU_texture_copy(radiance_behind_tx_, rb.combined_tx);
  }
  else {
    /* Dummy texture. Will not be used. */
    radiance_behind_tx_.ensure_2d(rb.color_format, int2(1), GPU_TEXTURE_USAGE_SHADER_READ);
  }

  GPU_framebuffer_bind(prepass_fb);
  inst_.manager->submit(prepass_ps_, render_view);

  inst_.hiz_buffer.swap_layer();
  /* Update for lighting pass or AO node. */
  inst_.hiz_buffer.update();

  inst_.irradiance_cache.set_view(render_view);
  inst_.shadows.set_view(render_view, inst_.render_buffers.depth_tx);

  if (/* FIXME(fclem): Vulkan doesn't implement load / store config yet. */
      GPU_backend_get_type() == GPU_BACKEND_VULKAN ||
      /* FIXME(fclem): Metal has bug in backend. */
      GPU_backend_get_type() == GPU_BACKEND_METAL)
  {
    inst_.gbuffer.header_tx.clear(int4(0));
  }

  int2 tile_mask_size;
  int tile_count;
  closure_tile_size_shift_ = 4;
  /* Increase tile size until they fit the budget. */
  for (int i = 0; i < 4; i++, closure_tile_size_shift_++) {
    tile_mask_size = math::divide_ceil(extent, int2(1u << closure_tile_size_shift_));
    tile_count = tile_mask_size.x * tile_mask_size.y;
    if (tile_count <= max_lighting_tile_count_) {
      break;
    }
  }

  int target_count = power_of_2_max_u(tile_count);
  for (int i = 0; i < ARRAY_SIZE(closure_bufs_); i++) {
    closure_bufs_[i].tile_buf_.resize(target_count);
    closure_bufs_[i].draw_buf_.clear_to_zero();
  }

  tile_mask_tx_.ensure_2d_array(GPU_R8UI, tile_mask_size, 4, usage_rw);
  tile_mask_tx_.clear(uint4(0));

  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    /* TODO(fclem): Load/store action is broken on Metal. */
    GPU_framebuffer_bind(gbuffer_fb);
  }
  else {
    GPU_framebuffer_bind_ex(
        gbuffer_fb,
        {
            {GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE},       /* Depth */
            {GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE},       /* Combined */
            {GPU_LOADACTION_CLEAR, GPU_STOREACTION_STORE, {0}}, /* GBuf Header */
            {GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_STORE},  /* GBuf Closure */
            {GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_STORE},  /* GBuf Color */
        });
  }

  inst_.manager->submit(gbuffer_ps_, render_view);

  int closure_count = count_bits_i(closure_bits_ &
                                   (CLOSURE_REFLECTION | CLOSURE_DIFFUSE | CLOSURE_TRANSLUCENT));
  for (int i = 0; i < ARRAY_SIZE(direct_radiance_txs_); i++) {
    direct_radiance_txs_[i].acquire(
        (closure_count > 1) ? extent : int2(1), GPU_R11F_G11F_B10F, usage_rw);
  }

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(eval_light_ps_, render_view);

  RayTraceResult indirect_result = inst_.raytracing.render(rt_buffer,
                                                           radiance_behind_tx_,
                                                           radiance_feedback_tx_,
                                                           radiance_feedback_persmat_,
                                                           closure_bits_,
                                                           main_view,
                                                           render_view,
                                                           do_screen_space_refraction);

  indirect_diffuse_tx_ = indirect_result.diffuse.get();
  indirect_reflect_tx_ = indirect_result.reflect.get();
  indirect_refract_tx_ = indirect_result.refract.get();

  inst_.subsurface.render(
      direct_radiance_txs_[0], indirect_diffuse_tx_, closure_bits_, render_view);

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(combine_ps_);

  indirect_result.release();

  for (int i = 0; i < ARRAY_SIZE(direct_radiance_txs_); i++) {
    direct_radiance_txs_[i].release();
  }

  if (do_screen_space_reflection) {
    GPU_texture_copy(radiance_feedback_tx_, rb.combined_tx);
    radiance_feedback_persmat_ = render_view.persmat();
  }

  inst_.pipelines.deferred.debug_draw(render_view, combined_fb);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Pipeline
 *
 * Closure data are written to intermediate buffer allowing screen space processing.
 * \{ */

void DeferredPipeline::begin_sync()
{
  opaque_layer_.begin_sync();
  refraction_layer_.begin_sync();
}

void DeferredPipeline::end_sync()
{
  opaque_layer_.end_sync();
  refraction_layer_.end_sync();

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
  inst.gbuffer.bind_resources(pass);
  pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void DeferredPipeline::debug_draw(draw::View &view, GPUFrameBuffer *combined_fb)
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
      inst.info = "Debug Mode: Deferred Lighting Cost";
      break;
    case eDebugMode::DEBUG_GBUFFER_STORAGE:
      inst.info = "Debug Mode: Gbuffer Storage Cost";
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
  if (blender_mat->blend_flag & MA_BL_SS_REFRACTION) {
    return refraction_layer_.prepass_add(blender_mat, gpumat, has_motion);
  }
  else {
    return opaque_layer_.prepass_add(blender_mat, gpumat, has_motion);
  }
}

PassMain::Sub *DeferredPipeline::material_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  if (blender_mat->blend_flag & MA_BL_SS_REFRACTION) {
    return refraction_layer_.material_add(blender_mat, gpumat);
  }
  else {
    return opaque_layer_.material_add(blender_mat, gpumat);
  }
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
  DRW_stats_group_start("Deferred.Opaque");
  opaque_layer_.render(main_view,
                       render_view,
                       prepass_fb,
                       combined_fb,
                       gbuffer_fb,
                       extent,
                       rt_buffer_opaque_layer,
                       true);
  DRW_stats_group_end();

  DRW_stats_group_start("Deferred.Refract");
  refraction_layer_.render(main_view,
                           render_view,
                           prepass_fb,
                           combined_fb,
                           gbuffer_fb,
                           extent,
                           rt_buffer_refract_layer,
                           false);
  DRW_stats_group_end();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Layer
 *
 * \{ */

void VolumeLayer::sync()
{
  object_bounds_.clear();
  use_hit_list = false;
  is_empty = true;
  finalized = false;

  draw::PassMain &layer_pass = volume_layer_ps_;
  layer_pass.init();
  {
    PassMain::Sub &pass = layer_pass.sub("occupancy_ps");
    /* Double sided without depth test. */
    pass.state_set(DRW_STATE_WRITE_DEPTH);
    inst_.bind_uniform_data(&pass);
    inst_.volume.bind_occupancy_buffers(pass);
    inst_.sampling.bind_resources(pass);
    occupancy_ps_ = &pass;
  }
  {
    PassMain::Sub &pass = layer_pass.sub("material_ps");
    pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    inst_.bind_uniform_data(&pass);
    inst_.volume.bind_properties_buffers(pass);
    inst_.sampling.bind_resources(pass);
    material_ps_ = &pass;
  }
}

PassMain::Sub *VolumeLayer::occupancy_add(const Object *ob,
                                          const ::Material *blender_mat,
                                          GPUMaterial *gpumat)
{
  BLI_assert_msg(GPU_material_has_volume_output(gpumat) == true,
                 "Only volume material should be added here");
  bool use_fast_occupancy = (ob->type == OB_VOLUME) ||
                            (blender_mat->volume_intersection_method == MA_VOLUME_ISECT_FAST);
  use_hit_list |= !use_fast_occupancy;
  is_empty = false;

  PassMain::Sub *pass = &occupancy_ps_->sub(GPU_material_get_name(gpumat));
  pass->material_set(*inst_.manager, gpumat);
  pass->push_constant("use_fast_method", use_fast_occupancy);
  return pass;
}

PassMain::Sub *VolumeLayer::material_add(const Object * /*ob*/,
                                         const ::Material * /*blender_mat*/,
                                         GPUMaterial *gpumat)
{
  BLI_assert_msg(GPU_material_has_volume_output(gpumat) == true,
                 "Only volume material should be added here");
  PassMain::Sub *pass = &material_ps_->sub(GPU_material_get_name(gpumat));
  pass->material_set(*inst_.manager, gpumat);
  return pass;
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
  enabled_ = false;
  has_scatter_ = false;
  has_absorption_ = false;
  for (auto &layer : layers_) {
    layer->sync();
  }
}

void VolumePipeline::render(View &view, Texture &occupancy_tx)
{
  BLI_assert_msg(enabled_, "Trying to run the volume object pipeline with no actual volume calls");

  for (auto &layer : layers_) {
    layer->render(view, occupancy_tx);
  }
}

GridAABB VolumePipeline::grid_aabb_from_object(Object *ob)
{
  const Camera &camera = inst_.camera;
  const VolumesInfoData &data = inst_.volume.data_;
  /* Returns the unified volume grid cell corner of a world space coordinate. */
  auto to_global_grid_coords = [&](float3 wP) -> int3 {
    /* TODO(fclem): Should we use the render view winmat and not the camera one? */
    const float4x4 &view_matrix = camera.data_get().viewmat;
    const float4x4 &projection_matrix = camera.data_get().winmat;

    float3 ndc_coords = math::project_point(projection_matrix * view_matrix, wP);
    ndc_coords = (ndc_coords * 0.5f) + float3(0.5f);

    float3 grid_coords = screen_to_volume(projection_matrix,
                                          data.depth_near,
                                          data.depth_far,
                                          data.depth_distribution,
                                          data.coord_scale,
                                          ndc_coords);
    /* Round to nearest grid corner. */
    return int3(grid_coords * float3(data.tex_size) + 0.5);
  };

  const Bounds<float3> bounds = BKE_object_boundbox_get(ob).value_or(Bounds(float3(0)));
  int3 min = int3(INT32_MAX);
  int3 max = int3(INT32_MIN);

  BoundBox bb;
  BKE_boundbox_init_from_minmax(&bb, bounds.min, bounds.max);
  for (float3 l_corner : bb.vec) {
    float3 w_corner = math::transform_point(float4x4(ob->object_to_world), l_corner);
    /* Note that this returns the nearest cell corner coordinate.
     * So sub-froxel AABB will effectively return the same coordinate
     * for each corner (making it empty and skipped) unless it
     * cover the center of the froxel. */
    math::min_max(to_global_grid_coords(w_corner), min, max);
  }
  return {min, max};
}

GridAABB VolumePipeline::grid_aabb_from_view()
{
  return {int3(0), inst_.volume.data_.tex_size};
}

VolumeLayer *VolumePipeline::register_and_get_layer(Object *ob)
{
  GridAABB object_aabb = grid_aabb_from_object(ob);
  GridAABB view_aabb = grid_aabb_from_view();
  if (object_aabb.intersection(view_aabb).is_empty()) {
    /* Skip invisible object with respect to raster grid and bounds density. */
    return nullptr;
  }
  /* Do linear search in all layers in order. This can be optimized. */
  for (auto &layer : layers_) {
    if (!layer->bounds_overlaps(object_aabb)) {
      layer->add_object_bound(object_aabb);
      return layer.get();
    }
  }
  /* No non-overlapping layer found. Create new one. */
  int64_t index = layers_.append_and_get_index(std::make_unique<VolumeLayer>(inst_));
  (*layers_[index]).add_object_bound(object_aabb);
  return layers_[index].get();
}

void VolumePipeline::material_call(MaterialPass &volume_material_pass,
                                   Object *ob,
                                   ResourceHandle res_handle)
{
  if (volume_material_pass.sub_pass == nullptr) {
    /* Can happen if shader is not compiled, or if object has been culled. */
    return;
  }

  /* TODO(fclem): This should be revisited, `volume_sub_pass()` should not decide on the volume
   * visibility. Instead, we should query visibility upstream and not try to even compile the
   * shader. */
  PassMain::Sub *object_pass = volume_sub_pass(
      *volume_material_pass.sub_pass, inst_.scene, ob, volume_material_pass.gpumat);
  if (object_pass) {
    /* Possible double work here. Should be relatively insignificant in practice. */
    GridAABB object_aabb = grid_aabb_from_object(ob);
    GridAABB view_aabb = grid_aabb_from_view();
    GridAABB visible_aabb = object_aabb.intersection(view_aabb);
    /* Invisible volumes should already have been clipped. */
    BLI_assert(visible_aabb.is_empty() == false);
    /* TODO(fclem): Use graphic pipeline instead of compute so we can leverage GPU culling,
     * resource indexing and other further optimizations. */
    object_pass->push_constant("drw_ResourceID", int(res_handle.resource_index()));
    object_pass->push_constant("grid_coords_min", visible_aabb.min);
    object_pass->dispatch(math::divide_ceil(visible_aabb.extent(), int3(VOLUME_GROUP_SIZE)));
    /* Notify the volume module to enable itself. */
    enabled_ = true;
    if (GPU_material_flag_get(volume_material_pass.gpumat, GPU_MATFLAG_VOLUME_SCATTER)) {
      has_scatter_ = true;
    }
    if (GPU_material_flag_get(volume_material_pass.gpumat, GPU_MATFLAG_VOLUME_ABSORPTION)) {
      has_absorption_ = true;
    }
  }
}

bool VolumePipeline::use_hit_list() const
{
  for (auto &layer : layers_) {
    if (layer->use_hit_list) {
      return true;
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Probe Layer
 * \{ */

void DeferredProbeLayer::begin_sync()
{
  {
    prepass_ps_.init();
    {
      /* Common resources. */

      /* Textures. */
      prepass_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);

      inst_.bind_uniform_data(&prepass_ps_);
      inst_.velocity.bind_resources(prepass_ps_);
      inst_.sampling.bind_resources(prepass_ps_);
    }

    DRWState state_depth_only = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
    /* Only setting up static pass because we don't use motion vectors for light-probes. */
    prepass_double_sided_static_ps_ = &prepass_ps_.sub("DoubleSided");
    prepass_double_sided_static_ps_->state_set(state_depth_only);
    prepass_single_sided_static_ps_ = &prepass_ps_.sub("SingleSided");
    prepass_single_sided_static_ps_->state_set(state_depth_only | DRW_STATE_CULL_BACK);
  }

  this->gbuffer_pass_sync(inst_);
}

void DeferredProbeLayer::end_sync()
{
  if (closure_bits_ & (CLOSURE_DIFFUSE | CLOSURE_REFLECTION)) {
    PassSimple &pass = eval_light_ps_;
    pass.init();
    /* Use depth test to reject background pixels. */
    pass.state_set(DRW_STATE_DEPTH_GREATER | DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
    pass.shader_set(inst_.shaders.static_shader_get(DEFERRED_CAPTURE_EVAL));
    pass.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
    pass.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    inst_.bind_uniform_data(&pass);
    inst_.gbuffer.bind_resources(pass);
    inst_.lights.bind_resources(pass);
    inst_.shadows.bind_resources(pass);
    inst_.sampling.bind_resources(pass);
    inst_.hiz_buffer.bind_resources(pass);
    inst_.irradiance_cache.bind_resources(pass);
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

PassMain::Sub *DeferredProbeLayer::prepass_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                            prepass_single_sided_static_ps_ :
                            prepass_double_sided_static_ps_;

  return &pass->sub(GPU_material_get_name(gpumat));
}

PassMain::Sub *DeferredProbeLayer::material_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  eClosureBits closure_bits = shader_closure_bits_from_flag(gpumat);
  closure_bits_ |= closure_bits;

  bool has_shader_to_rgba = (closure_bits & CLOSURE_SHADER_TO_RGBA) != 0;
  bool backface_culling = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) != 0;

  PassMain::Sub *pass = (has_shader_to_rgba) ?
                            ((backface_culling) ? gbuffer_single_sided_hybrid_ps_ :
                                                  gbuffer_double_sided_hybrid_ps_) :
                            ((backface_culling) ? gbuffer_single_sided_ps_ :
                                                  gbuffer_double_sided_ps_);

  return &pass->sub(GPU_material_get_name(gpumat));
}

void DeferredProbeLayer::render(View &view,
                                Framebuffer &prepass_fb,
                                Framebuffer &combined_fb,
                                Framebuffer &gbuffer_fb,
                                int2 extent)
{
  inst_.pipelines.data.is_probe_reflection = true;
  inst_.push_uniform_data();

  GPU_framebuffer_bind(prepass_fb);
  inst_.manager->submit(prepass_ps_, view);

  inst_.hiz_buffer.set_source(&inst_.render_buffers.depth_tx);
  inst_.lights.set_view(view, extent);
  inst_.shadows.set_view(view, inst_.render_buffers.depth_tx);
  inst_.irradiance_cache.set_view(view);

  /* Update for lighting pass. */
  inst_.hiz_buffer.update();

  GPU_framebuffer_bind(gbuffer_fb);
  inst_.manager->submit(gbuffer_ps_, view);

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(eval_light_ps_, view);

  inst_.pipelines.data.is_probe_reflection = false;
  inst_.push_uniform_data();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deferred Probe Pipeline
 *
 * Closure data are written to intermediate buffer allowing screen space processing.
 * \{ */

void DeferredProbePipeline::begin_sync()
{
  opaque_layer_.begin_sync();
}

void DeferredProbePipeline::end_sync()
{
  opaque_layer_.end_sync();
}

PassMain::Sub *DeferredProbePipeline::prepass_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  return opaque_layer_.prepass_add(blender_mat, gpumat);
}

PassMain::Sub *DeferredProbePipeline::material_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  return opaque_layer_.material_add(blender_mat, gpumat);
}

void DeferredProbePipeline::render(View &view,
                                   Framebuffer &prepass_fb,
                                   Framebuffer &combined_fb,
                                   Framebuffer &gbuffer_fb,
                                   int2 extent)
{
  GPU_debug_group_begin("Probe.Render");
  opaque_layer_.render(view, prepass_fb, combined_fb, gbuffer_fb, extent);
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
    inst_.bind_uniform_data(&prepass_ps_);
    inst_.sampling.bind_resources(prepass_ps_);

    DRWState state_depth_only = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;

    prepass_double_sided_static_ps_ = &prepass_ps_.sub("DoubleSided.Static");
    prepass_double_sided_static_ps_->state_set(state_depth_only);

    prepass_single_sided_static_ps_ = &prepass_ps_.sub("SingleSided.Static");
    prepass_single_sided_static_ps_->state_set(state_depth_only | DRW_STATE_CULL_BACK);
  }

  this->gbuffer_pass_sync(inst_);

  {
    PassSimple &pass = eval_light_ps_;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
    pass.shader_set(inst_.shaders.static_shader_get(DEFERRED_PLANAR_EVAL));
    pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    inst_.bind_uniform_data(&pass);
    inst_.gbuffer.bind_resources(pass);
    inst_.lights.bind_resources(pass);
    inst_.shadows.bind_resources(pass);
    inst_.sampling.bind_resources(pass);
    inst_.hiz_buffer.bind_resources(pass);
    inst_.reflection_probes.bind_resources(pass);
    inst_.irradiance_cache.bind_resources(pass);
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  closure_bits_ = CLOSURE_NONE;
}

void PlanarProbePipeline::end_sync()
{
  /* No-op for now. */
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
  closure_bits_ |= closure_bits;

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
                                 GPUTexture *depth_layer_tx,
                                 Framebuffer &gbuffer_fb,
                                 Framebuffer &combined_fb,
                                 int2 extent)
{
  GPU_debug_group_begin("Planar.Capture");

  inst_.pipelines.data.is_probe_reflection = true;
  inst_.push_uniform_data();

  GPU_framebuffer_bind(gbuffer_fb);
  GPU_framebuffer_clear_depth(gbuffer_fb, 1.0f);
  inst_.manager->submit(prepass_ps_, view);

  /* TODO(fclem): This is the only place where we use the layer source to HiZ.
   * This is because the texture layer view is still a layer texture. */
  inst_.hiz_buffer.set_source(&depth_layer_tx, 0);
  inst_.lights.set_view(view, extent);
  inst_.shadows.set_view(view, depth_layer_tx);
  inst_.irradiance_cache.set_view(view);

  /* Update for lighting pass. */
  inst_.hiz_buffer.update();

  GPU_framebuffer_bind_ex(gbuffer_fb,
                          {
                              {GPU_LOADACTION_LOAD, GPU_STOREACTION_STORE},          /* Depth */
                              {GPU_LOADACTION_CLEAR, GPU_STOREACTION_STORE, {0.0f}}, /* Combined */
                              {GPU_LOADACTION_CLEAR, GPU_STOREACTION_STORE, {0}}, /* GBuf Header */
                              {GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_STORE}, /* GBuf Closure */
                              {GPU_LOADACTION_DONT_CARE, GPU_STOREACTION_STORE}, /* GBuf Color */
                          });
  inst_.manager->submit(gbuffer_ps_, view);

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(eval_light_ps_, view);

  inst_.pipelines.data.is_probe_reflection = false;
  inst_.push_uniform_data();

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
  surface_ps_.framebuffer_set(&inst_.irradiance_cache.bake.empty_raster_fb_);

  surface_ps_.bind_ssbo(SURFEL_BUF_SLOT, &inst_.irradiance_cache.bake.surfels_buf_);
  surface_ps_.bind_ssbo(CAPTURE_BUF_SLOT, &inst_.irradiance_cache.bake.capture_info_buf_);

  surface_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  /* TODO(fclem): Remove. Bind to get the camera data,
   * but there should be no view dependent behavior during capture. */
  inst_.bind_uniform_data(&surface_ps_);
}

PassMain::Sub *CapturePipeline::surface_material_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  PassMain::Sub &sub_pass = surface_ps_.sub(GPU_material_get_name(gpumat));
  GPUPass *gpupass = GPU_material_get_pass(gpumat);
  sub_pass.shader_set(GPU_pass_shader_get(gpupass));
  sub_pass.push_constant("is_double_sided",
                         !(blender_mat->blend_flag & MA_BL_LIGHTPROBE_VOLUME_DOUBLE_SIDED));
  return &sub_pass;
}

void CapturePipeline::render(View &view)
{
  inst_.manager->submit(surface_ps_, view);
}

/** \} */

}  // namespace blender::eevee
