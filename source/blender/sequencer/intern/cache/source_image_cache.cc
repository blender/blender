/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_vector.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_time.hh"

#include "prefetch.hh"
#include "source_image_cache.hh"

namespace blender::seq {

static Mutex source_image_cache_mutex;

struct SourceImageCache {
  struct FrameEntry {
    ImBuf *image = nullptr;
    /**
     * Frame in timeline, relative to strip start. Used to determine which
     * entries to evict (furthest from the play-head). Due to reversed
     * frames, playback rate, retiming the relationship between source frame
     * index and timeline frame is not a simple one.
     */
    float strip_frame = 0;
  };

  struct StripEntry {
    /** Map key is {source media frame index (i.e. movie frame), view ID}. */
    Map<std::pair<float, int>, FrameEntry> frames;
  };

  Map<const Strip *, StripEntry> map_;

  ~SourceImageCache()
  {
    clear();
  }

  void clear()
  {
    for (const auto &item : map_.items()) {
      for (const auto &frame : item.value.frames.values()) {
        IMB_freeImBuf(frame.image);
      }
    }
    map_.clear();
  }

  void remove_entry(const Strip *strip)
  {
    StripEntry *entry = map_.lookup_ptr(strip);
    if (entry == nullptr) {
      return;
    }
    for (const auto &frame : entry->frames.values()) {
      IMB_freeImBuf(frame.image);
    }
    map_.remove_contained(strip);
  }
};

static SourceImageCache *ensure_source_image_cache(Scene *scene)
{
  SourceImageCache **cache = &scene->ed->runtime.source_image_cache;
  if (*cache == nullptr) {
    *cache = MEM_new<SourceImageCache>(__func__);
  }
  return *cache;
}

static SourceImageCache *query_source_image_cache(const Scene *scene)
{
  if (scene == nullptr || scene->ed == nullptr) {
    return nullptr;
  }
  return scene->ed->runtime.source_image_cache;
}

static float give_cache_frame_index(const Scene *scene, const Strip *strip, float timeline_frame)
{
  float frame_index = give_frame_index(scene, strip, timeline_frame);
  if (strip->type != STRIP_TYPE_SCENE) {
    /* Scene strips that are slowed down need fractional frame index for animation interpolation;
     * for others use integer index for better cache hit rates. */
    frame_index = std::trunc(frame_index);
  }
  if (strip->type == STRIP_TYPE_MOVIE) {
    frame_index += strip->anim_startofs;
  }
  return frame_index;
}

ImBuf *source_image_cache_get(const RenderData *context, const Strip *strip, float timeline_frame)
{
  if (context->skip_cache || context->is_proxy_render || strip == nullptr) {
    return nullptr;
  }

  Scene *scene = prefetch_get_original_scene_and_strip(context, strip);
  timeline_frame = math::round(timeline_frame);
  const float frame_index = give_cache_frame_index(scene, strip, timeline_frame);
  const int view_id = context->view_id;

  ImBuf *res = nullptr;
  {
    std::lock_guard lock(source_image_cache_mutex);
    SourceImageCache *cache = query_source_image_cache(scene);
    if (cache == nullptr) {
      return nullptr;
    }

    SourceImageCache::StripEntry *val = cache->map_.lookup_ptr(strip);
    if (val == nullptr) {
      /* Nothing in cache for this strip yet. */
      return nullptr;
    }
    /* Search entries for the frame we want. */
    SourceImageCache::FrameEntry *frame = val->frames.lookup_ptr({frame_index, view_id});
    if (frame != nullptr) {
      res = frame->image;
    }

    /* For effect and scene strips, check if the cached result matches our current
     * render resolution. If it does not, remove stale source entries for this strip. */
    if (res != nullptr && (strip->is_effect() || strip->type == STRIP_TYPE_SCENE)) {
      if (res->x != context->rectx || res->y != context->recty) {
        cache->remove_entry(strip);
        return nullptr;
      }
    }
  }

  if (res) {
    IMB_refImBuf(res);
  }
  return res;
}

void source_image_cache_put(const RenderData *context,
                            const Strip *strip,
                            float timeline_frame,
                            ImBuf *image)
{
  if (context->skip_cache || context->is_proxy_render || strip == nullptr || image == nullptr) {
    return;
  }

  Scene *scene = prefetch_get_original_scene_and_strip(context, strip);
  timeline_frame = math::round(timeline_frame);

  const float frame_index = give_cache_frame_index(scene, strip, timeline_frame);
  const int view_id = context->view_id;

  IMB_refImBuf(image);

  std::lock_guard lock(source_image_cache_mutex);
  SourceImageCache *cache = ensure_source_image_cache(scene);

  SourceImageCache::StripEntry *val = cache->map_.lookup_ptr(strip);

  if (val == nullptr) {
    /* Nothing in cache for this strip yet. */
    cache->map_.add_new(strip, {});
    val = cache->map_.lookup_ptr(strip);
  }
  BLI_assert_msg(val != nullptr, "Source image cache value should never be null here");

  SourceImageCache::FrameEntry &frame = val->frames.lookup_or_add_default({frame_index, view_id});
  if (frame.image != nullptr) {
    IMB_freeImBuf(frame.image);
  }
  frame.strip_frame = timeline_frame - strip->start;
  frame.image = image;
}

void source_image_cache_invalidate_strip(Scene *scene, const Strip *strip)
{
  std::lock_guard lock(source_image_cache_mutex);
  SourceImageCache *cache = query_source_image_cache(scene);
  if (cache != nullptr) {
    cache->remove_entry(strip);
  }
}

void source_image_cache_clear(Scene *scene)
{
  std::lock_guard lock(source_image_cache_mutex);
  SourceImageCache *cache = query_source_image_cache(scene);
  if (cache != nullptr) {
    scene->ed->runtime.source_image_cache->clear();
  }
}

void source_image_cache_destroy(Scene *scene)
{
  std::lock_guard lock(source_image_cache_mutex);
  SourceImageCache *cache = query_source_image_cache(scene);
  if (cache != nullptr) {
    BLI_assert(cache == scene->ed->runtime.source_image_cache);
    MEM_delete(scene->ed->runtime.source_image_cache);
    scene->ed->runtime.source_image_cache = nullptr;
  }
}

void source_image_cache_iterate(Scene *scene,
                                void *userdata,
                                void callback_iter(void *userdata,
                                                   const Strip *strip,
                                                   int timeline_frame))
{
  std::lock_guard lock(source_image_cache_mutex);
  SourceImageCache *cache = query_source_image_cache(scene);
  if (cache == nullptr) {
    return;
  }

  for (const auto &[strip, frames] : cache->map_.items()) {
    for (const auto &[frame_key, frame] : frames.frames.items()) {
      const float timeline_frame = strip->start + frame.strip_frame;
      callback_iter(userdata, strip, int(timeline_frame));
    }
  }
}

size_t source_image_cache_calc_memory_size(const Scene *scene)
{
  std::lock_guard lock(source_image_cache_mutex);
  SourceImageCache *cache = query_source_image_cache(scene);
  if (cache == nullptr) {
    return 0;
  }
  size_t size = 0;
  for (const SourceImageCache::StripEntry &entry : cache->map_.values()) {
    for (const SourceImageCache::FrameEntry &frame : entry.frames.values()) {
      size += IMB_get_size_in_memory(frame.image);
    }
  }
  return size;
}

size_t source_image_cache_get_image_count(const Scene *scene)
{
  std::lock_guard lock(source_image_cache_mutex);
  SourceImageCache *cache = query_source_image_cache(scene);
  if (cache == nullptr) {
    return 0;
  }
  size_t count = 0;
  for (const SourceImageCache::StripEntry &entry : cache->map_.values()) {
    count += entry.frames.size();
  }
  return count;
}

bool source_image_cache_evict(Scene *scene)
{
  std::lock_guard lock(source_image_cache_mutex);
  SourceImageCache *cache = query_source_image_cache(scene);
  if (cache == nullptr) {
    return false;
  }
  /* Find which entry to remove -- we pick the one that is furthest from the current frame,
   * biasing the ones that are behind the current frame.
   *
   * However, do not try to evict entries from the current prefetch job range -- we need to
   * be able to fully fill the cache from prefetching, and then actually stop the job when it
   * is full and no longer can evict anything. */
  int cur_prefetch_start = std::numeric_limits<int>::min();
  int cur_prefetch_end = std::numeric_limits<int>::min();
  if (scene->ed->cache_flag & SEQ_CACHE_STORE_RAW) {
    /* Only activate the prefetch guards if the cache is active. */
    seq_prefetch_get_time_range(scene, &cur_prefetch_start, &cur_prefetch_end);
  }
  const bool prefetch_loops_around = cur_prefetch_start > cur_prefetch_end;

  const int timeline_start = PSFRA;
  const int timeline_end = PEFRA;
  /* If we wrap around, treat the timeline start as the playback head position.
   * This is to try to mitigate un-needed cache evictions. */
  const int cur_frame = prefetch_loops_around ? timeline_start : scene->r.cfra;

  SourceImageCache::StripEntry *best_strip = nullptr;
  std::pair<float, int> best_key = {};
  int best_score = 0;
  for (const auto &strip : cache->map_.items()) {
    for (const auto &entry : strip.value.frames.items()) {
      const int item_frame = int(strip.key->start + entry.value.strip_frame);
      if (prefetch_loops_around) {
        if (item_frame >= timeline_start && item_frame <= cur_prefetch_end) {
          continue; /* Within active prefetch range, do not try to remove it. */
        }
        if (item_frame >= cur_prefetch_start && item_frame <= timeline_end) {
          continue; /* Within active prefetch range, do not try to remove it. */
        }
      }
      else if (item_frame >= cur_prefetch_start && item_frame <= cur_prefetch_end) {
        continue; /* Within active prefetch range, do not try to remove it. */
      }

      /* Score for removal is distance to current frame; 2x that if behind current frame. */
      int score = 0;
      if (item_frame < cur_frame) {
        score = (cur_frame - item_frame) * 2;
      }
      else if (item_frame > cur_frame) {
        score = item_frame - cur_frame;
      }
      if (score > best_score) {
        best_strip = &strip.value;
        best_key = entry.key;
        best_score = score;
      }
    }
  }

  /* Remove if we found one. */
  if (best_strip != nullptr) {
    IMB_freeImBuf(best_strip->frames.lookup(best_key).image);
    best_strip->frames.remove(best_key);
    return true;
  }

  return false;
}

}  // namespace blender::seq
