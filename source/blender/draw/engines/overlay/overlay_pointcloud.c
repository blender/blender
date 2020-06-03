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
 */

#include "DRW_render.h"

#include "DEG_depsgraph_query.h"

#include "DNA_pointcloud_types.h"

#include "BKE_pointcache.h"

#include "overlay_private.h"

/* -------------------------------------------------------------------- */
/** \name PointCloud
 * \{ */

void OVERLAY_pointcloud_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  GPUShader *sh;
  DRWShadingGroup *grp;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
  DRW_PASS_CREATE(psl->pointcloud_ps, state | pd->clipping_state);

  sh = OVERLAY_shader_pointcloud_dot();
  pd->pointcloud_dots_grp = grp = DRW_shgroup_create(sh, psl->pointcloud_ps);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
}

void OVERLAY_pointcloud_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  struct GPUBatch *geom = DRW_cache_pointcloud_get_dots(ob);

  const float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  DRWShadingGroup *grp = DRW_shgroup_create_sub(pd->pointcloud_dots_grp);
  DRW_shgroup_uniform_vec4_copy(grp, "color", color);
  DRW_shgroup_call(grp, geom, ob);
}

void OVERLAY_pointcloud_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->pointcloud_ps);
}

/** \} */
