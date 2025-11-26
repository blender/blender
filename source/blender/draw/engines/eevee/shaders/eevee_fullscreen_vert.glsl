/* SPDX-FileCopyrightText: 2015-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_deferred_infos.hh"
#include "infos/eevee_film_infos.hh"
#include "infos/eevee_forward_infos.hh"
#include "infos/eevee_fullscreen_infos.hh"
#include "infos/eevee_light_culling_infos.hh"

VERTEX_SHADER_CREATE_INFO(eevee_fullscreen)

#include "gpu_shader_fullscreen_lib.glsl"

void main()
{
  fullscreen_vertex(gl_VertexID, gl_Position, screen_uv);
}
