/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_add_shader(inout Closure shader1, inout Closure shader2, out Closure shader)
{
  shader = closure_add(shader1, shader2);
}
