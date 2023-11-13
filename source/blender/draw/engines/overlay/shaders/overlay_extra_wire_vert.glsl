/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

vec2 screen_position(vec4 p)
{
  return ((p.xy / p.w) * 0.5 + 0.5) * sizeViewport.xy;
}

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

#ifdef SELECT_EDGES
  /* HACK: to avoid losing sub-pixel object in selections, we add a bit of randomness to the
   * wire to at least create one fragment that will pass the occlusion query. */
  /* TODO(fclem): Limit this workaround to selection. It's not very noticeable but still... */
  gl_Position.xy += sizeViewportInv * gl_Position.w * ((gl_VertexID % 2 == 0) ? -1.0 : 1.0);
#endif

  stipple_coord = stipple_start = screen_position(gl_Position);

#ifdef OBJECT_WIRE
  /* Extract data packed inside the unused mat4 members. */
  finalColor = vec4(ModelMatrix[0][3], ModelMatrix[1][3], ModelMatrix[2][3], ModelMatrix[3][3]);
#else

  if (colorid != 0) {
    /* TH_CAMERA_PATH is the only color code at the moment.
     * Checking `colorid != 0` to avoid having to sync its value with the GLSL code. */
    finalColor = colorCameraPath;
    finalColor.a = 0.0; /* No Stipple */
  }
  else {
    finalColor = color;
    finalColor.a = 1.0; /* Stipple */
  }
#endif

#ifdef SELECT_EDGES
  finalColor.a = 0.0; /* No Stipple */
#endif

  view_clipping_distances(world_pos);
}
