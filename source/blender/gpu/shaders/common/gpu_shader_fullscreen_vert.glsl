/* SPDX-FileCopyrightText: 2015-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_fullscreen_infos.hh"
#include "gpu_shader_sequencer_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_fullscreen)

#include "gpu_shader_fullscreen_lib.glsl"

void main()
{
  fullscreen_vertex(gl_VertexID, gl_Position, screen_uv);
}
