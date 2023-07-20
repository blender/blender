/* SPDX-FileCopyrightText: 2010 Peter Schlaile <peter [at] schlaile [dot] de>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <memory.h>
#include <stddef.h>
#include <time.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h" /* for FILE_MAX. */

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_blenlib.h"
#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_path_util.h"
#include "BLI_threads.h"

#include "BKE_main.h"
#include "BKE_scene.h"

#include "SEQ_prefetch.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"

#include "disk_cache.h"
#include "image_cache.h"
#include "prefetch.h"
#include "strip_time.h"

/**
 * Sequencer Cache Design Notes
 * ============================
 *
 * Function:
 * All images created during rendering are added to cache, even if the cache is already full.
 * This is because:
 * - One image may be needed multiple times during rendering.
 * - Keeping the last rendered frame allows us for faster re-render when user edits strip in stack.
 * - We can decide if we keep frame only when it's completely rendered. Otherwise we risk having
 *   "holes" in the cache, which can be annoying.
 *
 * If the cache is full all entries for pending frame will have is_temp_cache set.
 *
 * Linking: We use links to reduce number of iterations over entries needed to manage cache.
 * Entries are linked in order as they are put into cache.
 * Only permanent (is_temp_cache = 0) cache entries are linked.
 * Putting #SEQ_CACHE_STORE_FINAL_OUT will reset linking
 *
 * Only entire frame can be freed to release resources for new entries (recycling).
 * Once again, this is to reduce number of iterations, but also more controllable than removing
 * entries one by one in reverse order to their creation.
 *
 * User can exclude caching of some images. Such entries will have is_temp_cache set.
 */

#define THUMB_CACHE_LIMIT 5000

struct SeqCache {
  Main *bmain;
  GHash *hash;
  ThreadMutex iterator_mutex;
  BLI_mempool *keys_pool;
  BLI_mempool *items_pool;
  SeqCacheKey *last_key;
  SeqDiskCache *disk_cache;
  int thumbnail_count;
};

struct SeqCacheItem {
  SeqCache *cache_owner;
  ImBuf *ibuf;
};

static ThreadMutex cache_create_lock = BLI_MUTEX_INITIALIZER;

static bool seq_cmp_render_data(const SeqRenderData *a, const SeqRenderData *b)
{
  return ((a->preview_render_size != b->preview_render_size) || (a->rectx != b->rectx) ||
          (a->recty != b->recty) || (a->bmain != b->bmain) || (a->scene != b->scene) ||
          (a->motion_blur_shutter != b->motion_blur_shutter) ||
          (a->motion_blur_samples != b->motion_blur_samples) ||
          (a->scene->r.views_format != b->scene->r.views_format) || (a->view_id != b->view_id));
}

static uint seq_hash_render_data(const SeqRenderData *a)
{
  uint rval = a->rectx + a->recty;

  rval ^= a->preview_render_size;
  rval ^= intptr_t(a->bmain) << 6;
  rval ^= intptr_t(a->scene) << 6;
  rval ^= int(a->motion_blur_shutter * 100.0f) << 10;
  rval ^= a->motion_blur_samples << 16;
  rval ^= ((a->scene->r.views_format * 2) + a->view_id) << 24;

  return rval;
}

static uint seq_cache_hashhash(const void *key_)
{
  const SeqCacheKey *key = static_cast<const SeqCacheKey *>(key_);
  uint rval = seq_hash_render_data(&key->context);

  rval ^= *(const uint *)&key->frame_index;
  rval += key->type;
  rval ^= intptr_t(key->seq) << 6;

  return rval;
}

static bool seq_cache_hashcmp(const void *a_, const void *b_)
{
  const SeqCacheKey *a = static_cast<const SeqCacheKey *>(a_);
  const SeqCacheKey *b = static_cast<const SeqCacheKey *>(b_);

  return ((a->seq != b->seq) || (a->frame_index != b->frame_index) || (a->type != b->type) ||
          seq_cmp_render_data(&a->context, &b->context));
}

