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
#include "BLI_math_vector.h"
#include "BLI_memblock.h"
#include "BLI_rect.h"

#include "BKE_colortools.h"

#include "IMB_colormanagement.h"

#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "GPU_framebuffer.h"
#include "GPU_glew.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_texture.h"
#include "GPU_uniformbuffer.h"
#include "GPU_viewport.h"

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

/* Struct storing a viewport specific GPUBatch.
 * The end-goal is to have a single batch shared across viewport and use a model matrix to place
 * the batch. Due to OCIO and Image/UV editor we are not able to use an model matrix yet. */
struct GPUViewportBatch {
  GPUBatch *batch;
  struct {
    rctf rect_pos;
    rctf rect_uv;
  } last_used_parameters;
};

static struct {
  GPUVertFormat format;
  struct {
    uint pos, tex_coord;
  } attr_id;
} g_viewport = {{0}};

struct GPUViewport {
  int size[2];
  int flag;

  /* Set the active view (for stereoscoptic viewport rendering). */
  int active_view;

  /* If engine_handles mismatch we free all ViewportEngineData in this viewport. */
  struct {
    void *handle;
    ViewportEngineData *data;
  } engine_data[MAX_ENABLE_ENGINE];

  DefaultFramebufferList *fbl;
  DefaultTextureList *txl;

  ViewportMemoryPool vmempool;           /* Used for rendering data structure. */
  struct DRWInstanceDataList *idatalist; /* Used for rendering data structure. */

  ListBase
      tex_pool; /* ViewportTempTexture list : Temporary textures shared across draw engines. */

  /* Profiling data. */
  double cache_time;

  /* Color management. */
  ColorManagedViewSettings view_settings;
  ColorManagedDisplaySettings display_settings;
  CurveMapping *orig_curve_mapping;
  float dither;
  /* TODO(fclem) the uvimage display use the viewport but do not set any view transform for the
   * moment. The end goal would be to let the GPUViewport do the color management. */
  bool do_color_management;
  struct GPUViewportBatch batch;
};

enum {
  DO_UPDATE = (1 << 0),
  GPU_VIEWPORT_STEREO = (1 << 1),
};

static void gpu_viewport_buffers_free(
    FramebufferList *fbl, int fbl_len, TextureList *txl, TextureList *txl_stereo, int txl_len);
static void gpu_viewport_storage_free(StorageList *stl, int stl_len);
static void gpu_viewport_passes_free(PassList *psl, int psl_len);
static void gpu_viewport_texture_pool_free(GPUViewport *viewport);

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
  viewport->do_color_management = false;
  viewport->size[0] = viewport->size[1] = -1;
  viewport->active_view = -1;
  return viewport;
}

GPUViewport *GPU_viewport_stereo_create(void)
{
  GPUViewport *viewport = GPU_viewport_create();
  viewport->flag = GPU_VIEWPORT_STEREO;
  return viewport;
}

