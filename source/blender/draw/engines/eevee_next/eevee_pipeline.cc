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

  ResourceHandle handle = manager.resource_handle(float4x4::identity());

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

  world_ps_.draw(DRW_cache_fullscreen_quad_get(), handle);
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
  ResourceHandle handle = manager.resource_handle(float4x4::identity());
  pass.material_set(manager, gpumat);
  pass.push_constant("world_opacity_fade", 1.0f);

  pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  inst_.bind_uniform_data(&pass);
  pass.bind_image("rp_normal_img", dummy_renderpass_tx_);
  pass.bind_image("rp_light_img", dummy_renderpass_tx_);
  pass.bind_image("rp_diffuse_color_img", dummy_renderpass_tx_);
  pass.bind_image("rp_specular_color_img", dummy_renderpass_tx_);
  pass.bind_image("rp_emission_img", dummy_renderpass_tx_);
  pass.bind_image("rp_cryptomatte_img", dummy_cryptomatte_tx_);
  pass.bind_image("rp_color_img", dummy_aov_color_tx_);
  pass.bind_image("rp_value_img", dummy_aov_value_tx_);
  /* Required by validation layers. */
  inst_.cryptomatte.bind_resources(pass);

  pass.bind_image("aov_color_img", dummy_aov_color_tx_);
  pass.bind_image("aov_value_img", dummy_aov_value_tx_);
  pass.bind_ssbo("aov_buf", &inst_.film.aovs_info);

  pass.draw(DRW_cache_fullscreen_quad_get(), handle);
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
  is_valid_ = GPU_material_status(gpumat) == GPU_MAT_SUCCESS;
  if (!is_valid_) {
    /* Skip if the material has not compiled yet. */
    return;
  }

  world_ps_.init();
  world_ps_.state_set(DRW_STATE_WRITE_COLOR);
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
    /* Skip if the material has not compiled yet. */
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
  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL;
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

  inst_.shadows.set_view(view);
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

void DeferredLayer::begin_sync()
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
  {
    gbuffer_ps_.init();
    gbuffer_ps_.clear_stencil(0x00u);
    gbuffer_ps_.state_stencil(0xFFu, 0xFFu, 0xFFu);

    {
      /* Common resources. */

      /* G-buffer. */
      gbuffer_ps_.bind_image(GBUF_CLOSURE_SLOT, &inst_.gbuffer.closure_tx);
      gbuffer_ps_.bind_image(GBUF_COLOR_SLOT, &inst_.gbuffer.color_tx);
      gbuffer_ps_.bind_image(GBUF_HEADER_SLOT, &inst_.gbuffer.header_tx);
      /* RenderPasses & AOVs. */
      gbuffer_ps_.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
      gbuffer_ps_.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
      /* Cryptomatte. */
      gbuffer_ps_.bind_image(RBUFS_CRYPTOMATTE_SLOT, &inst_.render_buffers.cryptomatte_tx);
      /* Storage Buffer. */
      /* Textures. */
      gbuffer_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);

      inst_.bind_uniform_data(&gbuffer_ps_);
      inst_.sampling.bind_resources(gbuffer_ps_);
      inst_.hiz_buffer.bind_resources(gbuffer_ps_);
      inst_.cryptomatte.bind_resources(gbuffer_ps_);
    }

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_WRITE_STENCIL |
                     DRW_STATE_STENCIL_ALWAYS;

    gbuffer_double_sided_ps_ = &gbuffer_ps_.sub("DoubleSided");
    gbuffer_double_sided_ps_->state_set(state);

    gbuffer_single_sided_ps_ = &gbuffer_ps_.sub("SingleSided");
    gbuffer_single_sided_ps_->state_set(state | DRW_STATE_CULL_BACK);
  }

  closure_bits_ = CLOSURE_NONE;
}

