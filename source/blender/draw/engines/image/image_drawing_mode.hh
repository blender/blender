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
 * Copyright 2021, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "image_private.hh"

namespace blender::draw::image_engine {

class DefaultDrawingMode : public AbstractDrawingMode {
 private:
  DRWPass *create_image_pass() const
  {
    /* Write depth is needed for background overlay rendering. Near depth is used for
     * transparency checker and Far depth is used for indicating the image size. */
    DRWState state = static_cast<DRWState>(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                           DRW_STATE_DEPTH_ALWAYS | DRW_STATE_BLEND_ALPHA_PREMUL);
    return DRW_pass_create("Image", state);
  }

  void add_to_shgroup(AbstractSpaceAccessor *space,
                      DRWShadingGroup *grp,
                      const Image *image,
                      const ImBuf *image_buffer) const
  {
    float image_mat[4][4];

    const DRWContextState *draw_ctx = DRW_context_state_get();
    const ARegion *region = draw_ctx->region;
    space->get_image_mat(image_buffer, region, image_mat);

    GPUBatch *geom = DRW_cache_quad_get();

    const bool is_tiled_texture = image && image->source == IMA_SRC_TILED;
    if (is_tiled_texture) {
      const float translate_x = image_mat[3][0];
      const float translate_y = image_mat[3][1];
      LISTBASE_FOREACH (ImageTile *, tile, &image->tiles) {
        const int tile_x = ((tile->tile_number - 1001) % 10);
        const int tile_y = ((tile->tile_number - 1001) / 10);
        image_mat[3][0] = (float)tile_x + translate_x;
        image_mat[3][1] = (float)tile_y + translate_y;
        DRW_shgroup_call_obmat(grp, geom, image_mat);
      }
    }
    else {
      DRW_shgroup_call_obmat(grp, geom, image_mat);
    }
  }

 public:
  void cache_init(IMAGE_Data *vedata) const override
  {
    IMAGE_PassList *psl = vedata->psl;

    psl->image_pass = create_image_pass();
  }

  void cache_image(AbstractSpaceAccessor *space,
                   IMAGE_Data *vedata,
                   Image *image,
                   ImageUser *iuser,
                   ImBuf *image_buffer) const override
  {
    IMAGE_PassList *psl = vedata->psl;
    IMAGE_StorageList *stl = vedata->stl;
    IMAGE_PrivateData *pd = stl->pd;

    GPUTexture *tex_tile_data = nullptr;
    space->get_gpu_textures(
        image, iuser, image_buffer, &pd->texture, &pd->owns_texture, &tex_tile_data);
    if (pd->texture == nullptr) {
      return;
    }
    const bool is_tiled_texture = tex_tile_data != nullptr;

    ShaderParameters sh_params;
    sh_params.use_premul_alpha = BKE_image_has_gpu_texture_premultiplied_alpha(image,
                                                                               image_buffer);
    const DRWContextState *draw_ctx = DRW_context_state_get();
    const Scene *scene = draw_ctx->scene;
    if (scene->camera && scene->camera->type == OB_CAMERA) {
      Camera *camera = static_cast<Camera *>(scene->camera->data);
      copy_v2_fl2(sh_params.far_near, camera->clip_end, camera->clip_start);
    }
    space->get_shader_parameters(sh_params, image_buffer, is_tiled_texture);

    GPUShader *shader = IMAGE_shader_image_get(is_tiled_texture);
    DRWShadingGroup *shgrp = DRW_shgroup_create(shader, psl->image_pass);
    if (is_tiled_texture) {
      DRW_shgroup_uniform_texture_ex(shgrp, "imageTileArray", pd->texture, GPU_SAMPLER_DEFAULT);
      DRW_shgroup_uniform_texture(shgrp, "imageTileData", tex_tile_data);
    }
    else {
      DRW_shgroup_uniform_texture_ex(shgrp, "imageTexture", pd->texture, GPU_SAMPLER_DEFAULT);
    }
    DRW_shgroup_uniform_vec2_copy(shgrp, "farNearDistances", sh_params.far_near);
    DRW_shgroup_uniform_vec4_copy(shgrp, "color", ShaderParameters::color);
    DRW_shgroup_uniform_vec4_copy(shgrp, "shuffle", sh_params.shuffle);
    DRW_shgroup_uniform_int_copy(shgrp, "drawFlags", sh_params.flags);
    DRW_shgroup_uniform_bool_copy(shgrp, "imgPremultiplied", sh_params.use_premul_alpha);

    add_to_shgroup(space, shgrp, image, image_buffer);
  }

  void draw_finish(IMAGE_Data *vedata) const override
  {
    IMAGE_StorageList *stl = vedata->stl;
    IMAGE_PrivateData *pd = stl->pd;

    if (pd->texture && pd->owns_texture) {
      GPU_texture_free(pd->texture);
      pd->owns_texture = false;
    }
    pd->texture = nullptr;
  }

  void draw_scene(IMAGE_Data *vedata) const override
  {
    IMAGE_PassList *psl = vedata->psl;
    IMAGE_PrivateData *pd = vedata->stl->pd;

    DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
    GPU_framebuffer_bind(dfbl->default_fb);
    static float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_framebuffer_clear_color_depth(dfbl->default_fb, clear_col, 1.0);

    DRW_view_set_active(pd->view);
    DRW_draw_pass(psl->image_pass);
    DRW_view_set_active(nullptr);
  }
};

}  // namespace blender::draw::image_engine
