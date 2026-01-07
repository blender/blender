/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

[[node]]
void node_mix_shader(float fac, Closure shader1, Closure shader2, Closure &shader)
{
  shader = closure_mix(shader1, shader2, fac);
}
