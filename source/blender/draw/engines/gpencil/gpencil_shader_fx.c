/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2017, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */
#include "DNA_camera_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_view3d_types.h"

#include "BKE_gpencil.h"

#include "BLI_link_utils.h"
#include "BLI_memblock.h"

#include "DRW_render.h"

#include "BKE_camera.h"

#include "gpencil_engine.h"

/* verify if this fx is active */
static bool effect_is_active(bGPdata *gpd, ShaderFxData *fx, bool is_viewport)
{
  if (fx == NULL) {
    return false;
  }

  if (gpd == NULL) {
    return false;
  }

  bool is_edit = GPENCIL_ANY_EDIT_MODE(gpd);
  if (((fx->mode & eShaderFxMode_Editmode) == 0) && (is_edit) && (is_viewport)) {
    return false;
  }

  if (((fx->mode & eShaderFxMode_Realtime) && (is_viewport == true)) ||
      ((fx->mode & eShaderFxMode_Render) && (is_viewport == false))) {
    return true;
  }

  return false;
}

typedef struct gpIterVfxData {
  GPENCIL_PrivateData *pd;
  GPENCIL_tObject *tgp_ob;
  GPUFrameBuffer **target_fb;
  GPUFrameBuffer **source_fb;
  GPUTexture **target_color_tx;
  GPUTexture **source_color_tx;
  GPUTexture **target_reveal_tx;
  GPUTexture **source_reveal_tx;
} gpIterVfxData;

static DRWShadingGroup *gpencil_vfx_pass_create(const char *name,
                                                DRWState state,
                                                gpIterVfxData *iter,
                                                GPUShader *sh)
{
  DRWPass *pass = DRW_pass_create(name, state);
  DRWShadingGroup *grp = DRW_shgroup_create(sh, pass);
  DRW_shgroup_uniform_texture_ref(grp, "colorBuf", iter->source_color_tx);
  DRW_shgroup_uniform_texture_ref(grp, "revealBuf", iter->source_reveal_tx);

  GPENCIL_tVfx *tgp_vfx = BLI_memblock_alloc(iter->pd->gp_vfx_pool);
  tgp_vfx->target_fb = iter->target_fb;
  tgp_vfx->vfx_ps = pass;

  SWAP(GPUFrameBuffer **, iter->target_fb, iter->source_fb);
  SWAP(GPUTexture **, iter->target_color_tx, iter->source_color_tx);
  SWAP(GPUTexture **, iter->target_reveal_tx, iter->source_reveal_tx);

  BLI_LINKS_APPEND(&iter->tgp_ob->vfx, tgp_vfx);

  return grp;
}

