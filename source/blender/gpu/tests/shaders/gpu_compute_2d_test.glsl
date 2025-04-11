/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  vec4 pixel = vec4(1.0f, 0.5f, 0.2f, 1.0f);
  imageStore(img_output, ivec2(gl_GlobalInvocationID.xy), pixel);
}