static float seq_cache_timeline_frame_to_frame_index(Scene *scene,
                                                     Sequence *seq,
                                                     float timeline_frame,
                                                     int type)
{
  /* With raw images, map timeline_frame to strip input media frame range. This means that static
   * images or extended frame range of movies will only generate one cache entry. No special
   * treatment in converting frame index to timeline_frame is needed. */
  if (ELEM(type, SEQ_CACHE_STORE_RAW, SEQ_CACHE_STORE_THUMBNAIL)) {
    return SEQ_give_frame_index(scene, seq, timeline_frame);
  }

  return timeline_frame - SEQ_time_start_frame_get(seq);
}

float seq_cache_frame_index_to_timeline_frame(Sequence *seq, float frame_index)
{
  return frame_index + SEQ_time_start_frame_get(seq);
}

static SeqCache *seq_cache_get_from_scene(Scene *scene)
{
  if (scene && scene->ed && scene->ed->cache) {
    return scene->ed->cache;
  }

  return nullptr;
}

static void seq_cache_lock(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);

  if (cache) {
    BLI_mutex_lock(&cache->iterator_mutex);
  }
}

static void seq_cache_unlock(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);

  if (cache) {
    BLI_mutex_unlock(&cache->iterator_mutex);
  }
}

static size_t seq_cache_get_mem_total()
{
  return (size_t(U.memcachelimit)) * 1024 * 1024;
}

static void seq_cache_keyfree(void *val)
{
  SeqCacheKey *key = static_cast<SeqCacheKey *>(val);
  BLI_mempool_free(key->cache_owner->keys_pool, key);
}

static void seq_cache_valfree(void *val)
{
  SeqCacheItem *item = (SeqCacheItem *)val;

  if (item->ibuf) {
    IMB_freeImBuf(item->ibuf);
  }

  BLI_mempool_free(item->cache_owner->items_pool, item);
}

static int get_stored_types_flag(Scene *scene, SeqCacheKey *key)
{
  int flag;
  if (key->seq->cache_flag & SEQ_CACHE_OVERRIDE) {
    flag = key->seq->cache_flag;
  }
  else {
    flag = scene->ed->cache_flag;
  }

  /* SEQ_CACHE_STORE_FINAL_OUT can not be overridden by strip cache */
  flag |= (scene->ed->cache_flag & SEQ_CACHE_STORE_FINAL_OUT);

  return flag;
}

static void seq_cache_put_ex(Scene *scene, SeqCacheKey *key, ImBuf *ibuf)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  SeqCacheItem *item;
  item = static_cast<SeqCacheItem *>(BLI_mempool_alloc(cache->items_pool));
  item->cache_owner = cache;
  item->ibuf = ibuf;

  const int stored_types_flag = get_stored_types_flag(scene, key);

  /* Item stored for later use. */
  if (stored_types_flag & key->type) {
    key->is_temp_cache = false;
    key->link_prev = cache->last_key;
  }

  BLI_assert(!BLI_ghash_haskey(cache->hash, key));
  BLI_ghash_insert(cache->hash, key, item);
  IMB_refImBuf(ibuf);

  /* Store pointer to last cached key. */
  SeqCacheKey *temp_last_key = cache->last_key;

  if (!key->is_temp_cache || key->type != SEQ_CACHE_STORE_THUMBNAIL) {
    cache->last_key = key;
  }

  /* Set last_key's reference to this key so we can look up chain backwards.
   * Item is already put in cache, so cache->last_key points to current key.
   */
  if (!key->is_temp_cache && temp_last_key) {
    temp_last_key->link_next = cache->last_key;
  }

  /* Reset linking. */
  if (key->type == SEQ_CACHE_STORE_FINAL_OUT) {
    cache->last_key = nullptr;
  }
}

static ImBuf *seq_cache_get_ex(SeqCache *cache, SeqCacheKey *key)
{
  SeqCacheItem *item = static_cast<SeqCacheItem *>(BLI_ghash_lookup(cache->hash, key));

  if (item && item->ibuf) {
    IMB_refImBuf(item->ibuf);

    return item->ibuf;
  }

  return nullptr;
}

static void seq_cache_key_unlink(SeqCacheKey *key)
{
  if (key->link_next) {
    BLI_assert(key == key->link_next->link_prev);
    key->link_next->link_prev = key->link_prev;
  }
  if (key->link_prev) {
    BLI_assert(key == key->link_prev->link_next);
    key->link_prev->link_next = key->link_next;
  }
}

