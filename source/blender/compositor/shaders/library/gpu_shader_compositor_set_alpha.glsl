/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define CMP_NODE_SETALPHA_MODE_APPLY 0
#define CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA 1

void node_composite_set_alpha(float4 color, float alpha, float type, out float4 result)
{
  switch (int(type)) {
    case CMP_NODE_SETALPHA_MODE_APPLY:
      result = color * alpha;
      break;
    case CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA:
      result = float4(color.rgb, alpha);
      break;
  }
}

#undef CMP_NODE_SETALPHA_MODE_APPLY
#undef CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA
