/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_wavelength(float wavelength, sampler1DArray spectrummap, float layer, out vec4 color)
{
  float t = (wavelength - 380.0) / (780.0 - 380.0);
  vec3 rgb = texture(spectrummap, vec2(t, layer)).rgb;
  rgb *= 1.0 / 2.52; /* Empirical scale from lg to make all comps <= 1. */
  color = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
