/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  imageStore(output_img, texel, vec4(vec2(texel) + vec2(0.5f), vec2(0.0f)));
}
