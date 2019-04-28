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
 * Peter Schlaile <peter [at] schlaile [dot] de> 2010
 */

/** \file
 * \ingroup bke
 */

#include <stddef.h>
#include <memory.h>

#include "MEM_guardedalloc.h"

#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_mempool.h"
#include "BLI_threads.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"

#include "BKE_sequencer.h"
#include "BKE_scene.h"
#include "BKE_main.h"

/* ***************************** Sequencer cache design notes ******************************
 *
 * Cache key members:
 * is_temp_cache - this cache entry will be freed before rendering next frame
 * creator_id - ID of thread that created entry
 * cost - In short: render time divided by playback frame rate
 * link_prev/next - link to another entry created during rendering of the frame
 *
 * Linking: We use links to reduce number of iterations needed to manage cache.
 * Entries are linked in order as they are put into cache.
 * Only pernament (is_temp_cache = 0) cache entries are linked.
 * Putting SEQ_CACHE_STORE_FINAL_OUT will reset linking
 *
 * Function:
 * All images created during rendering are added to cache, even if the cache is already full.
 * This is because:
 *  - one image may be needed multiple times during rendering.
 *  - keeping the last rendered frame allows us for faster re-render when user edits strip in stack
 *  - we can decide if we keep frame only when it's completely rendered. Otherwise we risk having
 *    "holes" in the cache, which can be annoying
 * If the cache is full all entries for pending frame will have is_temp_cache set.
 *
 * Only entire frame can be freed to release resources for new entries (recycling).
 * Once again, this is to reduce number of iterations, but also more controllable than removing
 * entries one by one in reverse order to their creation.
 *
 * User can exclude caching of some images. Such entries will have is_temp_cache set.
 */

typedef struct SeqCache {
  struct GHash *hash;
  ThreadMutex iterator_mutex;
  struct BLI_mempool *keys_pool;
  struct BLI_mempool *items_pool;
  struct SeqCacheKey *last_key;
  size_t memory_used;
} SeqCache;

typedef struct SeqCacheItem {
  struct SeqCache *cache_owner;
  struct ImBuf *ibuf;
} SeqCacheItem;

typedef struct SeqCacheKey {
  struct SeqCache *cache_owner;
  void *userkey;
  struct SeqCacheKey *link_prev; /* Used for linking intermediate items to final frame */
  struct SeqCacheKey *link_next; /* Used for linking intermediate items to final frame */
  struct Sequence *seq;
  SeqRenderData context;
  float cfra;
  float nfra;
  float cost;
  bool is_temp_cache;
  short creator_id;
  int type;
} SeqCacheKey;

static ThreadMutex cache_create_lock = BLI_MUTEX_INITIALIZER;

static bool seq_cmp_render_data(const SeqRenderData *a, const SeqRenderData *b)
{
  return ((a->preview_render_size != b->preview_render_size) || (a->rectx != b->rectx) ||
          (a->recty != b->recty) || (a->bmain != b->bmain) || (a->scene != b->scene) ||
          (a->motion_blur_shutter != b->motion_blur_shutter) ||
          (a->motion_blur_samples != b->motion_blur_samples) ||
          (a->scene->r.views_format != b->scene->r.views_format) || (a->view_id != b->view_id));
}

static unsigned int seq_hash_render_data(const SeqRenderData *a)
{
  unsigned int rval = a->rectx + a->recty;

  rval ^= a->preview_render_size;
  rval ^= ((intptr_t)a->bmain) << 6;
  rval ^= ((intptr_t)a->scene) << 6;
  rval ^= (int)(a->motion_blur_shutter * 100.0f) << 10;
  rval ^= a->motion_blur_samples << 16;
  rval ^= ((a->scene->r.views_format * 2) + a->view_id) << 24;

  return rval;
}

static unsigned int seq_cache_hashhash(const void *key_)
{
  const SeqCacheKey *key = key_;
  unsigned int rval = seq_hash_render_data(&key->context);

  rval ^= *(const unsigned int *)&key->nfra;
  rval += key->type;
  rval ^= ((intptr_t)key->seq) << 6;

  return rval;
}

