/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "ED_gpencil_legacy.hh"
#include "GPU_batch.hh"

#include "DEG_depsgraph_query.hh"

#include "BLI_hash.h"
#include "BLI_math_vector_types.hh"
#include "BLI_polyfill_2d.h"

#include "draw_cache.hh"
#include "draw_cache_impl.hh"

#include "../engines/gpencil/gpencil_defines.h"
#include "../engines/gpencil/gpencil_shader_shared.h"

#define BEZIER_HANDLE (1 << 3)
#define COLOR_SHIFT 5

struct GpencilBatchCache {
  /** Instancing Data */
  blender::gpu::VertBuf *vbo;
  blender::gpu::VertBuf *vbo_col;
  /** Indices in material order, then stroke order with fill first.
   * Strokes can be individually rendered using `gps->runtime.stroke_start` and
   * `gps->runtime.fill_start`. */
  blender::gpu::IndexBuf *ibo;
  /** Batches */
  blender::gpu::Batch *geom_batch;
  /** Stroke lines only */
  blender::gpu::Batch *lines_batch;

  /** Edit Mode */
  blender::gpu::VertBuf *edit_vbo;
  blender::gpu::Batch *edit_lines_batch;
  blender::gpu::Batch *edit_points_batch;
  /** Edit Curve Mode */
  blender::gpu::VertBuf *edit_curve_vbo;
  blender::gpu::Batch *edit_curve_handles_batch;
  blender::gpu::Batch *edit_curve_points_batch;

  /** Cache is dirty */
  bool is_dirty;
};

namespace blender::draw {

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

static void gpencil_batch_cache_clear(GpencilBatchCache *cache)
{
  if (!cache) {
    return;
  }

  GPU_BATCH_DISCARD_SAFE(cache->lines_batch);
  GPU_BATCH_DISCARD_SAFE(cache->geom_batch);
  GPU_VERTBUF_DISCARD_SAFE(cache->vbo);
  GPU_VERTBUF_DISCARD_SAFE(cache->vbo_col);
  GPU_INDEXBUF_DISCARD_SAFE(cache->ibo);

  GPU_BATCH_DISCARD_SAFE(cache->edit_lines_batch);
  GPU_BATCH_DISCARD_SAFE(cache->edit_points_batch);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_vbo);

  GPU_BATCH_DISCARD_SAFE(cache->edit_curve_handles_batch);
  GPU_BATCH_DISCARD_SAFE(cache->edit_curve_points_batch);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_curve_vbo);

  cache->is_dirty = true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE Callbacks
 * \{ */

void DRW_gpencil_batch_cache_dirty_tag(bGPdata *gpd)
{
  gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
}

void DRW_gpencil_batch_cache_free(bGPdata *gpd)
{
  gpencil_batch_cache_clear(gpd->runtime.gpencil_cache);
  MEM_SAFE_FREE(gpd->runtime.gpencil_cache);
  gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Sbuffer stroke batches.
 * \{ */

void DRW_cache_gpencil_sbuffer_clear(Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  MEM_SAFE_FREE(gpd->runtime.sbuffer_gps);
  GPU_BATCH_DISCARD_SAFE(gpd->runtime.sbuffer_batch);
  GPU_VERTBUF_DISCARD_SAFE(gpd->runtime.sbuffer_position_buf);
  GPU_VERTBUF_DISCARD_SAFE(gpd->runtime.sbuffer_color_buf);
}

/** \} */

}  // namespace blender::draw
