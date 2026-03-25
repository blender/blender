/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_image_coordinates_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_image_coordinates_pixel)

void main()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);

  imageStore(output_img, texel, int4(data_offset + texel, int2(0)));
}