static void gpencil_vfx_blur(BlurShaderFxData *fx, Object *ob, gpIterVfxData *iter)
{
  if (fx->radius[0] == 0.0f && fx->radius[1] == 0.0f) {
    return;
  }

  DRWShadingGroup *grp;
  const float s = sin(fx->rotation);
  const float c = cos(fx->rotation);

  float winmat[4][4], persmat[4][4];
  float blur_size[2] = {fx->radius[0], fx->radius[1]};
  DRW_view_persmat_get(NULL, persmat, false);
  const float w = fabsf(mul_project_m4_v3_zfac(persmat, ob->obmat[3]));

  if ((fx->flag & FX_BLUR_DOF_MODE) && iter->pd->camera != NULL) {
    /* Compute circle of confusion size. */
    float coc = (iter->pd->dof_params[0] / -w) - iter->pd->dof_params[1];
    copy_v2_fl(blur_size, fabsf(coc));
  }
  else {
    /* Modify by distance to camera and object scale. */
    DRW_view_winmat_get(NULL, winmat, false);
    const float *vp_size = DRW_viewport_size_get();
    float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
    float scale = mat4_to_scale(ob->obmat);
    float distance_factor = world_pixel_scale * scale * winmat[1][1] * vp_size[1] / w;
    mul_v2_fl(blur_size, distance_factor);
  }

  GPUShader *sh = GPENCIL_shader_fx_blur_get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  if (blur_size[0] > 0.0f) {
    grp = gpencil_vfx_pass_create("Fx Blur H", state, iter, sh);
    DRW_shgroup_uniform_vec2_copy(grp, "offset", (float[2]){blur_size[0] * c, blur_size[0] * s});
    DRW_shgroup_uniform_int_copy(grp, "sampCount", max_ii(1, min_ii(fx->samples, blur_size[0])));
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
  if (blur_size[1] > 0.0f) {
    grp = gpencil_vfx_pass_create("Fx Blur V", state, iter, sh);
    DRW_shgroup_uniform_vec2_copy(grp, "offset", (float[2]){-blur_size[1] * s, blur_size[1] * c});
    DRW_shgroup_uniform_int_copy(grp, "sampCount", max_ii(1, min_ii(fx->samples, blur_size[1])));
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
}

static void gpencil_vfx_colorize(ColorizeShaderFxData *fx, Object *UNUSED(ob), gpIterVfxData *iter)
{
  DRWShadingGroup *grp;

  GPUShader *sh = GPENCIL_shader_fx_colorize_get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  grp = gpencil_vfx_pass_create("Fx Colorize", state, iter, sh);
  DRW_shgroup_uniform_vec3_copy(grp, "lowColor", fx->low_color);
  DRW_shgroup_uniform_vec3_copy(grp, "highColor", fx->high_color);
  DRW_shgroup_uniform_float_copy(grp, "factor", fx->factor);
  DRW_shgroup_uniform_int_copy(grp, "mode", fx->mode);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
}

static void gpencil_vfx_flip(FlipShaderFxData *fx, Object *UNUSED(ob), gpIterVfxData *iter)
{
  DRWShadingGroup *grp;

  float axis_flip[2];
  axis_flip[0] = (fx->flag & FX_FLIP_HORIZONTAL) ? -1.0f : 1.0f;
  axis_flip[1] = (fx->flag & FX_FLIP_VERTICAL) ? -1.0f : 1.0f;

  GPUShader *sh = GPENCIL_shader_fx_transform_get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  grp = gpencil_vfx_pass_create("Fx Flip", state, iter, sh);
  DRW_shgroup_uniform_vec2_copy(grp, "axisFlip", axis_flip);
  DRW_shgroup_uniform_vec2_copy(grp, "waveOffset", (float[2]){0.0f, 0.0f});
  DRW_shgroup_uniform_float_copy(grp, "swirlRadius", 0.0f);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
}

static void gpencil_vfx_rim(RimShaderFxData *fx, Object *ob, gpIterVfxData *iter)
{
  DRWShadingGroup *grp;

  float winmat[4][4], persmat[4][4];
  float offset[2] = {fx->offset[0], fx->offset[1]};
  float blur_size[2] = {fx->blur[0], fx->blur[1]};
  DRW_view_winmat_get(NULL, winmat, false);
  DRW_view_persmat_get(NULL, persmat, false);
  const float *vp_size = DRW_viewport_size_get();
  const float *vp_size_inv = DRW_viewport_invert_size_get();

  const float w = fabsf(mul_project_m4_v3_zfac(persmat, ob->obmat[3]));

  /* Modify by distance to camera and object scale. */
  float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
  float scale = mat4_to_scale(ob->obmat);
  float distance_factor = (world_pixel_scale * scale * winmat[1][1] * vp_size[1]) / w;
  mul_v2_fl(offset, distance_factor);
  mul_v2_v2(offset, vp_size_inv);
  mul_v2_fl(blur_size, distance_factor);

  GPUShader *sh = GPENCIL_shader_fx_rim_get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  grp = gpencil_vfx_pass_create("Fx Rim H", state, iter, sh);
  DRW_shgroup_uniform_vec2_copy(grp, "blurDir", (float[2]){blur_size[0] * vp_size_inv[0], 0.0f});
  DRW_shgroup_uniform_vec2_copy(grp, "uvOffset", offset);
  DRW_shgroup_uniform_int_copy(grp, "sampCount", max_ii(1, min_ii(fx->samples, blur_size[0])));
  DRW_shgroup_uniform_vec3_copy(grp, "maskColor", fx->mask_rgb);
  DRW_shgroup_uniform_bool_copy(grp, "isFirstPass", true);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

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

  grp = gpencil_vfx_pass_create("Fx Rim V", state, iter, sh);
  DRW_shgroup_uniform_vec2_copy(grp, "blurDir", (float[2]){0.0f, blur_size[1] * vp_size_inv[1]});
  DRW_shgroup_uniform_vec2_copy(grp, "uvOffset", offset);
  DRW_shgroup_uniform_vec3_copy(grp, "rimColor", fx->rim_rgb);
  DRW_shgroup_uniform_int_copy(grp, "sampCount", max_ii(1, min_ii(fx->samples, blur_size[1])));
  DRW_shgroup_uniform_int_copy(grp, "blendMode", fx->mode);
  DRW_shgroup_uniform_bool_copy(grp, "isFirstPass", false);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

  if (fx->mode == eShaderFxRimMode_Overlay) {
    /* We cannot do custom blending on MultiTarget framebuffers.
     * Workaround by doing 2 passes. */
    grp = DRW_shgroup_create_sub(grp);
    DRW_shgroup_state_disable(grp, DRW_STATE_BLEND_MUL);
    DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ADD_FULL);
    DRW_shgroup_uniform_int_copy(grp, "blendMode", 999);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
}

static void gpencil_vfx_pixelize(PixelShaderFxData *fx, Object *ob, gpIterVfxData *iter)
{
  DRWShadingGroup *grp;

  float persmat[4][4], winmat[4][4], ob_center[3], pixsize_uniform[2];
  DRW_view_winmat_get(NULL, winmat, false);
  DRW_view_persmat_get(NULL, persmat, false);
  const float *vp_size = DRW_viewport_size_get();
  const float *vp_size_inv = DRW_viewport_invert_size_get();
  float pixel_size[2] = {fx->size[0], fx->size[1]};
  mul_v2_v2(pixel_size, vp_size_inv);

  /* Fixed pixelisation center from object center. */
  const float w = fabsf(mul_project_m4_v3_zfac(persmat, ob->obmat[3]));
  mul_v3_m4v3(ob_center, persmat, ob->obmat[3]);
  mul_v3_fl(ob_center, 1.0f / w);

  const bool use_antialiasing = ((fx->flag & FX_PIXEL_FILTER_NEAREST) == 0);

  /* Convert to uvs. */
  mul_v2_fl(ob_center, 0.5f);
  add_v2_fl(ob_center, 0.5f);

  /* Modify by distance to camera and object scale. */
  float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
  float scale = mat4_to_scale(ob->obmat);
  mul_v2_fl(pixel_size, (world_pixel_scale * scale * winmat[1][1] * vp_size[1]) / w);

  /* Center to texel */
  madd_v2_v2fl(ob_center, pixel_size, -0.5f);

  GPUShader *sh = GPENCIL_shader_fx_pixelize_get();

  DRWState state = DRW_STATE_WRITE_COLOR;

  /* Only if pixelated effect is bigger than 1px. */
  if (pixel_size[0] > vp_size_inv[0]) {
    copy_v2_fl2(pixsize_uniform, pixel_size[0], vp_size_inv[1]);
    grp = gpencil_vfx_pass_create("Fx Pixelize X", state, iter, sh);
    DRW_shgroup_uniform_vec2_copy(grp, "targetPixelSize", pixsize_uniform);
    DRW_shgroup_uniform_vec2_copy(grp, "targetPixelOffset", ob_center);
    DRW_shgroup_uniform_vec2_copy(grp, "accumOffset", (float[2]){pixel_size[0], 0.0f});
    int samp_count = (pixel_size[0] / vp_size_inv[0] > 3.0) ? 2 : 1;
    DRW_shgroup_uniform_int_copy(grp, "sampCount", use_antialiasing ? samp_count : 0);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }

  if (pixel_size[1] > vp_size_inv[1]) {
    copy_v2_fl2(pixsize_uniform, vp_size_inv[0], pixel_size[1]);
    grp = gpencil_vfx_pass_create("Fx Pixelize Y", state, iter, sh);
    DRW_shgroup_uniform_vec2_copy(grp, "targetPixelSize", pixsize_uniform);
    DRW_shgroup_uniform_vec2_copy(grp, "accumOffset", (float[2]){0.0f, pixel_size[1]});
    int samp_count = (pixel_size[1] / vp_size_inv[1] > 3.0) ? 2 : 1;
    DRW_shgroup_uniform_int_copy(grp, "sampCount", use_antialiasing ? samp_count : 0);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
}

static void gpencil_vfx_shadow(ShadowShaderFxData *fx, Object *ob, gpIterVfxData *iter)
{
  DRWShadingGroup *grp;

  const bool use_obj_pivot = (fx->flag & FX_SHADOW_USE_OBJECT) != 0;
  const bool use_wave = (fx->flag & FX_SHADOW_USE_WAVE) != 0;

  float uv_mat[4][4], winmat[4][4], persmat[4][4], rot_center[3];
  float wave_ofs[3], wave_dir[3], wave_phase, blur_dir[2], tmp[2];
  float offset[2] = {fx->offset[0], fx->offset[1]};
  float blur_size[2] = {fx->blur[0], fx->blur[1]};
  DRW_view_winmat_get(NULL, winmat, false);
  DRW_view_persmat_get(NULL, persmat, false);
  const float *vp_size = DRW_viewport_size_get();
  const float *vp_size_inv = DRW_viewport_invert_size_get();
  const float ratio = vp_size_inv[1] / vp_size_inv[0];

  copy_v3_v3(rot_center, (use_obj_pivot && fx->object) ? fx->object->obmat[3] : ob->obmat[3]);

  const float w = fabsf(mul_project_m4_v3_zfac(persmat, rot_center));
  mul_v3_m4v3(rot_center, persmat, rot_center);
  mul_v3_fl(rot_center, 1.0f / w);

  /* Modify by distance to camera and object scale. */
  float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
  float scale = mat4_to_scale(ob->obmat);
  float distance_factor = (world_pixel_scale * scale * winmat[1][1] * vp_size[1]) / w;
  mul_v2_fl(offset, distance_factor);
  mul_v2_v2(offset, vp_size_inv);
  mul_v2_fl(blur_size, distance_factor);

  rot_center[0] = rot_center[0] * 0.5f + 0.5f;
  rot_center[1] = rot_center[1] * 0.5f + 0.5f;

  /* UV transform matrix. (loc, rot, scale) Sent to shader as 2x3 matrix. */
  unit_m4(uv_mat);
  translate_m4(uv_mat, rot_center[0], rot_center[1], 0.0f);
  rescale_m4(uv_mat, (float[3]){1.0f / fx->scale[0], 1.0f / fx->scale[1], 1.0f});
  translate_m4(uv_mat, -offset[0], -offset[1], 0.0f);
  rescale_m4(uv_mat, (float[3]){1.0f / ratio, 1.0f, 1.0f});
  rotate_m4(uv_mat, 'Z', fx->rotation);
  rescale_m4(uv_mat, (float[3]){ratio, 1.0f, 1.0f});
  translate_m4(uv_mat, -rot_center[0], -rot_center[1], 0.0f);

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
    /* Rotate 90°. */
    copy_v2_v2(wave_ofs, wave_dir);
    SWAP(float, wave_ofs[0], wave_ofs[1]);
    wave_ofs[1] *= -1.0f;
    /* Keep world space scalling and aspect ratio. */
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

  GPUShader *sh = GPENCIL_shader_fx_shadow_get();

  copy_v2_fl2(blur_dir, blur_size[0] * vp_size_inv[0], 0.0f);

  DRWState state = DRW_STATE_WRITE_COLOR;
  grp = gpencil_vfx_pass_create("Fx Shadow H", state, iter, sh);
  DRW_shgroup_uniform_vec2_copy(grp, "blurDir", blur_dir);
  DRW_shgroup_uniform_vec2_copy(grp, "waveDir", wave_dir);
  DRW_shgroup_uniform_vec2_copy(grp, "waveOffset", wave_ofs);
  DRW_shgroup_uniform_float_copy(grp, "wavePhase", wave_phase);
  DRW_shgroup_uniform_vec2_copy(grp, "uvRotX", uv_mat[0]);
  DRW_shgroup_uniform_vec2_copy(grp, "uvRotY", uv_mat[1]);
  DRW_shgroup_uniform_vec2_copy(grp, "uvOffset", uv_mat[3]);
  DRW_shgroup_uniform_int_copy(grp, "sampCount", max_ii(1, min_ii(fx->samples, blur_size[0])));
  DRW_shgroup_uniform_bool_copy(grp, "isFirstPass", true);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

  unit_m4(uv_mat);
  zero_v2(wave_ofs);

  /* We reseted the uv_mat so we need to accound for the rotation in the  */
  copy_v2_fl2(tmp, 0.0f, blur_size[1]);
  rotate_v2_v2fl(blur_dir, tmp, -fx->rotation);
  mul_v2_v2(blur_dir, vp_size_inv);

  state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL;
  grp = gpencil_vfx_pass_create("Fx Shadow V", state, iter, sh);
  DRW_shgroup_uniform_vec4_copy(grp, "shadowColor", fx->shadow_rgba);
  DRW_shgroup_uniform_vec2_copy(grp, "blurDir", blur_dir);
  DRW_shgroup_uniform_vec2_copy(grp, "waveOffset", wave_ofs);
  DRW_shgroup_uniform_vec2_copy(grp, "uvRotX", uv_mat[0]);
  DRW_shgroup_uniform_vec2_copy(grp, "uvRotY", uv_mat[1]);
  DRW_shgroup_uniform_vec2_copy(grp, "uvOffset", uv_mat[3]);
  DRW_shgroup_uniform_int_copy(grp, "sampCount", max_ii(1, min_ii(fx->samples, blur_size[1])));
  DRW_shgroup_uniform_bool_copy(grp, "isFirstPass", false);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
}

static void gpencil_vfx_glow(GlowShaderFxData *fx, Object *UNUSED(ob), gpIterVfxData *iter)
{
  const bool use_glow_under = (fx->flag & FX_GLOW_USE_ALPHA) != 0;
  DRWShadingGroup *grp;
  const float s = sin(fx->rotation);
  const float c = cos(fx->rotation);

  GPUShader *sh = GPENCIL_shader_fx_glow_get();

  float ref_col[3];

  if (fx->mode == eShaderFxGlowMode_Luminance) {
    ref_col[0] = fx->threshold;
    ref_col[1] = -1.0f;
    ref_col[2] = -1.0f;
  }
  else {
    copy_v3_v3(ref_col, fx->select_color);
  }

  DRWState state = DRW_STATE_WRITE_COLOR;
  grp = gpencil_vfx_pass_create("Fx Glow H", state, iter, sh);
  DRW_shgroup_uniform_vec2_copy(grp, "offset", (float[2]){fx->blur[0] * c, fx->blur[0] * s});
  DRW_shgroup_uniform_int_copy(grp, "sampCount", max_ii(1, min_ii(fx->samples, fx->blur[0])));
  DRW_shgroup_uniform_vec3_copy(grp, "threshold", ref_col);
  DRW_shgroup_uniform_vec4_copy(grp, "glowColor", fx->glow_color);
  DRW_shgroup_uniform_bool_copy(grp, "glowUnder", use_glow_under);
  DRW_shgroup_uniform_bool_copy(grp, "firstPass", true);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

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
    iter->pd->use_signed_fb = true;
  }

  grp = gpencil_vfx_pass_create("Fx Glow V", state, iter, sh);
  DRW_shgroup_uniform_vec2_copy(grp, "offset", (float[2]){-fx->blur[1] * s, fx->blur[1] * c});
  DRW_shgroup_uniform_int_copy(grp, "sampCount", max_ii(1, min_ii(fx->samples, fx->blur[0])));
  DRW_shgroup_uniform_vec3_copy(grp, "threshold", (float[3]){-1.0f, -1.0f, -1.0f});
  DRW_shgroup_uniform_vec4_copy(grp, "glowColor", (float[4]){1.0f, 1.0f, 1.0f, fx->glow_color[3]});
  DRW_shgroup_uniform_bool_copy(grp, "firstPass", false);
  DRW_shgroup_uniform_int_copy(grp, "blendMode", fx->blend_mode);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
}

static void gpencil_vfx_wave(WaveShaderFxData *fx, Object *ob, gpIterVfxData *iter)
{
  DRWShadingGroup *grp;

  float winmat[4][4], persmat[4][4], wave_center[3];
  float wave_ofs[3], wave_dir[3], wave_phase;
  DRW_view_winmat_get(NULL, winmat, false);
  DRW_view_persmat_get(NULL, persmat, false);
  const float *vp_size = DRW_viewport_size_get();
  const float *vp_size_inv = DRW_viewport_invert_size_get();

  const float w = fabsf(mul_project_m4_v3_zfac(persmat, ob->obmat[3]));
  mul_v3_m4v3(wave_center, persmat, ob->obmat[3]);
  mul_v3_fl(wave_center, 1.0f / w);

  /* Modify by distance to camera and object scale. */
  float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
  float scale = mat4_to_scale(ob->obmat);
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
  /* Rotate 90°. */
  copy_v2_v2(wave_ofs, wave_dir);
  SWAP(float, wave_ofs[0], wave_ofs[1]);
  wave_ofs[1] *= -1.0f;
  /* Keep world space scalling and aspect ratio. */
  mul_v2_fl(wave_dir, 1.0f / (max_ff(1e-8f, fx->period) * distance_factor));
  mul_v2_v2(wave_dir, vp_size);
  mul_v2_fl(wave_ofs, fx->amplitude * distance_factor);
  mul_v2_v2(wave_ofs, vp_size_inv);
  /* Phase start at shadow center. */
  wave_phase = fx->phase - dot_v2v2(wave_center, wave_dir);

  GPUShader *sh = GPENCIL_shader_fx_transform_get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  grp = gpencil_vfx_pass_create("Fx Wave", state, iter, sh);
  DRW_shgroup_uniform_vec2_copy(grp, "axisFlip", (float[2]){1.0f, 1.0f});
  DRW_shgroup_uniform_vec2_copy(grp, "waveDir", wave_dir);
  DRW_shgroup_uniform_vec2_copy(grp, "waveOffset", wave_ofs);
  DRW_shgroup_uniform_float_copy(grp, "wavePhase", wave_phase);
  DRW_shgroup_uniform_float_copy(grp, "swirlRadius", 0.0f);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
}

static void gpencil_vfx_swirl(SwirlShaderFxData *fx, Object *UNUSED(ob), gpIterVfxData *iter)
{
  DRWShadingGroup *grp;

  if (fx->object == NULL) {
    return;
  }

  float winmat[4][4], persmat[4][4], swirl_center[3];
  DRW_view_winmat_get(NULL, winmat, false);
  DRW_view_persmat_get(NULL, persmat, false);
  const float *vp_size = DRW_viewport_size_get();

  copy_v3_v3(swirl_center, fx->object->obmat[3]);

  const float w = fabsf(mul_project_m4_v3_zfac(persmat, swirl_center));
  mul_v3_m4v3(swirl_center, persmat, swirl_center);
  mul_v3_fl(swirl_center, 1.0f / w);

  /* Modify by distance to camera and object scale. */
  float world_pixel_scale = 1.0f / GPENCIL_PIXEL_FACTOR;
  float scale = mat4_to_scale(fx->object->obmat);
  float distance_factor = (world_pixel_scale * scale * winmat[1][1] * vp_size[1]) / w;

  mul_v2_fl(swirl_center, 0.5f);
  add_v2_fl(swirl_center, 0.5f);
  mul_v2_v2(swirl_center, vp_size);

  float radius = fx->radius * distance_factor;
  if (radius < 1.0f) {
    return;
  }

  GPUShader *sh = GPENCIL_shader_fx_transform_get();

  DRWState state = DRW_STATE_WRITE_COLOR;
  grp = gpencil_vfx_pass_create("Fx Flip", state, iter, sh);
  DRW_shgroup_uniform_vec2_copy(grp, "axisFlip", (float[2]){1.0f, 1.0f});
  DRW_shgroup_uniform_vec2_copy(grp, "waveOffset", (float[2]){0.0f, 0.0f});
  DRW_shgroup_uniform_vec2_copy(grp, "swirlCenter", swirl_center);
  DRW_shgroup_uniform_float_copy(grp, "swirlAngle", fx->angle);
  DRW_shgroup_uniform_float_copy(grp, "swirlRadius", radius);
  DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
}

void gpencil_vfx_cache_populate(GPENCIL_Data *vedata, Object *ob, GPENCIL_tObject *tgp_ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  GPENCIL_FramebufferList *fbl = vedata->fbl;
  GPENCIL_PrivateData *pd = vedata->stl->pd;
  /* If simplify enabled, nothing more to do. */
  if (pd->simplify_fx) {
    return;
  }

  /* These may not be allocated yet, use adress of future pointer. */
  gpIterVfxData iter = {
      .pd = pd,
      .tgp_ob = tgp_ob,
      .target_fb = &fbl->layer_fb,
      .source_fb = &fbl->object_fb,
      .target_color_tx = &pd->color_layer_tx,
      .source_color_tx = &pd->color_object_tx,
      .target_reveal_tx = &pd->reveal_layer_tx,
      .source_reveal_tx = &pd->reveal_object_tx,
  };

  LISTBASE_FOREACH (ShaderFxData *, fx, &ob->shader_fx) {
    if (effect_is_active(gpd, fx, pd->is_viewport)) {
      switch (fx->type) {
        case eShaderFxType_Blur:
          gpencil_vfx_blur((BlurShaderFxData *)fx, ob, &iter);
          break;
        case eShaderFxType_Colorize:
          gpencil_vfx_colorize((ColorizeShaderFxData *)fx, ob, &iter);
          break;
        case eShaderFxType_Flip:
          gpencil_vfx_flip((FlipShaderFxData *)fx, ob, &iter);
          break;
        case eShaderFxType_Pixel:
          gpencil_vfx_pixelize((PixelShaderFxData *)fx, ob, &iter);
          break;
        case eShaderFxType_Rim:
          gpencil_vfx_rim((RimShaderFxData *)fx, ob, &iter);
          break;
        case eShaderFxType_Shadow:
          gpencil_vfx_shadow((ShadowShaderFxData *)fx, ob, &iter);
          break;
        case eShaderFxType_Glow:
          gpencil_vfx_glow((GlowShaderFxData *)fx, ob, &iter);
          break;
        case eShaderFxType_Swirl:
          gpencil_vfx_swirl((SwirlShaderFxData *)fx, ob, &iter);
          break;
        case eShaderFxType_Wave:
          gpencil_vfx_wave((WaveShaderFxData *)fx, ob, &iter);
          break;
        default:
          break;
      }
    }
  }

  if (tgp_ob->vfx.first != NULL) {
    /* We need an extra pass to combine result to main buffer. */
    iter.target_fb = &fbl->gpencil_fb;

    GPUShader *sh = GPENCIL_shader_fx_composite_get();

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_MUL;
    DRWShadingGroup *grp = gpencil_vfx_pass_create("GPencil Object Compose", state, &iter, sh);
    DRW_shgroup_uniform_int_copy(grp, "isFirstPass", true);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

    /* We cannot do custom blending on MultiTarget framebuffers.
     * Workaround by doing 2 passes. */
    grp = DRW_shgroup_create_sub(grp);
    DRW_shgroup_state_disable(grp, DRW_STATE_BLEND_MUL);
    DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ADD_FULL);
    DRW_shgroup_uniform_int_copy(grp, "isFirstPass", false);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

    pd->use_object_fb = true;
    pd->use_layer_fb = true;
  }
}