/* Choose a key out of 2 candidates(leftmost and rightmost items)
 * to recycle based on currently used strategy */
static SeqCacheKey *seq_cache_choose_key(Scene *scene, SeqCacheKey *lkey, SeqCacheKey *rkey)
{
  SeqCacheKey *finalkey = nullptr;

  /* Ideally, cache would not need to check the state of prefetching task
   * that is tricky to do however, because prefetch would need to know,
   * if a key, that is about to be created would be removed by itself.
   *
   * This can happen because only FINAL_OUT item insertion will trigger recycling
   * but that is also the point, where prefetch can be suspended.
   *
   * We could use temp cache as a shield and later make it a non-temporary entry,
   * but it is not worth of increasing system complexity.
   */
  if (scene->ed->cache_flag & SEQ_CACHE_PREFETCH_ENABLE && seq_prefetch_job_is_running(scene)) {
    int pfjob_start, pfjob_end;
    seq_prefetch_get_time_range(scene, &pfjob_start, &pfjob_end);

    if (lkey) {
      if (lkey->timeline_frame < pfjob_start || lkey->timeline_frame > pfjob_end) {
        return lkey;
      }
    }

    if (rkey) {
      if (rkey->timeline_frame < pfjob_start || rkey->timeline_frame > pfjob_end) {
        return rkey;
      }
    }

    return nullptr;
  }

  if (rkey && lkey) {
    if (lkey->timeline_frame > rkey->timeline_frame) {
      SeqCacheKey *swapkey = lkey;
      lkey = rkey;
      rkey = swapkey;
    }

    int l_diff = scene->r.cfra - lkey->timeline_frame;
    int r_diff = rkey->timeline_frame - scene->r.cfra;

    if (l_diff > r_diff) {
      finalkey = lkey;
    }
    else {
      finalkey = rkey;
    }
  }
  else {
    if (lkey) {
      finalkey = lkey;
    }
    else {
      finalkey = rkey;
    }
  }
  return finalkey;
}

static void seq_cache_recycle_linked(Scene *scene, SeqCacheKey *base)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  SeqCacheKey *next = base->link_next;

  while (base) {
    if (!BLI_ghash_haskey(cache->hash, base)) {
      break; /* Key has already been removed from cache. */
    }

    SeqCacheKey *prev = base->link_prev;
    if (prev != nullptr && prev->link_next != base) {
      /* Key has been removed and replaced and doesn't belong to this chain anymore. */
      base->link_prev = nullptr;
      break;
    }

    seq_cache_key_unlink(base);
    BLI_ghash_remove(cache->hash, base, seq_cache_keyfree, seq_cache_valfree);
    BLI_assert(base != cache->last_key);
    base = prev;
  }

  base = next;
  while (base) {
    if (!BLI_ghash_haskey(cache->hash, base)) {
      break; /* Key has already been removed from cache. */
    }

    next = base->link_next;
    if (next != nullptr && next->link_prev != base) {
      /* Key has been removed and replaced and doesn't belong to this chain anymore. */
      base->link_next = nullptr;
      break;
    }

    seq_cache_key_unlink(base);
    BLI_ghash_remove(cache->hash, base, seq_cache_keyfree, seq_cache_valfree);
    BLI_assert(base != cache->last_key);
    base = next;
  }
}

