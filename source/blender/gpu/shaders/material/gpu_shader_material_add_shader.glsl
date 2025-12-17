/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_add_shader(Closure shader1, Closure shader2, Closure &shader)
{
  shader = closure_add(shader1, shader2);
}