static bool seq_cache_hashcmp(const void *a_, const void *b_)
{
  const SeqCacheKey *a = a_;
  const SeqCacheKey *b = b_;

  return ((a->seq != b->seq) || (a->nfra != b->nfra) || (a->type != b->type) ||
          seq_cmp_render_data(&a->context, &b->context));
}

static SeqCache *seq_cache_get_from_scene(Scene *scene)
{
  if (scene && scene->ed && scene->ed->cache) {
    return scene->ed->cache;
  }

  return NULL;
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

static void seq_cache_keyfree(void *val)
{
  SeqCacheKey *key = val;
  BLI_mempool_free(key->cache_owner->keys_pool, key);
}

static void seq_cache_valfree(void *val)
{
  SeqCacheItem *item = (SeqCacheItem *)val;
  SeqCache *cache = item->cache_owner;

  if (item->ibuf) {
    cache->memory_used -= IMB_get_size_in_memory(item->ibuf);
    IMB_freeImBuf(item->ibuf);
  }

  BLI_mempool_free(item->cache_owner->items_pool, item);
}

static void seq_cache_put(SeqCache *cache, SeqCacheKey *key, ImBuf *ibuf)
{
  SeqCacheItem *item;
  item = BLI_mempool_alloc(cache->items_pool);
  item->cache_owner = cache;
  item->ibuf = ibuf;

  if (BLI_ghash_reinsert(cache->hash, key, item, seq_cache_keyfree, seq_cache_valfree)) {
    IMB_refImBuf(ibuf);
    cache->last_key = key;
    cache->memory_used += IMB_get_size_in_memory(ibuf);
  }
}

static ImBuf *seq_cache_get(SeqCache *cache, void *key)
{
  SeqCacheItem *item = BLI_ghash_lookup(cache->hash, key);

  if (item && item->ibuf) {
    IMB_refImBuf(item->ibuf);

    return item->ibuf;
  }

  return NULL;
}

static void seq_cache_relink_keys(SeqCacheKey *link_next, SeqCacheKey *link_prev)
{
  if (link_next) {
    link_next->link_prev = link_prev;
  }
  if (link_prev) {
    link_prev->link_next = link_next;
  }
}

static SeqCacheKey *seq_cache_choose_key(Scene *scene, SeqCacheKey *lkey, SeqCacheKey *rkey)
{
  SeqCacheKey *finalkey = NULL;

  if (rkey && lkey) {
    if (lkey->cfra > rkey->cfra) {
      SeqCacheKey *swapkey = lkey;
      lkey = rkey;
      rkey = swapkey;
    }

    int l_diff = scene->r.cfra - lkey->cfra;
    int r_diff = rkey->cfra - scene->r.cfra;

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
    SeqCacheKey *prev = base->link_prev;
    BLI_ghash_remove(cache->hash, base, seq_cache_keyfree, seq_cache_valfree);
    base = prev;
  }

  base = next;
  while (base) {
    next = base->link_next;
    BLI_ghash_remove(cache->hash, base, seq_cache_keyfree, seq_cache_valfree);
    base = next;
  }
}

static SeqCacheKey *seq_cache_get_item_for_removal(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  SeqCacheKey *finalkey = NULL;
  /*leftmost key*/
  SeqCacheKey *lkey = NULL;
  /*rightmost key*/
  SeqCacheKey *rkey = NULL;
  SeqCacheKey *key = NULL;

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  int total_count = 0;
  int cheap_count = 0;

  while (!BLI_ghashIterator_done(&gh_iter)) {
    key = BLI_ghashIterator_getKey(&gh_iter);
    SeqCacheItem *item = BLI_ghashIterator_getValue(&gh_iter);
    BLI_ghashIterator_step(&gh_iter);

    /* this shouldn't happen, but better be safe than sorry */
    if (!item->ibuf) {
      seq_cache_recycle_linked(scene, key);
      /* can not continue iterating after linked remove */
      BLI_ghashIterator_init(&gh_iter, cache->hash);
      continue;
    }

    if (key->is_temp_cache || key->link_next != NULL) {
      continue;
    }

    total_count++;

    if (key->cost <= scene->ed->recycle_max_cost) {
      cheap_count++;
      if (lkey) {
        if (key->cfra < lkey->cfra) {
          lkey = key;
        }
      }
      else {
        lkey = key;
      }
      if (rkey) {
        if (key->cfra > rkey->cfra) {
          rkey = key;
        }
      }
      else {
        rkey = key;
      }
    }
  }

  finalkey = seq_cache_choose_key(scene, lkey, rkey);
  return finalkey;
}

/* Find only "base" keys
 * Sources(other types) for a frame must be freed all at once
 */
static bool seq_cache_recycle_item(Scene *scene)
{
  size_t memory_total = ((size_t)U.memcachelimit) * 1024 * 1024;
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return false;
  }

  seq_cache_lock(scene);

  while (cache->memory_used > memory_total) {
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

static void BKE_sequencer_cache_create(Scene *scene)
{
  BLI_mutex_lock(&cache_create_lock);
  if (scene->ed->cache == NULL) {
    SeqCache *cache = MEM_callocN(sizeof(SeqCache), "SeqCache");
    cache->keys_pool = BLI_mempool_create(sizeof(SeqCacheKey), 0, 64, BLI_MEMPOOL_NOP);
    cache->items_pool = BLI_mempool_create(sizeof(SeqCacheItem), 0, 64, BLI_MEMPOOL_NOP);
    cache->hash = BLI_ghash_new(seq_cache_hashhash, seq_cache_hashcmp, "SeqCache hash");
    cache->last_key = NULL;
    BLI_mutex_init(&cache->iterator_mutex);
    scene->ed->cache = cache;
  }
  BLI_mutex_unlock(&cache_create_lock);
}

/* ***************************** API ****************************** */

void BKE_sequencer_cache_free_temp_cache(Scene *scene, short id, int cfra)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  seq_cache_lock(scene);

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  while (!BLI_ghashIterator_done(&gh_iter)) {
    SeqCacheKey *key = BLI_ghashIterator_getKey(&gh_iter);
    BLI_ghashIterator_step(&gh_iter);

    if (key->is_temp_cache && key->creator_id == id && key->cfra != cfra) {
      BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
    }
  }
  seq_cache_unlock(scene);
}

void BKE_sequencer_cache_destruct(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  BLI_ghash_free(cache->hash, seq_cache_keyfree, seq_cache_valfree);
  BLI_mempool_destroy(cache->keys_pool);
  BLI_mempool_destroy(cache->items_pool);
  BLI_mutex_end(&cache->iterator_mutex);
  MEM_freeN(cache);
  scene->ed->cache = NULL;
}

void BKE_sequencer_cache_cleanup_all(Main *bmain)
{
  for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
    BKE_sequencer_cache_cleanup(scene);
  }
}
void BKE_sequencer_cache_cleanup(Scene *scene)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  seq_cache_lock(scene);

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  while (!BLI_ghashIterator_done(&gh_iter)) {
    SeqCacheKey *key = BLI_ghashIterator_getKey(&gh_iter);

    BLI_ghashIterator_step(&gh_iter);
    BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
  }
  cache->last_key = NULL;
  seq_cache_unlock(scene);
}

