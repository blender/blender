/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);

  imageStore(output_img, texel, float4(float2(texel), float2(0.0f)));
}
