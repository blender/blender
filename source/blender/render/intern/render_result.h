/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

#include "BKE_global.hh" /* IWYU pragma: keep. Used in macro. */

#define PASS_VECTOR_MAX 10000.0f

#define RR_ALL_LAYERS NULL
#define RR_ALL_VIEWS NULL

struct ColorManagedDisplaySettings;
struct ColorManagedViewSettings;
struct ExrHandle;
struct ImBuf;
struct ListBase;
struct Render;
struct RenderData;
struct RenderLayer;
struct RenderResult;
struct ReportList;
struct rcti;

/* New */

/**
 * Called by main render as well for parts will read info from Render *re to define layers.
 * \note Called in threads.
 *
 * `re->winx`, `re->winy` is coordinate space of entire image, `partrct` the part within.
 */
struct RenderResult *render_result_new(struct Render *re,
                                       const struct rcti *partrct,
                                       const char *layername,
                                       const char *viewname);

void render_result_passes_allocated_ensure(struct RenderResult *rr);

/**
 * From `imbuf`, if a handle was returned and
 * it's not a single-layer multi-view we convert this to render result.
 */
struct RenderResult *render_result_new_from_exr(
    ExrHandle *exrhandle, const char *colorspace, bool predivide, int rectx, int recty);

void render_result_view_new(struct RenderResult *rr, const char *viewname);
void render_result_views_new(struct RenderResult *rr, const struct RenderData *rd);

/* Merge */

/**
 * Used when rendering to a full buffer, or when reading the EXR part-layer-pass file.
 * no test happens here if it fits... we also assume layers are in sync.
 * \note Is used within threads.
 */
void render_result_merge(struct RenderResult *rr, struct RenderResult *rrpart);

/* Add Passes */

void render_result_clone_passes(struct Render *re, struct RenderResult *rr, const char *viewname);

/* Free */

void render_result_free(struct RenderResult *rr);
/**
 * Version that's compatible with full-sample buffers.
 */
void render_result_free_list(struct ListBase *lb, struct RenderResult *rr);

/* Single Layer Render */

void render_result_single_layer_begin(struct Render *re);
/**
 * If #RenderData.scemode is #R_SINGLE_LAYER, at end of rendering, merge the both render results.
 */
void render_result_single_layer_end(struct Render *re);

/**
 * Render pass wrapper for grease-pencil.
 */
struct RenderPass *render_layer_add_pass(struct RenderResult *rr,
                                         struct RenderLayer *rl,
                                         int channels,
                                         const char *name,
                                         const char *viewname,
                                         const char *chan_id,
                                         bool allocate);

/**
 * Called for reading temp files, and for external engines.
 */
bool render_result_exr_file_read_path(struct RenderResult *rr,
                                      struct RenderLayer *rl_single,
                                      struct ReportList *reports,
                                      const char *filepath);

/* EXR cache */

void render_result_exr_file_cache_write(struct Render *re);
/**
 * For cache, makes exact copy of render result.
 */
bool render_result_exr_file_cache_read(struct Render *re);

/* Combined Pixel Rect */

struct ImBuf *render_result_rect_to_ibuf(struct RenderResult *rr,
                                         const struct RenderData *rd,
                                         int view_id);

void render_result_rect_fill_zero(struct RenderResult *rr, int view_id);
void render_result_rect_get_pixels(struct RenderResult *rr,
                                   unsigned int *rect,
                                   int rectx,
                                   int recty,
                                   const struct ColorManagedViewSettings *view_settings,
                                   const struct ColorManagedDisplaySettings *display_settings,
                                   int view_id);

/**
 * Create a new views #ListBase in rr without duplicating the memory pointers.
 */
void render_result_views_shallowcopy(struct RenderResult *dst, struct RenderResult *src);
/**
 * Free the views created temporarily.
 */
void render_result_views_shallowdelete(struct RenderResult *rr);

/**
 * Free GPU texture caches to reduce memory usage.
 */
void render_result_free_gpu_texture_caches(struct RenderResult *rr);

#define FOREACH_VIEW_LAYER_TO_RENDER_BEGIN(re_, iter_) \
  { \
    int nr_; \
    ViewLayer *iter_; \
    for (nr_ = 0, iter_ = static_cast<ViewLayer *>((re_)->scene->view_layers.first); \
         iter_ != NULL; \
         iter_ = iter_->next, nr_++) \
    { \
      if (!G.background && (re_)->r.scemode & R_SINGLE_LAYER) { \
        if (!STREQ(iter_->name, re->single_view_layer)) { \
          continue; \
        } \
      } \
      else { \
        if ((iter_->flag & VIEW_LAYER_RENDER) == 0) { \
          continue; \
        } \
      }

#define FOREACH_VIEW_LAYER_TO_RENDER_END \
  } \
  } \
  ((void)0)