void BKE_sequencer_cache_cleanup_sequence(Scene *scene, Sequence *seq)
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  seq_cache_lock(scene);

  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  while (!BLI_ghashIterator_done(&gh_iter)) {
    SeqCacheKey *key = BLI_ghashIterator_getKey(&gh_iter);
    BLI_ghashIterator_step(&gh_iter);

    if (key->seq == seq) {
      /* Relink keys, so we don't end up with orphaned keys */
      if (key->link_next || key->link_prev) {
        seq_cache_relink_keys(key->link_next, key->link_prev);
      }

      BLI_ghash_remove(cache->hash, key, seq_cache_keyfree, seq_cache_valfree);
    }
  }
  cache->last_key = NULL;
  seq_cache_unlock(scene);
}

struct ImBuf *BKE_sequencer_cache_get(const SeqRenderData *context,
                                      Sequence *seq,
                                      float cfra,
                                      int type)
{
  Scene *scene = context->scene;

  if (!scene->ed->cache) {
    BKE_sequencer_cache_create(scene);
    return NULL;
  }

  seq_cache_lock(scene);
  SeqCache *cache = seq_cache_get_from_scene(scene);
  ImBuf *ibuf = NULL;

  if (cache && seq) {
    SeqCacheKey key;

    key.seq = seq;
    key.context = *context;
    key.nfra = cfra - seq->start;
    key.type = type;

    ibuf = seq_cache_get(cache, &key);
  }
  seq_cache_unlock(scene);

  return ibuf;
}

