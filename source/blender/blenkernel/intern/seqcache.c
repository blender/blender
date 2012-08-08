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

#include "BLO_sys_types.h"  /* for intptr_t */

#include "MEM_guardedalloc.h"

#include "DNA_sequence_types.h"
#include "BKE_sequencer.h"

#include "IMB_moviecache.h"
#include "IMB_imbuf_types.h"

typedef struct SeqCacheKey {
	struct Sequence *seq;
	SeqRenderData context;
	float cfra;
	seq_stripelem_ibuf_t type;
} SeqCacheKey;

static struct MovieCache *moviecache = NULL;

static int seq_cmp_render_data(const SeqRenderData *a, const SeqRenderData *b)
{
	if (a->preview_render_size < b->preview_render_size) {
		return -1;
	}
	if (a->preview_render_size > b->preview_render_size) {
		return 1;
	}

	if (a->rectx < b->rectx) {
		return -1;
	}
	if (a->rectx > b->rectx) {
		return 1;
	}

	if (a->recty < b->recty) {
		return -1;
	}
	if (a->recty > b->recty) {
		return 1;
	}

	if (a->bmain < b->bmain) {
		return -1;
	}
	if (a->bmain > b->bmain) {
		return 1;
	}

	if (a->scene < b->scene) {
		return -1;
	}
	if (a->scene > b->scene) {
		return 1;
	}

	if (a->motion_blur_shutter < b->motion_blur_shutter) {
		return -1;
	}
	if (a->motion_blur_shutter > b->motion_blur_shutter) {
		return 1;
	}

	if (a->motion_blur_samples < b->motion_blur_samples) {
		return -1;
	}
	if (a->motion_blur_samples > b->motion_blur_samples) {
		return 1;
	}

	return 0;
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

static int seqcache_hashcmp(const void *a_, const void *b_)
{
	const SeqCacheKey *a = (SeqCacheKey *) a_;
	const SeqCacheKey *b = (SeqCacheKey *) b_;

	if (a->seq < b->seq) {
		return -1;
	}
	if (a->seq > b->seq) {
		return 1;
	}

	if (a->cfra < b->cfra) {
		return -1;
	}
	if (a->cfra > b->cfra) {
		return 1;
	}

	if (a->type < b->type) {
		return -1;
	}
	if (a->type > b->type) {
		return 1;
	}

	return seq_cmp_render_data(&a->context, &b->context);
}

void BKE_sequencer_stripelem_cache_destruct(void)
{
	if (moviecache)
		IMB_moviecache_free(moviecache);
}

void BKE_sequencer_stripelem_cache_cleanup(void)
{
	if (moviecache) {
		IMB_moviecache_free(moviecache);
		moviecache = IMB_moviecache_create("seqcache", sizeof(SeqCacheKey), seqcache_hashhash, seqcache_hashcmp);
	}
}

struct ImBuf *BKE_sequencer_cache_get(SeqRenderData context, Sequence *seq, float cfra, seq_stripelem_ibuf_t type)
{

	if (moviecache && seq) {
		SeqCacheKey key;

		key.seq = seq;
		key.context = context;
		key.cfra = cfra - seq->start;
		key.type = type;

		return IMB_moviecache_get(moviecache, &key);
	}

	return NULL;
}

void BKE_sequencer_cache_put(SeqRenderData context, Sequence *seq, float cfra, seq_stripelem_ibuf_t type, ImBuf *i)
{
	SeqCacheKey key;

	if (!i) {
		return;
	}

	if (!moviecache) {
		moviecache = IMB_moviecache_create("seqcache", sizeof(SeqCacheKey), seqcache_hashhash, seqcache_hashcmp);
	}

	key.seq = seq;
	key.context = context;
	key.cfra = cfra - seq->start;
	key.type = type;

	IMB_moviecache_put(moviecache, &key, i);
}
