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
 * \ingroup draw_editors
 *
 * Draw engine to draw the Image/UV editor
 */

#include "DRW_render.h"

#include "BKE_image.h"
#include "BKE_object.h"

#include "DNA_camera_types.h"

#include "IMB_imbuf_types.h"

#include "ED_image.h"

#include "GPU_batch.h"

#include "image_engine.h"
#include "image_private.h"

#define SIMA_DRAW_FLAG_SHOW_ALPHA (1 << 0)
#define SIMA_DRAW_FLAG_APPLY_ALPHA (1 << 1)
#define SIMA_DRAW_FLAG_SHUFFLING (1 << 2)
#define SIMA_DRAW_FLAG_DEPTH (1 << 3)
#define SIMA_DRAW_FLAG_DO_REPEAT (1 << 4)

static void image_cache_image_add(DRWShadingGroup *grp, Image *image)
{
  const bool is_tiled_texture = image && image->source == IMA_SRC_TILED;
  float obmat[4][4];
  unit_m4(obmat);

  GPUBatch *geom = DRW_cache_quad_get();

  if (is_tiled_texture) {
    LISTBASE_FOREACH (ImageTile *, tile, &image->tiles) {
      const int tile_x = ((tile->tile_number - 1001) % 10);
      const int tile_y = ((tile->tile_number - 1001) / 10);
      obmat[3][1] = (float)tile_y;
      obmat[3][0] = (float)tile_x;
      DRW_shgroup_call_obmat(grp, geom, obmat);
    }
  }
  else {
    DRW_shgroup_call_obmat(grp, geom, obmat);
  }
}

static void image_gpu_texture_get(Image *image,
                                  ImageUser *iuser,
                                  ImBuf *ibuf,
                                  GPUTexture **r_gpu_texture,
                                  bool *r_owns_texture,
                                  GPUTexture **r_tex_tile_data)
{

  const DRWContextState *draw_ctx = DRW_context_state_get();
  SpaceImage *sima = (SpaceImage *)draw_ctx->space_data;

  if (image) {
    if (BKE_image_is_multilayer(image)) {
      /* update multiindex and pass for the current eye */
      BKE_image_multilayer_index(image->rr, &sima->iuser);
    }
    else {
      BKE_image_multiview_index(image, &sima->iuser);
    }

    if (ibuf) {
      if (sima->flag & SI_SHOW_ZBUF && (ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels == 1))) {
        if (ibuf->zbuf) {
          BLI_assert(!"Integer based depth buffers not supported");
        }
        else if (ibuf->zbuf_float) {
          *r_gpu_texture = GPU_texture_create_2d(
              __func__, ibuf->x, ibuf->y, 0, GPU_R16F, ibuf->zbuf_float);
          *r_owns_texture = true;
        }
        else if (ibuf->rect_float && ibuf->channels == 1) {
          *r_gpu_texture = GPU_texture_create_2d(
              __func__, ibuf->x, ibuf->y, 0, GPU_R16F, ibuf->rect_float);
          *r_owns_texture = true;
        }
      }
      else if (image->source == IMA_SRC_TILED) {
        *r_gpu_texture = BKE_image_get_gpu_tiles(image, iuser, ibuf);
        *r_tex_tile_data = BKE_image_get_gpu_tilemap(image, iuser, NULL);
        *r_owns_texture = false;
      }
      else {
        *r_gpu_texture = BKE_image_get_gpu_texture(image, iuser, ibuf);
        *r_owns_texture = false;
      }
    }
  }
}

