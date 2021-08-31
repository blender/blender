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
 * \ingroup draw
 */

#include <stdio.h>

#include "draw_manager.h"

#include "DRW_render.h"

#include "GPU_batch.h"
#include "GPU_framebuffer.h"
#include "GPU_matrix.h"
#include "GPU_texture.h"

#include "DNA_space_types.h"

#include "BKE_colortools.h"

#include "IMB_colormanagement.h"

#include "draw_color_management.h"

/* -------------------------------------------------------------------- */
/** \name Color Management
 * \{ */

void DRW_viewport_colormanagement_set(GPUViewport *viewport, DRWContextState *draw_ctx)
{
  const Scene *scene = draw_ctx->scene;
  const View3D *v3d = draw_ctx->v3d;

  const ColorManagedDisplaySettings *display_settings = &scene->display_settings;
  ColorManagedViewSettings view_settings;
  float dither = 0.0f;

  bool use_render_settings = false;
  bool use_view_transform = false;

  if (v3d) {
    bool use_workbench = BKE_scene_uses_blender_workbench(scene);

    bool use_scene_lights = (!v3d ||
                             ((v3d->shading.type == OB_MATERIAL) &&
                              (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS)) ||
                             ((v3d->shading.type == OB_RENDER) &&
                              (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS_RENDER)));
    bool use_scene_world = (!v3d ||
                            ((v3d->shading.type == OB_MATERIAL) &&
                             (v3d->shading.flag & V3D_SHADING_SCENE_WORLD)) ||
                            ((v3d->shading.type == OB_RENDER) &&
                             (v3d->shading.flag & V3D_SHADING_SCENE_WORLD_RENDER)));
    use_view_transform = v3d && (v3d->shading.type >= OB_MATERIAL);
    use_render_settings = v3d && ((use_workbench && use_view_transform) || use_scene_lights ||
                                  use_scene_world);
  }
  else if (DST.draw_ctx.space_data && DST.draw_ctx.space_data->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = (SpaceImage *)DST.draw_ctx.space_data;
    Image *image = sima->image;

    /* Use inverse logic as there isn't a setting for `Color And Alpha`. */
    const eSpaceImage_Flag display_channels_mode = static_cast<eSpaceImage_Flag>(sima->flag);
    const bool display_color_channel = (display_channels_mode & (SI_SHOW_ALPHA | SI_SHOW_ZBUF)) ==
                                       0;
    if (display_color_channel && image && (image->source != IMA_SRC_GENERATED) &&
        ((image->flag & IMA_VIEW_AS_RENDER) != 0)) {
      use_render_settings = true;
    }
  }
  else if (DST.draw_ctx.space_data && DST.draw_ctx.space_data->spacetype == SPACE_NODE) {
    SpaceNode *snode = (SpaceNode *)DST.draw_ctx.space_data;
    const eSpaceNode_Flag display_channels_mode = static_cast<eSpaceNode_Flag>(snode->flag);
    const bool display_color_channel = (display_channels_mode & SNODE_SHOW_ALPHA) == 0;
    if (display_color_channel) {
      use_render_settings = true;
    }
  }
  else {
    use_render_settings = true;
    use_view_transform = false;
  }

  if (use_render_settings) {
    /* Use full render settings, for renders with scene lighting. */
    view_settings = scene->view_settings;
    dither = scene->r.dither_intensity;
  }
  else if (use_view_transform) {
    /* Use only view transform + look and nothing else for lookdev without
     * scene lighting, as exposure depends on scene light intensity. */
    BKE_color_managed_view_settings_init_render(&view_settings, display_settings, NULL);
    STRNCPY(view_settings.view_transform, scene->view_settings.view_transform);
    STRNCPY(view_settings.look, scene->view_settings.look);
    dither = scene->r.dither_intensity;
  }
  else {
    /* For workbench use only default view transform in configuration,
     * using no scene settings. */
    BKE_color_managed_view_settings_init_render(&view_settings, display_settings, NULL);
  }

  GPU_viewport_colorspace_set(viewport, &view_settings, display_settings, dither);
}

/* Draw texture to framebuffer without any color transforms */
void DRW_transform_none(GPUTexture *tex)
{
  drw_state_set(DRW_STATE_WRITE_COLOR);

  GPU_matrix_identity_set();
  GPU_matrix_identity_projection_set();

  /* Draw as texture for final render (without immediate mode). */
  GPUBatch *geom = DRW_cache_fullscreen_quad_get();
  GPU_batch_program_set_builtin(geom, GPU_SHADER_2D_IMAGE_COLOR);
  GPU_batch_uniform_4f(geom, "color", 1.0f, 1.0f, 1.0f, 1.0f);
  GPU_batch_texture_bind(geom, "image", tex);

  GPU_batch_draw(geom);

  GPU_texture_unbind(tex);
}

/** \} */
