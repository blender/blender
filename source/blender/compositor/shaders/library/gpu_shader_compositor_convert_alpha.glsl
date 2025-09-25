/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define CMP_NODE_ALPHA_CONVERT_PREMULTIPLY 0
#define CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY 1

void node_composite_convert_alpha(const float4 color, const float type, out float4 result)
{
  result = color;
  switch (int(type)) {
    case CMP_NODE_ALPHA_CONVERT_PREMULTIPLY:
      result = float4(color.xyz() * color.w, color.w);
      break;
    case CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY:
      result = color.w == 0.0f ? color : float4(color.xyz() / color.w, color.w);
      break;
  }
}

#undef CMP_NODE_ALPHA_CONVERT_PREMULTIPLY
#undef CMP_NODE_ALPHA_CONVERT_UNPREMULTIPLY
