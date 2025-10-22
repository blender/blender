/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "GPU_texture.hh"

#include "SEQ_preview_cache.hh"

namespace blender::seq {

struct PreviewCacheItem {
  int64_t last_used = -1;
  int timeline_frame = -1;
  int display_channel = -1;

  gpu::Texture *texture = nullptr;
  gpu::Texture *display_texture = nullptr;

  void clear()
  {
    last_used = -1;
    timeline_frame = -1;
    GPU_TEXTURE_FREE_SAFE(texture);
    GPU_TEXTURE_FREE_SAFE(display_texture);
  }
};

struct PreviewCache {
  static constexpr int cache_size = 4;
  PreviewCacheItem items[cache_size];
  int64_t tick_count = 0;

  ~PreviewCache()
  {
    clear();
  }

  void clear()
  {
    for (PreviewCacheItem &item : this->items) {
      item.clear();
    }
  }
};

static PreviewCache *query_preview_cache(Scene *scene)
{
  if (scene == nullptr || scene->ed == nullptr) {
    return nullptr;
  }
  return scene->ed->runtime.preview_cache;
}

static PreviewCache *ensure_preview_cache(Scene *scene)
{
  if (scene == nullptr || scene->ed == nullptr) {
    return nullptr;
  }
  PreviewCache *&cache = scene->ed->runtime.preview_cache;
  if (cache == nullptr) {
    cache = MEM_new<PreviewCache>(__func__);
  }
  return cache;
}

gpu::Texture *preview_cache_get_gpu_texture(Scene *scene, int timeline_frame, int display_channel)
{
  PreviewCache *cache = query_preview_cache(scene);
  if (cache == nullptr) {
    return nullptr;
  }
  cache->tick_count++;
  for (PreviewCacheItem &item : cache->items) {
    if (item.timeline_frame == timeline_frame && item.display_channel == display_channel &&
        item.texture != nullptr)
    {
      item.last_used = cache->tick_count;
      return item.texture;
    }
  }
  return nullptr;
}

gpu::Texture *preview_cache_get_gpu_display_texture(Scene *scene,
                                                    int timeline_frame,
                                                    int display_channel)
{
  PreviewCache *cache = query_preview_cache(scene);
  if (cache == nullptr) {
    return nullptr;
  }
  cache->tick_count++;
  for (PreviewCacheItem &item : cache->items) {
    if (item.timeline_frame == timeline_frame && item.display_channel == display_channel &&
        item.display_texture != nullptr)
    {
      item.last_used = cache->tick_count;
      return item.display_texture;
    }
  }
  return nullptr;
}

static PreviewCacheItem *find_slot(PreviewCache *cache, int timeline_frame, int display_channel)
{
  cache->tick_count++;

  /* Try to find an exact frame match. */
  for (PreviewCacheItem &item : cache->items) {
    if (item.timeline_frame == timeline_frame && item.display_channel == display_channel) {
      return &item;
    }
  }

  /* Find unused or least recently used slot. */
  PreviewCacheItem *best_slot = nullptr;
  int64_t best_score = -1;
  for (PreviewCacheItem &item : cache->items) {
    if (item.texture == nullptr && item.display_texture == nullptr) {
      return &item;
    }
    int64_t score = cache->tick_count - item.last_used;
    if (score >= best_score) {
      best_score = score;
      best_slot = &item;
    }
  }

  return best_slot;
}

void preview_cache_set_gpu_texture(Scene *scene,
                                   int timeline_frame,
                                   int display_channel,
                                   gpu::Texture *texture)
{
  PreviewCache *cache = ensure_preview_cache(scene);
  if (cache == nullptr || texture == nullptr) {
    return;
  }
  PreviewCacheItem *slot = find_slot(cache, timeline_frame, display_channel);
  if (slot == nullptr) {
    return;
  }

  slot->timeline_frame = timeline_frame;
  slot->display_channel = display_channel;
  slot->last_used = cache->tick_count;
  GPU_TEXTURE_FREE_SAFE(slot->texture);
  /* Free the display-space texture of this slot too. */
  GPU_TEXTURE_FREE_SAFE(slot->display_texture);
  slot->texture = texture;
}

void preview_cache_set_gpu_display_texture(Scene *scene,
                                           int timeline_frame,
                                           int display_channel,
                                           gpu::Texture *texture)
{
  PreviewCache *cache = ensure_preview_cache(scene);
  if (cache == nullptr || texture == nullptr) {
    return;
  }
  PreviewCacheItem *slot = find_slot(cache, timeline_frame, display_channel);
  if (slot == nullptr) {
    return;
  }

  slot->timeline_frame = timeline_frame;
  slot->display_channel = display_channel;
  slot->last_used = cache->tick_count;
  GPU_TEXTURE_FREE_SAFE(slot->display_texture);
  slot->display_texture = texture;
}

void preview_cache_invalidate(Scene *scene)
{
  PreviewCache *cache = query_preview_cache(scene);
  if (cache != nullptr) {
    cache->clear();
  }
}

void preview_cache_destroy(Scene *scene)
{
  PreviewCache *cache = query_preview_cache(scene);
  if (cache != nullptr) {
    MEM_SAFE_DELETE(scene->ed->runtime.preview_cache);
  }
}

}  // namespace blender::seq
