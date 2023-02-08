/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * Shading passes contain drawcalls specific to shading pipelines.
 * They are to be shared across views.
 * This file is only for shading passes. Other passes are declared in their own module.
 */

#include "eevee_instance.hh"

#include "eevee_pipeline.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name World Pipeline
 *
 * Used to draw background.
 * \{ */

void WorldPipeline::sync(GPUMaterial *gpumat)
{
  Manager &manager = *inst_.manager;
  RenderBuffers &rbufs = inst_.render_buffers;

  ResourceHandle handle = manager.resource_handle(float4x4::identity());

  world_ps_.init();
  world_ps_.state_set(DRW_STATE_WRITE_COLOR);
  world_ps_.material_set(manager, gpumat);
  world_ps_.push_constant("world_opacity_fade", inst_.film.background_opacity_get());
  world_ps_.bind_texture("utility_tx", inst_.pipelines.utility_tx);
  /* AOVs. */
  world_ps_.bind_image("aov_color_img", &rbufs.aov_color_tx);
  world_ps_.bind_image("aov_value_img", &rbufs.aov_value_tx);
  world_ps_.bind_ssbo("aov_buf", &inst_.film.aovs_info);
  /* RenderPasses. Cleared by background (even if bad practice). */
  world_ps_.bind_image("rp_normal_img", &rbufs.normal_tx);
  world_ps_.bind_image("rp_light_img", &rbufs.light_tx);
  world_ps_.bind_image("rp_diffuse_color_img", &rbufs.diffuse_color_tx);
  world_ps_.bind_image("rp_specular_color_img", &rbufs.specular_color_tx);
  world_ps_.bind_image("rp_emission_img", &rbufs.emission_tx);
  world_ps_.bind_image("rp_cryptomatte_img", &rbufs.cryptomatte_tx);

  world_ps_.bind_ubo(CAMERA_BUF_SLOT, inst_.camera.ubo_get());

  world_ps_.draw(DRW_cache_fullscreen_quad_get(), handle);
  /* To allow opaque pass rendering over it. */
  world_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

void WorldPipeline::render(View &view)
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
  /* TODO(fclem): Add state for rendering to empty framebuffer without depth test.
   * For now this is only here for avoiding the rasterizer discard state. */
  surface_ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
  surface_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  surface_ps_.bind_texture(SHADOW_RENDER_MAP_SLOT, &inst_.shadows.render_map_tx_);
  surface_ps_.bind_image(SHADOW_ATLAS_SLOT, &inst_.shadows.atlas_tx_);
  surface_ps_.bind_ubo(CAMERA_BUF_SLOT, inst_.camera.ubo_get());
  surface_ps_.bind_ssbo(SHADOW_PAGE_INFO_SLOT, &inst_.shadows.pages_infos_data_);
  inst_.sampling.bind_resources(&surface_ps_);

  surface_ps_.framebuffer_set(&inst_.shadows.render_fb_);
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
      /* Uniform Buf. */
      prepass_ps_.bind_ubo(CAMERA_BUF_SLOT, inst_.camera.ubo_get());

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

      /* RenderPasses. */
      opaque_ps_.bind_image(RBUFS_NORMAL_SLOT, &inst_.render_buffers.normal_tx);
      opaque_ps_.bind_image(RBUFS_LIGHT_SLOT, &inst_.render_buffers.light_tx);
      opaque_ps_.bind_image(RBUFS_DIFF_COLOR_SLOT, &inst_.render_buffers.diffuse_color_tx);
      opaque_ps_.bind_image(RBUFS_SPEC_COLOR_SLOT, &inst_.render_buffers.specular_color_tx);
      opaque_ps_.bind_image(RBUFS_EMISSION_SLOT, &inst_.render_buffers.emission_tx);
      /* AOVs. */
      opaque_ps_.bind_image(RBUFS_AOV_COLOR_SLOT, &inst_.render_buffers.aov_color_tx);
      opaque_ps_.bind_image(RBUFS_AOV_VALUE_SLOT, &inst_.render_buffers.aov_value_tx);
      /* Cryptomatte. */
      opaque_ps_.bind_image(RBUFS_CRYPTOMATTE_SLOT, &inst_.render_buffers.cryptomatte_tx);
      /* Storage Buf. */
      opaque_ps_.bind_ssbo(RBUFS_AOV_BUF_SLOT, &inst_.film.aovs_info);
      /* Textures. */
      opaque_ps_.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
      /* Uniform Buf. */
      opaque_ps_.bind_ubo(CAMERA_BUF_SLOT, inst_.camera.ubo_get());

      inst_.lights.bind_resources(&opaque_ps_);
      inst_.shadows.bind_resources(&opaque_ps_);
      inst_.sampling.bind_resources(&opaque_ps_);
      inst_.cryptomatte.bind_resources(&opaque_ps_);
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
    /* Uniform Buf. */
    sub.bind_ubo(CAMERA_BUF_SLOT, inst_.camera.ubo_get());

    inst_.lights.bind_resources(&sub);
    inst_.shadows.bind_resources(&sub);
    inst_.sampling.bind_resources(&sub);
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
  UNUSED_VARS(view);

  DRW_stats_group_start("Forward.Opaque");

  GPU_framebuffer_bind(prepass_fb);
  inst_.manager->submit(prepass_ps_, view);

  // if (!DRW_pass_is_empty(prepass_ps_)) {
  inst_.hiz_buffer.set_dirty();
  // }

  // if (inst_.raytracing.enabled()) {
  //   rt_buffer.radiance_copy(combined_tx);
  //   inst_.hiz_buffer.update();
  // }

  inst_.shadows.set_view(view);

  GPU_framebuffer_bind(combined_fb);
  inst_.manager->submit(opaque_ps_, view);

  DRW_stats_group_end();

  inst_.manager->submit(transparent_ps_, view);

  // if (inst_.raytracing.enabled()) {
  //   gbuffer.ray_radiance_tx.release();
  // }
}

/** \} */

}  // namespace blender::eevee
