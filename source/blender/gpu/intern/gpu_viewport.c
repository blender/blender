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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * System that manages viewport drawing.
 */

#include <string.h>

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_memblock.h"

#include "BIF_gl.h"

#include "DNA_vec_types.h"
#include "DNA_userdef_types.h"

#include "GPU_framebuffer.h"
#include "GPU_glew.h"
#include "GPU_immediate.h"
#include "GPU_texture.h"
#include "GPU_viewport.h"
#include "GPU_draw.h"

#include "DRW_engine.h"

#include "MEM_guardedalloc.h"

static const int default_fbl_len = (sizeof(DefaultFramebufferList)) / sizeof(void *);
static const int default_txl_len = (sizeof(DefaultTextureList)) / sizeof(void *);

#define MAX_ENABLE_ENGINE 8

/* Maximum number of simultaneous engine enabled at the same time.
 * Setting it lower than the real number will do lead to
 * higher VRAM usage due to sub-efficient buffer reuse. */
#define MAX_ENGINE_BUFFER_SHARING 5

typedef struct ViewportTempTexture {
  struct ViewportTempTexture *next, *prev;
  void *user[MAX_ENGINE_BUFFER_SHARING];
  GPUTexture *texture;
} ViewportTempTexture;

struct GPUViewport {
  int size[2];
  int samples;
  int flag;

  /* If engine_handles mismatch we free all ViewportEngineData in this viewport */
  struct {
    void *handle;
    ViewportEngineData *data;
  } engine_data[MAX_ENABLE_ENGINE];

  DefaultFramebufferList *fbl;
  DefaultTextureList *txl;

  ViewportMemoryPool vmempool;           /* Used for rendering data structure. */
  struct DRWInstanceDataList *idatalist; /* Used for rendering data structure. */

  ListBase tex_pool; /* ViewportTempTexture list : Temporary textures shared across draw engines */

  /* Profiling data */
  double cache_time;
};

enum {
  DO_UPDATE = (1 << 0),
};

static void gpu_viewport_buffers_free(FramebufferList *fbl,
                                      int fbl_len,
                                      TextureList *txl,
                                      int txl_len);
static void gpu_viewport_storage_free(StorageList *stl, int stl_len);
static void gpu_viewport_passes_free(PassList *psl, int psl_len);
static void gpu_viewport_texture_pool_free(GPUViewport *viewport);
static void gpu_viewport_default_fb_create(GPUViewport *viewport);

void GPU_viewport_tag_update(GPUViewport *viewport)
{
  viewport->flag |= DO_UPDATE;
}

bool GPU_viewport_do_update(GPUViewport *viewport)
{
  bool ret = (viewport->flag & DO_UPDATE);
  viewport->flag &= ~DO_UPDATE;
  return ret;
}

GPUViewport *GPU_viewport_create(void)
{
  GPUViewport *viewport = MEM_callocN(sizeof(GPUViewport), "GPUViewport");
  viewport->fbl = MEM_callocN(sizeof(DefaultFramebufferList), "FramebufferList");
  viewport->txl = MEM_callocN(sizeof(DefaultTextureList), "TextureList");
  viewport->idatalist = DRW_instance_data_list_create();

  viewport->size[0] = viewport->size[1] = -1;

  return viewport;
}

GPUViewport *GPU_viewport_create_from_offscreen(struct GPUOffScreen *ofs)
{
  GPUViewport *viewport = GPU_viewport_create();
  GPUTexture *color, *depth;
  GPUFrameBuffer *fb;
  viewport->size[0] = GPU_offscreen_width(ofs);
  viewport->size[1] = GPU_offscreen_height(ofs);

  GPU_offscreen_viewport_data_get(ofs, &fb, &color, &depth);

  if (GPU_texture_samples(color)) {
    viewport->txl->multisample_color = color;
    viewport->txl->multisample_depth = depth;
    viewport->fbl->multisample_fb = fb;
    gpu_viewport_default_fb_create(viewport);
  }
  else {
    viewport->fbl->default_fb = fb;
    viewport->txl->color = color;
    viewport->txl->depth = depth;
    GPU_framebuffer_ensure_config(
        &viewport->fbl->color_only_fb,
        {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(viewport->txl->color)});
    GPU_framebuffer_ensure_config(
        &viewport->fbl->depth_only_fb,
        {GPU_ATTACHMENT_TEXTURE(viewport->txl->depth), GPU_ATTACHMENT_NONE});
  }

  return viewport;
}
/**
 * Clear vars assigned from offscreen, so we don't free data owned by `GPUOffScreen`.
 */
