/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_image_overlays_stereo_merge_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_image_overlays_stereo_merge)

#define S3D_DISPLAY_ANAGLYPH 0
#define S3D_DISPLAY_INTERLACE 1

#define S3D_INTERLACE_ROW 0
#define S3D_INTERLACE_COLUMN 1
#define S3D_INTERLACE_CHECKERBOARD 2

#define stereo_display_mode (stereoDisplaySettings & ((1 << 3) - 1))
#define stereo_interlace_mode ((stereoDisplaySettings >> 3) & ((1 << 3) - 1))
#define stereo_interlace_swap bool(stereoDisplaySettings >> 6)

bool interlace(int2 texel)
{
  int interlace_mode = stereo_interlace_mode;
  switch (interlace_mode) {
    case S3D_INTERLACE_CHECKERBOARD:
      return ((texel.x + texel.y) & 1) != 0;
    case S3D_INTERLACE_ROW:
      return (texel.y & 1) != 0;
    case S3D_INTERLACE_COLUMN:
      return (texel.x & 1) != 0;
  }
  return false;
}

void main()
{
  int2 texel = int2(gl_FragCoord.xy);

  if (stereo_display_mode == S3D_DISPLAY_INTERLACE && (interlace(texel) == stereo_interlace_swap))
  {
    gpu_discard_fragment();
  }

  imageColor = texelFetch(imageTexture, texel, 0);
  overlayColor = texelFetch(overlayTexture, texel, 0);
}