static void gpu_viewport_framebuffer_view_set(GPUViewport *viewport, int view)
{
  /* Early check if the view is the latest requested. */
  if (viewport->active_view == view) {
    return;
  }
  DefaultFramebufferList *dfbl = viewport->fbl;
  DefaultTextureList *dtxl = viewport->txl;

  /* Only swap the texture when this is a Stereo Viewport. */
  if (((viewport->flag & GPU_VIEWPORT_STEREO) != 0)) {
    SWAP(GPUTexture *, dtxl->color, dtxl->color_stereo);
    SWAP(GPUTexture *, dtxl->color_overlay, dtxl->color_overlay_stereo);

    for (int i = 0; i < MAX_ENABLE_ENGINE; i++) {
      if (viewport->engine_data[i].handle != NULL) {
        ViewportEngineData *data = viewport->engine_data[i].data;
        SWAP(StorageList *, data->stl, data->stl_stereo);
        SWAP(TextureList *, data->txl, data->txl_stereo);
      }
      else {
        break;
      }
    }
  }

  GPU_framebuffer_ensure_config(&dfbl->default_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_TEXTURE(dtxl->color),
                                });

  GPU_framebuffer_ensure_config(&dfbl->overlay_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_TEXTURE(dtxl->color_overlay),
                                });

  GPU_framebuffer_ensure_config(&dfbl->depth_only_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_NONE,
                                });

  GPU_framebuffer_ensure_config(&dfbl->color_only_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(dtxl->color),
                                });

  GPU_framebuffer_ensure_config(&dfbl->overlay_only_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(dtxl->color_overlay),
                                });

  viewport->active_view = view;
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

  if ((viewport->flag & GPU_VIEWPORT_STEREO) != 0) {
    data->txl_stereo = MEM_callocN((sizeof(void *) * txl_len) + sizeof(TextureList),
                                   "TextureList");
    data->stl_stereo = MEM_callocN((sizeof(void *) * stl_len) + sizeof(StorageList),
                                   "StorageList");
  }

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

    gpu_viewport_buffers_free(data->fbl, fbl_len, data->txl, data->txl_stereo, txl_len);
    gpu_viewport_passes_free(data->psl, psl_len);
    gpu_viewport_storage_free(data->stl, stl_len);

    MEM_freeN(data->fbl);
    MEM_freeN(data->txl);
    MEM_freeN(data->psl);
    MEM_freeN(data->stl);

    if ((viewport->flag & GPU_VIEWPORT_STEREO) != 0) {
      gpu_viewport_storage_free(data->stl_stereo, stl_len);
      MEM_freeN(data->txl_stereo);
      MEM_freeN(data->stl_stereo);
    }
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

/* Note this function is only allowed to be called from `DRW_notify_view_update`. The rest
 * should bind the correct viewport.
 *
 * The reason is that DRW_notify_view_update can be called from a different thread, but needs
 * access to the engine data. */
void GPU_viewport_active_view_set(GPUViewport *viewport, int view)
{
  gpu_viewport_framebuffer_view_set(viewport, view);
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
  copy_v2_v2_int(size, viewport->size);
}

/**
 * Special case, this is needed for when we have a viewport without a frame-buffer output
 * (occlusion queries for eg)
 * but still need to set the size since it may be used for other calculations.
 */
