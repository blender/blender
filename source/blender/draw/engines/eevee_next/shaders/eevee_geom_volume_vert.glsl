/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_pointcloud_lib.glsl"
#include "draw_model_lib.glsl"
#include "eevee_surf_lib.glsl"

void main()
{
  DRW_VIEW_FROM_RESOURCE_ID;

  init_interface();

  /* TODO(fclem): Find a better way? This is reverting what draw_resource_finalize does. */
  vec3 size = safe_rcp(OrcoTexCoFactors[1].xyz * 2.0);                    /* Box half-extent. */
  vec3 loc = size + (OrcoTexCoFactors[0].xyz / -OrcoTexCoFactors[1].xyz); /* Box center. */

  /* Use bounding box geometry for now. */
  vec3 lP = loc + pos * size;
  interp.P = drw_point_object_to_world(lP);

  gl_Position = drw_point_world_to_homogenous(interp.P);
}
