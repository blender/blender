/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  vec4 pixel = vec4(1.0, 0.5, 0.2, 1.0);
  imageStore(img_output, ivec2(gl_GlobalInvocationID.xy), pixel);
}
