/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Separable Hexagonal Bokeh Blur by Colin Barré-Brisebois
 * https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited-part-1-basic-3-pass-version/
 * Converted and adapted from HLSL to GLSL by Clément Foucault
 */

#include "infos/workbench_effect_dof_infos.hh"

#include "draw_view_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "workbench_effect_dof_lib.glsl"

FRAGMENT_SHADER_CREATE_INFO(workbench_effect_dof_blur2)

/**
 * ----------------- STEP 3 ------------------
 * 3x3 Median Filter
 * Morgan McGuire and Kyle Whitson
 * http://graphics.cs.williams.edu
 *
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2006 Morgan McGuire and Williams College, All rights reserved.
 */

void main()
{
  /* Half Res pass */
  float2 pixel_size = 1.0f / float2(textureSize(blur_tx, 0).xy);
  float2 uv = gl_FragCoord.xy * pixel_size.xy;
  float coc = dof_decode_coc(texture(input_coc_tx, uv).rg);
  /* Only use this filter if coc is > 9.0f
   * since this filter is not weighted by CoC
   * and can bleed a bit. */
  float rad = clamp(coc - 9.0f, 0.0f, 1.0f);

#define vec float4
#define toVec(x) x.rgba

#define s2(a, b) \
  temp = a; \
  a = min(a, b); \
  b = max(temp, b);
#define mn3(a, b, c) \
  s2(a, b); \
  s2(a, c);
#define mx3(a, b, c) \
  s2(b, c); \
  s2(a, c);

#define mnmx3(a, b, c) \
  mx3(a, b, c); \
  s2(a, b); /* 3 exchanges */
#define mnmx4(a, b, c, d) \
  s2(a, b); \
  s2(c, d); \
  s2(a, c); \
  s2(b, d); /* 4 exchanges */
#define mnmx5(a, b, c, d, e) \
  s2(a, b); \
  s2(c, d); \
  mn3(a, c, e); \
  mx3(b, d, e); /* 6 exchanges */
#define mnmx6(a, b, c, d, e, f) \
  s2(a, d); \
  s2(b, e); \
  s2(c, f); \
  mn3(a, b, c); \
  mx3(d, e, f); /* 7 exchanges */

  vec v[9];

  /* Add the pixels which make up our window to the pixel array. */
  for (int dX = -1; dX <= 1; dX++) {
    for (int dY = -1; dY <= 1; dY++) {
      float2 offset = float2(float(dX), float(dY));
      /* If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) +
       * (dY + R) of the pixel array. This will fill the pixel array, with the top left pixel of
       * the window at pixel[0] and the bottom right pixel of the window at pixel[N-1]. */
      v[(dX + 1) * 3 + (dY + 1)] = toVec(texture(blur_tx, uv + offset * pixel_size * rad));
    }
  }

  vec temp;

  /* Starting with a subset of size 6, remove the min and max each time */
  mnmx6(v[0], v[1], v[2], v[3], v[4], v[5]);
  mnmx5(v[1], v[2], v[3], v[4], v[6]);
  mnmx4(v[2], v[3], v[4], v[7]);
  mnmx3(v[3], v[4], v[8]);
  toVec(final_color) = v[4];
}
