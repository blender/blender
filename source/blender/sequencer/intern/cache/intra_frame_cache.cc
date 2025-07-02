/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "BLI_map.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "IMB_imbuf.hh"

#include "intra_frame_cache.hh"

namespace blender::seq {

struct StripImageMap {
  Map<const Strip *, ImBuf *> map_;
  ImBuf *get(const Strip *strip) const;
  void put(const Strip *strip, ImBuf *image);
  void invalidate(const Strip *strip);
  void clear();
};

struct IntraFrameCache {
  StripImageMap preprocessed;
  StripImageMap composite;
  float timeline_frame = -1.0f;
  int view_id = -1;
  int width = -1;
  int height = -1;

  ~IntraFrameCache()
  {
    preprocessed.clear();
    composite.clear();
  }
};

static IntraFrameCache *query_intra_frame_cache(Scene *scene)
{
  if (scene == nullptr || scene->ed == nullptr) {
    return nullptr;
  }
  return scene->ed->runtime.intra_frame_cache;
}

void intra_frame_cache_invalidate(Scene *scene)
{
  IntraFrameCache *cache = query_intra_frame_cache(scene);
  if (cache != nullptr) {
    cache->preprocessed.clear();
    cache->composite.clear();
    cache->timeline_frame = -1.0f;
    cache->view_id = -1;
    cache->width = -1;
    cache->height = -1;
  }
}

void intra_frame_cache_invalidate(Scene *scene, const Strip *strip)
{
  if (strip == nullptr) {
    return;
  }
  IntraFrameCache *cache = query_intra_frame_cache(scene);
  if (cache != nullptr) {
    cache->preprocessed.invalidate(strip);
    cache->composite.invalidate(strip);
  }
}

void StripImageMap::invalidate(const Strip *strip)
{
  /* Invalidate this strip, and all strips that are above it. */
  for (auto it = this->map_.items().begin(); it != this->map_.items().end(); it++) {
    const Strip *key = (*it).key;
    if (key == strip || key->channel >= strip->channel) {
      IMB_freeImBuf((*it).value);
      this->map_.remove(it);
    }
  }
}

ImBuf *StripImageMap::get(const Strip *strip) const
{
  ImBuf *image = this->map_.lookup_default(strip, nullptr);
  if (image != nullptr) {
    IMB_refImBuf(image);
  }
  return image;
}

void StripImageMap::put(const Strip *strip, ImBuf *image)
{
  BLI_assert(strip != nullptr);
  if (image == nullptr) {
    return;
  }
  ImBuf *existing = this->map_.lookup_default(strip, nullptr);
  if (existing != nullptr) {
    IMB_freeImBuf(existing);
  }
  this->map_.add_overwrite(strip, image);
  IMB_refImBuf(image);
}

void StripImageMap::clear()
{
  for (const auto &item : this->map_.items()) {
    IMB_freeImBuf(item.value);
  }
  this->map_.clear();
}

ImBuf *intra_frame_cache_get_preprocessed(Scene *scene, const Strip *strip)
{
  IntraFrameCache *cache = query_intra_frame_cache(scene);
  if (strip == nullptr || cache == nullptr) {
    return nullptr;
  }
  return cache->preprocessed.get(strip);
}

ImBuf *intra_frame_cache_get_composite(Scene *scene, const Strip *strip)
{
  IntraFrameCache *cache = query_intra_frame_cache(scene);
  if (strip == nullptr || cache == nullptr) {
    return nullptr;
  }
  return cache->composite.get(strip);
}

void intra_frame_cache_put_preprocessed(Scene *scene, const Strip *strip, ImBuf *image)
{
  if (scene == nullptr || scene->ed == nullptr || strip == nullptr || image == nullptr) {
    return;
  }
  IntraFrameCache *&cache = scene->ed->runtime.intra_frame_cache;
  if (cache == nullptr) {
    cache = MEM_new<IntraFrameCache>(__func__);
  }
  cache->preprocessed.put(strip, image);
}

void intra_frame_cache_put_composite(Scene *scene, const Strip *strip, ImBuf *image)
{
  if (scene == nullptr || scene->ed == nullptr || strip == nullptr || image == nullptr) {
    return;
  }
  IntraFrameCache *&cache = scene->ed->runtime.intra_frame_cache;
  if (cache == nullptr) {
    cache = MEM_new<IntraFrameCache>(__func__);
  }
  cache->composite.put(strip, image);
}

void intra_frame_cache_destroy(Scene *scene)
{
  IntraFrameCache *cache = query_intra_frame_cache(scene);
  if (cache != nullptr) {
    MEM_SAFE_DELETE(scene->ed->runtime.intra_frame_cache);
  }
}

void intra_frame_cache_set_cur_frame(Scene *scene, float frame, int view_id, int width, int height)
{
  IntraFrameCache *cache = query_intra_frame_cache(scene);
  if (cache != nullptr) {
    if (cache->timeline_frame != frame || cache->view_id != view_id || cache->width != width ||
        cache->height != height)
    {
      cache->timeline_frame = frame;
      cache->view_id = view_id;
      cache->width = width;
      cache->height = height;
      cache->preprocessed.clear();
      cache->composite.clear();
    }
  }
}

}  // namespace blender::seq