void DeferredLayer::end_sync()
{
  eClosureBits evaluated_closures = CLOSURE_DIFFUSE | CLOSURE_REFLECTION | CLOSURE_REFRACTION;
  if (closure_bits_ & evaluated_closures) {
    {
      PassSimple &pass = eval_light_ps_;
      pass.init();
      /* Use stencil test to reject pixel not written by this layer. */
      pass.state_set(DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_NEQUAL);
      pass.state_stencil(0x00u, 0x00u, evaluated_closures);
      pass.shader_set(inst_.shaders.static_shader_get(DEFERRED_LIGHT));
      pass.bind_image("direct_diffuse_img", &direct_diffuse_tx_);
      pass.bind_image("direct_reflect_img", &direct_reflect_tx_);
      pass.bind_image("direct_refract_img", &direct_refract_tx_);
      pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
      pass.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
      pass.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
      inst_.bind_uniform_data(&pass);
      inst_.gbuffer.bind_resources(pass);
      inst_.lights.bind_resources(pass);
      inst_.shadows.bind_resources(pass);
      inst_.sampling.bind_resources(pass);
      inst_.hiz_buffer.bind_resources(pass);
      pass.barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
      pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
    {
      PassSimple &pass = combine_ps_;
      pass.init();
      /* Use stencil test to reject pixel not written by this layer. */
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_NEQUAL | DRW_STATE_BLEND_ADD_FULL);
      pass.state_stencil(0x00u, 0x00u, evaluated_closures);
      pass.shader_set(inst_.shaders.static_shader_get(DEFERRED_COMBINE));
      pass.bind_image("direct_diffuse_img", &direct_diffuse_tx_);
      pass.bind_image("direct_reflect_img", &direct_reflect_tx_);
      pass.bind_image("direct_refract_img", &direct_refract_tx_);
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

  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                            gbuffer_single_sided_ps_ :
                            gbuffer_double_sided_ps_;
  pass = &pass->sub(GPU_material_get_name(gpumat));
  pass->state_stencil(closure_bits, 0xFFu, 0xFFu);
  return pass;
}

void DeferredLayer::render(View &main_view,
                           View &render_view,
                           Framebuffer &prepass_fb,
                           Framebuffer &combined_fb,
                           int2 extent,
                           RayTraceBuffer &rt_buffer,
                           bool is_first_pass)
{
  RenderBuffers &rb = inst_.render_buffers;

  /* The first pass will never have any surfaces behind it. Nothing is refracted except the
   * environment. So in this case, disable tracing and fallback to probe. */
  bool do_screen_space_refraction = !is_first_pass && (closure_bits_ & CLOSURE_REFRACTION);
  bool do_screen_space_reflection = (closure_bits_ & CLOSURE_REFLECTION);

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

  inst_.gbuffer.acquire(extent, closure_bits_);

  if (closure_bits_ & CLOSURE_AMBIENT_OCCLUSION) {
    /* If the shader needs Ambient Occlusion, we need to update the HiZ here. */
    if (do_screen_space_refraction) {
      /* TODO(fclem): This update conflicts with the refraction screen tracing which need the depth
       * behind the refractive surface. In this case, we do not update the Hi-Z and only consider
       * surfaces already in the Hi-Z buffer for the ambient occlusion computation. This might be
       * solved (if really problematic) by having another copy of the Hi-Z buffer. */
    }
    else {
      inst_.hiz_buffer.update();
    }
  }

  /* TODO(fclem): Clear in pass when Gbuffer will render with framebuffer. */
  inst_.gbuffer.header_tx.clear(uint4(0));

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(gbuffer_ps_, render_view);

  inst_.hiz_buffer.set_dirty();

  inst_.irradiance_cache.set_view(render_view);

  RayTraceResult refract_result = inst_.raytracing.trace(rt_buffer,
                                                         radiance_behind_tx_,
                                                         render_view.persmat(),
                                                         closure_bits_,
                                                         CLOSURE_REFRACTION,
                                                         main_view,
                                                         render_view,
                                                         !do_screen_space_refraction);

  /* Only update the HiZ after refraction tracing. */
  inst_.hiz_buffer.update();

  inst_.shadows.set_view(render_view);

  {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                             GPU_TEXTURE_USAGE_ATTACHMENT;
    direct_diffuse_tx_.acquire(extent, GPU_RGBA16F, usage);
    direct_reflect_tx_.acquire(extent, GPU_RGBA16F, usage);
    direct_refract_tx_.acquire(extent, GPU_RGBA16F, usage);
  }

  inst_.manager->submit(eval_light_ps_, render_view);

  RayTraceResult diffuse_result = inst_.raytracing.trace(rt_buffer,
                                                         radiance_feedback_tx_,
                                                         radiance_feedback_persmat_,
                                                         closure_bits_,
                                                         CLOSURE_DIFFUSE,
                                                         main_view,
                                                         render_view);

  RayTraceResult reflect_result = inst_.raytracing.trace(rt_buffer,
                                                         radiance_feedback_tx_,
                                                         radiance_feedback_persmat_,
                                                         closure_bits_,
                                                         CLOSURE_REFLECTION,
                                                         main_view,
                                                         render_view);

  indirect_diffuse_tx_ = diffuse_result.get();
  indirect_reflect_tx_ = reflect_result.get();
  indirect_refract_tx_ = refract_result.get();

  inst_.subsurface.render(direct_diffuse_tx_, indirect_diffuse_tx_, closure_bits_, render_view);

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(combine_ps_);

  diffuse_result.release();
  refract_result.release();
  reflect_result.release();

  direct_diffuse_tx_.release();
  direct_reflect_tx_.release();
  direct_refract_tx_.release();

  if (do_screen_space_reflection) {
    GPU_texture_copy(radiance_feedback_tx_, rb.combined_tx);
    radiance_feedback_persmat_ = render_view.persmat();
  }

  inst_.gbuffer.release();
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
                              int2 extent,
                              RayTraceBuffer &rt_buffer_opaque_layer,
                              RayTraceBuffer &rt_buffer_refract_layer)
{
  DRW_stats_group_start("Deferred.Opaque");
  opaque_layer_.render(
      main_view, render_view, prepass_fb, combined_fb, extent, rt_buffer_opaque_layer, true);
  DRW_stats_group_end();

  DRW_stats_group_start("Deferred.Refract");
  refraction_layer_.render(
      main_view, render_view, prepass_fb, combined_fb, extent, rt_buffer_refract_layer, false);
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

  const BoundBox bbox = *BKE_object_boundbox_get(ob);
  int3 min = int3(INT32_MAX);
  int3 max = int3(INT32_MIN);

  for (float3 l_corner : bbox.vec) {
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
  {
    gbuffer_ps_.init();
    gbuffer_ps_.clear_stencil(0x00u);
    gbuffer_ps_.state_stencil(0xFFu, 0xFFu, 0xFFu);

    {
      /* Common resources. */

      /* G-buffer. */
      gbuffer_ps_.bind_image(GBUF_CLOSURE_SLOT, &inst_.gbuffer.closure_tx);
      gbuffer_ps_.bind_image(GBUF_COLOR_SLOT, &inst_.gbuffer.color_tx);
      gbuffer_ps_.bind_image(GBUF_HEADER_SLOT, &inst_.gbuffer.header_tx);
      /* RenderPasses & AOVs. */
      gbuffer_ps_.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
      gbuffer_ps_.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
      /* Cryptomatte. */
      gbuffer_ps_.bind_image(RBUFS_CRYPTOMATTE_SLOT, &inst_.render_buffers.cryptomatte_tx);
      /* Storage Buffer. */
      /* Textures. */
      gbuffer_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);

      inst_.bind_uniform_data(&gbuffer_ps_);
      inst_.sampling.bind_resources(gbuffer_ps_);
      inst_.hiz_buffer.bind_resources(gbuffer_ps_);
      inst_.cryptomatte.bind_resources(gbuffer_ps_);
    }

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_WRITE_STENCIL |
                     DRW_STATE_STENCIL_ALWAYS;

    gbuffer_double_sided_ps_ = &gbuffer_ps_.sub("DoubleSided");
    gbuffer_double_sided_ps_->state_set(state);

    gbuffer_single_sided_ps_ = &gbuffer_ps_.sub("SingleSided");
    gbuffer_single_sided_ps_->state_set(state | DRW_STATE_CULL_BACK);
  }
}

void DeferredProbeLayer::end_sync()
{
  if (closure_bits_ & (CLOSURE_DIFFUSE | CLOSURE_REFLECTION)) {
    PassSimple &pass = eval_light_ps_;
    pass.init();
    /* Use stencil test to reject pixel not written by this layer. */
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_NEQUAL);
    pass.state_stencil(0x00u, 0x00u, (CLOSURE_DIFFUSE | CLOSURE_REFLECTION));
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

  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                            gbuffer_single_sided_ps_ :
                            gbuffer_double_sided_ps_;
  pass = &pass->sub(GPU_material_get_name(gpumat));
  pass->state_stencil(closure_bits, 0xFFu, 0xFFu);
  return pass;
}

void DeferredProbeLayer::render(View &view,
                                Framebuffer &prepass_fb,
                                Framebuffer &combined_fb,
                                int2 extent)
{
  GPU_framebuffer_bind(prepass_fb);
  inst_.manager->submit(prepass_ps_, view);

  inst_.hiz_buffer.set_source(&inst_.render_buffers.depth_tx);
  inst_.hiz_buffer.set_dirty();
  inst_.lights.set_view(view, extent);
  inst_.shadows.set_view(view);
  inst_.irradiance_cache.set_view(view);

  inst_.gbuffer.acquire(extent, closure_bits_);

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(gbuffer_ps_, view);

  inst_.manager->submit(eval_light_ps_, view);

  inst_.gbuffer.release();
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
                                   int2 extent)
{
  GPU_debug_group_begin("Probe.Render");
  opaque_layer_.render(view, prepass_fb, combined_fb, extent);
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
  {
    gbuffer_ps_.init();
    gbuffer_ps_.bind_image(GBUF_CLOSURE_SLOT, &inst_.gbuffer.closure_tx);
    gbuffer_ps_.bind_image(GBUF_COLOR_SLOT, &inst_.gbuffer.color_tx);
    gbuffer_ps_.bind_image(GBUF_HEADER_SLOT, &inst_.gbuffer.header_tx);
    gbuffer_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
    inst_.bind_uniform_data(&gbuffer_ps_);
    inst_.sampling.bind_resources(gbuffer_ps_);
    inst_.hiz_buffer.bind_resources(gbuffer_ps_);
    /* Cryptomatte. */
    gbuffer_ps_.bind_image(RBUFS_CRYPTOMATTE_SLOT, &inst_.render_buffers.cryptomatte_tx);
    /* RenderPasses & AOVs. */
    gbuffer_ps_.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
    gbuffer_ps_.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);
    inst_.cryptomatte.bind_resources(gbuffer_ps_);

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;

    gbuffer_double_sided_ps_ = &gbuffer_ps_.sub("DoubleSided");
    gbuffer_double_sided_ps_->state_set(state);

    gbuffer_single_sided_ps_ = &gbuffer_ps_.sub("SingleSided");
    gbuffer_single_sided_ps_->state_set(state | DRW_STATE_CULL_BACK);
  }
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

  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                            gbuffer_single_sided_ps_ :
                            gbuffer_double_sided_ps_;
  return &pass->sub(GPU_material_get_name(gpumat));
}

void PlanarProbePipeline::render(View &view, Framebuffer &combined_fb, int layer_id, int2 extent)
{
  GPU_debug_group_begin("Planar.Capture");

  inst_.hiz_buffer.set_source(&inst_.planar_probes.depth_tx_, layer_id);
  inst_.hiz_buffer.set_dirty();

  GPU_framebuffer_bind(combined_fb);
  GPU_framebuffer_clear_depth(combined_fb, 1.0f);
  inst_.manager->submit(prepass_ps_, view);

  inst_.lights.set_view(view, extent);
  inst_.shadows.set_view(view);
  inst_.irradiance_cache.set_view(view);

  inst_.gbuffer.acquire(extent, closure_bits_);

  inst_.hiz_buffer.update();
  GPU_framebuffer_bind(combined_fb);
  GPU_framebuffer_clear_color(combined_fb, float4(0.0f, 0.0f, 0.0f, 1.0f));
  inst_.manager->submit(gbuffer_ps_, view);
  inst_.manager->submit(eval_light_ps_, view);

  inst_.gbuffer.release();

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
