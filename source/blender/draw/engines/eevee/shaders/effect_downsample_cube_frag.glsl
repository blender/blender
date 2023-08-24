/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Simple down-sample shader. Takes the average of the 4 texels of lower mip.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

void main()
{

  /* Global scope arrays get allocated using local memory in Metal. Moving inside function scope to
   * reduce register pressure. */
  const vec3 maj_axes[6] = vec3[6](vec3(1.0, 0.0, 0.0),
                                   vec3(-1.0, 0.0, 0.0),
                                   vec3(0.0, 1.0, 0.0),
                                   vec3(0.0, -1.0, 0.0),
                                   vec3(0.0, 0.0, 1.0),
                                   vec3(0.0, 0.0, -1.0));
  const vec3 x_axis[6] = vec3[6](vec3(0.0, 0.0, -1.0),
                                 vec3(0.0, 0.0, 1.0),
                                 vec3(1.0, 0.0, 0.0),
                                 vec3(1.0, 0.0, 0.0),
                                 vec3(1.0, 0.0, 0.0),
                                 vec3(-1.0, 0.0, 0.0));
  const vec3 y_axis[6] = vec3[6](vec3(0.0, -1.0, 0.0),
                                 vec3(0.0, -1.0, 0.0),
                                 vec3(0.0, 0.0, 1.0),
                                 vec3(0.0, 0.0, -1.0),
                                 vec3(0.0, -1.0, 0.0),
                                 vec3(0.0, -1.0, 0.0));

  vec2 uvs = gl_FragCoord.xy * texelSize;

  uvs = 2.0 * uvs - 1.0;

  vec3 cubevec = x_axis[geom_iface_flat.fFace] * uvs.x + y_axis[geom_iface_flat.fFace] * uvs.y +
                 maj_axes[geom_iface_flat.fFace];

  FragColor = textureLod(source, cubevec, 0.0);
}
