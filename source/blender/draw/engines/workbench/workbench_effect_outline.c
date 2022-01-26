/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2020, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 *
 * Outline Effect:
 *
 * Simple effect that just samples an object id buffer to detect objects outlines.
 */

#include "DRW_render.h"

#include "workbench_engine.h"
#include "workbench_private.h"

void workbench_outline_cache_init(WORKBENCH_Data *data)
{
  WORKBENCH_PassList *psl = data->psl;
  WORKBENCH_PrivateData *wpd = data->stl->wpd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  struct GPUShader *sh;
  DRWShadingGroup *grp;

  if (OBJECT_OUTLINE_ENABLED(wpd)) {
    int state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL;
    DRW_PASS_CREATE(psl->outline_ps, state);

    sh = workbench_shader_outline_get();

    grp = DRW_shgroup_create(sh, psl->outline_ps);
    DRW_shgroup_uniform_texture(grp, "objectIdBuffer", wpd->object_id_tx);
    DRW_shgroup_uniform_texture(grp, "depthBuffer", dtxl->depth);
    DRW_shgroup_uniform_block(grp, "world_data", wpd->world_ubo);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
  else {
    psl->outline_ps = NULL;
  }
}
