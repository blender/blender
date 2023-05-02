/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DNA_camera_types.h"
#include "DNA_screen_types.h"

#include "DEG_depsgraph_query.h"

#include "ED_image.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "overlay_private.hh"

BLI_STATIC_ASSERT(SI_GRID_STEPS_LEN == OVERLAY_GRID_STEPS_LEN, "")

void OVERLAY_grid_init(OVERLAY_Data *vedata)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  OVERLAY_GridData *grid = &pd->grid_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  float *grid_axes = pd->grid.grid_axes;
  float *zplane_axes = pd->grid.zplane_axes;
  float grid_steps[SI_GRID_STEPS_LEN] = {
      0.001f, 0.01f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f};
  float grid_steps_y[SI_GRID_STEPS_LEN] = {0.0f}; /* When zero, use value from grid_steps. */
  OVERLAY_GridBits grid_flag = OVERLAY_GridBits(0), zneg_flag = OVERLAY_GridBits(0),
                   zpos_flag = OVERLAY_GridBits(0);
  grid->line_size = max_ff(0.0f, U.pixelsize - 1.0f) * 0.5f;
  /* Default, nothing is drawn. */
  pd->grid.grid_flag = pd->grid.zneg_flag = pd->grid.zpos_flag = OVERLAY_GridBits(0);

  if (pd->space_type == SPACE_IMAGE) {
    SpaceImage *sima = (SpaceImage *)draw_ctx->space_data;
    View2D *v2d = &draw_ctx->region->v2d;

    /* Only UV Edit mode has the various Overlay options for now. */
    const bool is_uv_edit = sima->mode == SI_MODE_UV;

    const bool background_enabled = is_uv_edit ? (!pd->hide_overlays &&
                                                  (sima->overlay.flag &
                                                   SI_OVERLAY_SHOW_GRID_BACKGROUND) != 0) :
                                                 true;
    if (background_enabled) {
      grid_flag = GRID_BACK | PLANE_IMAGE;
      if (sima->flag & SI_GRID_OVER_IMAGE) {
        grid_flag = PLANE_IMAGE;
      }
    }

    const bool draw_grid = is_uv_edit || !ED_space_image_has_buffer(sima);
    if (background_enabled && draw_grid) {
      grid_flag |= SHOW_GRID;
      if (is_uv_edit) {
        if (sima->grid_shape_source != SI_GRID_SHAPE_DYNAMIC) {
          grid_flag |= CUSTOM_GRID;
        }
      }
    }

    grid->distance = 1.0f;
    copy_v3_fl3(grid->size, 1.0f, 1.0f, 1.0f);
    if (is_uv_edit) {
      grid->size[0] = float(sima->tile_grid_shape[0]);
      grid->size[1] = float(sima->tile_grid_shape[1]);
    }

    grid->zoom_factor = ED_space_image_zoom_level(v2d, SI_GRID_STEPS_LEN);
    ED_space_image_grid_steps(sima, grid_steps, grid_steps_y, SI_GRID_STEPS_LEN);
  }
  else {
    /* SPACE_VIEW3D */
    View3D *v3d = draw_ctx->v3d;
    Scene *scene = draw_ctx->scene;
    RegionView3D *rv3d = draw_ctx->rv3d;

    const bool show_axis_x = (pd->v3d_gridflag & V3D_SHOW_X) != 0;
    const bool show_axis_y = (pd->v3d_gridflag & V3D_SHOW_Y) != 0;
    const bool show_axis_z = (pd->v3d_gridflag & V3D_SHOW_Z) != 0;
    const bool show_floor = (pd->v3d_gridflag & V3D_SHOW_FLOOR) != 0;
    const bool show_ortho_grid = (pd->v3d_gridflag & V3D_SHOW_ORTHO_GRID) != 0;

    if (pd->hide_overlays || !(pd->v3d_gridflag & (V3D_SHOW_X | V3D_SHOW_Y | V3D_SHOW_Z |
                                                   V3D_SHOW_FLOOR | V3D_SHOW_ORTHO_GRID)))
    {
      return;
    }

    float viewinv[4][4], wininv[4][4];
    float viewmat[4][4], winmat[4][4];
    DRW_view_winmat_get(nullptr, winmat, false);
    DRW_view_winmat_get(nullptr, wininv, true);
    DRW_view_viewmat_get(nullptr, viewmat, false);
    DRW_view_viewmat_get(nullptr, viewinv, true);

    /* If perspective view or non-axis aligned view. */
    if (winmat[3][3] == 0.0f || rv3d->view == RV3D_VIEW_USER) {
      if (show_axis_x) {
        grid_flag |= PLANE_XY | SHOW_AXIS_X;
      }
      if (show_axis_y) {
        grid_flag |= PLANE_XY | SHOW_AXIS_Y;
      }
      if (show_floor) {
        grid_flag |= PLANE_XY | SHOW_GRID;
      }
    }
    else {
      if (show_ortho_grid && ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT)) {
        grid_flag = PLANE_YZ | SHOW_AXIS_Y | SHOW_AXIS_Z | SHOW_GRID | GRID_BACK;
      }
      else if (show_ortho_grid && ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM)) {
        grid_flag = PLANE_XY | SHOW_AXIS_X | SHOW_AXIS_Y | SHOW_GRID | GRID_BACK;
      }
      else if (show_ortho_grid && ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK)) {
        grid_flag = PLANE_XZ | SHOW_AXIS_X | SHOW_AXIS_Z | SHOW_GRID | GRID_BACK;
      }
    }

    grid_axes[0] = float((grid_flag & (PLANE_XZ | PLANE_XY)) != 0);
    grid_axes[1] = float((grid_flag & (PLANE_YZ | PLANE_XY)) != 0);
    grid_axes[2] = float((grid_flag & (PLANE_YZ | PLANE_XZ)) != 0);

    /* Z axis if needed */
    if (((rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO)) && show_axis_z) {
      zpos_flag = SHOW_AXIS_Z;

      float zvec[3], campos[3];
      negate_v3_v3(zvec, viewinv[2]);
      copy_v3_v3(campos, viewinv[3]);

      /* z axis : chose the most facing plane */
      if (fabsf(zvec[0]) < fabsf(zvec[1])) {
        zpos_flag |= PLANE_XZ;
      }
      else {
        zpos_flag |= PLANE_YZ;
      }

      zneg_flag = zpos_flag;

      /* Perspective: If camera is below floor plane, we switch clipping.
       * Orthographic: If eye vector is looking up, we switch clipping. */
      if (((winmat[3][3] == 0.0f) && (campos[2] > 0.0f)) ||
          ((winmat[3][3] != 0.0f) && (zvec[2] < 0.0f))) {
        zpos_flag |= CLIP_ZPOS;
        zneg_flag |= CLIP_ZNEG;
      }
      else {
        zpos_flag |= CLIP_ZNEG;
        zneg_flag |= CLIP_ZPOS;
      }

      zplane_axes[0] = float((zpos_flag & (PLANE_XZ | PLANE_XY)) != 0);
      zplane_axes[1] = float((zpos_flag & (PLANE_YZ | PLANE_XY)) != 0);
      zplane_axes[2] = float((zpos_flag & (PLANE_YZ | PLANE_XZ)) != 0);
    }
    else {
      zneg_flag = zpos_flag = CLIP_ZNEG | CLIP_ZPOS;
    }

    float dist;
    if (rv3d->persp == RV3D_CAMOB && v3d->camera && v3d->camera->type == OB_CAMERA) {
      Object *camera_object = DEG_get_evaluated_object(draw_ctx->depsgraph, v3d->camera);
      dist = ((Camera *)(camera_object->data))->clip_end;
      grid_flag |= GRID_CAMERA;
      zneg_flag |= GRID_CAMERA;
      zpos_flag |= GRID_CAMERA;
    }
    else {
      dist = v3d->clip_end;
    }

    if (winmat[3][3] == 0.0f) {
      copy_v3_fl(grid->size, dist);
    }
    else {
      float viewdist = 1.0f / min_ff(fabsf(winmat[0][0]), fabsf(winmat[1][1]));
      copy_v3_fl(grid->size, viewdist * dist);
    }

    grid->distance = dist / 2.0f;

    ED_view3d_grid_steps(scene, v3d, rv3d, grid_steps);

    if ((v3d->flag & (V3D_XR_SESSION_SURFACE | V3D_XR_SESSION_MIRROR)) != 0) {
      /* The calculations for the grid parameters assume that the view matrix has no scale
       * component, which may not be correct if the user is "shrunk" or "enlarged" by zooming in or
       * out. Therefore, we need to compensate the values here. */
      /* Assumption is uniform scaling (all column vectors are of same length). */
      float viewinvscale = len_v3(viewinv[0]);
      grid->distance *= viewinvscale;
    }
  }

  /* Convert to UBO alignment. */
  for (int i = 0; i < SI_GRID_STEPS_LEN; i++) {
    grid->steps[i][0] = grid_steps[i];
    grid->steps[i][1] = (grid_steps_y[i] != 0.0f) ? grid_steps_y[i] : grid_steps[i];
  }
  pd->grid.grid_flag = grid_flag;
  pd->grid.zneg_flag = zneg_flag;
  pd->grid.zpos_flag = zpos_flag;
}

