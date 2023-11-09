/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  vec2 texel_size = 1.0 / vec2(textureSize(colorBuffer, 0));
  vec2 uv = gl_FragCoord.xy * texel_size;

  fragColor = vec4(0.0);
  int i = 0;
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++, i++) {
      vec4 color = texture(colorBuffer, uv + vec2(x, y) * texel_size);
      /* Clamp infinite inputs (See #112211). */
      color = clamp(color, vec4(0.0), vec4(1e10));
      /* Use log2 space to avoid highlights creating too much aliasing. */
      color = log2(color + 0.5);

      fragColor += color * samplesWeights[i];
    }
  }
}
