/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */
#include "DNA_camera_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_view3d_types.h"

#include "BLI_link_utils.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "DRW_render.hh"

#include "BKE_camera.h"

#include "gpencil_engine_private.hh"

namespace blender::draw::gpencil {

/* verify if this fx is active */
static bool effect_is_active(ShaderFxData *fx, bool is_edit, bool is_viewport)
{
  if (fx == nullptr) {
    return false;
  }

  if (((fx->mode & eShaderFxMode_Editmode) == 0) && (is_edit) && (is_viewport)) {
    return false;
  }

  if (((fx->mode & eShaderFxMode_Realtime) && (is_viewport == true)) ||
      ((fx->mode & eShaderFxMode_Render) && (is_viewport == false)))
  {
    return true;
  }

  return false;
}

PassSimple &Instance::vfx_pass_create(
    const char *name, DRWState state, GPUShader *sh, tObject *tgp_ob, GPUSamplerState sampler)
{
  UNUSED_VARS(name);

  int64_t id = gp_vfx_pool->append_and_get_index({});
  tVfx &tgp_vfx = (*gp_vfx_pool)[id];
  tgp_vfx.target_fb = vfx_swapchain_.next().fb;

  PassSimple &pass = *tgp_vfx.vfx_ps;
  pass.init();
  pass.state_set(state);
  pass.shader_set(sh);
  pass.bind_texture("color_buf", vfx_swapchain_.current().color_tx, sampler);
  pass.bind_texture("reveal_buf", vfx_swapchain_.current().reveal_tx, sampler);

  vfx_swapchain_.swap();

  BLI_LINKS_APPEND(&tgp_ob->vfx, &tgp_vfx);

  return pass;
}

void Instance::vfx_blur_sync(BlurShaderFxData *fx, Object *ob, tObject *tgp_ob)
{
  if ((fx->samples == 0.0f) || (fx->radius[0] == 0.0f && fx->radius[1] == 0.0f)) {
    return;
  }

  if ((fx->flag & FX_BLUR_DOF_MODE) && this->camera == nullptr) {
    /* No blur outside camera view (or when DOF is disabled on the camera). */
    return;
  }

  const float s = sin(fx->rotation);
  const float c = cos(fx->rotation);

  float4x4 winmat, persmat;
  float blur_size[2] = {fx->radius[0], fx->radius[1]};
  persmat = View::default_get().persmat();
  const float w = fabsf(mul_project_m4_v3_zfac(persmat.ptr(), ob->object_to_world().location()));

  if (fx->flag & FX_BLUR_DOF_MODE) {
    /* Compute circle of confusion size. */
    float coc = (this->dof_params[0] / -w) - this->dof_params[1];
    copy_v2_fl(blur_size, fabsf(coc));
  }
  else {
    /* Modify by distance to camera and object scale. */
    winmat = View::default_get().winmat();
    const float2 vp_size = this->draw_ctx->viewport_size_get();
    float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
    float scale = mat4_to_scale(ob->object_to_world().ptr());
    float distance_factor = world_pixel_scale * scale * winmat[1][1] * vp_size[1] / w;
    mul_v2_fl(blur_size, distance_factor);
  }

  GPUShader *sh = ShaderCache::get().fx_blur.get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  if (blur_size[0] > 0.0f) {
    auto &grp = vfx_pass_create("Fx Blur H", state, sh, tgp_ob);
    grp.push_constant("offset", float2(blur_size[0] * c, blur_size[0] * s));
    grp.push_constant("samp_count", max_ii(1, min_ii(fx->samples, blur_size[0])));
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
  if (blur_size[1] > 0.0f) {
    auto &grp = vfx_pass_create("Fx Blur V", state, sh, tgp_ob);
    grp.push_constant("offset", float2(-blur_size[1] * s, blur_size[1] * c));
    grp.push_constant("samp_count", max_ii(1, min_ii(fx->samples, blur_size[1])));
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void Instance::vfx_colorize_sync(ColorizeShaderFxData *fx, Object * /*ob*/, tObject *tgp_ob)
{
  GPUShader *sh = ShaderCache::get().fx_colorize.get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  auto &grp = vfx_pass_create("Fx Colorize", state, sh, tgp_ob);
  grp.push_constant("low_color", float3(fx->low_color));
  grp.push_constant("high_color", float3(fx->high_color));
  grp.push_constant("factor", fx->factor);
  grp.push_constant("mode", fx->mode);
  grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void Instance::vfx_flip_sync(FlipShaderFxData *fx, Object * /*ob*/, tObject *tgp_ob)
{
  float axis_flip[2];
  axis_flip[0] = (fx->flag & FX_FLIP_HORIZONTAL) ? -1.0f : 1.0f;
  axis_flip[1] = (fx->flag & FX_FLIP_VERTICAL) ? -1.0f : 1.0f;

  GPUShader *sh = ShaderCache::get().fx_transform.get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  auto &grp = vfx_pass_create("Fx Flip", state, sh, tgp_ob);
  grp.push_constant("axis_flip", float2(axis_flip));
  grp.push_constant("wave_offset", float2(0.0f, 0.0f));
  grp.push_constant("swirl_radius", 0.0f);
  grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void Instance::vfx_rim_sync(RimShaderFxData *fx, Object *ob, tObject *tgp_ob)
{
  float4x4 winmat, persmat;
  float offset[2] = {float(fx->offset[0]), float(fx->offset[1])};
  float blur_size[2] = {float(fx->blur[0]), float(fx->blur[1])};
  winmat = View::default_get().winmat();
  persmat = View::default_get().persmat();
  const float2 vp_size = this->draw_ctx->viewport_size_get();
  const float2 vp_size_inv = 1.0f / vp_size;

  const float w = fabsf(mul_project_m4_v3_zfac(persmat.ptr(), ob->object_to_world().location()));

  /* Modify by distance to camera and object scale. */
  float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
  float scale = mat4_to_scale(ob->object_to_world().ptr());
  float distance_factor = (world_pixel_scale * scale * winmat[1][1] * vp_size[1]) / w;
  mul_v2_fl(offset, distance_factor);
  mul_v2_v2(offset, vp_size_inv);
  mul_v2_fl(blur_size, distance_factor);

  GPUShader *sh = ShaderCache::get().fx_rim.get();

  {
    DRWState state = DRW_STATE_WRITE_COLOR;
    auto &grp = vfx_pass_create("Fx Rim H", state, sh, tgp_ob);
    grp.push_constant("blur_dir", float2(blur_size[0] * vp_size_inv[0], 0.0f));
    grp.push_constant("uv_offset", float2(offset));
    grp.push_constant("samp_count", max_ii(1, min_ii(fx->samples, blur_size[0])));
    grp.push_constant("mask_color", float3(fx->mask_rgb));
    grp.push_constant("is_first_pass", true);
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  {
    DRWState state = DRW_STATE_WRITE_COLOR;
    switch (fx->mode) {
      case eShaderFxRimMode_Normal:
        state |= DRW_STATE_BLEND_ALPHA_PREMUL;
        break;
      case eShaderFxRimMode_Add:
        state |= DRW_STATE_BLEND_ADD_FULL;
        break;
      case eShaderFxRimMode_Subtract:
        state |= DRW_STATE_BLEND_SUB;
        break;
      case eShaderFxRimMode_Multiply:
      case eShaderFxRimMode_Divide:
      case eShaderFxRimMode_Overlay:
        state |= DRW_STATE_BLEND_MUL;
        break;
    }

    zero_v2(offset);

    auto &grp = vfx_pass_create("Fx Rim V", state, sh, tgp_ob);
    grp.push_constant("blur_dir", float2(0.0f, blur_size[1] * vp_size_inv[1]));
    grp.push_constant("uv_offset", float2(offset));
    grp.push_constant("rim_color", float3(fx->rim_rgb));
    grp.push_constant("samp_count", max_ii(1, min_ii(fx->samples, blur_size[1])));
    grp.push_constant("blend_mode", fx->mode);
    grp.push_constant("is_first_pass", false);
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);

    if (fx->mode == eShaderFxRimMode_Overlay) {
      /* We cannot do custom blending on multi-target frame-buffers.
       * Workaround by doing 2 passes. */
      grp.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
      grp.push_constant("blend_mode", 999);
      grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
  }
}

void Instance::vfx_pixelize_sync(PixelShaderFxData *fx, Object *ob, tObject *tgp_ob)
{
  float4x4 persmat, winmat;
  float ob_center[3], pixsize_uniform[2];
  winmat = View::default_get().winmat();
  persmat = View::default_get().persmat();
  const float2 vp_size = this->draw_ctx->viewport_size_get();
  const float2 vp_size_inv = 1.0f / vp_size;
  float pixel_size[2] = {float(fx->size[0]), float(fx->size[1])};
  mul_v2_v2(pixel_size, vp_size_inv);

  /* Fixed pixelisation center from object center. */
  const float w = fabsf(mul_project_m4_v3_zfac(persmat.ptr(), ob->object_to_world().location()));
  mul_v3_m4v3(ob_center, persmat.ptr(), ob->object_to_world().location());
  mul_v3_fl(ob_center, 1.0f / w);

  const bool use_antialiasing = ((fx->flag & FX_PIXEL_FILTER_NEAREST) == 0);

  /* Convert to uvs. */
  mul_v2_fl(ob_center, 0.5f);
  add_v2_fl(ob_center, 0.5f);

  /* Modify by distance to camera and object scale. */
  float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
  float scale = mat4_to_scale(ob->object_to_world().ptr());
  mul_v2_fl(pixel_size, (world_pixel_scale * scale * winmat[1][1] * vp_size[1]) / w);

  /* Center to texel */
  madd_v2_v2fl(ob_center, pixel_size, -0.5f);

  GPUShader *sh = ShaderCache::get().fx_pixelize.get();

  DRWState state = DRW_STATE_WRITE_COLOR;

  /* Only if pixelated effect is bigger than 1px. */
  if (pixel_size[0] > vp_size_inv[0]) {
    copy_v2_fl2(pixsize_uniform, pixel_size[0], vp_size_inv[1]);
    GPUSamplerState sampler = (use_antialiasing) ? GPUSamplerState::internal_sampler() :
                                                   GPUSamplerState::default_sampler();

    auto &grp = vfx_pass_create("Fx Pixelize X", state, sh, tgp_ob, sampler);
    grp.push_constant("target_pixel_size", float2(pixsize_uniform));
    grp.push_constant("target_pixel_offset", float2(ob_center));
    grp.push_constant("accum_offset", float2(pixel_size[0], 0.0f));
    int samp_count = (pixel_size[0] / vp_size_inv[0] > 3.0) ? 2 : 1;
    grp.push_constant("samp_count", (use_antialiasing ? samp_count : 0));
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  if (pixel_size[1] > vp_size_inv[1]) {
    GPUSamplerState sampler = (use_antialiasing) ? GPUSamplerState::internal_sampler() :
                                                   GPUSamplerState::default_sampler();
    copy_v2_fl2(pixsize_uniform, vp_size_inv[0], pixel_size[1]);
    auto &grp = vfx_pass_create("Fx Pixelize Y", state, sh, tgp_ob, sampler);
    grp.push_constant("target_pixel_size", float2(pixsize_uniform));
    grp.push_constant("accum_offset", float2(0.0f, pixel_size[1]));
    int samp_count = (pixel_size[1] / vp_size_inv[1] > 3.0) ? 2 : 1;
    grp.push_constant("samp_count", (use_antialiasing ? samp_count : 0));
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void Instance::vfx_shadow_sync(ShadowShaderFxData *fx, Object *ob, tObject *tgp_ob)
{
  const bool use_obj_pivot = (fx->flag & FX_SHADOW_USE_OBJECT) != 0;
  const bool use_wave = (fx->flag & FX_SHADOW_USE_WAVE) != 0;

  float4x4 uv_mat, winmat, persmat;
  float rot_center[3];
  float wave_ofs[3], wave_dir[3], wave_phase, blur_dir[2], tmp[2];
  float offset[2] = {float(fx->offset[0]), float(fx->offset[1])};
  float blur_size[2] = {float(fx->blur[0]), float(fx->blur[1])};
  winmat = View::default_get().winmat();
  persmat = View::default_get().persmat();
  const float2 vp_size = this->draw_ctx->viewport_size_get();
  const float2 vp_size_inv = 1.0f / vp_size;
  const float ratio = vp_size_inv[1] / vp_size_inv[0];

  copy_v3_v3(rot_center,
             (use_obj_pivot && fx->object) ? fx->object->object_to_world().location() :
                                             ob->object_to_world().location());

  const float w = fabsf(mul_project_m4_v3_zfac(persmat.ptr(), rot_center));
  mul_v3_m4v3(rot_center, persmat.ptr(), rot_center);
  mul_v3_fl(rot_center, 1.0f / w);

  /* Modify by distance to camera and object scale. */
  float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
  float scale = mat4_to_scale(ob->object_to_world().ptr());
  float distance_factor = (world_pixel_scale * scale * winmat[1][1] * vp_size[1]) / w;
  mul_v2_fl(offset, distance_factor);
  mul_v2_v2(offset, vp_size_inv);
  mul_v2_fl(blur_size, distance_factor);

  rot_center[0] = rot_center[0] * 0.5f + 0.5f;
  rot_center[1] = rot_center[1] * 0.5f + 0.5f;

  /* UV transform matrix. (loc, rot, scale) Sent to shader as 2x3 matrix. */
  unit_m4(uv_mat.ptr());
  translate_m4(uv_mat.ptr(), rot_center[0], rot_center[1], 0.0f);
  rescale_m4(uv_mat.ptr(), float3{1.0f / fx->scale[0], 1.0f / fx->scale[1], 1.0f});
  translate_m4(uv_mat.ptr(), -offset[0], -offset[1], 0.0f);
  rescale_m4(uv_mat.ptr(), float3{1.0f / ratio, 1.0f, 1.0f});
  rotate_m4(uv_mat.ptr(), 'Z', fx->rotation);
  rescale_m4(uv_mat.ptr(), float3{ratio, 1.0f, 1.0f});
  translate_m4(uv_mat.ptr(), -rot_center[0], -rot_center[1], 0.0f);

  if (use_wave) {
    float dir[2];
    if (fx->orientation == 0) {
      /* Horizontal */
      copy_v2_fl2(dir, 1.0f, 0.0f);
    }
    else {
      /* Vertical */
      copy_v2_fl2(dir, 0.0f, 1.0f);
    }
    /* This is applied after rotation. Counter the rotation to keep aligned with global axis. */
    rotate_v2_v2fl(wave_dir, dir, fx->rotation);
    /* Rotate 90 degrees. */
    copy_v2_v2(wave_ofs, wave_dir);
    std::swap(wave_ofs[0], wave_ofs[1]);
    wave_ofs[1] *= -1.0f;
    /* Keep world space scaling and aspect ratio. */
    mul_v2_fl(wave_dir, 1.0f / (max_ff(1e-8f, fx->period) * distance_factor));
    mul_v2_v2(wave_dir, vp_size);
    mul_v2_fl(wave_ofs, fx->amplitude * distance_factor);
    mul_v2_v2(wave_ofs, vp_size_inv);
    /* Phase start at shadow center. */
    wave_phase = fx->phase - dot_v2v2(rot_center, wave_dir);
  }
  else {
    zero_v2(wave_dir);
    zero_v2(wave_ofs);
    wave_phase = 0.0f;
  }

  GPUShader *sh = ShaderCache::get().fx_shadow.get();

  copy_v2_fl2(blur_dir, blur_size[0] * vp_size_inv[0], 0.0f);

  {
    DRWState state = DRW_STATE_WRITE_COLOR;
    auto &grp = vfx_pass_create("Fx Shadow H", state, sh, tgp_ob);
    grp.push_constant("blur_dir", float2(blur_dir));
    grp.push_constant("wave_dir", float2(wave_dir));
    grp.push_constant("wave_offset", float2(wave_ofs));
    grp.push_constant("wave_phase", wave_phase);
    grp.push_constant("uv_rot_x", float2(uv_mat[0]));
    grp.push_constant("uv_rot_y", float2(uv_mat[1]));
    grp.push_constant("uv_offset", float2(uv_mat[3]));
    grp.push_constant("samp_count", max_ii(1, min_ii(fx->samples, blur_size[0])));
    grp.push_constant("is_first_pass", true);
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  unit_m4(uv_mat.ptr());
  zero_v2(wave_ofs);

  /* Reset the `uv_mat` to account for rotation in the Y-axis (Shadow-V parameter). */
  copy_v2_fl2(tmp, 0.0f, blur_size[1]);
  rotate_v2_v2fl(blur_dir, tmp, -fx->rotation);
  mul_v2_v2(blur_dir, vp_size_inv);

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL;
    auto &grp = vfx_pass_create("Fx Shadow V", state, sh, tgp_ob);
    grp.push_constant("shadow_color", float4(fx->shadow_rgba));
    grp.push_constant("blur_dir", float2(blur_dir));
    grp.push_constant("wave_offset", float2(wave_ofs));
    grp.push_constant("uv_rot_x", float2(uv_mat[0]));
    grp.push_constant("uv_rot_y", float2(uv_mat[1]));
    grp.push_constant("uv_offset", float2(uv_mat[3]));
    grp.push_constant("samp_count", max_ii(1, min_ii(fx->samples, blur_size[1])));
    grp.push_constant("is_first_pass", false);
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void Instance::vfx_glow_sync(GlowShaderFxData *fx, Object * /*ob*/, tObject *tgp_ob)
{
  const bool use_glow_under = (fx->flag & FX_GLOW_USE_ALPHA) != 0;
  const float s = sin(fx->rotation);
  const float c = cos(fx->rotation);

  GPUShader *sh = ShaderCache::get().fx_glow.get();

  float ref_col[4];

  if (fx->mode == eShaderFxGlowMode_Luminance) {
    /* Only pass in the first value for luminance. */
    ref_col[0] = fx->threshold;
    ref_col[1] = -1.0f;
    ref_col[2] = -1.0f;
    ref_col[3] = -1.0f;
  }
  else {
    /* First three values are the RGB for the selected color, last value the threshold. */
    copy_v3_v3(ref_col, fx->select_color);
    ref_col[3] = fx->threshold;
  }

  DRWState state = DRW_STATE_WRITE_COLOR;
  auto &grp = vfx_pass_create("Fx Glow H", state, sh, tgp_ob);
  grp.push_constant("offset", float2(fx->blur[0] * c, fx->blur[0] * s));
  grp.push_constant("samp_count", max_ii(1, min_ii(fx->samples, fx->blur[0])));
  grp.push_constant("threshold", float4(ref_col));
  grp.push_constant("glow_color", float4(fx->glow_color));
  grp.push_constant("glow_under", use_glow_under);
  grp.push_constant("first_pass", true);
  grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  state = DRW_STATE_WRITE_COLOR;
  /* Blending: Force blending. */
  switch (fx->blend_mode) {
    case eGplBlendMode_Regular:
      state |= DRW_STATE_BLEND_ALPHA_PREMUL;
      break;
    case eGplBlendMode_Add:
      state |= DRW_STATE_BLEND_ADD_FULL;
      break;
    case eGplBlendMode_Subtract:
      state |= DRW_STATE_BLEND_SUB;
      break;
    case eGplBlendMode_Multiply:
    case eGplBlendMode_Divide:
      state |= DRW_STATE_BLEND_MUL;
      break;
  }

  /* Small Hack: We ask for RGBA16F buffer if using use_glow_under to store original
   * revealage in alpha channel. */
  if (fx->blend_mode == eGplBlendMode_Subtract || use_glow_under) {
    /* For this effect to propagate, we need a signed floating point buffer. */
    this->use_signed_fb = true;
  }

  {
    auto &grp = vfx_pass_create("Fx Glow V", state, sh, tgp_ob);
    grp.push_constant("offset", float2(-fx->blur[1] * s, fx->blur[1] * c));
    grp.push_constant("samp_count", max_ii(1, min_ii(fx->samples, fx->blur[0])));
    grp.push_constant("threshold", float4{-1.0f, -1.0f, -1.0f, -1.0});
    grp.push_constant("glow_color", float4{1.0f, 1.0f, 1.0f, fx->glow_color[3]});
    grp.push_constant("first_pass", false);
    grp.push_constant("blend_mode", fx->blend_mode);
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void Instance::vfx_wave_sync(WaveShaderFxData *fx, Object *ob, tObject *tgp_ob)
{
  float4x4 winmat, persmat;
  float wave_center[3];
  float wave_ofs[3], wave_dir[3], wave_phase;
  winmat = View::default_get().winmat();
  persmat = View::default_get().persmat();
  const float2 vp_size = this->draw_ctx->viewport_size_get();
  const float2 vp_size_inv = 1.0f / vp_size;

  const float w = fabsf(mul_project_m4_v3_zfac(persmat.ptr(), ob->object_to_world().location()));
  mul_v3_m4v3(wave_center, persmat.ptr(), ob->object_to_world().location());
  mul_v3_fl(wave_center, 1.0f / w);

  /* Modify by distance to camera and object scale. */
  float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
  float scale = mat4_to_scale(ob->object_to_world().ptr());
  float distance_factor = (world_pixel_scale * scale * winmat[1][1] * vp_size[1]) / w;

  wave_center[0] = wave_center[0] * 0.5f + 0.5f;
  wave_center[1] = wave_center[1] * 0.5f + 0.5f;

  if (fx->orientation == 0) {
    /* Horizontal */
    copy_v2_fl2(wave_dir, 1.0f, 0.0f);
  }
  else {
    /* Vertical */
    copy_v2_fl2(wave_dir, 0.0f, 1.0f);
  }
  /* Rotate 90 degrees. */
  copy_v2_v2(wave_ofs, wave_dir);
  std::swap(wave_ofs[0], wave_ofs[1]);
  wave_ofs[1] *= -1.0f;
  /* Keep world space scaling and aspect ratio. */
  mul_v2_fl(wave_dir, 1.0f / (max_ff(1e-8f, fx->period) * distance_factor));
  mul_v2_v2(wave_dir, vp_size);
  mul_v2_fl(wave_ofs, fx->amplitude * distance_factor);
  mul_v2_v2(wave_ofs, vp_size_inv);
  /* Phase start at shadow center. */
  wave_phase = fx->phase - dot_v2v2(wave_center, wave_dir);

  GPUShader *sh = ShaderCache::get().fx_transform.get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  auto &grp = vfx_pass_create("Fx Wave", state, sh, tgp_ob);
  grp.push_constant("axis_flip", float2(1.0f, 1.0f));
  grp.push_constant("wave_dir", float2(wave_dir));
  grp.push_constant("wave_offset", float2(wave_ofs));
  grp.push_constant("wave_phase", wave_phase);
  grp.push_constant("swirl_radius", 0.0f);
  grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void Instance::vfx_swirl_sync(SwirlShaderFxData *fx, Object * /*ob*/, tObject *tgp_ob)
{
  if (fx->object == nullptr) {
    return;
  }

  float4x4 winmat, persmat;
  float swirl_center[3];
  winmat = View::default_get().winmat();
  persmat = View::default_get().persmat();
  const float2 vp_size = this->draw_ctx->viewport_size_get();

  copy_v3_v3(swirl_center, fx->object->object_to_world().location());

  const float w = fabsf(mul_project_m4_v3_zfac(persmat.ptr(), swirl_center));
  mul_v3_m4v3(swirl_center, persmat.ptr(), swirl_center);
  mul_v3_fl(swirl_center, 1.0f / w);

  /* Modify by distance to camera and object scale. */
  float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
  float scale = mat4_to_scale(fx->object->object_to_world().ptr());
  float distance_factor = (world_pixel_scale * scale * winmat[1][1] * vp_size[1]) / w;

  mul_v2_fl(swirl_center, 0.5f);
  add_v2_fl(swirl_center, 0.5f);
  mul_v2_v2(swirl_center, vp_size);

  float radius = fx->radius * distance_factor;
  if (radius < 1.0f) {
    return;
  }

  GPUShader *sh = ShaderCache::get().fx_transform.get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  auto &grp = vfx_pass_create("Fx Flip", state, sh, tgp_ob);
  grp.push_constant("axis_flip", float2(1.0f, 1.0f));
  grp.push_constant("wave_offset", float2(0.0f, 0.0f));
  grp.push_constant("swirl_center", float2(swirl_center));
  grp.push_constant("swirl_angle", fx->angle);
  grp.push_constant("swirl_radius", radius);
  grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void Instance::vfx_sync(Object *ob, tObject *tgp_ob)
{
  const bool is_edit_mode = ELEM(
      ob->mode, OB_MODE_EDIT, OB_MODE_SCULPT_GREASE_PENCIL, OB_MODE_WEIGHT_GREASE_PENCIL);

  vfx_swapchain_.next().fb = &layer_fb;
  vfx_swapchain_.next().color_tx = &color_layer_tx;
  vfx_swapchain_.next().reveal_tx = &reveal_layer_tx;
  vfx_swapchain_.current().fb = &object_fb;
  vfx_swapchain_.current().color_tx = &color_object_tx;
  vfx_swapchain_.current().reveal_tx = &reveal_object_tx;

  /* If simplify enabled, nothing more to do. */
  if (!this->simplify_fx) {
    LISTBASE_FOREACH (ShaderFxData *, fx, &ob->shader_fx) {
      if (effect_is_active(fx, is_edit_mode, this->is_viewport)) {
        switch (fx->type) {
          case eShaderFxType_Blur:
            vfx_blur_sync((BlurShaderFxData *)fx, ob, tgp_ob);
            break;
          case eShaderFxType_Colorize:
            vfx_colorize_sync((ColorizeShaderFxData *)fx, ob, tgp_ob);
            break;
          case eShaderFxType_Flip:
            vfx_flip_sync((FlipShaderFxData *)fx, ob, tgp_ob);
            break;
          case eShaderFxType_Pixel:
            vfx_pixelize_sync((PixelShaderFxData *)fx, ob, tgp_ob);
            break;
          case eShaderFxType_Rim:
            vfx_rim_sync((RimShaderFxData *)fx, ob, tgp_ob);
            break;
          case eShaderFxType_Shadow:
            vfx_shadow_sync((ShadowShaderFxData *)fx, ob, tgp_ob);
            break;
          case eShaderFxType_Glow:
            vfx_glow_sync((GlowShaderFxData *)fx, ob, tgp_ob);
            break;
          case eShaderFxType_Swirl:
            vfx_swirl_sync((SwirlShaderFxData *)fx, ob, tgp_ob);
            break;
          case eShaderFxType_Wave:
            vfx_wave_sync((WaveShaderFxData *)fx, ob, tgp_ob);
            break;
          default:
            break;
        }
      }
    }
  }

  if ((!this->simplify_fx && tgp_ob->vfx.first != nullptr) || tgp_ob->do_mat_holdout) {
    /* We need an extra pass to combine result to main buffer. */
    vfx_swapchain_.next().fb = &this->gpencil_fb;

    GPUShader *sh = ShaderCache::get().fx_composite.get();

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_MUL;
    auto &grp = vfx_pass_create("GPencil Object Compose", state, sh, tgp_ob);
    grp.push_constant("is_first_pass", true);
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);

    /* We cannot do custom blending on multi-target frame-buffers.
     * Workaround by doing 2 passes. */
    grp.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
    grp.push_constant("is_first_pass", false);
    grp.draw_procedural(GPU_PRIM_TRIS, 1, 3);

    this->use_object_fb = true;
    this->use_layer_fb = true;
  }
}

}  // namespace blender::draw::gpencil
