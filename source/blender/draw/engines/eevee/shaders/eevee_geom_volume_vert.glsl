/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_geom_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_clip_plane)
VERTEX_SHADER_CREATE_INFO(eevee_geom_volume)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_surf_lib.glsl"

void main()
{
  DRW_VIEW_FROM_RESOURCE_ID;

  init_interface();

  /* TODO(fclem): Find a better way? This is reverting what draw_resource_finalize does. */
  ObjectInfos info = drw_object_infos();
  float3 size = safe_rcp(info.orco_mul * 2.0f);         /* Box half-extent. */
  float3 loc = size + (info.orco_add / -info.orco_mul); /* Box center. */

  /* Use bounding box geometry for now. */
  float3 lP = loc + pos * size;
  interp.P = drw_point_object_to_world(lP);

  gl_Position = reverse_z::transform(drw_point_world_to_homogenous(interp.P));

#ifdef MAT_SHADOW
  /* Volumes currently do not support shadow. But the shader validation pipeline still compiles the
   * shadow variant of this shader. Avoid linking error on Intel Windows drivers. */
#  ifdef SHADOW_UPDATE_ATOMIC_RASTER
  shadow_iface.shadow_view_id = 0;
#  endif
  shadow_clip.position = float3(0);
  shadow_clip.vector = float3(0);
#endif
}
