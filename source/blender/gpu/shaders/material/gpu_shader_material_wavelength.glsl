/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_wavelength(float wavelength, sampler1DArray spectrummap, float layer, out vec4 color)
{
  float t = (wavelength - 380.0f) / (780.0f - 380.0f);
  vec3 rgb = texture(spectrummap, vec2(t, layer)).rgb;
  rgb *= 1.0f / 2.52f; /* Empirical scale from lg to make all comps <= 1. */
  color = vec4(clamp(rgb, 0.0f, 1.0f), 1.0f);
}