static SeqCacheKey *seq_cache_get_item_for_removal(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  SeqCacheKey *finalkey = nullptr;
  /* Leftmost key. */
  SeqCacheKey *lkey = nullptr;
  /* Rightmost key. */
  SeqCacheKey *rkey = nullptr;
  SeqCacheKey *key = nullptr;

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  int total_count = 0;

  while (!BLI_ghashIterator_done(&gh_iter)) {
    key = static_cast<SeqCacheKey *>(BLI_ghashIterator_getKey(&gh_iter));
    SeqCacheItem *item = static_cast<SeqCacheItem *>(BLI_ghashIterator_getValue(&gh_iter));
    BLI_ghashIterator_step(&gh_iter);
    BLI_assert(key->cache_owner == cache);

    /* This shouldn't happen, but better be safe than sorry. */
    if (!item->ibuf) {
      seq_cache_recycle_linked(scene, key);
      /* Can not continue iterating after linked remove. */
      BLI_ghashIterator_init(&gh_iter, cache->hash);
      continue;
    }

    if (key->is_temp_cache || key->link_next != nullptr) {
      continue;
    }

    total_count++;

    if (lkey) {
      if (key->timeline_frame < lkey->timeline_frame) {
        lkey = key;
      }
    }
    else {
      lkey = key;
    }
    if (rkey) {
      if (key->timeline_frame > rkey->timeline_frame) {
        rkey = key;
      }
    }
    else {
      rkey = key;
    }
  }
  (void)total_count; /* Quiet set-but-unused warning (may be removed). */

  finalkey = seq_cache_choose_key(scene, lkey, rkey);

  return finalkey;
}

bool seq_cache_recycle_item(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return false;
  }

  seq_cache_lock(scene);

  while (seq_cache_is_full()) {
    SeqCacheKey *finalkey = seq_cache_get_item_for_removal(scene);

    if (finalkey) {
      seq_cache_recycle_linked(scene, finalkey);
    }
    else {
      seq_cache_unlock(scene);
      return false;
    }
  }
  seq_cache_unlock(scene);
  return true;
}

static void seq_cache_set_temp_cache_linked(Scene *scene, SeqCacheKey *base)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);

  if (!cache || !base) {
    return;
  }

  SeqCacheKey *next = base->link_next;

  while (base) {
    SeqCacheKey *prev = base->link_prev;
    base->is_temp_cache = true;
    base = prev;
  }

  base = next;
  while (base) {
    next = base->link_next;
    base->is_temp_cache = true;
    base = next;
  }
}

static void seq_cache_create(Main *bmain, Scene *scene)
{
  BLI_mutex_lock(&cache_create_lock);
  if (scene->ed->cache == nullptr) {
    SeqCache *cache = static_cast<SeqCache *>(MEM_callocN(sizeof(SeqCache), "SeqCache"));
    cache->keys_pool = BLI_mempool_create(sizeof(SeqCacheKey), 0, 64, BLI_MEMPOOL_NOP);
    cache->items_pool = BLI_mempool_create(sizeof(SeqCacheItem), 0, 64, BLI_MEMPOOL_NOP);
    cache->hash = BLI_ghash_new(seq_cache_hashhash, seq_cache_hashcmp, "SeqCache hash");
    cache->last_key = nullptr;
    cache->bmain = bmain;
    cache->thumbnail_count = 0;
    BLI_mutex_init(&cache->iterator_mutex);
    scene->ed->cache = cache;

    if (scene->ed->disk_cache_timestamp == 0) {
      scene->ed->disk_cache_timestamp = time(nullptr);
    }
  }
  BLI_mutex_unlock(&cache_create_lock);
}

static void seq_cache_populate_key(SeqCacheKey *key,
                                   const SeqRenderData *context,
                                   Sequence *seq,
                                   const float timeline_frame,
                                   const int type)
{
  key->cache_owner = seq_cache_get_from_scene(context->scene);
  key->seq = seq;
  key->context = *context;
  key->frame_index = seq_cache_timeline_frame_to_frame_index(
      context->scene, seq, timeline_frame, type);
  key->timeline_frame = timeline_frame;
  key->type = type;
  key->link_prev = nullptr;
  key->link_next = nullptr;
  key->is_temp_cache = true;
  key->task_id = context->task_id;
}

static SeqCacheKey *seq_cache_allocate_key(SeqCache *cache,
                                           const SeqRenderData *context,
                                           Sequence *seq,
                                           const float timeline_frame,
                                           const int type)
{
  SeqCacheKey *key = static_cast<SeqCacheKey *>(BLI_mempool_alloc(cache->keys_pool));
  seq_cache_populate_key(key, context, seq, timeline_frame, type);
  return key;
}

/* ***************************** API ****************************** */

