/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(drw_CustomID);
  vec3 world_pos = point_object_to_world(pos);
  if (isCameraBackground) {
    /* Model matrix converts to view position to avoid jittering (see #91398). */
#ifdef DEPTH_BIAS
    gl_Position = depth_bias_winmat * vec4(world_pos, 1.0);
#else
    gl_Position = point_view_to_ndc(world_pos);
#endif
    /* Camera background images are not really part of the 3D space.
     * It makes no sense to apply clipping on them. */
    view_clipping_distances_bypass();
  }
  else {
#ifdef DEPTH_BIAS
    gl_Position = depth_bias_winmat * (ViewMatrix * vec4(world_pos, 1.0));
#else
    gl_Position = point_world_to_ndc(world_pos);
#endif
    view_clipping_distances(world_pos);
  }

  if (depthSet) {
    /* Result in a position at 1.0 (far plane). Small epsilon to avoid precision issue.
     * This mimics the effect of infinite projection matrix
     * (see http://www.terathon.com/gdc07_lengyel.pdf). */
    gl_Position.z = gl_Position.w - 2.4e-7;
    view_clipping_distances_bypass();
  }

  uvs = pos.xy * 0.5 + 0.5;
}