static void image_cache_image(IMAGE_Data *vedata, Image *image, ImageUser *iuser, ImBuf *ibuf)
{
  IMAGE_PassList *psl = vedata->psl;
  IMAGE_StorageList *stl = vedata->stl;
  IMAGE_PrivateData *pd = stl->pd;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;
  SpaceImage *sima = (SpaceImage *)draw_ctx->space_data;

  GPUTexture *tex_tile_data = NULL;
  image_gpu_texture_get(image, iuser, ibuf, &pd->texture, &pd->owns_texture, &tex_tile_data);

  if (pd->texture) {
    static float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    static float shuffle[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    static float far_near[2] = {100.0f, 0.0f};

    if (scene->camera && scene->camera->type == OB_CAMERA) {
      far_near[1] = ((Camera *)scene->camera->data)->clip_start;
      far_near[0] = ((Camera *)scene->camera->data)->clip_end;
    }

    const bool use_premul_alpha = image->alpha_mode == IMA_ALPHA_PREMUL;
    const bool is_tiled_texture = tex_tile_data != NULL;
    const bool do_repeat = (!is_tiled_texture) && ((sima->flag & SI_DRAW_TILE) != 0);
    const bool is_zoom_out = sima->zoom < 1.0f;

    /* use interpolation filtering when zooming out */
    eGPUSamplerState state = 0;
    SET_FLAG_FROM_TEST(state, is_zoom_out, GPU_SAMPLER_FILTER);

    int draw_flags = 0;
    SET_FLAG_FROM_TEST(draw_flags, do_repeat, SIMA_DRAW_FLAG_DO_REPEAT);

    if ((sima->flag & SI_USE_ALPHA) != 0) {
      /* Show RGBA */
      draw_flags |= SIMA_DRAW_FLAG_SHOW_ALPHA | SIMA_DRAW_FLAG_APPLY_ALPHA;
    }
    else if ((sima->flag & SI_SHOW_ALPHA) != 0) {
      draw_flags |= SIMA_DRAW_FLAG_SHUFFLING;
      copy_v4_fl4(shuffle, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    else if ((sima->flag & SI_SHOW_ZBUF) != 0) {
      draw_flags |= SIMA_DRAW_FLAG_DEPTH | SIMA_DRAW_FLAG_SHUFFLING;
      copy_v4_fl4(shuffle, 1.0f, 0.0f, 0.0f, 0.0f);
    }
    else if ((sima->flag & SI_SHOW_R) != 0) {
      draw_flags |= SIMA_DRAW_FLAG_APPLY_ALPHA | SIMA_DRAW_FLAG_SHUFFLING;
      copy_v4_fl4(shuffle, 1.0f, 0.0f, 0.0f, 0.0f);
    }
    else if ((sima->flag & SI_SHOW_G) != 0) {
      draw_flags |= SIMA_DRAW_FLAG_APPLY_ALPHA | SIMA_DRAW_FLAG_SHUFFLING;
      copy_v4_fl4(shuffle, 0.0f, 1.0f, 0.0f, 0.0f);
    }
    else if ((sima->flag & SI_SHOW_B) != 0) {
      draw_flags |= SIMA_DRAW_FLAG_APPLY_ALPHA | SIMA_DRAW_FLAG_SHUFFLING;
      copy_v4_fl4(shuffle, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    else /* RGB */ {
      draw_flags |= SIMA_DRAW_FLAG_APPLY_ALPHA;
    }

    GPUShader *shader = IMAGE_shader_image_get(is_tiled_texture);
    DRWShadingGroup *shgrp = DRW_shgroup_create(shader, psl->image_pass);
    if (is_tiled_texture) {
      DRW_shgroup_uniform_texture_ex(shgrp, "imageTileArray", pd->texture, state);
      DRW_shgroup_uniform_texture(shgrp, "imageTileData", tex_tile_data);
    }
    else {
      DRW_shgroup_uniform_texture_ex(shgrp, "imageTexture", pd->texture, state);
    }
    DRW_shgroup_uniform_vec2_copy(shgrp, "farNearDistances", far_near);
    DRW_shgroup_uniform_vec4_copy(shgrp, "color", color);
    DRW_shgroup_uniform_vec4_copy(shgrp, "shuffle", shuffle);
    DRW_shgroup_uniform_int_copy(shgrp, "drawFlags", draw_flags);
    DRW_shgroup_uniform_bool_copy(shgrp, "imgPremultiplied", use_premul_alpha);
    image_cache_image_add(shgrp, image);
  }
}

/* -------------------------------------------------------------------- */
/** \name Engine Callbacks
 * \{ */
static void IMAGE_engine_init(void *ved)
{
  IMAGE_shader_library_ensure();
  IMAGE_Data *vedata = (IMAGE_Data *)ved;
  IMAGE_StorageList *stl = vedata->stl;
  if (!stl->pd) {
    stl->pd = MEM_callocN(sizeof(IMAGE_PrivateData), __func__);
  }
  IMAGE_PrivateData *pd = stl->pd;

  pd->ibuf = NULL;
  pd->lock = NULL;
  pd->texture = NULL;
}

static void IMAGE_cache_init(void *ved)
{
  IMAGE_Data *vedata = (IMAGE_Data *)ved;
  IMAGE_StorageList *stl = vedata->stl;
  IMAGE_PrivateData *pd = stl->pd;
  IMAGE_PassList *psl = vedata->psl;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  SpaceImage *sima = (SpaceImage *)draw_ctx->space_data;

  {
    /* Write depth is needed for background overlay rendering. Near depth is used for
     * transparency checker and Far depth is used for indicating the image size. */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS |
                     DRW_STATE_BLEND_ALPHA_PREMUL;
    psl->image_pass = DRW_pass_create("Image", state);
  }

  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  GPU_framebuffer_bind(dfbl->default_fb);
  static float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  GPU_framebuffer_clear_color_depth(dfbl->default_fb, clear_col, 1.0);

  {
    Image *image = ED_space_image(sima);
    ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &pd->lock, 0);
    image_cache_image(vedata, image, &sima->iuser, ibuf);
    pd->ibuf = ibuf;
  }
}

static void IMAGE_cache_populate(void *UNUSED(vedata), Object *UNUSED(ob))
{
  /* Function intentional left empty. `cache_populate` is required to be implemented. */
}

static void image_draw_finish(IMAGE_Data *ved)
{
  IMAGE_Data *vedata = (IMAGE_Data *)ved;
  IMAGE_StorageList *stl = vedata->stl;
  IMAGE_PrivateData *pd = stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  SpaceImage *sima = (SpaceImage *)draw_ctx->space_data;

  ED_space_image_release_buffer(sima, pd->ibuf, pd->lock);

  if (pd->texture && pd->owns_texture) {
    GPU_texture_free(pd->texture);
    pd->owns_texture = false;
  }
  pd->texture = NULL;
}

static void IMAGE_draw_scene(void *ved)
{
  IMAGE_Data *vedata = (IMAGE_Data *)ved;
  IMAGE_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->image_pass);

  image_draw_finish(vedata);
}

static void IMAGE_engine_free(void)
{
  IMAGE_shader_free();
}

/* \} */
static const DrawEngineDataSize IMAGE_data_size = DRW_VIEWPORT_DATA_SIZE(IMAGE_Data);

DrawEngineType draw_engine_image_type = {
    NULL,                  /* next */
    NULL,                  /* prev */
    N_("UV/Image"),        /* idname */
    &IMAGE_data_size,      /* vedata_size */
    &IMAGE_engine_init,    /* engine_init */
    &IMAGE_engine_free,    /* engine_free */
    &IMAGE_cache_init,     /* cache_init */
    &IMAGE_cache_populate, /* cache_populate */
    NULL,                  /* cache_finish */
    &IMAGE_draw_scene,     /* draw_scene */
    NULL,                  /* view_update */
    NULL,                  /* id_update */
    NULL,                  /* render_to_image */
};
