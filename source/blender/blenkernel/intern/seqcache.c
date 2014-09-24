/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/seqcache.c
 *  \ingroup bke
 */

#include <stddef.h>

#include "BLI_sys_types.h"  /* for intptr_t */

#include "MEM_guardedalloc.h"

#include "DNA_sequence_types.h"

#include "IMB_moviecache.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_listbase.h"

#include "BKE_sequencer.h"

typedef struct SeqCacheKey {
	struct Sequence *seq;
	SeqRenderData context;
	float cfra;
	seq_stripelem_ibuf_t type;
} SeqCacheKey;

typedef struct SeqPreprocessCacheElem {
	struct SeqPreprocessCacheElem *next, *prev;

	struct Sequence *seq;
	SeqRenderData context;
	seq_stripelem_ibuf_t type;

	ImBuf *ibuf;
} SeqPreprocessCacheElem;

typedef struct SeqPreprocessCache {
	int cfra;
	ListBase elems;
} SeqPreprocessCache;

static struct MovieCache *moviecache = NULL;
static struct SeqPreprocessCache *preprocess_cache = NULL;

static void preprocessed_cache_destruct(void);

static bool seq_cmp_render_data(const SeqRenderData *a, const SeqRenderData *b)
{
	return ((a->preview_render_size != b->preview_render_size) ||
	        (a->rectx != b->rectx) ||
	        (a->recty != b->recty) ||
	        (a->bmain != b->bmain) ||
	        (a->scene != b->scene) ||
	        (a->motion_blur_shutter != b->motion_blur_shutter) ||
	        (a->motion_blur_samples != b->motion_blur_samples));
}

static unsigned int seq_hash_render_data(const SeqRenderData *a)
{
	unsigned int rval = a->rectx + a->recty;

	rval ^= a->preview_render_size;
	rval ^= ((intptr_t) a->bmain) << 6;
	rval ^= ((intptr_t) a->scene) << 6;
	rval ^= (int)(a->motion_blur_shutter * 100.0f) << 10;
	rval ^= a->motion_blur_samples << 24;

	return rval;
}

static unsigned int seqcache_hashhash(const void *key_)
{
	const SeqCacheKey *key = (SeqCacheKey *) key_;
	unsigned int rval = seq_hash_render_data(&key->context);

	rval ^= *(unsigned int *) &key->cfra;
	rval += key->type;
	rval ^= ((intptr_t) key->seq) << 6;

	return rval;
}

static bool seqcache_hashcmp(const void *a_, const void *b_)
{
	const SeqCacheKey *a = (SeqCacheKey *) a_;
	const SeqCacheKey *b = (SeqCacheKey *) b_;

	return ((a->seq != b->seq) ||
	        (a->cfra != b->cfra) ||
	        (a->type != b->type) ||
	        seq_cmp_render_data(&a->context, &b->context));
}

void BKE_sequencer_cache_destruct(void)
{
	if (moviecache)
		IMB_moviecache_free(moviecache);

	preprocessed_cache_destruct();
}

void BKE_sequencer_cache_cleanup(void)
{
	if (moviecache) {
		IMB_moviecache_free(moviecache);
		moviecache = IMB_moviecache_create("seqcache", sizeof(SeqCacheKey), seqcache_hashhash, seqcache_hashcmp);
	}

	BKE_sequencer_preprocessed_cache_cleanup();
}

static bool seqcache_key_check_seq(ImBuf *UNUSED(ibuf), void *userkey, void *userdata)
{
	SeqCacheKey *key = (SeqCacheKey *) userkey;
	Sequence *seq = (Sequence *) userdata;

	return key->seq == seq;
}

void BKE_sequencer_cache_cleanup_sequence(Sequence *seq)
{
	if (moviecache)
		IMB_moviecache_cleanup(moviecache, seqcache_key_check_seq, seq);
}

struct ImBuf *BKE_sequencer_cache_get(const SeqRenderData *context, Sequence *seq, float cfra, seq_stripelem_ibuf_t type)
{
	if (moviecache && seq) {
		SeqCacheKey key;

		key.seq = seq;
		key.context = *context;
		key.cfra = cfra - seq->start;
		key.type = type;

		return IMB_moviecache_get(moviecache, &key);
	}

	return NULL;
}

void BKE_sequencer_cache_put(const SeqRenderData *context, Sequence *seq, float cfra, seq_stripelem_ibuf_t type, ImBuf *i)
{
	SeqCacheKey key;

	if (i == NULL || context->skip_cache) {
		return;
	}

	if (!moviecache) {
		moviecache = IMB_moviecache_create("seqcache", sizeof(SeqCacheKey), seqcache_hashhash, seqcache_hashcmp);
	}

	key.seq = seq;
	key.context = *context;
	key.cfra = cfra - seq->start;
	key.type = type;

	IMB_moviecache_put(moviecache, &key, i);
}

void BKE_sequencer_preprocessed_cache_cleanup(void)
{
	SeqPreprocessCacheElem *elem;

	if (!preprocess_cache)
		return;

	for (elem = preprocess_cache->elems.first; elem; elem = elem->next) {
		IMB_freeImBuf(elem->ibuf);
	}
	BLI_freelistN(&preprocess_cache->elems);

	BLI_listbase_clear(&preprocess_cache->elems);
}

static void preprocessed_cache_destruct(void)
{
	if (!preprocess_cache)
		return;

	BKE_sequencer_preprocessed_cache_cleanup();

	MEM_freeN(preprocess_cache);
	preprocess_cache = NULL;
}

ImBuf *BKE_sequencer_preprocessed_cache_get(const SeqRenderData *context, Sequence *seq, float cfra, seq_stripelem_ibuf_t type)
{
	SeqPreprocessCacheElem *elem;

	if (!preprocess_cache)
		return NULL;

	if (preprocess_cache->cfra != cfra)
		return NULL;

	for (elem = preprocess_cache->elems.first; elem; elem = elem->next) {
		if (elem->seq != seq)
			continue;

		if (elem->type != type)
			continue;

		if (seq_cmp_render_data(&elem->context, context) != 0)
			continue;

		IMB_refImBuf(elem->ibuf);
		return elem->ibuf;
	}

	return NULL;
}

void BKE_sequencer_preprocessed_cache_put(const SeqRenderData *context, Sequence *seq, float cfra, seq_stripelem_ibuf_t type, ImBuf *ibuf)
{
	SeqPreprocessCacheElem *elem;

	if (!preprocess_cache) {
		preprocess_cache = MEM_callocN(sizeof(SeqPreprocessCache), "sequencer preprocessed cache");
	}
	else {
		if (preprocess_cache->cfra != cfra)
			BKE_sequencer_preprocessed_cache_cleanup();
	}

	elem = MEM_callocN(sizeof(SeqPreprocessCacheElem), "sequencer preprocessed cache element");

	elem->seq = seq;
	elem->type = type;
	elem->context = *context;
	elem->ibuf = ibuf;

	preprocess_cache->cfra = cfra;

	IMB_refImBuf(ibuf);

	BLI_addtail(&preprocess_cache->elems, elem);
}

void BKE_sequencer_preprocessed_cache_cleanup_sequence(Sequence *seq)
{
	SeqPreprocessCacheElem *elem, *elem_next;

	if (!preprocess_cache)
		return;

	for (elem = preprocess_cache->elems.first; elem; elem = elem_next) {
		elem_next = elem->next;

		if (elem->seq == seq) {
			IMB_freeImBuf(elem->ibuf);

			BLI_freelinkN(&preprocess_cache->elems, elem);
		}
	}
}