void GPU_viewport_clear_from_offscreen(GPUViewport *viewport)
{
  DefaultFramebufferList *dfbl = viewport->fbl;
  DefaultTextureList *dtxl = viewport->txl;

  if (dfbl->multisample_fb) {
    /* GPUViewport expect the final result to be in default_fb but
     * GPUOffscreen wants it in its multisample_fb, so we sync it back. */
    GPU_framebuffer_blit(
        dfbl->default_fb, 0, dfbl->multisample_fb, 0, GPU_COLOR_BIT | GPU_DEPTH_BIT);
    dfbl->multisample_fb = NULL;
    dtxl->multisample_color = NULL;
    dtxl->multisample_depth = NULL;
  }
  else {
    viewport->fbl->default_fb = NULL;
    dtxl->color = NULL;
    dtxl->depth = NULL;
  }
}

void *GPU_viewport_engine_data_create(GPUViewport *viewport, void *engine_type)
{
  ViewportEngineData *data = MEM_callocN(sizeof(ViewportEngineData), "ViewportEngineData");
  int fbl_len, txl_len, psl_len, stl_len;

  DRW_engine_viewport_data_size_get(engine_type, &fbl_len, &txl_len, &psl_len, &stl_len);

  data->engine_type = engine_type;

  data->fbl = MEM_callocN((sizeof(void *) * fbl_len) + sizeof(FramebufferList), "FramebufferList");
  data->txl = MEM_callocN((sizeof(void *) * txl_len) + sizeof(TextureList), "TextureList");
  data->psl = MEM_callocN((sizeof(void *) * psl_len) + sizeof(PassList), "PassList");
  data->stl = MEM_callocN((sizeof(void *) * stl_len) + sizeof(StorageList), "StorageList");

  for (int i = 0; i < MAX_ENABLE_ENGINE; i++) {
    if (viewport->engine_data[i].handle == NULL) {
      viewport->engine_data[i].handle = engine_type;
      viewport->engine_data[i].data = data;
      return data;
    }
  }

  BLI_assert(!"Too many draw engines enabled at the same time");
  return NULL;
}

static void gpu_viewport_engines_data_free(GPUViewport *viewport)
{
  int fbl_len, txl_len, psl_len, stl_len;

  for (int i = 0; i < MAX_ENABLE_ENGINE && viewport->engine_data[i].handle; i++) {
    ViewportEngineData *data = viewport->engine_data[i].data;

    DRW_engine_viewport_data_size_get(data->engine_type, &fbl_len, &txl_len, &psl_len, &stl_len);

    gpu_viewport_buffers_free(data->fbl, fbl_len, data->txl, txl_len);
    gpu_viewport_passes_free(data->psl, psl_len);
    gpu_viewport_storage_free(data->stl, stl_len);

    MEM_freeN(data->fbl);
    MEM_freeN(data->txl);
    MEM_freeN(data->psl);
    MEM_freeN(data->stl);

    /* We could handle this in the DRW module */
    if (data->text_draw_cache) {
      extern void DRW_text_cache_destroy(struct DRWTextStore * dt);
      DRW_text_cache_destroy(data->text_draw_cache);
      data->text_draw_cache = NULL;
    }

    MEM_freeN(data);

    /* Mark as unused*/
    viewport->engine_data[i].handle = NULL;
  }

  gpu_viewport_texture_pool_free(viewport);
}

void *GPU_viewport_engine_data_get(GPUViewport *viewport, void *engine_handle)
{
  BLI_assert(engine_handle != NULL);

  for (int i = 0; i < MAX_ENABLE_ENGINE; i++) {
    if (viewport->engine_data[i].handle == engine_handle) {
      return viewport->engine_data[i].data;
    }
  }
  return NULL;
}

ViewportMemoryPool *GPU_viewport_mempool_get(GPUViewport *viewport)
{
  return &viewport->vmempool;
}

struct DRWInstanceDataList *GPU_viewport_instance_data_list_get(GPUViewport *viewport)
{
  return viewport->idatalist;
}

void *GPU_viewport_framebuffer_list_get(GPUViewport *viewport)
{
  return viewport->fbl;
}

void *GPU_viewport_texture_list_get(GPUViewport *viewport)
{
  return viewport->txl;
}

void GPU_viewport_size_get(const GPUViewport *viewport, int size[2])
{
  size[0] = viewport->size[0];
  size[1] = viewport->size[1];
}

