/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Depth of field post process effect.
 *
 * There are 2 methods to achieve this effect.
 * - The first uses projection matrix offsetting and sample accumulation to give
 * reference quality depth of field. But this needs many samples to hide the
 * under-sampling.
 * - The second one is a post-processing based one. It follows the
 * implementation described in the presentation
 * "Life of a Bokeh - SIGGRAPH 2018" from Guillaume Abadie.
 * There are some difference with our actual implementation that prioritize quality.
 */

#include "DRW_render.hh"

#include "BKE_camera.h"
#include "DNA_camera_types.h"

#include "GPU_platform.hh"
#include "GPU_texture.hh"

#include "GPU_debug.hh"

#include "eevee_camera.hh"
#include "eevee_instance.hh"
#include "eevee_sampling.hh"
#include "eevee_shader.hh"
#include "eevee_velocity_shared.hh"

#include "eevee_depth_of_field.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Depth of field
 * \{ */

void DepthOfField::init()
{
  const SceneEEVEE &sce_eevee = inst_.scene->eevee;
  const Object *camera_object_eval = inst_.camera_eval_object;
  const ::Camera *camera = (camera_object_eval && camera_object_eval->type == OB_CAMERA) ?
                               reinterpret_cast<const ::Camera *>(camera_object_eval->data) :
                               nullptr;

  enabled_ = camera && (camera->dof.flag & CAM_DOF_ENABLED) != 0;

  if (enabled_ == false) {
    /* Set to invalid value for update detection */
    data_.scatter_color_threshold = -1.0f;
    return;
  }
  /* Reminder: These are parameters not interpolated by motion blur. */
  int sce_flag = sce_eevee.flag;
  do_jitter_ = (sce_flag & SCE_EEVEE_DOF_JITTER) != 0;
  user_overblur_ = sce_eevee.bokeh_overblur / 100.0f;
  fx_max_coc_ = sce_eevee.bokeh_max_size;
  data_.scatter_color_threshold = sce_eevee.bokeh_threshold;
  data_.scatter_neighbor_max_color = sce_eevee.bokeh_neighbor_max;
  data_.bokeh_blades = float(camera->dof.aperture_blades);
}

void DepthOfField::sync()
{
  const Camera &camera = inst_.camera;
  const Object *camera_object_eval = inst_.camera_eval_object;
  const ::Camera *camera_data = (camera_object_eval && camera_object_eval->type == OB_CAMERA) ?
                                    reinterpret_cast<const ::Camera *>(camera_object_eval->data) :
                                    nullptr;

  if (inst_.debug_mode == DEBUG_DOF_PLANES) {
    /* Set debug message even if DOF is not enabled. */
    inst_.info_append(
        "Debug Mode: Depth Of Field Buffers\n"
        " - Purple: Gap Fill\n"
        " - Blue: Background\n"
        " - Red: Slight Out Of Focus\n"
        " - Yellow: In Focus\n"
        " - Green: Foreground\n");
  }

  if (enabled_ == false) {
    jitter_radius_ = 0.0f;
    fx_radius_ = 0.0f;
    return;
  }

  float2 anisotropic_scale = {clamp_f(1.0f / camera_data->dof.aperture_ratio, 1e-5f, 1.0f),
                              clamp_f(camera_data->dof.aperture_ratio, 1e-5f, 1.0f)};
  data_.bokeh_anisotropic_scale = anisotropic_scale;
  data_.bokeh_rotation = camera_data->dof.aperture_rotation;
  focus_distance_ = BKE_camera_object_dof_distance(camera_object_eval);
  data_.bokeh_anisotropic_scale_inv = 1.0f / data_.bokeh_anisotropic_scale;

  float fstop = max_ff(camera_data->dof.aperture_fstop, 1e-5f);

  float aperture = 1.0f / (2.0f * fstop);
  if (camera.is_perspective()) {
    aperture *= camera_data->lens * 1e-3f;
  }

  if (camera.is_orthographic()) {
    /* FIXME: Why is this needed? Some kind of implicit unit conversion? */
    aperture *= 0.04f;
  }

  if (camera.is_panoramic()) {
    /* FIXME: Eyeballed. */
    aperture *= 0.185f;
  }

  if (camera_data->dof.aperture_ratio < 1.0) {
    /* If ratio is scaling the bokeh outwards, we scale the aperture so that
     * the gather kernel size will encompass the maximum axis. */
    aperture /= max_ff(camera_data->dof.aperture_ratio, 1e-5f);
  }

  float jitter_radius, fx_radius;

  /* Balance blur radius between fx dof and jitter dof. */
  if (do_jitter_ && (inst_.sampling.dof_ring_count_get() > 0) && !camera.is_panoramic() &&
      !inst_.is_viewport())
  {
    /* Compute a minimal overblur radius to fill the gaps between the samples.
     * This is just the simplified form of dividing the area of the bokeh by
     * the number of samples. */
    float minimal_overblur = 1.0f / sqrtf(inst_.sampling.dof_sample_count_get());

    fx_radius = (minimal_overblur + user_overblur_) * aperture;
    /* Avoid dilating the shape. Over-blur only soften. */
    jitter_radius = max_ff(0.0f, aperture - fx_radius);
  }
  else {
    jitter_radius = 0.0f;
    fx_radius = aperture;
  }

  /* Disable post fx if result wouldn't be noticeable. */
  if (fx_max_coc_ <= 0.5f) {
    fx_radius = 0.0f;
  }

  jitter_radius_ = jitter_radius;
  fx_radius_ = fx_radius;

  if (fx_radius_ == 0.0f) {
    return;
  }

  /* TODO(fclem): Once we render into multiple view, we will need to use the maximum resolution. */
  int2 max_render_res = inst_.film.render_extent_get();
  int2 half_res = math::divide_ceil(max_render_res, int2(2));
  int2 reduce_size = math::ceil_to_multiple(half_res, int2(DOF_REDUCE_GROUP_SIZE));

  data_.gather_uv_fac = 1.0f / float2(reduce_size);

  /* Now that we know the maximum render resolution of every view, using depth of field, allocate
   * the reduced buffers. Color needs to be signed format here. See note in shader for
   * explanation. Do not use texture pool because of needs mipmaps. */
  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                           GPU_TEXTURE_USAGE_SHADER_WRITE;
  reduced_color_tx_.ensure_2d(
      gpu::TextureFormat::SFLOAT_16_16_16_16, reduce_size, usage, nullptr, DOF_MIP_COUNT);
  reduced_coc_tx_.ensure_2d(
      gpu::TextureFormat::SFLOAT_16, reduce_size, usage, nullptr, DOF_MIP_COUNT);
  reduced_color_tx_.ensure_mip_views();
  reduced_coc_tx_.ensure_mip_views();

  /* Resize the scatter list to contain enough entry to cover half the screen with sprites (which
   * is unlikely due to local contrast test). */
  data_.scatter_max_rect = (reduced_color_tx_.pixel_count() / 4) / 2;
  scatter_fg_list_buf_.resize(data_.scatter_max_rect);
  scatter_bg_list_buf_.resize(data_.scatter_max_rect);

  bokeh_lut_pass_sync();
  setup_pass_sync();
  stabilize_pass_sync();
  downsample_pass_sync();
  reduce_pass_sync();
  tiles_flatten_pass_sync();
  tiles_dilate_pass_sync();
  gather_pass_sync();
  filter_pass_sync();
  scatter_pass_sync();
  hole_fill_pass_sync();
  resolve_pass_sync();
}

void DepthOfField::jitter_apply(float4x4 &winmat, float4x4 &viewmat)
{
  if (jitter_radius_ == 0.0f) {
    return;
  }

  float radius, theta;
  inst_.sampling.dof_disk_sample_get(&radius, &theta);

  if (data_.bokeh_blades >= 3.0f) {
    theta = circle_to_polygon_angle(data_.bokeh_blades, theta);
    radius *= circle_to_polygon_radius(data_.bokeh_blades, theta);
  }
  radius *= jitter_radius_;
  theta += data_.bokeh_rotation;

  /* Sample in View Space. */
  float2 sample = float2(radius * cosf(theta), radius * sinf(theta));
  sample *= data_.bokeh_anisotropic_scale;
  /* Convert to NDC Space. */
  float3 jitter = float3(UNPACK2(sample), -focus_distance_);
  float3 center = float3(0.0f, 0.0f, -focus_distance_);
  mul_project_m4_v3(winmat.ptr(), jitter);
  mul_project_m4_v3(winmat.ptr(), center);

  const bool is_ortho = (winmat[2][3] != -1.0f);
  if (is_ortho) {
    sample *= focus_distance_;
  }
  /* Translate origin. */
  sub_v2_v2(viewmat[3], sample);
  /* Skew winmat Z axis. */
  add_v2_v2(winmat[2], center - jitter);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Passes setup.
 * \{ */

void DepthOfField::bokeh_lut_pass_sync()
{
  const bool has_anisotropy = data_.bokeh_anisotropic_scale != float2(1.0f);
  if (!has_anisotropy && (data_.bokeh_blades == 0.0)) {
    /* No need for LUTs in these cases. */
    use_bokeh_lut_ = false;
    return;
  }
  use_bokeh_lut_ = true;

  /* Precompute bokeh texture. */
  bokeh_lut_ps_.init();
  bokeh_lut_ps_.shader_set(inst_.shaders.static_shader_get(DOF_BOKEH_LUT));
  bokeh_lut_ps_.bind_ubo("dof_buf", data_);
  bokeh_lut_ps_.bind_image("out_gather_lut_img", &bokeh_gather_lut_tx_);
  bokeh_lut_ps_.bind_image("out_scatter_lut_img", &bokeh_scatter_lut_tx_);
  bokeh_lut_ps_.bind_image("out_resolve_lut_img", &bokeh_resolve_lut_tx_);
  bokeh_lut_ps_.dispatch(int3(1, 1, 1));
}

void DepthOfField::setup_pass_sync()
{
  RenderBuffers &render_buffers = inst_.render_buffers;

  setup_ps_.init();
  setup_ps_.shader_set(inst_.shaders.static_shader_get(DOF_SETUP));
  setup_ps_.bind_texture("color_tx", &input_color_tx_, no_filter);
  setup_ps_.bind_texture("depth_tx", &render_buffers.depth_tx, no_filter);
  setup_ps_.bind_ubo("dof_buf", data_);
  setup_ps_.bind_image("out_color_img", &setup_color_tx_);
  setup_ps_.bind_image("out_coc_img", &setup_coc_tx_);
  setup_ps_.dispatch(&dispatch_setup_size_);
  setup_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
}

void DepthOfField::stabilize_pass_sync()
{
  RenderBuffers &render_buffers = inst_.render_buffers;
  VelocityModule &velocity = inst_.velocity;

  stabilize_ps_.init();
  stabilize_ps_.shader_set(inst_.shaders.static_shader_get(DOF_STABILIZE));
  stabilize_ps_.bind_ubo("camera_prev", &(*velocity.camera_steps[STEP_PREVIOUS]));
  stabilize_ps_.bind_ubo("camera_curr", &(*velocity.camera_steps[STEP_CURRENT]));
  /* This is only for temporal stability. The next step is not needed. */
  stabilize_ps_.bind_ubo("camera_next", &(*velocity.camera_steps[STEP_PREVIOUS]));
  stabilize_ps_.bind_texture("coc_tx", &setup_coc_tx_, no_filter);
  stabilize_ps_.bind_texture("color_tx", &setup_color_tx_, no_filter);
  stabilize_ps_.bind_texture("velocity_tx", &render_buffers.vector_tx, no_filter);
  stabilize_ps_.bind_texture("in_history_tx", &stabilize_input_, with_filter);
  stabilize_ps_.bind_texture("depth_tx", &render_buffers.depth_tx, no_filter);
  stabilize_ps_.bind_ubo("dof_buf", data_);
  stabilize_ps_.push_constant("u_use_history", &stabilize_valid_history_, 1);
  stabilize_ps_.bind_image("out_coc_img", reduced_coc_tx_.mip_view(0));
  stabilize_ps_.bind_image("out_color_img", reduced_color_tx_.mip_view(0));
  stabilize_ps_.bind_image("out_history_img", &stabilize_output_tx_);
  stabilize_ps_.dispatch(&dispatch_stabilize_size_);
  stabilize_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

void DepthOfField::downsample_pass_sync()
{
  downsample_ps_.init();
  downsample_ps_.shader_set(inst_.shaders.static_shader_get(DOF_DOWNSAMPLE));
  downsample_ps_.bind_texture("color_tx", reduced_color_tx_.mip_view(0), no_filter);
  downsample_ps_.bind_texture("coc_tx", reduced_coc_tx_.mip_view(0), no_filter);
  downsample_ps_.bind_image("out_color_img", &downsample_tx_);
  downsample_ps_.dispatch(&dispatch_downsample_size_);
  downsample_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
}

void DepthOfField::reduce_pass_sync()
{
  reduce_ps_.init();
  reduce_ps_.shader_set(inst_.shaders.static_shader_get(DOF_REDUCE));
  reduce_ps_.bind_ubo("dof_buf", data_);
  reduce_ps_.bind_texture("downsample_tx", &downsample_tx_, no_filter);
  reduce_ps_.bind_ssbo("scatter_fg_list_buf", scatter_fg_list_buf_);
  reduce_ps_.bind_ssbo("scatter_bg_list_buf", scatter_bg_list_buf_);
  reduce_ps_.bind_ssbo("scatter_fg_indirect_buf", scatter_fg_indirect_buf_);
  reduce_ps_.bind_ssbo("scatter_bg_indirect_buf", scatter_bg_indirect_buf_);
  reduce_ps_.bind_image("inout_color_lod0_img", reduced_color_tx_.mip_view(0));
  reduce_ps_.bind_image("out_color_lod1_img", reduced_color_tx_.mip_view(1));
  reduce_ps_.bind_image("out_color_lod2_img", reduced_color_tx_.mip_view(2));
  reduce_ps_.bind_image("out_color_lod3_img", reduced_color_tx_.mip_view(3));
  reduce_ps_.bind_image("in_coc_lod0_img", reduced_coc_tx_.mip_view(0));
  reduce_ps_.bind_image("out_coc_lod1_img", reduced_coc_tx_.mip_view(1));
  reduce_ps_.bind_image("out_coc_lod2_img", reduced_coc_tx_.mip_view(2));
  reduce_ps_.bind_image("out_coc_lod3_img", reduced_coc_tx_.mip_view(3));
  reduce_ps_.dispatch(&dispatch_reduce_size_);
  /* NOTE: Command buffer barrier is done automatically by the GPU backend. */
  reduce_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH | GPU_BARRIER_SHADER_STORAGE);
}

void DepthOfField::tiles_flatten_pass_sync()
{
  tiles_flatten_ps_.init();
  tiles_flatten_ps_.shader_set(inst_.shaders.static_shader_get(DOF_TILES_FLATTEN));
  /* NOTE(fclem): We should use the reduced_coc_tx_ as it is stable, but we need the slight focus
   * flag from the setup pass. A better way would be to do the brute-force in focus gather without
   * this. */
  tiles_flatten_ps_.bind_texture("coc_tx", &setup_coc_tx_, no_filter);
  tiles_flatten_ps_.bind_image("out_tiles_fg_img", &tiles_fg_tx_.current());
  tiles_flatten_ps_.bind_image("out_tiles_bg_img", &tiles_bg_tx_.current());
  tiles_flatten_ps_.dispatch(&dispatch_tiles_flatten_size_);
  tiles_flatten_ps_.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
}

void DepthOfField::tiles_dilate_pass_sync()
{
  for (int pass = 0; pass < 2; pass++) {
    PassSimple &drw_pass = (pass == 0) ? tiles_dilate_minmax_ps_ : tiles_dilate_minabs_ps_;
    eShaderType sh_type = (pass == 0) ? DOF_TILES_DILATE_MINMAX : DOF_TILES_DILATE_MINABS;
    drw_pass.init();
    drw_pass.shader_set(inst_.shaders.static_shader_get(sh_type));
    drw_pass.bind_image("in_tiles_fg_img", &tiles_fg_tx_.previous());
    drw_pass.bind_image("in_tiles_bg_img", &tiles_bg_tx_.previous());
    drw_pass.bind_image("out_tiles_fg_img", &tiles_fg_tx_.current());
    drw_pass.bind_image("out_tiles_bg_img", &tiles_bg_tx_.current());
    drw_pass.push_constant("ring_count", &tiles_dilate_ring_count_, 1);
    drw_pass.push_constant("ring_width_multiplier", &tiles_dilate_ring_width_mul_, 1);
    drw_pass.dispatch(&dispatch_tiles_dilate_size_);
    drw_pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }
}

void DepthOfField::gather_pass_sync()
{
  const GPUSamplerState gather_bilinear = {GPU_SAMPLER_FILTERING_MIPMAP |
                                           GPU_SAMPLER_FILTERING_LINEAR};
  const GPUSamplerState gather_nearest = {GPU_SAMPLER_FILTERING_MIPMAP};

  for (int pass = 0; pass < 2; pass++) {
    PassSimple &drw_pass = (pass == 0) ? gather_fg_ps_ : gather_bg_ps_;
    SwapChain<TextureFromPool, 2> &color_chain = (pass == 0) ? color_fg_tx_ : color_bg_tx_;
    SwapChain<TextureFromPool, 2> &weight_chain = (pass == 0) ? weight_fg_tx_ : weight_bg_tx_;
    eShaderType sh_type = (pass == 0) ?
                              (use_bokeh_lut_ ? DOF_GATHER_FOREGROUND_LUT :
                                                DOF_GATHER_FOREGROUND) :
                              (use_bokeh_lut_ ? DOF_GATHER_BACKGROUND_LUT : DOF_GATHER_BACKGROUND);
    drw_pass.init();
    drw_pass.bind_resources(inst_.sampling);
    drw_pass.shader_set(inst_.shaders.static_shader_get(sh_type));
    drw_pass.bind_ubo("dof_buf", data_);
    drw_pass.bind_texture("color_bilinear_tx", reduced_color_tx_, gather_bilinear);
    drw_pass.bind_texture("color_tx", reduced_color_tx_, gather_nearest);
    drw_pass.bind_texture("coc_tx", reduced_coc_tx_, gather_nearest);
    drw_pass.bind_image("in_tiles_fg_img", &tiles_fg_tx_.current());
    drw_pass.bind_image("in_tiles_bg_img", &tiles_bg_tx_.current());
    drw_pass.bind_image("out_color_img", &color_chain.current());
    drw_pass.bind_image("out_weight_img", &weight_chain.current());
    drw_pass.bind_image("out_occlusion_img", &occlusion_tx_);
    drw_pass.bind_texture("bokeh_lut_tx", &bokeh_gather_lut_tx_);
    drw_pass.dispatch(&dispatch_gather_size_);
    drw_pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
  }
}

void DepthOfField::filter_pass_sync()
{
  for (int pass = 0; pass < 2; pass++) {
    PassSimple &drw_pass = (pass == 0) ? filter_fg_ps_ : filter_bg_ps_;
    SwapChain<TextureFromPool, 2> &color_chain = (pass == 0) ? color_fg_tx_ : color_bg_tx_;
    SwapChain<TextureFromPool, 2> &weight_chain = (pass == 0) ? weight_fg_tx_ : weight_bg_tx_;
    drw_pass.init();
    drw_pass.shader_set(inst_.shaders.static_shader_get(DOF_FILTER));
    drw_pass.bind_texture("color_tx", &color_chain.previous());
    drw_pass.bind_texture("weight_tx", &weight_chain.previous());
    drw_pass.bind_image("out_color_img", &color_chain.current());
    drw_pass.bind_image("out_weight_img", &weight_chain.current());
    drw_pass.dispatch(&dispatch_filter_size_);
    drw_pass.barrier(GPU_BARRIER_TEXTURE_FETCH);
  }
}

void DepthOfField::scatter_pass_sync()
{
  for (int pass = 0; pass < 2; pass++) {
    PassSimple &drw_pass = (pass == 0) ? scatter_fg_ps_ : scatter_bg_ps_;
    drw_pass.init();
    drw_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
    drw_pass.shader_set(inst_.shaders.static_shader_get(DOF_SCATTER));
    drw_pass.bind_ubo("dof_buf", data_);
    drw_pass.push_constant("use_bokeh_lut", use_bokeh_lut_);
    drw_pass.bind_texture("bokeh_lut_tx", &bokeh_scatter_lut_tx_);
    drw_pass.bind_texture("occlusion_tx", &occlusion_tx_);
    if (pass == 0) {
      drw_pass.bind_ssbo("scatter_list_buf", scatter_fg_list_buf_);
      drw_pass.draw_procedural_indirect(GPU_PRIM_TRI_STRIP, scatter_fg_indirect_buf_);
      /* Avoid background gather pass writing to the occlusion_tx mid pass. */
      drw_pass.barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
    }
    else {
      drw_pass.bind_ssbo("scatter_list_buf", scatter_bg_list_buf_);
      drw_pass.draw_procedural_indirect(GPU_PRIM_TRI_STRIP, scatter_bg_indirect_buf_);
    }
  }
}

void DepthOfField::hole_fill_pass_sync()
{
  const GPUSamplerState gather_bilinear = {GPU_SAMPLER_FILTERING_MIPMAP |
                                           GPU_SAMPLER_FILTERING_LINEAR};
  const GPUSamplerState gather_nearest = {GPU_SAMPLER_FILTERING_MIPMAP};

  hole_fill_ps_.init();
  hole_fill_ps_.bind_resources(inst_.sampling);
  hole_fill_ps_.shader_set(inst_.shaders.static_shader_get(DOF_GATHER_HOLE_FILL));
  hole_fill_ps_.bind_ubo("dof_buf", data_);
  hole_fill_ps_.bind_texture("color_bilinear_tx", reduced_color_tx_, gather_bilinear);
  hole_fill_ps_.bind_texture("color_tx", reduced_color_tx_, gather_nearest);
  hole_fill_ps_.bind_texture("coc_tx", reduced_coc_tx_, gather_nearest);
  hole_fill_ps_.bind_image("in_tiles_fg_img", &tiles_fg_tx_.current());
  hole_fill_ps_.bind_image("in_tiles_bg_img", &tiles_bg_tx_.current());
  hole_fill_ps_.bind_image("out_color_img", &hole_fill_color_tx_);
  hole_fill_ps_.bind_image("out_weight_img", &hole_fill_weight_tx_);
  hole_fill_ps_.dispatch(&dispatch_gather_size_);
  hole_fill_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
}

void DepthOfField::resolve_pass_sync()
{
  GPUSamplerState with_filter = {GPU_SAMPLER_FILTERING_LINEAR};
  RenderBuffers &render_buffers = inst_.render_buffers;
  gpu::Shader *sh = inst_.shaders.static_shader_get(use_bokeh_lut_ ? DOF_RESOLVE_LUT :
                                                                     DOF_RESOLVE);

  resolve_ps_.init();
  resolve_ps_.specialize_constant(sh, "do_debug_color", inst_.debug_mode == DEBUG_DOF_PLANES);
  resolve_ps_.shader_set(sh);
  resolve_ps_.bind_ubo("dof_buf", data_);
  resolve_ps_.bind_texture("depth_tx", &render_buffers.depth_tx, no_filter);
  resolve_ps_.bind_texture("color_tx", &input_color_tx_, no_filter);
  resolve_ps_.bind_texture("stable_color_tx", &resolve_stable_color_tx_, no_filter);
  resolve_ps_.bind_texture("color_bg_tx", &color_bg_tx_.current(), with_filter);
  resolve_ps_.bind_texture("color_fg_tx", &color_fg_tx_.current(), with_filter);
  resolve_ps_.bind_image("in_tiles_fg_img", &tiles_fg_tx_.current());
  resolve_ps_.bind_image("in_tiles_bg_img", &tiles_bg_tx_.current());
  resolve_ps_.bind_texture("weight_bg_tx", &weight_bg_tx_.current());
  resolve_ps_.bind_texture("weight_fg_tx", &weight_fg_tx_.current());
  resolve_ps_.bind_texture("color_hole_fill_tx", &hole_fill_color_tx_);
  resolve_ps_.bind_texture("weight_hole_fill_tx", &hole_fill_weight_tx_);
  resolve_ps_.bind_texture("bokeh_lut_tx", &bokeh_resolve_lut_tx_);
  resolve_ps_.bind_image("out_color_img", &output_color_tx_);
  resolve_ps_.bind_resources(inst_.sampling);
  resolve_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
  resolve_ps_.dispatch(&dispatch_resolve_size_);
  resolve_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Post-FX Rendering.
 * \{ */

void DepthOfField::update_sample_table()
{
  float2 subpixel_offset = inst_.film.pixel_jitter_get();
  /* Since the film jitter is in full-screen res, divide by 2 to get the jitter in half res. */
  subpixel_offset *= 0.5;

  /* Same offsets as in dof_spatial_filtering(). */
  const std::array<int2, 4> plus_offsets = {int2(-1, 0), int2(0, -1), int2(1, 0), int2(0, 1)};

  const float radius = 1.5f;
  int i = 0;
  for (int2 offset : plus_offsets) {
    float2 pixel_ofs = float2(offset) - subpixel_offset;
    data_.filter_samples_weight[i++] = film_filter_weight(radius, math::length_squared(pixel_ofs));
  }
  data_.filter_center_weight = film_filter_weight(radius, math::length_squared(subpixel_offset));
}

void DepthOfField::render(View &view,
                          gpu::Texture **input_tx,
                          gpu::Texture **output_tx,
                          DepthOfFieldBuffer &dof_buffer)
{
  if (fx_radius_ == 0.0f) {
    return;
  }

  input_color_tx_ = *input_tx;
  output_color_tx_ = *output_tx;
  extent_ = {GPU_texture_width(input_color_tx_), GPU_texture_height(input_color_tx_)};

  {
    const CameraData &cam_data = inst_.camera.data_get();
    data_.camera_type = cam_data.type;
    /* OPTI(fclem) Could be optimized. */
    float3 jitter = float3(fx_radius_, 0.0f, -focus_distance_);
    float3 center = float3(0.0f, 0.0f, -focus_distance_);
    mul_project_m4_v3(cam_data.winmat.ptr(), jitter);
    mul_project_m4_v3(cam_data.winmat.ptr(), center);
    /* Simplify CoC calculation to a simple MADD. */
    if (inst_.camera.is_orthographic()) {
      data_.coc_mul = (center[0] - jitter[0]) * 0.5f * extent_[0];
      data_.coc_bias = focus_distance_ * data_.coc_mul;
    }
    else {
      data_.coc_bias = -(center[0] - jitter[0]) * 0.5f * extent_[0];
      data_.coc_mul = focus_distance_ * data_.coc_bias;
    }

    float min_fg_coc = coc_radius_from_camera_depth(data_, -cam_data.clip_near);
    float max_bg_coc = coc_radius_from_camera_depth(data_, -cam_data.clip_far);
    if (data_.camera_type != CAMERA_ORTHO) {
      /* Background is at infinity so maximum CoC is the limit of coc_radius_from_camera_depth
       * at -inf. We only do this for perspective camera since orthographic coc limit is inf. */
      max_bg_coc = data_.coc_bias;
    }
    /* Clamp with user defined max. */
    data_.coc_abs_max = min_ff(max_ff(fabsf(min_fg_coc), fabsf(max_bg_coc)), fx_max_coc_);
    /* TODO(fclem): Make this dependent of the quality of the gather pass. */
    data_.scatter_coc_threshold = 4.0f;

    update_sample_table();

    data_.push_update();
  }

  int2 half_res = math::divide_ceil(extent_, int2(2));
  int2 quarter_res = math::divide_ceil(extent_, int2(4));
  int2 tile_res = math::divide_ceil(half_res, int2(DOF_TILES_SIZE));

  dispatch_setup_size_ = int3(math::divide_ceil(half_res, int2(DOF_DEFAULT_GROUP_SIZE)), 1);
  dispatch_stabilize_size_ = int3(math::divide_ceil(half_res, int2(DOF_STABILIZE_GROUP_SIZE)), 1);
  dispatch_downsample_size_ = int3(math::divide_ceil(quarter_res, int2(DOF_DEFAULT_GROUP_SIZE)),
                                   1);
  dispatch_reduce_size_ = int3(math::divide_ceil(half_res, int2(DOF_REDUCE_GROUP_SIZE)), 1);
  dispatch_tiles_flatten_size_ = int3(math::divide_ceil(half_res, int2(DOF_TILES_SIZE)), 1);
  dispatch_tiles_dilate_size_ = int3(
      math::divide_ceil(tile_res, int2(DOF_TILES_DILATE_GROUP_SIZE)), 1);
  dispatch_gather_size_ = int3(math::divide_ceil(half_res, int2(DOF_GATHER_GROUP_SIZE)), 1);
  dispatch_filter_size_ = int3(math::divide_ceil(half_res, int2(DOF_FILTER_GROUP_SIZE)), 1);
  dispatch_resolve_size_ = int3(math::divide_ceil(extent_, int2(DOF_RESOLVE_GROUP_SIZE)), 1);

  if (GPU_type_matches_ex(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
    /* On Mesa, there is a sync bug which can make a portion of the main pass (usually one shader)
     * leave blocks of un-initialized memory. Doing a flush seems to alleviate the issue. */
    GPU_flush();
  }

  GPU_debug_group_begin("Depth of Field");

  Manager &drw = *inst_.manager;

  constexpr eGPUTextureUsage usage_readwrite = GPU_TEXTURE_USAGE_SHADER_READ |
                                               GPU_TEXTURE_USAGE_SHADER_WRITE;
  constexpr eGPUTextureUsage usage_readwrite_attach = usage_readwrite |
                                                      GPU_TEXTURE_USAGE_ATTACHMENT;
  {
    GPU_debug_group_begin("Setup");
    {
      bokeh_gather_lut_tx_.acquire(int2(DOF_BOKEH_LUT_SIZE), gpu::TextureFormat::SFLOAT_16_16);
      bokeh_scatter_lut_tx_.acquire(int2(DOF_BOKEH_LUT_SIZE), gpu::TextureFormat::SFLOAT_16);
      bokeh_resolve_lut_tx_.acquire(int2(DOF_MAX_SLIGHT_FOCUS_RADIUS * 2 + 1),
                                    gpu::TextureFormat::SFLOAT_16);

      if (use_bokeh_lut_) {
        drw.submit(bokeh_lut_ps_, view);
      }
    }
    {
      setup_color_tx_.acquire(half_res, gpu::TextureFormat::SFLOAT_16_16_16_16, usage_readwrite);
      setup_coc_tx_.acquire(half_res, gpu::TextureFormat::SFLOAT_16);

      drw.submit(setup_ps_, view);
    }
    {
      stabilize_output_tx_.acquire(half_res, gpu::TextureFormat::SFLOAT_16_16_16_16);
      stabilize_valid_history_ = !dof_buffer.stabilize_history_tx_.ensure_2d(
          gpu::TextureFormat::SFLOAT_16_16_16_16, half_res);

      if (stabilize_valid_history_ == false) {
        /* Avoid uninitialized memory that can contain NaNs. */
        dof_buffer.stabilize_history_tx_.clear(float4(0.0f));
      }

      stabilize_input_ = dof_buffer.stabilize_history_tx_;
      /* Outputs to reduced_*_tx_ mip 0. */
      drw.submit(stabilize_ps_, view);

      /* WATCH(fclem): Swap Texture an TextureFromPool internal gpu::Texture in order to
       * reuse the one that we just consumed. */
      TextureFromPool::swap(stabilize_output_tx_, dof_buffer.stabilize_history_tx_);

      /* Used by stabilize pass. */
      stabilize_output_tx_.release();
      setup_color_tx_.release();
    }
    {
      GPU_debug_group_begin("Tile Prepare");

      /* WARNING: If format changes, make sure dof_tile_* GLSL constants are properly encoded. */
      tiles_fg_tx_.previous().acquire(
          tile_res, gpu::TextureFormat::UFLOAT_11_11_10, usage_readwrite);
      tiles_bg_tx_.previous().acquire(
          tile_res, gpu::TextureFormat::UFLOAT_11_11_10, usage_readwrite);
      tiles_fg_tx_.current().acquire(
          tile_res, gpu::TextureFormat::UFLOAT_11_11_10, usage_readwrite);
      tiles_bg_tx_.current().acquire(
          tile_res, gpu::TextureFormat::UFLOAT_11_11_10, usage_readwrite);

      drw.submit(tiles_flatten_ps_, view);

      /* Used by tile_flatten and stabilize_ps pass. */
      setup_coc_tx_.release();

      /* Error introduced by gather center jittering. */
      const float error_multiplier = 1.0f + 1.0f / (DOF_GATHER_RING_COUNT + 0.5f);
      int dilation_end_radius = ceilf((fx_max_coc_ * error_multiplier) / (DOF_TILES_SIZE * 2));

      /* Run dilation twice. One for minmax and one for minabs. */
      for (int pass = 0; pass < 2; pass++) {
        /* This algorithm produce the exact dilation radius by dividing it in multiple passes. */
        int dilation_radius = 0;
        while (dilation_radius < dilation_end_radius) {
          int remainder = dilation_end_radius - dilation_radius;
          /* Do not step over any unvisited tile. */
          int max_multiplier = dilation_radius + 1;

          int ring_count = min_ii(DOF_DILATE_RING_COUNT, ceilf(remainder / float(max_multiplier)));
          int multiplier = min_ii(max_multiplier, floorf(remainder / float(ring_count)));

          dilation_radius += ring_count * multiplier;

          tiles_dilate_ring_count_ = ring_count;
          tiles_dilate_ring_width_mul_ = multiplier;

          tiles_fg_tx_.swap();
          tiles_bg_tx_.swap();

          drw.submit((pass == 0) ? tiles_dilate_minmax_ps_ : tiles_dilate_minabs_ps_, view);
        }
      }

      tiles_fg_tx_.previous().release();
      tiles_bg_tx_.previous().release();

      GPU_debug_group_end();
    }

    downsample_tx_.acquire(quarter_res, gpu::TextureFormat::SFLOAT_16_16_16_16, usage_readwrite);

    drw.submit(downsample_ps_, view);

    scatter_fg_indirect_buf_.clear_to_zero();
    scatter_bg_indirect_buf_.clear_to_zero();

    drw.submit(reduce_ps_, view);

    /* Used by reduce pass. */
    downsample_tx_.release();

    GPU_debug_group_end();
  }

  for (int is_background = 0; is_background < 2; is_background++) {
    GPU_debug_group_begin(is_background ? "Background Convolution" : "Foreground Convolution");

    SwapChain<TextureFromPool, 2> &color_tx = is_background ? color_bg_tx_ : color_fg_tx_;
    SwapChain<TextureFromPool, 2> &weight_tx = is_background ? weight_bg_tx_ : weight_fg_tx_;
    Framebuffer &scatter_fb = is_background ? scatter_bg_fb_ : scatter_fg_fb_;
    PassSimple &gather_ps = is_background ? gather_bg_ps_ : gather_fg_ps_;
    PassSimple &filter_ps = is_background ? filter_bg_ps_ : filter_fg_ps_;
    PassSimple &scatter_ps = is_background ? scatter_bg_ps_ : scatter_fg_ps_;

    color_tx.current().acquire(
        half_res, gpu::TextureFormat::SFLOAT_16_16_16_16, usage_readwrite_attach);
    weight_tx.current().acquire(half_res, gpu::TextureFormat::SFLOAT_16, usage_readwrite);
    occlusion_tx_.acquire(half_res, gpu::TextureFormat::SFLOAT_16_16);

    drw.submit(gather_ps, view);

    {
      /* Filtering pass. */
      color_tx.swap();
      weight_tx.swap();

      color_tx.current().acquire(
          half_res, gpu::TextureFormat::SFLOAT_16_16_16_16, usage_readwrite_attach);
      weight_tx.current().acquire(half_res, gpu::TextureFormat::SFLOAT_16, usage_readwrite);

      drw.submit(filter_ps, view);

      color_tx.previous().release();
      weight_tx.previous().release();
    }

    GPU_memory_barrier(GPU_BARRIER_FRAMEBUFFER);

    scatter_fb.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(color_tx.current()));

    if (GPU_type_matches_ex(
            GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OPENSOURCE, GPU_BACKEND_OPENGL))
    {
      /* WORKAROUND(fclem): Mesa has some synchronization issues between the previous compute
       * shader and the following graphic pass (see #141198). */
      GPU_flush();
    }

    GPU_framebuffer_bind(scatter_fb);
    drw.submit(scatter_ps, view);

    /* Used by scatter pass. */
    occlusion_tx_.release();

    GPU_debug_group_end();
  }
  {
    GPU_debug_group_begin("Hole Fill");

    bokeh_gather_lut_tx_.release();
    bokeh_scatter_lut_tx_.release();

    hole_fill_color_tx_.acquire(half_res, gpu::TextureFormat::SFLOAT_16_16_16_16, usage_readwrite);
    hole_fill_weight_tx_.acquire(half_res, gpu::TextureFormat::SFLOAT_16, usage_readwrite);

    drw.submit(hole_fill_ps_, view);

    /* NOTE: We do not filter the hole-fill pass as effect is likely to not be noticeable. */

    GPU_debug_group_end();
  }
  {
    GPU_debug_group_begin("Resolve");

    resolve_stable_color_tx_ = dof_buffer.stabilize_history_tx_;

    drw.submit(resolve_ps_, view);

    color_bg_tx_.current().release();
    color_fg_tx_.current().release();
    weight_bg_tx_.current().release();
    weight_fg_tx_.current().release();
    tiles_fg_tx_.current().release();
    tiles_bg_tx_.current().release();
    hole_fill_color_tx_.release();
    hole_fill_weight_tx_.release();
    bokeh_resolve_lut_tx_.release();

    GPU_debug_group_end();
  }

  GPU_debug_group_end();

  /* Swap buffers so that next effect has the right input. */
  std::swap(*input_tx, *output_tx);
}

/** \} */

}  // namespace blender::eevee