void GPU_viewport_size_set(GPUViewport *viewport, const int size[2])
{
  copy_v2_v2_int(viewport->size, size);
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

  LISTBASE_FOREACH (ViewportTempTexture *, tmp_tex, &viewport->tex_pool) {
    if ((GPU_texture_format(tmp_tex->texture) == format) &&
        (GPU_texture_width(tmp_tex->texture) == width) &&
        (GPU_texture_height(tmp_tex->texture) == height)) {
      /* Search if the engine is not already using this texture */
      for (int i = 0; i < MAX_ENGINE_BUFFER_SHARING; i++) {
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
  /* Doing filtering for depth does not make sense when not doing shadow mapping,
   * and enabling texture filtering on integer texture make them unreadable. */
  bool do_filter = !GPU_texture_depth(tex) && !GPU_texture_integer(tex);
  GPU_texture_filter_mode(tex, do_filter);

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
    for (int i = 0; i < MAX_ENGINE_BUFFER_SHARING; i++) {
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
  LISTBASE_FOREACH (ViewportTempTexture *, tmp_tex, &viewport->tex_pool) {
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

  dtxl->color = GPU_texture_create_2d(size[0], size[1], GPU_RGBA16F, NULL, NULL);
  dtxl->color_overlay = GPU_texture_create_2d(size[0], size[1], GPU_SRGB8_A8, NULL, NULL);
  if (((viewport->flag & GPU_VIEWPORT_STEREO) != 0)) {
    dtxl->color_stereo = GPU_texture_create_2d(size[0], size[1], GPU_RGBA16F, NULL, NULL);
    dtxl->color_overlay_stereo = GPU_texture_create_2d(size[0], size[1], GPU_SRGB8_A8, NULL, NULL);
  }

  /* Can be shared with GPUOffscreen. */
  if (dtxl->depth == NULL) {
    dtxl->depth = GPU_texture_create_2d(size[0], size[1], GPU_DEPTH24_STENCIL8, NULL, NULL);
  }

  if (!dtxl->depth || !dtxl->color) {
    ok = false;
    goto cleanup;
  }

  gpu_viewport_framebuffer_view_set(viewport, 0);

  ok = ok && GPU_framebuffer_check_valid(dfbl->default_fb, NULL);
  ok = ok && GPU_framebuffer_check_valid(dfbl->overlay_fb, NULL);
  ok = ok && GPU_framebuffer_check_valid(dfbl->color_only_fb, NULL);
  ok = ok && GPU_framebuffer_check_valid(dfbl->depth_only_fb, NULL);
  ok = ok && GPU_framebuffer_check_valid(dfbl->overlay_only_fb, NULL);
cleanup:
  if (!ok) {
    GPU_viewport_free(viewport);
    DRW_opengl_context_disable();
    return;
  }

  GPU_framebuffer_restore();
}

void GPU_viewport_bind(GPUViewport *viewport, int view, const rcti *rect)
{
  DefaultFramebufferList *dfbl = viewport->fbl;
  int fbl_len, txl_len;

  int rect_size[2];
  /* add one pixel because of scissor test */
  rect_size[0] = BLI_rcti_size_x(rect) + 1;
  rect_size[1] = BLI_rcti_size_y(rect) + 1;

  DRW_opengl_context_enable();

  if (dfbl->default_fb) {
    if (!equals_v2v2_int(viewport->size, rect_size)) {
      gpu_viewport_buffers_free((FramebufferList *)viewport->fbl,
                                default_fbl_len,
                                (TextureList *)viewport->txl,
                                NULL,
                                default_txl_len);

      for (int i = 0; i < MAX_ENABLE_ENGINE && viewport->engine_data[i].handle; i++) {
        ViewportEngineData *data = viewport->engine_data[i].data;
        DRW_engine_viewport_data_size_get(data->engine_type, &fbl_len, &txl_len, NULL, NULL);
        gpu_viewport_buffers_free(data->fbl, fbl_len, data->txl, data->txl_stereo, txl_len);
      }

      gpu_viewport_texture_pool_free(viewport);
      viewport->active_view = -1;
    }
  }

  copy_v2_v2_int(viewport->size, rect_size);

  gpu_viewport_texture_pool_clear_users(viewport);

  if (!dfbl->default_fb) {
    gpu_viewport_default_fb_create(viewport);
  }
  gpu_viewport_framebuffer_view_set(viewport, view);
}

void GPU_viewport_bind_from_offscreen(GPUViewport *viewport, struct GPUOffScreen *ofs)
{
  DefaultFramebufferList *dfbl = viewport->fbl;
  DefaultTextureList *dtxl = viewport->txl;
  GPUTexture *color, *depth;
  GPUFrameBuffer *fb;
  viewport->size[0] = GPU_offscreen_width(ofs);
  viewport->size[1] = GPU_offscreen_height(ofs);

  GPU_offscreen_viewport_data_get(ofs, &fb, &color, &depth);

  /* This is the only texture we can share. */
  dtxl->depth = depth;

  gpu_viewport_texture_pool_clear_users(viewport);

  if (!dfbl->default_fb) {
    gpu_viewport_default_fb_create(viewport);
  }
}

void GPU_viewport_colorspace_set(GPUViewport *viewport,
                                 ColorManagedViewSettings *view_settings,
                                 ColorManagedDisplaySettings *display_settings,
                                 float dither)
{
  /**
   * HACK(fclem): We copy the settings here to avoid use after free if an update frees the scene
   * and the viewport stays cached (see T75443). But this means the OCIO curve-mapping caching
   * (which is based on #CurveMap pointer address) cannot operate correctly and it will create
   * a different OCIO processor for each viewport. We try to only reallocate the curve-map copy
   * if needed to avoid unneeded cache invalidation.
   */
  if (view_settings->curve_mapping) {
    if (viewport->view_settings.curve_mapping) {
      if (view_settings->curve_mapping->changed_timestamp !=
          viewport->view_settings.curve_mapping->changed_timestamp) {
        BKE_color_managed_view_settings_free(&viewport->view_settings);
      }
    }
  }

  if (viewport->orig_curve_mapping != view_settings->curve_mapping) {
    viewport->orig_curve_mapping = view_settings->curve_mapping;
    BKE_color_managed_view_settings_free(&viewport->view_settings);
  }
  /* Don't copy the curve mapping already. */
  CurveMapping *tmp_curve_mapping = view_settings->curve_mapping;
  CurveMapping *tmp_curve_mapping_vp = viewport->view_settings.curve_mapping;
  view_settings->curve_mapping = NULL;
  viewport->view_settings.curve_mapping = NULL;

  BKE_color_managed_view_settings_copy(&viewport->view_settings, view_settings);
  /* Restore. */
  view_settings->curve_mapping = tmp_curve_mapping;
  viewport->view_settings.curve_mapping = tmp_curve_mapping_vp;
  /* Only copy curvemapping if needed. Avoid uneeded OCIO cache miss. */
  if (tmp_curve_mapping && viewport->view_settings.curve_mapping == NULL) {
    BKE_color_managed_view_settings_free(&viewport->view_settings);
    viewport->view_settings.curve_mapping = BKE_curvemapping_copy(tmp_curve_mapping);
  }

  BKE_color_managed_display_settings_copy(&viewport->display_settings, display_settings);
  viewport->dither = dither;
  viewport->do_color_management = true;
}

/* Merge the stereo textures. `color` and `overlay` texture will be modified. */
void GPU_viewport_stereo_composite(GPUViewport *viewport, Stereo3dFormat *stereo_format)
{
  if (!ELEM(stereo_format->display_mode, S3D_DISPLAY_ANAGLYPH, S3D_DISPLAY_INTERLACE)) {
    /* Early Exit: the other display modes need access to the full screen and cannot be
     * done from a single viewport. See `wm_stereo.c` */
    return;
  }
  gpu_viewport_framebuffer_view_set(viewport, 0);
  DefaultTextureList *dtxl = viewport->txl;
  DefaultFramebufferList *dfbl = viewport->fbl;

  /* The composite framebuffer object needs to be created in the window context. */
  GPU_framebuffer_ensure_config(&dfbl->stereo_comp_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(dtxl->color),
                                    GPU_ATTACHMENT_TEXTURE(dtxl->color_overlay),
                                });

  GPUVertFormat *vert_format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(vert_format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  GPU_framebuffer_bind(dfbl->stereo_comp_fb);
  GPU_matrix_push();
  GPU_matrix_push_projection();
  GPU_matrix_identity_set();
  GPU_matrix_identity_projection_set();
  immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_OVERLAYS_STEREO_MERGE);
  immUniform1i("imageTexture", 0);
  immUniform1i("overlayTexture", 1);
  int settings = stereo_format->display_mode;
  if (settings == S3D_DISPLAY_ANAGLYPH) {
    switch (stereo_format->anaglyph_type) {
      case S3D_ANAGLYPH_REDCYAN:
        glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
        break;
      case S3D_ANAGLYPH_GREENMAGENTA:
        glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
        break;
      case S3D_ANAGLYPH_YELLOWBLUE:
        glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_TRUE);
        break;
    }
  }
  else if (settings == S3D_DISPLAY_INTERLACE) {
    settings |= stereo_format->interlace_type << 3;
    SET_FLAG_FROM_TEST(settings, stereo_format->flag & S3D_INTERLACE_SWAP, 1 << 6);
  }
  immUniform1i("stereoDisplaySettings", settings);

  GPU_texture_bind(dtxl->color_stereo, 0);
  GPU_texture_bind(dtxl->color_overlay_stereo, 1);

  immBegin(GPU_PRIM_TRI_STRIP, 4);

  immVertex2f(pos, -1.0f, -1.0f);
  immVertex2f(pos, 1.0f, -1.0f);
  immVertex2f(pos, -1.0f, 1.0f);
  immVertex2f(pos, 1.0f, 1.0f);

  immEnd();

  GPU_texture_unbind(dtxl->color_stereo);
  GPU_texture_unbind(dtxl->color_overlay_stereo);

  immUnbindProgram();
  GPU_matrix_pop_projection();
  GPU_matrix_pop();

  if (settings == S3D_DISPLAY_ANAGLYPH) {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  GPU_framebuffer_restore();
}
/* -------------------------------------------------------------------- */
/** \name Viewport Batches
 * \{ */

static GPUVertFormat *gpu_viewport_batch_format(void)
{
  if (g_viewport.format.attr_len == 0) {
    GPUVertFormat *format = &g_viewport.format;
    g_viewport.attr_id.pos = GPU_vertformat_attr_add(
        format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    g_viewport.attr_id.tex_coord = GPU_vertformat_attr_add(
        format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  }
  return &g_viewport.format;
}

static GPUBatch *gpu_viewport_batch_create(const rctf *rect_pos, const rctf *rect_uv)
{
  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(gpu_viewport_batch_format());
  const uint vbo_len = 4;
  GPU_vertbuf_data_alloc(vbo, vbo_len);

  GPUVertBufRaw pos_step, tex_coord_step;
  GPU_vertbuf_attr_get_raw_data(vbo, g_viewport.attr_id.pos, &pos_step);
  GPU_vertbuf_attr_get_raw_data(vbo, g_viewport.attr_id.tex_coord, &tex_coord_step);

  copy_v2_fl2(GPU_vertbuf_raw_step(&pos_step), rect_pos->xmin, rect_pos->ymin);
  copy_v2_fl2(GPU_vertbuf_raw_step(&tex_coord_step), rect_uv->xmin, rect_uv->ymin);
  copy_v2_fl2(GPU_vertbuf_raw_step(&pos_step), rect_pos->xmax, rect_pos->ymin);
  copy_v2_fl2(GPU_vertbuf_raw_step(&tex_coord_step), rect_uv->xmax, rect_uv->ymin);
  copy_v2_fl2(GPU_vertbuf_raw_step(&pos_step), rect_pos->xmin, rect_pos->ymax);
  copy_v2_fl2(GPU_vertbuf_raw_step(&tex_coord_step), rect_uv->xmin, rect_uv->ymax);
  copy_v2_fl2(GPU_vertbuf_raw_step(&pos_step), rect_pos->xmax, rect_pos->ymax);
  copy_v2_fl2(GPU_vertbuf_raw_step(&tex_coord_step), rect_uv->xmax, rect_uv->ymax);

  return GPU_batch_create_ex(GPU_PRIM_TRI_STRIP, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

static GPUBatch *gpu_viewport_batch_get(GPUViewport *viewport,
                                        const rctf *rect_pos,
                                        const rctf *rect_uv)
{
  const float compare_limit = 0.0001f;
  const bool parameters_changed =
      (!BLI_rctf_compare(
           &viewport->batch.last_used_parameters.rect_pos, rect_pos, compare_limit) ||
       !BLI_rctf_compare(&viewport->batch.last_used_parameters.rect_uv, rect_uv, compare_limit));

  if (viewport->batch.batch && parameters_changed) {
    GPU_batch_discard(viewport->batch.batch);
    viewport->batch.batch = NULL;
  }

  if (!viewport->batch.batch) {
    viewport->batch.batch = gpu_viewport_batch_create(rect_pos, rect_uv);
    viewport->batch.last_used_parameters.rect_pos = *rect_pos;
    viewport->batch.last_used_parameters.rect_uv = *rect_uv;
  }
  return viewport->batch.batch;
}

static void gpu_viewport_batch_free(GPUViewport *viewport)
{
  if (viewport->batch.batch) {
    GPU_batch_discard(viewport->batch.batch);
    viewport->batch.batch = NULL;
  }
}

/** \} */

static void gpu_viewport_draw_colormanaged(GPUViewport *viewport,
                                           const rctf *rect_pos,
                                           const rctf *rect_uv,
                                           bool display_colorspace)
{
  DefaultTextureList *dtxl = viewport->txl;
  GPUTexture *color = dtxl->color;
  GPUTexture *color_overlay = dtxl->color_overlay;

  bool use_ocio = false;

  if (viewport->do_color_management && display_colorspace) {
    /* During the binding process the last used VertexFormat is tested and can assert as it is not
     * valid. By calling the `immVertexFormat` the last used VertexFormat is reset and the assert
     * does not happen. This solves a chicken and egg problem when using GPUBatches. GPUBatches
     * contain the correct vertex format, but can only bind after the shader is bound.
     *
     * Image/UV editor still uses imm, after that has been changed we could move this fix to the
     * OCIO. */
    immVertexFormat();
    use_ocio = IMB_colormanagement_setup_glsl_draw_from_space(&viewport->view_settings,
                                                              &viewport->display_settings,
                                                              NULL,
                                                              viewport->dither,
                                                              false,
                                                              true);
  }

  GPUBatch *batch = gpu_viewport_batch_get(viewport, rect_pos, rect_uv);
  if (use_ocio) {
    GPU_batch_program_set_imm_shader(batch);
  }
  else {
    GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_IMAGE_OVERLAYS_MERGE);
    GPU_batch_uniform_1i(batch, "display_transform", display_colorspace);
    GPU_batch_uniform_1i(batch, "image_texture", 0);
    GPU_batch_uniform_1i(batch, "overlays_texture", 1);
  }

  GPU_texture_bind(color, 0);
  GPU_texture_bind(color_overlay, 1);
  GPU_batch_draw(batch);
  GPU_texture_unbind(color);
  GPU_texture_unbind(color_overlay);

  if (use_ocio) {
    IMB_colormanagement_finish_glsl_draw();
  }
}

/**
 * Version of #GPU_viewport_draw_to_screen() that lets caller decide if display colorspace
 * transform should be performed.
 */
void GPU_viewport_draw_to_screen_ex(GPUViewport *viewport,
                                    int view,
                                    const rcti *rect,
                                    bool display_colorspace)
{
  gpu_viewport_framebuffer_view_set(viewport, view);
  DefaultFramebufferList *dfbl = viewport->fbl;
  DefaultTextureList *dtxl = viewport->txl;
  GPUTexture *color = dtxl->color;

  if (dfbl->default_fb == NULL) {
    return;
  }

  const float w = (float)GPU_texture_width(color);
  const float h = (float)GPU_texture_height(color);

  /* We allow rects with min/max swapped, but we also need coorectly assigned coordinates. */
  rcti sanitized_rect = *rect;
  BLI_rcti_sanitize(&sanitized_rect);

  BLI_assert(w == BLI_rcti_size_x(&sanitized_rect) + 1);
  BLI_assert(h == BLI_rcti_size_y(&sanitized_rect) + 1);

  /* wmOrtho for the screen has this same offset */
  const float halfx = GLA_PIXEL_OFS / w;
  const float halfy = GLA_PIXEL_OFS / h;

  rctf pos_rect = {
      .xmin = sanitized_rect.xmin,
      .ymin = sanitized_rect.ymin,
      .xmax = sanitized_rect.xmin + w,
      .ymax = sanitized_rect.ymin + h,
  };

  rctf uv_rect = {
      .xmin = halfx,
      .ymin = halfy,
      .xmax = halfx + 1.0f,
      .ymax = halfy + 1.0f,
  };
  /* Mirror the UV rect in case axis-swapped drawing is requested (by passing a rect with min and
   * max values swapped). */
  if (BLI_rcti_size_x(rect) < 0) {
    SWAP(float, uv_rect.xmin, uv_rect.xmax);
  }
  if (BLI_rcti_size_y(rect) < 0) {
    SWAP(float, uv_rect.ymin, uv_rect.ymax);
  }

  gpu_viewport_draw_colormanaged(viewport, &pos_rect, &uv_rect, display_colorspace);
}

/**
 * Merge and draw the buffers of \a viewport into the currently active framebuffer, performing
 * color transform to display space.
 *
 * \param rect: Coordinates to draw into. By swapping min and max values, drawing can be done
 * with inversed axis coordinates (upside down or sideways).
 */
void GPU_viewport_draw_to_screen(GPUViewport *viewport, int view, const rcti *rect)
{
  GPU_viewport_draw_to_screen_ex(viewport, view, rect, true);
}

/**
 * Clear vars assigned from offscreen, so we don't free data owned by `GPUOffScreen`.
 */
void GPU_viewport_unbind_from_offscreen(GPUViewport *viewport,
                                        struct GPUOffScreen *ofs,
                                        bool display_colorspace)
{
  DefaultFramebufferList *dfbl = viewport->fbl;
  DefaultTextureList *dtxl = viewport->txl;

  if (dfbl->default_fb == NULL) {
    return;
  }

  GPU_depth_test(false);
  GPU_offscreen_bind(ofs, false);

  rctf pos_rect = {
      .xmin = -1.0f,
      .ymin = -1.0f,
      .xmax = 1.0f,
      .ymax = 1.0f,
  };

  rctf uv_rect = {
      .xmin = 0.0f,
      .ymin = 0.0f,
      .xmax = 1.0f,
      .ymax = 1.0f,
  };

  gpu_viewport_draw_colormanaged(viewport, &pos_rect, &uv_rect, display_colorspace);

  /* This one is from the offscreen. Don't free it with the viewport. */
  dtxl->depth = NULL;
}

void GPU_viewport_unbind(GPUViewport *UNUSED(viewport))
{
  GPU_framebuffer_restore();
  DRW_opengl_context_disable();
}

GPUTexture *GPU_viewport_color_texture(GPUViewport *viewport, int view)
{
  DefaultFramebufferList *dfbl = viewport->fbl;

  if (dfbl->default_fb) {
    DefaultTextureList *dtxl = viewport->txl;
    if (viewport->active_view == view) {
      return dtxl->color;
    }
    else {
      return dtxl->color_stereo;
    }
  }

  return NULL;
}

static void gpu_viewport_buffers_free(
    FramebufferList *fbl, int fbl_len, TextureList *txl, TextureList *txl_stereo, int txl_len)
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
  if (txl_stereo != NULL) {
    for (int i = 0; i < txl_len; i++) {
      GPUTexture *tex = txl_stereo->textures[i];
      if (tex) {
        GPU_texture_free(tex);
        txl_stereo->textures[i] = NULL;
      }
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
  memset(psl->passes, 0, sizeof(*psl->passes) * psl_len);
}

/* Must be executed inside Drawmanager Opengl Context. */
void GPU_viewport_free(GPUViewport *viewport)
{
  gpu_viewport_engines_data_free(viewport);

  gpu_viewport_buffers_free((FramebufferList *)viewport->fbl,
                            default_fbl_len,
                            (TextureList *)viewport->txl,
                            NULL,
                            default_txl_len);

  gpu_viewport_texture_pool_free(viewport);

  MEM_freeN(viewport->fbl);
  MEM_freeN(viewport->txl);

  if (viewport->vmempool.commands != NULL) {
    BLI_memblock_destroy(viewport->vmempool.commands, NULL);
  }
  if (viewport->vmempool.commands_small != NULL) {
    BLI_memblock_destroy(viewport->vmempool.commands_small, NULL);
  }
  if (viewport->vmempool.callbuffers != NULL) {
    BLI_memblock_destroy(viewport->vmempool.callbuffers, NULL);
  }
  if (viewport->vmempool.obmats != NULL) {
    BLI_memblock_destroy(viewport->vmempool.obmats, NULL);
  }
  if (viewport->vmempool.obinfos != NULL) {
    BLI_memblock_destroy(viewport->vmempool.obinfos, NULL);
  }
  if (viewport->vmempool.cullstates != NULL) {
    BLI_memblock_destroy(viewport->vmempool.cullstates, NULL);
  }
  if (viewport->vmempool.shgroups != NULL) {
    BLI_memblock_destroy(viewport->vmempool.shgroups, NULL);
  }
  if (viewport->vmempool.uniforms != NULL) {
    BLI_memblock_destroy(viewport->vmempool.uniforms, NULL);
  }
  if (viewport->vmempool.views != NULL) {
    BLI_memblock_destroy(viewport->vmempool.views, NULL);
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

  for (int i = 0; i < viewport->vmempool.ubo_len; i++) {
    GPU_uniformbuffer_free(viewport->vmempool.matrices_ubo[i]);
    GPU_uniformbuffer_free(viewport->vmempool.obinfos_ubo[i]);
  }
  MEM_SAFE_FREE(viewport->vmempool.matrices_ubo);
  MEM_SAFE_FREE(viewport->vmempool.obinfos_ubo);

  DRW_instance_data_list_free(viewport->idatalist);
  MEM_freeN(viewport->idatalist);

  BKE_color_managed_view_settings_free(&viewport->view_settings);
  gpu_viewport_batch_free(viewport);

  MEM_freeN(viewport);
}
