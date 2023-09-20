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
  inst_.cryptomatte.bind_resources(&world_ps_);

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
  inst_.cryptomatte.bind_resources(&pass);

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
  world_ps_.init();
  world_ps_.state_set(DRW_STATE_WRITE_COLOR);
  inst_.bind_uniform_data(&world_ps_);
  inst_.volume.bind_properties_buffers(world_ps_);
  inst_.sampling.bind_resources(&world_ps_);

  if (GPU_material_status(gpumat) != GPU_MAT_SUCCESS) {
    /* Skip if the material has not compiled yet. */
    return;
  }

  world_ps_.material_set(*inst_.manager, gpumat);
  volume_sub_pass(world_ps_, nullptr, nullptr, gpumat);

  world_ps_.dispatch(math::divide_ceil(inst_.volume.grid_size(), int3(VOLUME_GROUP_SIZE)));
  /* Sync with object property pass. */
  world_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

void WorldVolumePipeline::render(View &view)
{
  inst_.manager->submit(world_ps_, view);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadow Pipeline
 *
 * \{ */

void ShadowPipeline::sync()
{
  surface_ps_.init();
  surface_ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
  surface_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  surface_ps_.bind_image(SHADOW_ATLAS_IMG_SLOT, inst_.shadows.atlas_tx_);
  surface_ps_.bind_ssbo(SHADOW_RENDER_MAP_BUF_SLOT, &inst_.shadows.render_map_buf_);
  surface_ps_.bind_ssbo(SHADOW_VIEWPORT_INDEX_BUF_SLOT, &inst_.shadows.viewport_index_buf_);
  surface_ps_.bind_ssbo(SHADOW_PAGE_INFO_SLOT, &inst_.shadows.pages_infos_data_);
  inst_.bind_uniform_data(&surface_ps_);
  inst_.sampling.bind_resources(&surface_ps_);
}

PassMain::Sub *ShadowPipeline::surface_material_add(GPUMaterial *gpumat)
{
  return &surface_ps_.sub(GPU_material_get_name(gpumat));
}

void ShadowPipeline::render(View &view)
{
  inst_.manager->submit(surface_ps_, view);
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
      inst_.velocity.bind_resources(&prepass_ps_);
      inst_.sampling.bind_resources(&prepass_ps_);
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
      inst_.lights.bind_resources(&opaque_ps_);
      inst_.shadows.bind_resources(&opaque_ps_);
      inst_.sampling.bind_resources(&opaque_ps_);
      inst_.hiz_buffer.bind_resources(&opaque_ps_);
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
    inst_.lights.bind_resources(&sub);
    inst_.shadows.bind_resources(&sub);
    inst_.volume.bind_resources(sub);
    inst_.sampling.bind_resources(&sub);
    inst_.hiz_buffer.bind_resources(&sub);
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
  return &pass->sub(GPU_material_get_name(gpumat));
}

PassMain::Sub *ForwardPipeline::material_opaque_add(::Material *blender_mat, GPUMaterial *gpumat)
{
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

void ForwardPipeline::render(View &view,
                             Framebuffer &prepass_fb,
                             Framebuffer &combined_fb,
                             GPUTexture * /*combined_tx*/)
{
  DRW_stats_group_start("Forward.Opaque");

  prepass_fb.bind();
  inst_.manager->submit(prepass_ps_, view);

  // if (!DRW_pass_is_empty(prepass_ps_)) {
  inst_.hiz_buffer.set_dirty();
  // }

  // if (inst_.raytracing.enabled()) {
  //   rt_buffer.radiance_copy(combined_tx);
  //   inst_.hiz_buffer.update();
  // }

  inst_.shadows.set_view(view);
  inst_.irradiance_cache.set_view(view);

  combined_fb.bind();
  inst_.manager->submit(opaque_ps_, view);

  DRW_stats_group_end();

  inst_.volume.draw_resolve(view);

  inst_.manager->submit(transparent_ps_, view);

  // if (inst_.raytracing.enabled()) {
  //   gbuffer.ray_radiance_tx.release();
  // }
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
      inst_.velocity.bind_resources(&prepass_ps_);
      inst_.sampling.bind_resources(&prepass_ps_);
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
      inst_.sampling.bind_resources(&gbuffer_ps_);
      inst_.hiz_buffer.bind_resources(&gbuffer_ps_);
      inst_.cryptomatte.bind_resources(&gbuffer_ps_);
    }

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM | DRW_STATE_DEPTH_EQUAL |
                     DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_ALWAYS;

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
      inst_.gbuffer.bind_resources(&pass);
      inst_.lights.bind_resources(&pass);
      inst_.shadows.bind_resources(&pass);
      inst_.sampling.bind_resources(&pass);
      inst_.hiz_buffer.bind_resources(&pass);
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
      inst_.gbuffer.bind_resources(&pass);
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

  if (closure_bits_ & CLOSURE_SSS) {
    inst_.subsurface.render(render_view, combined_fb, direct_diffuse_tx_);
  }

  TextureFromPool dummy_diffuse_indirect;
  dummy_diffuse_indirect.acquire(int2(1), RAYTRACE_RADIANCE_FORMAT);
  dummy_diffuse_indirect.clear(float4(0));
  RayTraceResult diffuse_result = {dummy_diffuse_indirect};

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
/** \name Volume Pipeline
 *
 * \{ */

void VolumePipeline::sync()
{
  volume_ps_.init();
  volume_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  inst_.bind_uniform_data(&volume_ps_);
  inst_.volume.bind_properties_buffers(volume_ps_);
  inst_.sampling.bind_resources(&volume_ps_);
}

PassMain::Sub *VolumePipeline::volume_material_add(GPUMaterial *gpumat)
{
  return &volume_ps_.sub(GPU_material_get_name(gpumat));
}

void VolumePipeline::render(View &view)
{
  inst_.manager->submit(volume_ps_, view);
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
      inst_.velocity.bind_resources(&prepass_ps_);
      inst_.sampling.bind_resources(&prepass_ps_);
    }

    DRWState state_depth_only = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;

    prepass_double_sided_ps_ = &prepass_ps_.sub("DoubleSided");
    prepass_double_sided_ps_->state_set(state_depth_only);

    prepass_single_sided_ps_ = &prepass_ps_.sub("SingleSided");
    prepass_single_sided_ps_->state_set(state_depth_only | DRW_STATE_CULL_BACK);
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
      inst_.sampling.bind_resources(&gbuffer_ps_);
      inst_.hiz_buffer.bind_resources(&gbuffer_ps_);
      inst_.cryptomatte.bind_resources(&gbuffer_ps_);
    }

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM | DRW_STATE_DEPTH_EQUAL |
                     DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_ALWAYS;

    gbuffer_double_sided_ps_ = &gbuffer_ps_.sub("DoubleSided");
    gbuffer_double_sided_ps_->state_set(state);

    gbuffer_single_sided_ps_ = &gbuffer_ps_.sub("SingleSided");
    gbuffer_single_sided_ps_->state_set(state | DRW_STATE_CULL_BACK);
  }

  /* Light evaluate resources. */
  {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE;
    dummy_light_tx_.ensure_2d(GPU_RGBA16F, int2(1), usage);
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
    inst_.gbuffer.bind_resources(&pass);
    inst_.lights.bind_resources(&pass);
    inst_.shadows.bind_resources(&pass);
    inst_.sampling.bind_resources(&pass);
    inst_.hiz_buffer.bind_resources(&pass);
    inst_.reflection_probes.bind_resources(&pass);
    inst_.irradiance_cache.bind_resources(&pass);
    pass.barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

PassMain::Sub *DeferredProbeLayer::prepass_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  PassMain::Sub *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                            prepass_single_sided_ps_ :
                            prepass_double_sided_ps_;

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

PassMain::Sub *CapturePipeline::surface_material_add(GPUMaterial *gpumat)
{
  return &surface_ps_.sub(GPU_material_get_name(gpumat));
}

void CapturePipeline::render(View &view)
{
  inst_.manager->submit(surface_ps_, view);
}

/** \} */

}  // namespace blender::eevee
