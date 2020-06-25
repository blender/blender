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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DNA_camera_types.h"

#include "DEG_depsgraph_query.h"

#include "ED_view3d.h"

#include "overlay_private.h"

enum {
  SHOW_AXIS_X = (1 << 0),
  SHOW_AXIS_Y = (1 << 1),
  SHOW_AXIS_Z = (1 << 2),
  SHOW_GRID = (1 << 3),
  PLANE_XY = (1 << 4),
  PLANE_XZ = (1 << 5),
  PLANE_YZ = (1 << 6),
  CLIP_ZPOS = (1 << 7),
  CLIP_ZNEG = (1 << 8),
  GRID_BACK = (1 << 9),
  GRID_CAMERA = (1 << 10),
};

void OVERLAY_grid_init(OVERLAY_Data *vedata)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  OVERLAY_ShadingData *shd = &pd->shdata;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  Scene *scene = draw_ctx->scene;
  RegionView3D *rv3d = draw_ctx->rv3d;

  const bool show_axis_x = (pd->v3d_gridflag & V3D_SHOW_X) != 0;
  const bool show_axis_y = (pd->v3d_gridflag & V3D_SHOW_Y) != 0;
  const bool show_axis_z = (pd->v3d_gridflag & V3D_SHOW_Z) != 0;
  const bool show_floor = (pd->v3d_gridflag & V3D_SHOW_FLOOR) != 0;
  const bool show_ortho_grid = (pd->v3d_gridflag & V3D_SHOW_ORTHO_GRID) != 0;

  shd->grid_flag = 0;
  shd->zneg_flag = 0;
  shd->zpos_flag = 0;

  if (pd->hide_overlays || !(pd->v3d_gridflag & (V3D_SHOW_X | V3D_SHOW_Y | V3D_SHOW_Z |
                                                 V3D_SHOW_FLOOR | V3D_SHOW_ORTHO_GRID))) {
    return;
  }

  float viewinv[4][4], wininv[4][4];
  float viewmat[4][4], winmat[4][4];
  DRW_view_winmat_get(NULL, winmat, false);
  DRW_view_winmat_get(NULL, wininv, true);
  DRW_view_viewmat_get(NULL, viewmat, false);
  DRW_view_viewmat_get(NULL, viewinv, true);

  /* if perps */
  if (winmat[3][3] == 0.0f || rv3d->view == RV3D_VIEW_USER) {
    if (show_axis_x) {
      shd->grid_flag |= PLANE_XY | SHOW_AXIS_X;
    }
    if (show_axis_y) {
      shd->grid_flag |= PLANE_XY | SHOW_AXIS_Y;
    }
    if (show_floor) {
      shd->grid_flag |= PLANE_XY | SHOW_GRID;
    }
  }
  else {
    if (show_ortho_grid && ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT)) {
      shd->grid_flag = PLANE_YZ | SHOW_AXIS_Y | SHOW_AXIS_Z | SHOW_GRID | GRID_BACK;
    }
    else if (show_ortho_grid && ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
      shd->grid_flag = PLANE_XY | SHOW_AXIS_X | SHOW_AXIS_Y | SHOW_GRID | GRID_BACK;
    }
    else if (show_ortho_grid && ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK)) {
      shd->grid_flag = PLANE_XZ | SHOW_AXIS_X | SHOW_AXIS_Z | SHOW_GRID | GRID_BACK;
    }
  }

  shd->grid_axes[0] = (float)((shd->grid_flag & (PLANE_XZ | PLANE_XY)) != 0);
  shd->grid_axes[1] = (float)((shd->grid_flag & (PLANE_YZ | PLANE_XY)) != 0);
  shd->grid_axes[2] = (float)((shd->grid_flag & (PLANE_YZ | PLANE_XZ)) != 0);

  /* Z axis if needed */
  if (((rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO)) && show_axis_z) {
    shd->zpos_flag = SHOW_AXIS_Z;

    float zvec[3], campos[3];
    negate_v3_v3(zvec, viewinv[2]);
    copy_v3_v3(campos, viewinv[3]);

    /* z axis : chose the most facing plane */
    if (fabsf(zvec[0]) < fabsf(zvec[1])) {
      shd->zpos_flag |= PLANE_XZ;
    }
    else {
      shd->zpos_flag |= PLANE_YZ;
    }

    shd->zneg_flag = shd->zpos_flag;

    /* Persp : If camera is below floor plane, we switch clipping
     * Ortho : If eye vector is looking up, we switch clipping */
    if (((winmat[3][3] == 0.0f) && (campos[2] > 0.0f)) ||
        ((winmat[3][3] != 0.0f) && (zvec[2] < 0.0f))) {
      shd->zpos_flag |= CLIP_ZPOS;
      shd->zneg_flag |= CLIP_ZNEG;
    }
    else {
      shd->zpos_flag |= CLIP_ZNEG;
      shd->zneg_flag |= CLIP_ZPOS;
    }

    shd->zplane_axes[0] = (float)((shd->zpos_flag & (PLANE_XZ | PLANE_XY)) != 0);
    shd->zplane_axes[1] = (float)((shd->zpos_flag & (PLANE_YZ | PLANE_XY)) != 0);
    shd->zplane_axes[2] = (float)((shd->zpos_flag & (PLANE_YZ | PLANE_XZ)) != 0);
  }
  else {
    shd->zneg_flag = shd->zpos_flag = CLIP_ZNEG | CLIP_ZPOS;
  }

  float dist;
  if (rv3d->persp == RV3D_CAMOB && v3d->camera && v3d->camera->type == OB_CAMERA) {
    Object *camera_object = DEG_get_evaluated_object(draw_ctx->depsgraph, v3d->camera);
    dist = ((Camera *)(camera_object->data))->clip_end;
    shd->grid_flag |= GRID_CAMERA;
    shd->zneg_flag |= GRID_CAMERA;
    shd->zpos_flag |= GRID_CAMERA;
  }
  else {
    dist = v3d->clip_end;
  }

  if (winmat[3][3] == 0.0f) {
    shd->grid_mesh_size = dist;
  }
  else {
    float viewdist = 1.0f / min_ff(fabsf(winmat[0][0]), fabsf(winmat[1][1]));
    shd->grid_mesh_size = viewdist * dist;
  }

  shd->grid_distance = dist / 2.0f;
  shd->grid_line_size = max_ff(0.0f, U.pixelsize - 1.0f) * 0.5f;

  ED_view3d_grid_steps(scene, v3d, rv3d, shd->grid_steps);
}

