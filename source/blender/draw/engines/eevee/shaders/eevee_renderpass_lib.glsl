/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_render_pass_out)

void output_renderpass_color(int id, float4 color)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  if (id >= 0) {
    int2 texel = int2(gl_FragCoord.xy);
    imageStoreFast(rp_color_img, int3(texel, id), color);
  }
#endif
}

void output_renderpass_value(int id, float value)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  if (id >= 0) {
    int2 texel = int2(gl_FragCoord.xy);
    imageStoreFast(rp_value_img, int3(texel, id), float4(value));
  }
#endif
}

void clear_aovs()
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  for (int i = 0; i < AOV_MAX && i < uniform_buf.render_pass.aovs.color_len; i++) {
    output_renderpass_color(uniform_buf.render_pass.color_len + i, float4(0));
  }
  for (int i = 0; i < AOV_MAX && i < uniform_buf.render_pass.aovs.value_len; i++) {
    output_renderpass_value(uniform_buf.render_pass.value_len + i, 0.0f);
  }
#endif
}

void output_aov(float4 color, float value, uint hash)
{
#if defined(MAT_RENDER_PASS_SUPPORT) && defined(GPU_FRAGMENT_SHADER)
  for (int i = 0; i < AOV_MAX && i < uniform_buf.render_pass.aovs.color_len; i++) {
    if (uniform_buf.render_pass.aovs.hash_color[i].x == hash) {
      imageStoreFast(
          rp_color_img, int3(int2(gl_FragCoord.xy), uniform_buf.render_pass.color_len + i), color);
      return;
    }
  }
  for (int i = 0; i < AOV_MAX && i < uniform_buf.render_pass.aovs.value_len; i++) {
    if (uniform_buf.render_pass.aovs.hash_value[i].x == hash) {
      imageStoreFast(rp_value_img,
                     int3(int2(gl_FragCoord.xy), uniform_buf.render_pass.value_len + i),
                     float4(value));
      return;
    }
  }
#endif
}
