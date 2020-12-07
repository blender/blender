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

#include "UI_resources.h"

#include "draw_manager_text.h"
#include "overlay_private.h"

#define BG_SOLID 0
#define BG_GRADIENT 1
#define BG_CHECKER 2
#define BG_RADIAL 3
#define BG_SOLID_CHECKER 4
#define BG_MASK 5

void OVERLAY_background_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;
  const RegionView3D *rv3d = draw_ctx->rv3d;
  const BoundBox *bb = rv3d ? rv3d->clipbb : NULL;
  const View3D *v3d = draw_ctx->v3d;
  bool draw_clipping_bounds = (pd->clipping_state != 0);

  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_BACKGROUND;
    float color_override[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int background_type;

    if (DRW_state_is_opengl_render() && !DRW_state_draw_background()) {
      background_type = BG_SOLID;
      color_override[3] = 1.0f;
    }
    else if (pd->space_type == SPACE_IMAGE) {
      background_type = BG_SOLID_CHECKER;
    }
    else if (pd->space_type == SPACE_NODE) {
      background_type = BG_MASK;
      state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_MUL;
    }
    else if (!DRW_state_draw_background()) {
      background_type = BG_CHECKER;
    }
    else if (v3d->shading.background_type == V3D_SHADING_BACKGROUND_WORLD && scene->world) {
      background_type = BG_SOLID;
      /* TODO(fclem): this is a scene referred linear color. we should convert
       * it to display linear here. */
      copy_v3_v3(color_override, &scene->world->horr);
      color_override[3] = 1.0f;
    }
    else if (v3d->shading.background_type == V3D_SHADING_BACKGROUND_VIEWPORT &&
             v3d->shading.type <= OB_SOLID) {
      background_type = BG_SOLID;
      copy_v3_v3(color_override, v3d->shading.background_color);
      color_override[3] = 1.0f;
    }
    else {
      switch (UI_GetThemeValue(TH_BACKGROUND_TYPE)) {
        case TH_BACKGROUND_GRADIENT_LINEAR:
          background_type = BG_GRADIENT;
          break;
        case TH_BACKGROUND_GRADIENT_RADIAL:
          background_type = BG_RADIAL;
          break;
        default:
        case TH_BACKGROUND_SINGLE_COLOR:
          background_type = BG_SOLID;
          break;
      }
    }

    DRW_PASS_CREATE(psl->background_ps, state);

    GPUShader *sh = OVERLAY_shader_background();
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->background_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_texture_ref(grp, "colorBuffer", &dtxl->color);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
    DRW_shgroup_uniform_vec4_copy(grp, "colorOverride", color_override);
    DRW_shgroup_uniform_int_copy(grp, "bgType", background_type);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }

  if (draw_clipping_bounds) {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA | DRW_STATE_CULL_BACK;
    DRW_PASS_CREATE(psl->clipping_frustum_ps, state);

    GPUShader *sh = OVERLAY_shader_clipbound();
    DRWShadingGroup *grp = DRW_shgroup_create(sh, psl->clipping_frustum_ps);
    DRW_shgroup_uniform_vec4_copy(grp, "color", G_draw.block.colorClippingBorder);
    DRW_shgroup_uniform_vec3(grp, "boundbox", &bb->vec[0][0], 8);

    struct GPUBatch *cube = DRW_cache_cube_get();
    DRW_shgroup_call(grp, cube, NULL);
  }
  else {
    psl->clipping_frustum_ps = NULL;
  }
}

void OVERLAY_background_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (DRW_state_is_fbo()) {
    if (psl->clipping_frustum_ps) {
      DRW_draw_pass(psl->clipping_frustum_ps);
    }

    DRW_draw_pass(psl->background_ps);
  }
}
