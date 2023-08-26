/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* This shader is used to add default values to the volume accumulate textures.
 * so it looks similar (transmittance = 1, scattering = 0). */
void main()
{
  FragColor0 = vec4(0.0);
  FragColor1 = vec4(1.0);
}
