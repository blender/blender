/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_keyframe_shape_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_keyframe_shape)

#define line_falloff 1.0f
#define circle_scale sqrt(2.0f / 3.1416f)
#define square_scale sqrt(0.5f)
#define diagonal_scale sqrt(0.5f)

bool test(uint bit)
{
  return (flags & bit) != 0u;
}

float2 line_thresholds(float width)
{
  return float2(max(0.0f, width - line_falloff), width);
}

void main()
{
  gl_Position = ModelViewProjectionMatrix * float4(pos, 0.0f, 1.0f);

  /* Align to pixel grid if the viewport size is known. */
  if (ViewportSize.x > 0) {
    float2 scale = ViewportSize * 0.5f;
    float2 px_pos = (gl_Position.xy + 1) * scale;
    float2 adj_pos = round(px_pos - 0.5f) + 0.5f;
    gl_Position.xy = adj_pos / scale - 1;
  }

  /* Pass through parameters. */
  finalColor = color;
  finalOutlineColor = outlineColor;
  finalFlags = flags;

  if (!test(GPU_KEYFRAME_SHAPE_DIAMOND | GPU_KEYFRAME_SHAPE_CIRCLE |
            GPU_KEYFRAME_SHAPE_CLIPPED_VERTICAL | GPU_KEYFRAME_SHAPE_CLIPPED_HORIZONTAL))
  {
    finalFlags |= GPU_KEYFRAME_SHAPE_DIAMOND;
  }

  /* Size-dependent line thickness. */
  float half_width = (0.06f + (size - 10) * 0.04f);
  float line_width = half_width + line_falloff;

  /* Outline thresholds. */
  thresholds.xy = line_thresholds(line_width * outline_scale);

  /* Inner dot thresholds. */
  thresholds.zw = line_thresholds(line_width * 1.6f);

  /* Extend the primitive size by half line width on either side; odd for symmetry. */
  float ext_radius = round(0.5f * size) + thresholds.x;

  gl_PointSize = ceil(ext_radius + thresholds.y) * 2 + 1;

  /* Diamond radius. */
  radii[0] = ext_radius * diagonal_scale;

  /* Circle radius. */
  radii[1] = ext_radius * circle_scale;

  /* Square radius. */
  radii[2] = round(ext_radius * square_scale);

  /* Min/max cutout offset. */
  radii[3] = -line_falloff;

  /* Convert to PointCoord units. */
  radii /= gl_PointSize;
  thresholds /= gl_PointSize;
}