void OVERLAY_grid_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_ShadingData *shd = &vedata->stl->pd->shdata;
  OVERLAY_PassList *psl = vedata->psl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  psl->grid_ps = NULL;

  if ((shd->grid_flag == 0 && shd->zpos_flag == 0) || !DRW_state_is_fbo()) {
    return;
  }

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
  DRW_PASS_CREATE(psl->grid_ps, state);

  GPUShader *sh = OVERLAY_shader_grid();
  struct GPUBatch *geom = DRW_cache_grid_get();

  /* Create 3 quads to render ordered transparency Z axis */
  DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->grid_ps);
  DRW_shgroup_uniform_int(grp, "gridFlag", &shd->zneg_flag, 1);
  DRW_shgroup_uniform_vec3(grp, "planeAxes", shd->zplane_axes, 1);
  DRW_shgroup_uniform_float(grp, "gridDistance", &shd->grid_distance, 1);
  DRW_shgroup_uniform_float_copy(grp, "lineKernel", shd->grid_line_size);
  DRW_shgroup_uniform_float_copy(grp, "meshSize", shd->grid_mesh_size);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
  if (shd->zneg_flag) {
    DRW_shgroup_call(grp, geom, NULL);
  }

  grp = DRW_shgroup_create(sh, psl->grid_ps);
  DRW_shgroup_uniform_int(grp, "gridFlag", &shd->grid_flag, 1);
  DRW_shgroup_uniform_vec3(grp, "planeAxes", shd->grid_axes, 1);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
  DRW_shgroup_uniform_float(grp, "gridSteps", shd->grid_steps, ARRAY_SIZE(shd->grid_steps));
  if (shd->grid_flag) {
    DRW_shgroup_call(grp, geom, NULL);
  }

  grp = DRW_shgroup_create(sh, psl->grid_ps);
  DRW_shgroup_uniform_int(grp, "gridFlag", &shd->zpos_flag, 1);
  DRW_shgroup_uniform_vec3(grp, "planeAxes", shd->zplane_axes, 1);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
  if (shd->zpos_flag) {
    DRW_shgroup_call(grp, geom, NULL);
  }
}

void OVERLAY_grid_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (psl->grid_ps) {
    DRW_draw_pass(psl->grid_ps);
  }
}
