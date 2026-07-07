/* SPDX-FileCopyrightText: 2015-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_vfx_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpencil_fx_common)

#include "gpu_shader_fullscreen_lib.glsl"

void main()
{
  fullscreen_vertex(gl_VertexID, gl_Position);
}