void seq_cache_free_temp_cache(Scene *scene, short id, int timeline_frame)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  seq_cache_lock(scene);

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  while (!BLI_ghashIterator_done(&gh_iter)) {
    SeqCacheKey *key = static_cast<SeqCacheKey *>(BLI_ghashIterator_getKey(&gh_iter));
    BLI_ghashIterator_step(&gh_iter);
    BLI_assert(key->cache_owner == cache);

    if (key->is_temp_cache && key->task_id == id && key->type != SEQ_CACHE_STORE_THUMBNAIL) {
      /* Use frame_index here to avoid freeing raw images if they are used for multiple frames. */
      float frame_index = seq_cache_timeline_frame_to_frame_index(
          scene, key->seq, timeline_frame, key->type);
      if (frame_index != key->frame_index ||
          timeline_frame > SEQ_time_right_handle_frame_get(scene, key->seq) ||
          timeline_frame < SEQ_time_left_handle_frame_get(scene, key->seq))
      {
        seq_cache_key_unlink(key);
        BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
        BLI_assert(key != cache->last_key);
      }
    }
  }
  seq_cache_unlock(scene);
}

void seq_cache_destruct(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  BLI_ghash_free(cache->hash, seq_cache_keyfree, seq_cache_valfree);
  BLI_mempool_destroy(cache->keys_pool);
  BLI_mempool_destroy(cache->items_pool);
  BLI_mutex_end(&cache->iterator_mutex);

  if (cache->disk_cache != nullptr) {
    seq_disk_cache_free(cache->disk_cache);
  }

  MEM_freeN(cache);
  scene->ed->cache = nullptr;
}

void seq_cache_cleanup_all(Main *bmain)
{
  for (Scene *scene = static_cast<Scene *>(bmain->scenes.first); scene != nullptr;
       scene = static_cast<Scene *>(scene->id.next))
  {
    SEQ_cache_cleanup(scene);
  }
}
void SEQ_cache_cleanup(Scene *scene)
{
  SEQ_prefetch_stop(scene);

  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  seq_cache_lock(scene);

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  while (!BLI_ghashIterator_done(&gh_iter)) {
    SeqCacheKey *key = static_cast<SeqCacheKey *>(BLI_ghashIterator_getKey(&gh_iter));
    BLI_assert(key->cache_owner == cache);

    BLI_ghashIterator_step(&gh_iter);

    /* NOTE: no need to call #seq_cache_key_unlink as all keys are removed. */
    BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
  }
  cache->last_key = nullptr;
  cache->thumbnail_count = 0;
  seq_cache_unlock(scene);
}

void seq_cache_cleanup_sequence(Scene *scene,
                                Sequence *seq,
                                Sequence *seq_changed,
                                int invalidate_types,
                                bool force_seq_changed_range)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  if (seq_disk_cache_is_enabled(cache->bmain) && cache->disk_cache != nullptr) {
    seq_disk_cache_invalidate(cache->disk_cache, scene, seq, seq_changed, invalidate_types);
  }

  seq_cache_lock(scene);

  int range_start = SEQ_time_left_handle_frame_get(scene, seq_changed);
  int range_end = SEQ_time_right_handle_frame_get(scene, seq_changed);

  if (!force_seq_changed_range) {
    range_start = max_ii(range_start, SEQ_time_left_handle_frame_get(scene, seq));
    range_end = min_ii(range_end, SEQ_time_right_handle_frame_get(scene, seq));
  }

  int invalidate_composite = invalidate_types & SEQ_CACHE_STORE_FINAL_OUT;
  int invalidate_source = invalidate_types & (SEQ_CACHE_STORE_RAW | SEQ_CACHE_STORE_PREPROCESSED |
                                              SEQ_CACHE_STORE_COMPOSITE);

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  while (!BLI_ghashIterator_done(&gh_iter)) {
    SeqCacheKey *key = static_cast<SeqCacheKey *>(BLI_ghashIterator_getKey(&gh_iter));
    BLI_ghashIterator_step(&gh_iter);
    BLI_assert(key->cache_owner == cache);

    /* Clean all final and composite in intersection of seq and seq_changed. */
    if (key->type & invalidate_composite && key->timeline_frame >= range_start &&
        key->timeline_frame <= range_end)
    {
      seq_cache_key_unlink(key);
      BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
    }
    else if (key->type & invalidate_source && key->seq == seq &&
             key->timeline_frame >= SEQ_time_left_handle_frame_get(scene, seq_changed) &&
             key->timeline_frame <= SEQ_time_right_handle_frame_get(scene, seq_changed))
    {
      seq_cache_key_unlink(key);
      BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
    }
  }
  cache->last_key = nullptr;
  seq_cache_unlock(scene);
}

