/* SPDX-FileCopyrightText: 2015-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_fullscreen_infos.hh"
#include "infos/overlay_outline_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_fullscreen)

#include "gpu_shader_fullscreen_lib.glsl"

void main()
{
  fullscreen_vertex(gl_VertexID, gl_Position, screen_uv);
}
