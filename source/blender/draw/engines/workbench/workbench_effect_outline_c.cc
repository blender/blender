/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  GPUShader *sh;
  DRWShadingGroup *grp;

  if (OBJECT_OUTLINE_ENABLED(wpd)) {
    int state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL;
    DRW_PASS_CREATE(psl->outline_ps, DRWState(state));

    sh = workbench_shader_outline_get();

    grp = DRW_shgroup_create(sh, psl->outline_ps);
    DRW_shgroup_uniform_texture(grp, "objectIdBuffer", wpd->object_id_tx);
    DRW_shgroup_uniform_texture(grp, "depthBuffer", dtxl->depth);
    DRW_shgroup_uniform_block(grp, "world_data", wpd->world_ubo);
    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }
  else {
    psl->outline_ps = nullptr;
  }
}
