/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  finalColor = color;

  vec4 wPos = InstanceModelMatrix * vec4(pos * size, 1.0);
  gl_Position = ViewProjectionMatrix * wPos;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(wPos.xyz);
#endif
}
