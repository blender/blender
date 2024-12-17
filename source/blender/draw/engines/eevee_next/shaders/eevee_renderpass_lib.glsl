/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_info.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_render_pass_out)

void output_renderpass_color(int id, vec4 color)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  if (id >= 0) {
    ivec2 texel = ivec2(gl_FragCoord.xy);
    imageStoreFast(rp_color_img, ivec3(texel, id), color);
  }
#endif
}

void output_renderpass_value(int id, float value)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  if (id >= 0) {
    ivec2 texel = ivec2(gl_FragCoord.xy);
    imageStoreFast(rp_value_img, ivec3(texel, id), vec4(value));
  }
#endif
}

void clear_aovs()
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  for (int i = 0; i < AOV_MAX && i < uniform_buf.render_pass.aovs.color_len; i++) {
    output_renderpass_color(uniform_buf.render_pass.color_len + i, vec4(0));
  }
  for (int i = 0; i < AOV_MAX && i < uniform_buf.render_pass.aovs.value_len; i++) {
    output_renderpass_value(uniform_buf.render_pass.value_len + i, 0.0);
  }
#endif
}

int aov_color_index(uint hash)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  for (int i = 0; i < AOV_MAX && i < uniform_buf.render_pass.aovs.color_len; i++) {
    if (uniform_buf.render_pass.aovs.hash_color[i].x == hash) {
      return i;
    }
  }
#endif
  return -1;
}

int aov_value_index(uint hash)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  for (int i = 0; i < AOV_MAX && i < uniform_buf.render_pass.aovs.value_len; i++) {
    if (uniform_buf.render_pass.aovs.hash_value[i].x == hash) {
      return i;
    }
  }
#endif
  return -1;
}

void output_aov(vec4 color, float value, uint hash)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  int color_index = aov_color_index(hash);
  if (color_index >= 0) {
    output_renderpass_color(uniform_buf.render_pass.color_len + color_index, color);
    return;
  }

  int value_index = aov_value_index(hash);
  if (value_index >= 0) {
    output_renderpass_value(uniform_buf.render_pass.value_len + value_index, value);
  }
#endif
}
