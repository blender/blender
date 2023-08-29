/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef USE_WORLD_CLIP_PLANES
#  if defined(GPU_VERTEX_SHADER) || defined(GPU_GEOMETRY_SHADER)

/* When all shaders are builtin shaders are migrated this could be applied directly. */
#    ifdef USE_GPU_SHADER_CREATE_INFO
#      define WorldClipPlanes clipPlanes.world
#    else
uniform vec4 WorldClipPlanes[6];
#    endif

void world_clip_planes_calc_clip_distance(vec3 wpos)
{
  vec4 pos = vec4(wpos, 1.0);

  gl_ClipDistance[0] = dot(WorldClipPlanes[0], pos);
  gl_ClipDistance[1] = dot(WorldClipPlanes[1], pos);
  gl_ClipDistance[2] = dot(WorldClipPlanes[2], pos);
  gl_ClipDistance[3] = dot(WorldClipPlanes[3], pos);
  gl_ClipDistance[4] = dot(WorldClipPlanes[4], pos);
  gl_ClipDistance[5] = dot(WorldClipPlanes[5], pos);
}

#  endif

#endif
