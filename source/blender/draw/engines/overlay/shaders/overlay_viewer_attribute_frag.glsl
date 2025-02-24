/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_viewer_attribute_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_viewer_attribute_pointcloud)

void main()
{
  out_color = finalColor;
  out_color.a *= opacity;
  /* Writing to this second texture is necessary to avoid undefined behavior. */
  lineOutput = vec4(0.0);
}