/**
 * Special case, this is needed for when we have a viewport without a frame-buffer output
 * (occlusion queries for eg)
 * but still need to set the size since it may be used for other calculations.
 */
void GPU_viewport_size_set(GPUViewport *viewport, const int size[2])
{
  viewport->size[0] = size[0];
  viewport->size[1] = size[1];
}

double *GPU_viewport_cache_time_get(GPUViewport *viewport)
{
  return &viewport->cache_time;
}

/**
 * Try to find a texture corresponding to params into the texture pool.
 * If no texture was found, create one and add it to the pool.
 */
GPUTexture *GPU_viewport_texture_pool_query(
    GPUViewport *viewport, void *engine, int width, int height, int format)
{
  GPUTexture *tex;

  for (ViewportTempTexture *tmp_tex = viewport->tex_pool.first; tmp_tex; tmp_tex = tmp_tex->next) {
    if ((GPU_texture_format(tmp_tex->texture) == format) &&
        (GPU_texture_width(tmp_tex->texture) == width) &&
        (GPU_texture_height(tmp_tex->texture) == height)) {
      /* Search if the engine is not already using this texture */
      for (int i = 0; i < MAX_ENGINE_BUFFER_SHARING; ++i) {
        if (tmp_tex->user[i] == engine) {
          break;
        }

        if (tmp_tex->user[i] == NULL) {
          tmp_tex->user[i] = engine;
          return tmp_tex->texture;
        }
      }
    }
  }

  tex = GPU_texture_create_2d(width, height, format, NULL, NULL);
  GPU_texture_bind(tex, 0);
  /* Doing filtering for depth does not make sense when not doing shadow mapping,
   * and enabling texture filtering on integer texture make them unreadable. */
  bool do_filter = !GPU_texture_depth(tex) && !GPU_texture_integer(tex);
  GPU_texture_filter_mode(tex, do_filter);
  GPU_texture_unbind(tex);

  ViewportTempTexture *tmp_tex = MEM_callocN(sizeof(ViewportTempTexture), "ViewportTempTexture");
  tmp_tex->texture = tex;
  tmp_tex->user[0] = engine;
  BLI_addtail(&viewport->tex_pool, tmp_tex);

  return tex;
}

static void gpu_viewport_texture_pool_clear_users(GPUViewport *viewport)
{
  ViewportTempTexture *tmp_tex_next;

  for (ViewportTempTexture *tmp_tex = viewport->tex_pool.first; tmp_tex; tmp_tex = tmp_tex_next) {
    tmp_tex_next = tmp_tex->next;
    bool no_user = true;
    for (int i = 0; i < MAX_ENGINE_BUFFER_SHARING; ++i) {
      if (tmp_tex->user[i] != NULL) {
        tmp_tex->user[i] = NULL;
        no_user = false;
      }
    }

    if (no_user) {
      GPU_texture_free(tmp_tex->texture);
      BLI_freelinkN(&viewport->tex_pool, tmp_tex);
    }
  }
}

static void gpu_viewport_texture_pool_free(GPUViewport *viewport)
{
  for (ViewportTempTexture *tmp_tex = viewport->tex_pool.first; tmp_tex; tmp_tex = tmp_tex->next) {
    GPU_texture_free(tmp_tex->texture);
  }

  BLI_freelistN(&viewport->tex_pool);
}

/* Takes an NULL terminated array of engine_handle. Returns true is data is still valid. */
bool GPU_viewport_engines_data_validate(GPUViewport *viewport, void **engine_handle_array)
{
  for (int i = 0; i < MAX_ENABLE_ENGINE && engine_handle_array[i]; i++) {
    if (viewport->engine_data[i].handle != engine_handle_array[i]) {
      gpu_viewport_engines_data_free(viewport);
      return false;
    }
  }
  return true;
}

void GPU_viewport_cache_release(GPUViewport *viewport)
{
  for (int i = 0; i < MAX_ENABLE_ENGINE && viewport->engine_data[i].handle; i++) {
    ViewportEngineData *data = viewport->engine_data[i].data;
    int psl_len;
    DRW_engine_viewport_data_size_get(data->engine_type, NULL, NULL, &psl_len, NULL);
    gpu_viewport_passes_free(data->psl, psl_len);
  }
}