bool BKE_sequencer_cache_put_if_possible(
    const SeqRenderData *context, Sequence *seq, float cfra, int type, ImBuf *ibuf, float cost)
{
  Scene *scene = context->scene;

  if (seq_cache_recycle_item(scene)) {
    BKE_sequencer_cache_put(context, seq, cfra, type, ibuf, cost);
    return true;
  }
  else {
    seq_cache_set_temp_cache_linked(scene, scene->ed->cache->last_key);
    scene->ed->cache->last_key = NULL;
    return false;
  }
}

void BKE_sequencer_cache_put(
    const SeqRenderData *context, Sequence *seq, float cfra, int type, ImBuf *i, float cost)
{
  Scene *scene = context->scene;
  short creator_id = 0;

  if (i == NULL || context->skip_cache || context->is_proxy_render || !seq) {
    return;
  }

  /* Prevent reinserting, it breaks cache key linking */
  ImBuf *test = BKE_sequencer_cache_get(context, seq, cfra, type);
  if (test) {
    IMB_freeImBuf(test);
    return;
  }

  if (!scene->ed->cache) {
    BKE_sequencer_cache_create(scene);
  }

  seq_cache_lock(scene);

  SeqCache *cache = seq_cache_get_from_scene(scene);
  int flag;

  if (seq->cache_flag & SEQ_CACHE_OVERRIDE) {
    flag = seq->cache_flag;
    flag |= scene->ed->cache_flag & SEQ_CACHE_STORE_FINAL_OUT;
  }
  else {
    flag = scene->ed->cache_flag;
  }

  if (cost > SEQ_CACHE_COST_MAX) {
    cost = SEQ_CACHE_COST_MAX;
  }

  SeqCacheKey *key;
  key = BLI_mempool_alloc(cache->keys_pool);
  key->cache_owner = cache;
  key->seq = seq;
  key->context = *context;
  key->cfra = cfra;
  key->nfra = cfra - seq->start;
  key->type = type;
  key->cost = cost;
  key->cache_owner = cache;
  key->link_prev = NULL;
  key->link_next = NULL;
  key->is_temp_cache = true;
  key->creator_id = creator_id;

  /* Item stored for later use */
  if (flag & type) {
    key->is_temp_cache = false;
    key->link_prev = cache->last_key;
  }

  SeqCacheKey *temp_last_key = cache->last_key;
  seq_cache_put(cache, key, i);

  /* Restore pointer to previous item as this one will be freed when stack is rendered */
  if (key->is_temp_cache) {
    cache->last_key = temp_last_key;
  }

  /* Set last_key's reference to this key so we can look up chain backwards
  * Item is already put in cache, so cache->last_key points to current key;
  */
  if (flag & type && temp_last_key) {
    temp_last_key->link_next = cache->last_key;
  }

  /* Reset linking */
  if (key->type == SEQ_CACHE_STORE_FINAL_OUT) {
    cache->last_key = NULL;
  }

  seq_cache_unlock(scene);
}

void BKE_sequencer_cache_iterate(
    struct Scene *scene,
    void *userdata,
    bool callback(void *userdata, struct Sequence *seq, int cfra, int cache_type, float cost))
{
  SeqCache *cache = seq_cache_get_from_scene(scene);
  if (!cache) {
    return;
  }

  seq_cache_lock(scene);
  GHashIterator gh_iter;
  BLI_ghashIterator_init(&gh_iter, cache->hash);
  bool interrupt = false;

  while (!BLI_ghashIterator_done(&gh_iter) && !interrupt) {
    SeqCacheKey *key = BLI_ghashIterator_getKey(&gh_iter);
    BLI_ghashIterator_step(&gh_iter);

    interrupt = callback(userdata, key->seq, key->cfra, key->type, key->cost);
  }

  cache->last_key = NULL;
  seq_cache_unlock(scene);
}
