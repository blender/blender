/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_wavelength(float wavelength, sampler1DArray spectrummap, float layer, out float4 color)
{
  float t = (wavelength - 380.0f) / (780.0f - 380.0f);
  float3 rgb = texture(spectrummap, float2(t, layer)).rgb;
  rgb *= 1.0f / 2.52f; /* Empirical scale from lg to make all comps <= 1. */
  color = float4(clamp(rgb, 0.0f, 1.0f), 1.0f);
}