static void gpu_viewport_default_fb_create(GPUViewport *viewport)
{
  DefaultFramebufferList *dfbl = viewport->fbl;
  DefaultTextureList *dtxl = viewport->txl;
  int *size = viewport->size;
  bool ok = true;

  dtxl->color = GPU_texture_create_2d(size[0], size[1], GPU_RGBA8, NULL, NULL);
  dtxl->depth = GPU_texture_create_2d(size[0], size[1], GPU_DEPTH24_STENCIL8, NULL, NULL);

  if (!(dtxl->depth && dtxl->color)) {
    ok = false;
    goto cleanup;
  }

  GPU_framebuffer_ensure_config(
      &dfbl->default_fb,
      {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_TEXTURE(dtxl->color)});

  GPU_framebuffer_ensure_config(&dfbl->depth_only_fb,
                                {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_NONE});

  GPU_framebuffer_ensure_config(&dfbl->color_only_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(dtxl->color)});

  ok = ok && GPU_framebuffer_check_valid(dfbl->default_fb, NULL);
  ok = ok && GPU_framebuffer_check_valid(dfbl->color_only_fb, NULL);
  ok = ok && GPU_framebuffer_check_valid(dfbl->depth_only_fb, NULL);

cleanup:
  if (!ok) {
    GPU_viewport_free(viewport);
    DRW_opengl_context_disable();
    return;
  }

  GPU_framebuffer_restore();
}

static void gpu_viewport_default_multisample_fb_create(GPUViewport *viewport)
{
  DefaultFramebufferList *dfbl = viewport->fbl;
  DefaultTextureList *dtxl = viewport->txl;
  int *size = viewport->size;
  int samples = viewport->samples;
  bool ok = true;

  dtxl->multisample_color = GPU_texture_create_2d_multisample(
      size[0], size[1], GPU_RGBA8, NULL, samples, NULL);
  dtxl->multisample_depth = GPU_texture_create_2d_multisample(
      size[0], size[1], GPU_DEPTH24_STENCIL8, NULL, samples, NULL);

  if (!(dtxl->multisample_depth && dtxl->multisample_color)) {
    ok = false;
    goto cleanup;
  }

  GPU_framebuffer_ensure_config(&dfbl->multisample_fb,
                                {GPU_ATTACHMENT_TEXTURE(dtxl->multisample_depth),
                                 GPU_ATTACHMENT_TEXTURE(dtxl->multisample_color)});

  ok = ok && GPU_framebuffer_check_valid(dfbl->multisample_fb, NULL);

cleanup:
  if (!ok) {
    GPU_viewport_free(viewport);
    DRW_opengl_context_disable();
    return;
  }

  GPU_framebuffer_restore();
}

void GPU_viewport_bind(GPUViewport *viewport, const rcti *rect)
{
  DefaultFramebufferList *dfbl = viewport->fbl;
  int fbl_len, txl_len;

  /* add one pixel because of scissor test */
  int rect_w = BLI_rcti_size_x(rect) + 1;
  int rect_h = BLI_rcti_size_y(rect) + 1;

  DRW_opengl_context_enable();

  if (dfbl->default_fb) {
    if (rect_w != viewport->size[0] || rect_h != viewport->size[1] ||
        U.ogl_multisamples != viewport->samples) {
      gpu_viewport_buffers_free((FramebufferList *)viewport->fbl,
                                default_fbl_len,
                                (TextureList *)viewport->txl,
                                default_txl_len);

      for (int i = 0; i < MAX_ENABLE_ENGINE && viewport->engine_data[i].handle; i++) {
        ViewportEngineData *data = viewport->engine_data[i].data;
        DRW_engine_viewport_data_size_get(data->engine_type, &fbl_len, &txl_len, NULL, NULL);
        gpu_viewport_buffers_free(data->fbl, fbl_len, data->txl, txl_len);
      }

      gpu_viewport_texture_pool_free(viewport);
    }
  }

  viewport->size[0] = rect_w;
  viewport->size[1] = rect_h;
  viewport->samples = U.ogl_multisamples;

  gpu_viewport_texture_pool_clear_users(viewport);

  /* Multisample Buffer */
  if (viewport->samples > 0) {
    if (!dfbl->default_fb) {
      gpu_viewport_default_multisample_fb_create(viewport);
    }
  }

  if (!dfbl->default_fb) {
    gpu_viewport_default_fb_create(viewport);
  }
}

