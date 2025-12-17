/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_shader_to_rgba(Closure cl, float4 &outcol, float &outalpha)
{
  outcol = closure_to_rgba(cl);
  outalpha = outcol.a;
}
