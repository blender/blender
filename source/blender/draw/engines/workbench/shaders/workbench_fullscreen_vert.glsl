/* SPDX-FileCopyrightText: 2015-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_depth_infos.hh"
#include "infos/workbench_effect_dof_infos.hh"

VERTEX_SHADER_CREATE_INFO(workbench_merge_depth)

#include "gpu_shader_fullscreen_lib.glsl"

void main()
{
  fullscreen_vertex(gl_VertexID, gl_Position);
}
