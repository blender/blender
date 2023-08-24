/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_blackbody(float temperature, sampler1DArray spectrummap, float layer, out vec4 color)
{
  float t = (temperature - 800.0) / (12000.0 - 800.0);
  color = vec4(texture(spectrummap, vec2(t, layer)).rgb, 1.0);
}