void GPU_viewport_draw_to_screen(GPUViewport *viewport, const rcti *rect)
{
  DefaultFramebufferList *dfbl = viewport->fbl;

  if (dfbl->default_fb == NULL) {
    return;
  }

  DefaultTextureList *dtxl = viewport->txl;

  GPUTexture *color = dtxl->color;

  const float w = (float)GPU_texture_width(color);
  const float h = (float)GPU_texture_height(color);

  BLI_assert(w == BLI_rcti_size_x(rect) + 1);
  BLI_assert(h == BLI_rcti_size_y(rect) + 1);

  /* wmOrtho for the screen has this same offset */
  const float halfx = GLA_PIXEL_OFS / w;
  const float halfy = GLA_PIXEL_OFS / h;

  float x1 = rect->xmin;
  float x2 = rect->xmin + w;
  float y1 = rect->ymin;
  float y2 = rect->ymin + h;

  GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_2D_IMAGE_RECT_COLOR);
  GPU_shader_bind(shader);

  GPU_texture_bind(color, 0);
  glUniform1i(GPU_shader_get_uniform_ensure(shader, "image"), 0);
  glUniform4f(GPU_shader_get_uniform_ensure(shader, "rect_icon"),
              halfx,
              halfy,
              1.0f + halfx,
              1.0f + halfy);
  glUniform4f(GPU_shader_get_uniform_ensure(shader, "rect_geom"), x1, y1, x2, y2);
  glUniform4f(GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_COLOR), 1.0f, 1.0f, 1.0f, 1.0f);

  GPU_draw_primitive(GPU_PRIM_TRI_STRIP, 4);

  GPU_texture_unbind(color);
}

void GPU_viewport_unbind(GPUViewport *UNUSED(viewport))
{
  GPU_framebuffer_restore();
  DRW_opengl_context_disable();
}

GPUTexture *GPU_viewport_color_texture(GPUViewport *viewport)
{
  DefaultFramebufferList *dfbl = viewport->fbl;

  if (dfbl->default_fb) {
    DefaultTextureList *dtxl = viewport->txl;
    return dtxl->color;
  }

  return NULL;
}

static void gpu_viewport_buffers_free(FramebufferList *fbl,
                                      int fbl_len,
                                      TextureList *txl,
                                      int txl_len)
{
  for (int i = 0; i < fbl_len; i++) {
    GPUFrameBuffer *fb = fbl->framebuffers[i];
    if (fb) {
      GPU_framebuffer_free(fb);
      fbl->framebuffers[i] = NULL;
    }
  }
  for (int i = 0; i < txl_len; i++) {
    GPUTexture *tex = txl->textures[i];
    if (tex) {
      GPU_texture_free(tex);
      txl->textures[i] = NULL;
    }
  }
}

static void gpu_viewport_storage_free(StorageList *stl, int stl_len)
{
  for (int i = 0; i < stl_len; i++) {
    void *storage = stl->storage[i];
    if (storage) {
      MEM_freeN(storage);
      stl->storage[i] = NULL;
    }
  }
}

static void gpu_viewport_passes_free(PassList *psl, int psl_len)
{
  memset(psl, 0, sizeof(struct DRWPass *) * psl_len);
}

/* Must be executed inside Drawmanager Opengl Context. */
void GPU_viewport_free(GPUViewport *viewport)
{
  gpu_viewport_engines_data_free(viewport);

  gpu_viewport_buffers_free((FramebufferList *)viewport->fbl,
                            default_fbl_len,
                            (TextureList *)viewport->txl,
                            default_txl_len);

  gpu_viewport_texture_pool_free(viewport);

  MEM_freeN(viewport->fbl);
  MEM_freeN(viewport->txl);

  if (viewport->vmempool.calls != NULL) {
    BLI_memblock_destroy(viewport->vmempool.calls, NULL);
  }
  if (viewport->vmempool.states != NULL) {
    BLI_memblock_destroy(viewport->vmempool.states, NULL);
  }
  if (viewport->vmempool.shgroups != NULL) {
    BLI_memblock_destroy(viewport->vmempool.shgroups, NULL);
  }
  if (viewport->vmempool.uniforms != NULL) {
    BLI_memblock_destroy(viewport->vmempool.uniforms, NULL);
  }
  if (viewport->vmempool.passes != NULL) {
    BLI_memblock_destroy(viewport->vmempool.passes, NULL);
  }
  if (viewport->vmempool.images != NULL) {
    BLI_memblock_iter iter;
    GPUTexture **tex;
    BLI_memblock_iternew(viewport->vmempool.images, &iter);
    while ((tex = BLI_memblock_iterstep(&iter))) {
      GPU_texture_free(*tex);
    }
    BLI_memblock_destroy(viewport->vmempool.images, NULL);
  }

  DRW_instance_data_list_free(viewport->idatalist);
  MEM_freeN(viewport->idatalist);

  MEM_freeN(viewport);
}