void seq_cache_thumbnail_cleanup(Scene *scene, rctf *view_area_safe)
{
  /* Add offsets to the left and right end to keep some frames in cache. */
  view_area_safe->xmax += 200;
  view_area_safe->xmin -= 200;
  view_area_safe->ymin -= 1;
  view_area_safe->ymax += 1;

  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  while (!BLI_ghashIterator_done(&gh_iter)) {
    SeqCacheKey *key = static_cast<SeqCacheKey *>(BLI_ghashIterator_getKey(&gh_iter));
    BLI_ghashIterator_step(&gh_iter);
    BLI_assert(key->cache_owner == cache);

    const int frame_index = key->timeline_frame - SEQ_time_left_handle_frame_get(scene, key->seq);
    const int frame_step = SEQ_render_thumbnails_guaranteed_set_frame_step_get(scene, key->seq);
    const int relative_base_frame = round_fl_to_int(frame_index / float(frame_step)) * frame_step;
    const int nearest_guaranted_absolute_frame = relative_base_frame +
                                                 SEQ_time_left_handle_frame_get(scene, key->seq);

    if (nearest_guaranted_absolute_frame == key->timeline_frame) {
      continue;
    }

    if ((key->type & SEQ_CACHE_STORE_THUMBNAIL) &&
        (key->timeline_frame > view_area_safe->xmax ||
         key->timeline_frame < view_area_safe->xmin || key->seq->machine > view_area_safe->ymax ||
         key->seq->machine < view_area_safe->ymin))
    {
      seq_cache_key_unlink(key);
      BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
      cache->thumbnail_count--;
    }
  }
  cache->last_key = nullptr;
}

ImBuf *seq_cache_get(const SeqRenderData *context, Sequence *seq, float timeline_frame, int type)
{

  if (context->skip_cache || context->is_proxy_render || !seq) {
    return nullptr;
  }

  Scene *scene = context->scene;

  if (context->is_prefetch_render) {
    context = seq_prefetch_get_original_context(context);
    scene = context->scene;
    seq = seq_prefetch_get_original_sequence(seq, scene);
  }

  if (!seq) {
    return nullptr;
  }

  if (!scene->ed->cache) {
    seq_cache_create(context->bmain, scene);
  }

  seq_cache_lock(scene);
  SeqCache *cache = seq_cache_get_from_scene(scene);
  ImBuf *ibuf = nullptr;
  SeqCacheKey key;

  /* Try RAM cache: */
  if (cache && seq) {
    seq_cache_populate_key(&key, context, seq, timeline_frame, type);
    ibuf = seq_cache_get_ex(cache, &key);
  }
  seq_cache_unlock(scene);

  if (ibuf) {
    return ibuf;
  }

  /* Try disk cache: */
  if (seq_disk_cache_is_enabled(context->bmain)) {
    if (cache->disk_cache == nullptr) {
      cache->disk_cache = seq_disk_cache_create(context->bmain, context->scene);
    }

    ibuf = seq_disk_cache_read_file(cache->disk_cache, &key);

    if (ibuf == nullptr) {
      return nullptr;
    }

    /* Store read image in RAM. Only recycle item for final type. */
    if (key.type != SEQ_CACHE_STORE_FINAL_OUT || seq_cache_recycle_item(scene)) {
      SeqCacheKey *new_key = seq_cache_allocate_key(cache, context, seq, timeline_frame, type);
      seq_cache_put_ex(scene, new_key, ibuf);
    }
  }

  return ibuf;
}

