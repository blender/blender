/* SPDX-FileCopyrightText: 2017-2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

vec2 mapping_octahedron(vec3 cubevec, vec2 texel_size)
{
  /* projection onto octahedron */
  cubevec /= dot(vec3(1.0), abs(cubevec));

  /* out-folding of the downward faces */
  if (cubevec.z < 0.0) {
    vec2 cubevec_sign = step(0.0, cubevec.xy) * 2.0 - 1.0;
    cubevec.xy = (1.0 - abs(cubevec.yx)) * cubevec_sign;
  }

  /* mapping to [0;1]ˆ2 texture space */
  vec2 uvs = cubevec.xy * (0.5) + 0.5;

  /* edge filtering fix */
  uvs = (1.0 - 2.0 * texel_size) * uvs + texel_size;

  return uvs;
}
