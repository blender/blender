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
  RenderBuffers &rbufs = inst_.render_buffers;

  DRWState state = DRW_STATE_WRITE_COLOR;
  world_ps_ = DRW_pass_create("World", state);

  /* Push a matrix at the same location as the camera. */
  float4x4 camera_mat = float4x4::identity();
  // copy_v3_v3(camera_mat[3], inst_.camera.data_get().viewinv[3]);

  DRWShadingGroup *grp = DRW_shgroup_material_create(gpumat, world_ps_);
  DRW_shgroup_uniform_texture(grp, "utility_tx", inst_.pipelines.utility_tx);
  DRW_shgroup_call_obmat(grp, DRW_cache_fullscreen_quad_get(), camera_mat.ptr());
  /* AOVs. */
  DRW_shgroup_uniform_image_ref(grp, "aov_color_img", &rbufs.aov_color_tx);
  DRW_shgroup_uniform_image_ref(grp, "aov_value_img", &rbufs.aov_value_tx);
  DRW_shgroup_storage_block_ref(grp, "aov_buf", &inst_.film.aovs_info);
  /* RenderPasses. Cleared by background (even if bad practice). */
  DRW_shgroup_uniform_image_ref(grp, "rp_normal_img", &rbufs.normal_tx);
  DRW_shgroup_uniform_image_ref(grp, "rp_diffuse_light_img", &rbufs.diffuse_light_tx);
  DRW_shgroup_uniform_image_ref(grp, "rp_diffuse_color_img", &rbufs.diffuse_color_tx);
  DRW_shgroup_uniform_image_ref(grp, "rp_specular_light_img", &rbufs.specular_light_tx);
  DRW_shgroup_uniform_image_ref(grp, "rp_specular_color_img", &rbufs.specular_color_tx);
  DRW_shgroup_uniform_image_ref(grp, "rp_emission_img", &rbufs.emission_tx);
  /* To allow opaque pass rendering over it. */
  DRW_shgroup_barrier(grp, GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

void WorldPipeline::render()
{
  DRW_draw_pass(world_ps_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Forward Pass
 *
 * NPR materials (using Closure to RGBA) or material using ALPHA_BLEND.
 * \{ */

void ForwardPipeline::sync()
{
  {
    DRWState state = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
    prepass_ps_ = DRW_pass_create("Forward.Opaque.Prepass", state);
    prepass_velocity_ps_ = DRW_pass_create("Forward.Opaque.Prepass.Velocity",
                                           state | DRW_STATE_WRITE_COLOR);

    state |= DRW_STATE_CULL_BACK;
    prepass_culled_ps_ = DRW_pass_create("Forward.Opaque.Prepass.Culled", state);
    prepass_culled_velocity_ps_ = DRW_pass_create("Forward.Opaque.Prepass.Velocity",
                                                  state | DRW_STATE_WRITE_COLOR);

    DRW_pass_link(prepass_ps_, prepass_velocity_ps_);
    DRW_pass_link(prepass_velocity_ps_, prepass_culled_ps_);
    DRW_pass_link(prepass_culled_ps_, prepass_culled_velocity_ps_);
  }
  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL;
    opaque_ps_ = DRW_pass_create("Forward.Opaque", state);

    state |= DRW_STATE_CULL_BACK;
    opaque_culled_ps_ = DRW_pass_create("Forward.Opaque.Culled", state);

    DRW_pass_link(opaque_ps_, opaque_culled_ps_);
  }
  {
    DRWState state = DRW_STATE_DEPTH_LESS_EQUAL;
    transparent_ps_ = DRW_pass_create("Forward.Transparent", state);
  }
}

DRWShadingGroup *ForwardPipeline::material_opaque_add(::Material *blender_mat, GPUMaterial *gpumat)
{
  RenderBuffers &rbufs = inst_.render_buffers;
  DRWPass *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ? opaque_culled_ps_ : opaque_ps_;
  // LightModule &lights = inst_.lights;
  // LightProbeModule &lightprobes = inst_.lightprobes;
  // RaytracingModule &raytracing = inst_.raytracing;
  // eGPUSamplerState no_interp = GPU_SAMPLER_DEFAULT;
  DRWShadingGroup *grp = DRW_shgroup_material_create(gpumat, pass);
  // lights.shgroup_resources(grp);
  // DRW_shgroup_uniform_block(grp, "sampling_buf", inst_.sampling.ubo_get());
  // DRW_shgroup_uniform_block(grp, "grids_buf", lightprobes.grid_ubo_get());
  // DRW_shgroup_uniform_block(grp, "cubes_buf", lightprobes.cube_ubo_get());
  // DRW_shgroup_uniform_block(grp, "probes_buf", lightprobes.info_ubo_get());
  // DRW_shgroup_uniform_texture_ref(grp, "lightprobe_grid_tx", lightprobes.grid_tx_ref_get());
  // DRW_shgroup_uniform_texture_ref(grp, "lightprobe_cube_tx", lightprobes.cube_tx_ref_get());
  DRW_shgroup_uniform_texture(grp, "utility_tx", inst_.pipelines.utility_tx);
  /* AOVs. */
  DRW_shgroup_uniform_image_ref(grp, "aov_color_img", &rbufs.aov_color_tx);
  DRW_shgroup_uniform_image_ref(grp, "aov_value_img", &rbufs.aov_value_tx);
  DRW_shgroup_storage_block_ref(grp, "aov_buf", &inst_.film.aovs_info);
  /* RenderPasses. */
  DRW_shgroup_uniform_image_ref(grp, "rp_normal_img", &rbufs.normal_tx);
  DRW_shgroup_uniform_image_ref(grp, "rp_diffuse_light_img", &rbufs.diffuse_light_tx);
  DRW_shgroup_uniform_image_ref(grp, "rp_diffuse_color_img", &rbufs.diffuse_color_tx);
  DRW_shgroup_uniform_image_ref(grp, "rp_specular_light_img", &rbufs.specular_light_tx);
  DRW_shgroup_uniform_image_ref(grp, "rp_specular_color_img", &rbufs.specular_color_tx);
  DRW_shgroup_uniform_image_ref(grp, "rp_emission_img", &rbufs.emission_tx);

  /* TODO(fclem): Make this only needed if material uses it ... somehow. */
  // if (true) {
  //   DRW_shgroup_uniform_texture_ref(
  //       grp, "sss_transmittance_tx", inst_.subsurface.transmittance_ref_get());
  // }
  // if (raytracing.enabled()) {
  // DRW_shgroup_uniform_block(grp, "rt_diffuse_buf", raytracing.diffuse_data);
  // DRW_shgroup_uniform_block(grp, "rt_reflection_buf", raytracing.reflection_data);
  // DRW_shgroup_uniform_block(grp, "rt_refraction_buf", raytracing.refraction_data);
  // DRW_shgroup_uniform_texture_ref_ex(grp, "radiance_tx", &input_screen_radiance_tx_,
  // no_interp);
  // }
  // if (raytracing.enabled()) {
  // DRW_shgroup_uniform_block(grp, "hiz_buf", inst_.hiz.ubo_get());
  // DRW_shgroup_uniform_texture_ref(grp, "hiz_tx", inst_.hiz_front.texture_ref_get());
  // }
  return grp;
}

DRWShadingGroup *ForwardPipeline::prepass_opaque_add(::Material *blender_mat,
                                                     GPUMaterial *gpumat,
                                                     bool has_motion)
{
  DRWPass *pass = (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) ?
                      (has_motion ? prepass_culled_velocity_ps_ : prepass_culled_ps_) :
                      (has_motion ? prepass_velocity_ps_ : prepass_ps_);
  DRWShadingGroup *grp = DRW_shgroup_material_create(gpumat, pass);
  if (has_motion) {
    inst_.velocity.bind_resources(grp);
  }
  return grp;
}

DRWShadingGroup *ForwardPipeline::material_transparent_add(::Material *blender_mat,
                                                           GPUMaterial *gpumat)
{
  // LightModule &lights = inst_.lights;
  // LightProbeModule &lightprobes = inst_.lightprobes;
  // RaytracingModule &raytracing = inst_.raytracing;
  // eGPUSamplerState no_interp = GPU_SAMPLER_DEFAULT;
  DRWShadingGroup *grp = DRW_shgroup_material_create(gpumat, transparent_ps_);
  // lights.shgroup_resources(grp);
  // DRW_shgroup_uniform_block(grp, "sampling_buf", inst_.sampling.ubo_get());
  // DRW_shgroup_uniform_block(grp, "grids_buf", lightprobes.grid_ubo_get());
  // DRW_shgroup_uniform_block(grp, "cubes_buf", lightprobes.cube_ubo_get());
  // DRW_shgroup_uniform_block(grp, "probes_buf", lightprobes.info_ubo_get());
  // DRW_shgroup_uniform_texture_ref(grp, "lightprobe_grid_tx", lightprobes.grid_tx_ref_get());
  // DRW_shgroup_uniform_texture_ref(grp, "lightprobe_cube_tx", lightprobes.cube_tx_ref_get());
  // DRW_shgroup_uniform_texture(grp, "utility_tx", inst_.pipelines.utility_tx);
  /* TODO(fclem): Make this only needed if material uses it ... somehow. */
  // if (true) {
  // DRW_shgroup_uniform_texture_ref(
  //     grp, "sss_transmittance_tx", inst_.subsurface.transmittance_ref_get());
  // }
  // if (raytracing.enabled()) {
  // DRW_shgroup_uniform_block(grp, "rt_diffuse_buf", raytracing.diffuse_data);
  // DRW_shgroup_uniform_block(grp, "rt_reflection_buf", raytracing.reflection_data);
  // DRW_shgroup_uniform_block(grp, "rt_refraction_buf", raytracing.refraction_data);
  // DRW_shgroup_uniform_texture_ref_ex(
  //     grp, "rt_radiance_tx", &input_screen_radiance_tx_, no_interp);
  // }
  // if (raytracing.enabled()) {
  // DRW_shgroup_uniform_block(grp, "hiz_buf", inst_.hiz.ubo_get());
  // DRW_shgroup_uniform_texture_ref(grp, "hiz_tx", inst_.hiz_front.texture_ref_get());
  // }

  DRWState state_disable = DRW_STATE_WRITE_DEPTH;
  DRWState state_enable = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM;
  if (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) {
    state_enable |= DRW_STATE_CULL_BACK;
  }
  DRW_shgroup_state_disable(grp, state_disable);
  DRW_shgroup_state_enable(grp, state_enable);
  return grp;
}

DRWShadingGroup *ForwardPipeline::prepass_transparent_add(::Material *blender_mat,
                                                          GPUMaterial *gpumat)
{
  if ((blender_mat->blend_flag & MA_BL_HIDE_BACKFACE) == 0) {
    return nullptr;
  }

  DRWShadingGroup *grp = DRW_shgroup_material_create(gpumat, transparent_ps_);

  DRWState state_disable = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM;
  DRWState state_enable = DRW_STATE_WRITE_DEPTH;
  if (blender_mat->blend_flag & MA_BL_CULL_BACKFACE) {
    state_enable |= DRW_STATE_CULL_BACK;
  }
  DRW_shgroup_state_disable(grp, state_disable);
  DRW_shgroup_state_enable(grp, state_enable);
  return grp;
}

void ForwardPipeline::render(const DRWView *view,
                             Framebuffer &prepass_fb,
                             Framebuffer &combined_fb,
                             GPUTexture *depth_tx,
                             GPUTexture *UNUSED(combined_tx))
{
  UNUSED_VARS(view, depth_tx, prepass_fb, combined_fb);
  // HiZBuffer &hiz = inst_.hiz_front;

  DRW_stats_group_start("ForwardOpaque");

  GPU_framebuffer_bind(prepass_fb);
  DRW_draw_pass(prepass_ps_);

  // hiz.set_dirty();

  // if (inst_.raytracing.enabled()) {
  //   rt_buffer.radiance_copy(combined_tx);
  //   hiz.update(depth_tx);
  // }

  // inst_.shadows.set_view(view, depth_tx);

  GPU_framebuffer_bind(combined_fb);
  DRW_draw_pass(opaque_ps_);

  DRW_stats_group_end();

  DRW_stats_group_start("ForwardTransparent");
  /* TODO(fclem) This is suboptimal. We could sort during sync. */
  /* FIXME(fclem) This wont work for panoramic, where we need
   * to sort by distance to camera, not by z. */
  DRW_pass_sort_shgroup_z(transparent_ps_);
  DRW_draw_pass(transparent_ps_);
  DRW_stats_group_end();

  // if (inst_.raytracing.enabled()) {
  //   gbuffer.ray_radiance_tx.release();
  // }
}

/** \} */

}  // namespace blender::eevee