bool seq_cache_put_if_possible(
    const SeqRenderData *context, Sequence *seq, float timeline_frame, int type, ImBuf *ibuf)
{
  Scene *scene = context->scene;

  if (context->is_prefetch_render) {
    context = seq_prefetch_get_original_context(context);
    scene = context->scene;
    seq = seq_prefetch_get_original_sequence(seq, scene);
  }

  if (!seq) {
    return false;
  }

  if (seq_cache_recycle_item(scene)) {
    seq_cache_put(context, seq, timeline_frame, type, ibuf);
    return true;
  }

  seq_cache_set_temp_cache_linked(scene, scene->ed->cache->last_key);
  scene->ed->cache->last_key = nullptr;
  return false;
}

void seq_cache_thumbnail_put(const SeqRenderData *context,
                             Sequence *seq,
                             float timeline_frame,
                             ImBuf *i,
                             const rctf *view_area)
{
  Scene *scene = context->scene;

  if (!scene->ed->cache) {
    seq_cache_create(context->bmain, scene);
  }

  seq_cache_lock(scene);
  SeqCache *cache = seq_cache_get_from_scene(scene);
  SeqCacheKey *key = seq_cache_allocate_key(
      cache, context, seq, timeline_frame, SEQ_CACHE_STORE_THUMBNAIL);

  /* Prevent reinserting, it breaks cache key linking. */
  if (BLI_ghash_haskey(cache->hash, key)) {
    seq_cache_unlock(scene);
    return;
  }

  /* Limit cache to THUMB_CACHE_LIMIT (5000) images stored. */
  if (cache->thumbnail_count >= THUMB_CACHE_LIMIT) {
    rctf view_area_safe = *view_area;
    seq_cache_thumbnail_cleanup(scene, &view_area_safe);
  }

  seq_cache_put_ex(scene, key, i);
  cache->thumbnail_count++;
  seq_cache_unlock(scene);
}

void seq_cache_put(
    const SeqRenderData *context, Sequence *seq, float timeline_frame, int type, ImBuf *i)
{
  if (i == nullptr || context->skip_cache || context->is_proxy_render || !seq) {
    return;
  }

  Scene *scene = context->scene;

  if (context->is_prefetch_render) {
    context = seq_prefetch_get_original_context(context);
    scene = context->scene;
    seq = seq_prefetch_get_original_sequence(seq, scene);
    BLI_assert(seq != nullptr);
  }

  /* Prevent reinserting, it breaks cache key linking. */
  ImBuf *test = seq_cache_get(context, seq, timeline_frame, type);
  if (test) {
    IMB_freeImBuf(test);
    return;
  }

  if (!scene->ed->cache) {
    seq_cache_create(context->bmain, scene);
  }

  seq_cache_lock(scene);
  SeqCache *cache = seq_cache_get_from_scene(scene);
  SeqCacheKey *key = seq_cache_allocate_key(cache, context, seq, timeline_frame, type);
  seq_cache_put_ex(scene, key, i);
  seq_cache_unlock(scene);

  if (!key->is_temp_cache) {
    if (seq_disk_cache_is_enabled(context->bmain)) {
      if (cache->disk_cache == nullptr) {
        seq_disk_cache_create(context->bmain, context->scene);
      }

      seq_disk_cache_write_file(cache->disk_cache, key, i);
      seq_disk_cache_enforce_limits(cache->disk_cache);
    }
  }
}

void SEQ_cache_iterate(
    Scene *scene,
    void *userdata,
    bool callback_init(void *userdata, size_t item_count),
    bool callback_iter(void *userdata, Sequence *seq, int timeline_frame, int cache_type))
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  seq_cache_lock(scene);
  bool interrupt = callback_init(userdata, BLI_ghash_len(cache->hash));

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);

  while (!BLI_ghashIterator_done(&gh_iter) && !interrupt) {
    SeqCacheKey *key = static_cast<SeqCacheKey *>(BLI_ghashIterator_getKey(&gh_iter));
    BLI_ghashIterator_step(&gh_iter);
    BLI_assert(key->cache_owner == cache);

    interrupt = callback_iter(userdata, key->seq, key->timeline_frame, key->type);
  }

  cache->last_key = nullptr;
  seq_cache_unlock(scene);
}

bool seq_cache_is_full()
{
  return seq_cache_get_mem_total() < MEM_get_memory_in_use();
}
