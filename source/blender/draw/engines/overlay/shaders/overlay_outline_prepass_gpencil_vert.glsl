/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_gpencil_lib.glsl"
#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

uint outline_colorid_get()
{
#ifdef OBINFO_NEW
  eObjectInfoFlag ob_flag = eObjectInfoFlag(floatBitsToUint(drw_infos[resource_id].infos.w));
  bool is_active = flag_test(ob_flag, OBJECT_ACTIVE);
#else
  int flag = int(abs(ObjectInfo.w));
  bool is_active = (flag & DRW_BASE_ACTIVE) != 0;
#endif

  if (isTransform) {
    return 0u; /* colorTransform */
  }
  else if (is_active) {
    return 3u; /* colorActive */
  }
  else {
    return 1u; /* colorSelect */
  }

  return 0u;
}

/* Replace top 2 bits (of the 16bit output) by outlineId.
 * This leaves 16K different IDs to create outlines between objects.
 * SHIFT = (32 - (16 - 2)) */
#define SHIFT 18u

void main()
{
  vec3 world_pos;
  vec3 unused_N;
  vec4 unused_color;
  float unused_strength;
  vec2 unused_uv;

  gl_Position = gpencil_vertex(vec4(sizeViewport, sizeViewportInv),
                               world_pos,
                               unused_N,
                               unused_color,
                               unused_strength,
                               unused_uv,
                               gp_interp_flat.sspos,
                               gp_interp_flat.aspect,
                               gp_interp_noperspective.thickness,
                               gp_interp_noperspective.hardness);

  /* Small bias to always be on top of the geom. */
  gl_Position.z -= 1e-3;

  /* ID 0 is nothing (background) */
  interp.ob_id = uint(resource_handle + 1);

  /* Should be 2 bits only [0..3]. */
  uint outline_id = outline_colorid_get();

  /* Combine for 16bit uint target. */
  interp.ob_id = (outline_id << 14u) | ((interp.ob_id << SHIFT) >> SHIFT);

  view_clipping_distances(world_pos);
}