void OVERLAY_grid_cache_init(OVERLAY_Data *ved)
{
  OVERLAY_StorageList *stl = ved->stl;
  OVERLAY_PrivateData *pd = stl->pd;
  OVERLAY_GridData *grid = &pd->grid_data;

  OVERLAY_PassList *psl = ved->psl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  psl->grid_ps = nullptr;

  if ((pd->grid.grid_flag == 0 && pd->grid.zpos_flag == 0) || !DRW_state_is_fbo()) {
    return;
  }

  if (ved->instance->grid_ubo == nullptr) {
    ved->instance->grid_ubo = GPU_uniformbuf_create(sizeof(OVERLAY_GridData));
  }
  GPU_uniformbuf_update(ved->instance->grid_ubo, &pd->grid_data);

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
  DRW_PASS_CREATE(psl->grid_ps, state);

  if (pd->space_type == SPACE_IMAGE) {
    float mat[4][4];

    /* Add quad background. */
    GPUShader *sh = OVERLAY_shader_grid_background();
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->grid_ps);
    float color_back[4];
    interp_v4_v4v4(color_back, G_draw.block.color_background, G_draw.block.color_grid, 0.5);
    DRW_shgroup_uniform_vec4_copy(grp, "ucolor", color_back);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
    unit_m4(mat);
    mat[0][0] = grid->size[0];
    mat[1][1] = grid->size[1];
    mat[2][2] = grid->size[2];
    DRW_shgroup_call_obmat(grp, DRW_cache_quad_get(), mat);
  }

  {
    DRWShadingGroup *grp;
    struct GPUBatch *geom = DRW_cache_grid_get();

    GPUShader *sh = OVERLAY_shader_grid();

    /* Create 3 quads to render ordered transparency Z axis */
    grp = DRW_shgroup_create(sh, psl->grid_ps);
    DRW_shgroup_uniform_block(grp, "grid_buf", ved->instance->grid_ubo);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_texture_ref(grp, "depth_tx", &dtxl->depth);

    DRW_shgroup_uniform_int_copy(grp, "grid_flag", pd->grid.zneg_flag);
    DRW_shgroup_uniform_vec3_copy(grp, "plane_axes", pd->grid.zplane_axes);
    if (pd->grid.zneg_flag & SHOW_AXIS_Z) {
      DRW_shgroup_call(grp, geom, nullptr);
    }

    grp = DRW_shgroup_create_sub(grp);
    DRW_shgroup_uniform_int_copy(grp, "grid_flag", pd->grid.grid_flag);
    DRW_shgroup_uniform_vec3_copy(grp, "plane_axes", pd->grid.grid_axes);
    if (pd->grid.grid_flag) {
      DRW_shgroup_call(grp, geom, nullptr);
    }

    grp = DRW_shgroup_create_sub(grp);
    DRW_shgroup_uniform_int_copy(grp, "grid_flag", pd->grid.zpos_flag);
    DRW_shgroup_uniform_vec3_copy(grp, "plane_axes", pd->grid.zplane_axes);
    if (pd->grid.zpos_flag & SHOW_AXIS_Z) {
      DRW_shgroup_call(grp, geom, nullptr);
    }
  }

  if (pd->space_type == SPACE_IMAGE) {
    float theme_color[4];
    UI_GetThemeColorShade4fv(TH_BACK, 60, theme_color);
    srgb_to_linearrgb_v4(theme_color, theme_color);

    float mat[4][4];
    /* add wire border */
    GPUShader *sh = OVERLAY_shader_grid_image();
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->grid_ps);
    DRW_shgroup_uniform_vec4_copy(grp, "ucolor", theme_color);
    unit_m4(mat);
    for (int x = 0; x < grid->size[0]; x++) {
      mat[3][0] = x;
      for (int y = 0; y < grid->size[1]; y++) {
        mat[3][1] = y;
        DRW_shgroup_call_obmat(grp, DRW_cache_quad_wires_get(), mat);
      }
    }
  }
}

void OVERLAY_grid_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (psl->grid_ps) {
    DRW_draw_pass(psl->grid_ps);
  }
}
