/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_extra_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_image_base)
VERTEX_SHADER_CREATE_INFO(draw_modelmat)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(drw_custom_id());
  float3 world_pos = drw_point_object_to_world(pos);
  if (is_camera_background) {
    /* Model matrix converts to view position to avoid jittering (see #91398). */
#ifdef DEPTH_BIAS
    gl_Position = depth_bias_winmat * float4(world_pos, 1.0f);
#else
    gl_Position = drw_point_view_to_homogenous(world_pos);
#endif
    /* Camera background images are not really part of the 3D space.
     * It makes no sense to apply clipping on them. */
    view_clipping_distances_bypass();
  }
  else {
#ifdef DEPTH_BIAS
    gl_Position = depth_bias_winmat * (drw_view().viewmat * float4(world_pos, 1.0f));
#else
    gl_Position = drw_point_world_to_homogenous(world_pos);
#endif
    view_clipping_distances(world_pos);
  }

  if (depth_set) {
    /* Result in a position at 1.0 (far plane). Small epsilon to avoid precision issue.
     * This mimics the effect of infinite projection matrix
     * (see http://www.terathon.com/gdc07_lengyel.pdf). */
    gl_Position.z = gl_Position.w - 2.4e-7f;
    view_clipping_distances_bypass();
  }

  uvs = pos.xy * 0.5f + 0.5f;
}
