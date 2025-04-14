/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  float4 pixel = float4(1.0f, 0.5f, 0.2f, 1.0f);
  imageStore(img_output, int2(gl_GlobalInvocationID.xy), pixel);
}
