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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/tools/bmesh_region_match.c
 *  \ingroup bmesh
 *
 * Given a contiguous region of faces,
 * find multiple matching regions (based on topology) and return them.
 */

#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_alloca.h"
#include "BLI_ghash.h"
#include "BLI_mempool.h"
#include "BLI_linklist_stack.h"

#include "bmesh.h"

#include "tools/bmesh_region_match.h"  /* own incldue */

/* avoid re-creating ghash and pools for each search */
#define USE_WALKER_REUSE

/* do a first-pass id of all vertices,
 * this avoids expensive checks on every item later on
 * (works fine without, just slower) */
#define USE_PIVOT_FASTMATCH

/* otherwise use active element as pivot, for quick tests only */
#define USE_PIVOT_SEARCH

// #define DEBUG_TIME
// #define DEBUG_PRINT

#ifdef DEBUG_TIME
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

#include "BLI_strict_flags.h"


/* -------------------------------------------------------------------- */
/* UUID-Walk API */

/** \name Internal UUIDWalk API
 * \{ */

#define PRIME_VERT_INIT 100003

typedef uintptr_t UUID_Int;

typedef struct UUIDWalk {

	/* List of faces we can step onto (UUIDFaceStep's) */
	ListBase faces_step;

	/* Face & Vert UUID's */
	GHash *verts_uuid;
	GHash *faces_uuid;

	/* memory pool for LinkNode's */
	BLI_mempool *link_pool;

	/* memory pool for LinkBase's */
	BLI_mempool *lbase_pool;

	/* memory pool for UUIDFaceStep's */
	BLI_mempool *step_pool;
	BLI_mempool *step_pool_items;

	/* Optionaly use face-tag to isolate search */
	bool use_face_isolate;

	/* Increment for each pass added */
	UUID_Int pass;

	/* runtime vars, aviod re-creating each pass */
	struct {
		GHash *verts_uuid;  /* BMVert -> UUID */
		GSet  *faces_step;  /* BMFace */

		GHash *faces_from_uuid;   /* UUID -> UUIDFaceStepItem */

		UUID_Int    *rehash_store;
		unsigned int rehash_store_len;
	} cache;

} UUIDWalk;

/* stores a set of potential faces to step onto */
typedef struct UUIDFaceStep {
	struct UUIDFaceStep *next, *prev;

	/* unsorted 'BMFace' */
	LinkNode *faces;

	/* faces sorted into 'UUIDFaceStepItem' */
	ListBase items;
} UUIDFaceStep;

/* store face-lists with same uuid */
typedef struct UUIDFaceStepItem {
	struct UUIDFaceStepItem *next, *prev;
	uintptr_t uuid;

	LinkNode    *list;
	unsigned int list_len;
} UUIDFaceStepItem;

BLI_INLINE bool bm_uuidwalk_face_test(
        UUIDWalk *uuidwalk, BMFace *f)
{
	if (uuidwalk->use_face_isolate) {
		return BM_elem_flag_test_bool(f, BM_ELEM_TAG);
	}
	else {
		return true;
	}
}

BLI_INLINE bool bm_uuidwalk_vert_lookup(
        UUIDWalk *uuidwalk, BMVert *v, UUID_Int *r_uuid)
{
	void **ret;
	ret = BLI_ghash_lookup_p(uuidwalk->verts_uuid, v);
	if (ret) {
		*r_uuid = (UUID_Int)(*ret);
		return true;
	}
	else {
		return false;
	}
}

BLI_INLINE bool bm_uuidwalk_face_lookup(
        UUIDWalk *uuidwalk, BMFace *f, UUID_Int *r_uuid)
{
	void **ret;
	ret = BLI_ghash_lookup_p(uuidwalk->faces_uuid, f);
	if (ret) {
		*r_uuid = (UUID_Int)(*ret);
		return true;
	}
	else {
		return false;
	}
}

static unsigned int ghashutil_bmelem_indexhash(const void *key)
{
	const BMElem *ele = key;
	return (unsigned int)BM_elem_index_get(ele);
}

static bool ghashutil_bmelem_indexcmp(const void *a, const void *b)
{
	BLI_assert((a != b) == (BM_elem_index_get((BMElem *)a) != BM_elem_index_get((BMElem *)b)));
	return (a != b);
}

static GHash *ghash_bmelem_new_ex(const char *info,
                                  const unsigned int nentries_reserve)
{
	return BLI_ghash_new_ex(ghashutil_bmelem_indexhash, ghashutil_bmelem_indexcmp, info, nentries_reserve);
}

static GSet *gset_bmelem_new_ex(const char *info,
                             const unsigned int nentries_reserve)
{
	return BLI_gset_new_ex(ghashutil_bmelem_indexhash, ghashutil_bmelem_indexcmp, info, nentries_reserve);
}


static GHash *ghash_bmelem_new(const char *info)
{
	return ghash_bmelem_new_ex(info, 0);
}

static GSet *gset_bmelem_new(const char *info)
{
	return gset_bmelem_new_ex(info, 0);
}


static void bm_uuidwalk_init(
        UUIDWalk *uuidwalk,
        const unsigned int faces_src_region_len,
        const unsigned int verts_src_region_len)
{
	BLI_listbase_clear(&uuidwalk->faces_step);

	uuidwalk->verts_uuid = ghash_bmelem_new_ex(__func__, verts_src_region_len);
	uuidwalk->faces_uuid = ghash_bmelem_new_ex(__func__, faces_src_region_len);

	uuidwalk->cache.verts_uuid = ghash_bmelem_new(__func__);
	uuidwalk->cache.faces_step = gset_bmelem_new(__func__);

	/* works because 'int' ghash works for intptr_t too */
	uuidwalk->cache.faces_from_uuid = BLI_ghash_int_new(__func__);

	uuidwalk->cache.rehash_store = NULL;
	uuidwalk->cache.rehash_store_len = 0;

	uuidwalk->use_face_isolate = false;

	/* smaller pool's for faster clearing */
	uuidwalk->link_pool = BLI_mempool_create(sizeof(LinkNode), 64, 64, BLI_MEMPOOL_NOP);
	uuidwalk->step_pool = BLI_mempool_create(sizeof(UUIDFaceStep), 64, 64, BLI_MEMPOOL_NOP);
	uuidwalk->step_pool_items = BLI_mempool_create(sizeof(UUIDFaceStepItem), 64, 64, BLI_MEMPOOL_NOP);

	uuidwalk->pass = 1;
}

static void bm_uuidwalk_clear(
        UUIDWalk *uuidwalk)
{
	BLI_listbase_clear(&uuidwalk->faces_step);

	BLI_ghash_clear(uuidwalk->verts_uuid, NULL, NULL);
	BLI_ghash_clear(uuidwalk->faces_uuid, NULL, NULL);

	BLI_ghash_clear(uuidwalk->cache.verts_uuid, NULL, NULL);
	BLI_gset_clear(uuidwalk->cache.faces_step, NULL);
	BLI_ghash_clear(uuidwalk->cache.faces_from_uuid, NULL, NULL);

	/* keep rehash_store as-is, for reuse */

	uuidwalk->use_face_isolate = false;

	BLI_mempool_clear(uuidwalk->link_pool);
	BLI_mempool_clear(uuidwalk->step_pool);
	BLI_mempool_clear(uuidwalk->step_pool_items);

	uuidwalk->pass = 1;
}

static void bm_uuidwalk_free(
        UUIDWalk *uuidwalk)
{
	/**
	 * Handled by pools
	 *
	 * - uuidwalk->faces_step
	 */

	BLI_ghash_free(uuidwalk->verts_uuid, NULL, NULL);
	BLI_ghash_free(uuidwalk->faces_uuid, NULL, NULL);

	/* cache */
	BLI_ghash_free(uuidwalk->cache.verts_uuid, NULL, NULL);
	BLI_gset_free(uuidwalk->cache.faces_step, NULL);
	BLI_ghash_free(uuidwalk->cache.faces_from_uuid, NULL, NULL);
	MEM_SAFE_FREE(uuidwalk->cache.rehash_store);

	BLI_mempool_destroy(uuidwalk->link_pool);
	BLI_mempool_destroy(uuidwalk->step_pool);
	BLI_mempool_destroy(uuidwalk->step_pool_items);
}

static UUID_Int bm_uuidwalk_calc_vert_uuid(
        UUIDWalk *uuidwalk, BMVert *v)
{
#define PRIME_VERT_SMALL  7
#define PRIME_VERT_MID    43
#define PRIME_VERT_LARGE  1031

#define PRIME_FACE_SMALL  13
#define PRIME_FACE_MID    53

	UUID_Int uuid;

	uuid = uuidwalk->pass * PRIME_VERT_LARGE;

	/* vert -> other */
	{
		unsigned int tot = 0;
		BMIter eiter;
		BMEdge *e;
		BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
			BMVert *v_other = BM_edge_other_vert(e, v);
			UUID_Int uuid_other;
			if (bm_uuidwalk_vert_lookup(uuidwalk, v_other, &uuid_other)) {
				uuid ^= (uuid_other * PRIME_VERT_SMALL);
				tot += 1;
			}
		}
		uuid ^= (tot * PRIME_VERT_MID);
	}

	/* faces */
	{
		unsigned int tot = 0;
		BMIter iter;
		BMFace *f;

		BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
			UUID_Int uuid_other;
			if (bm_uuidwalk_face_lookup(uuidwalk, f, &uuid_other)) {
				uuid ^= (uuid_other * PRIME_FACE_SMALL);
				tot += 1;
			}
		}
		uuid ^= (tot * PRIME_FACE_MID);
	}

	return uuid;

#undef PRIME_VERT_SMALL
#undef PRIME_VERT_MID
#undef PRIME_VERT_LARGE

#undef PRIME_FACE_SMALL
#undef PRIME_FACE_MID
}

static UUID_Int bm_uuidwalk_calc_face_uuid(
        UUIDWalk *uuidwalk, BMFace *f)
{
#define PRIME_VERT_SMALL  11

#define PRIME_FACE_SMALL  17
#define PRIME_FACE_LARGE  1013

	UUID_Int uuid;

	uuid = uuidwalk->pass * (unsigned int)f->len * PRIME_FACE_LARGE;

	/* face-verts */
	{
		BMLoop *l_iter, *l_first;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			UUID_Int uuid_other;
			if (bm_uuidwalk_vert_lookup(uuidwalk, l_iter->v, &uuid_other)) {
				uuid ^= (uuid_other * PRIME_VERT_SMALL);
			}
		} while ((l_iter = l_iter->next) != l_first);
	}

	/* face-faces (connected by edge) */
	{
		BMLoop *l_iter, *l_first;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			if (l_iter->radial_next != l_iter) {
				BMLoop *l_iter_radial = l_iter->radial_next;
				do {
					UUID_Int uuid_other;
					if (bm_uuidwalk_face_lookup(uuidwalk, l_iter_radial->f, &uuid_other)) {
						uuid ^= (uuid_other * PRIME_FACE_SMALL);
					}
				} while ((l_iter_radial = l_iter_radial->radial_next) != l_iter);
			}
		} while ((l_iter = l_iter->next) != l_first);
	}

	return uuid;

#undef PRIME_VERT_SMALL

#undef PRIME_FACE_SMALL
#undef PRIME_FACE_LARGE
}

static void bm_uuidwalk_rehash_reserve(
        UUIDWalk *uuidwalk, unsigned int rehash_store_len_new)
{
	if (UNLIKELY(rehash_store_len_new > uuidwalk->cache.rehash_store_len)) {
		/* avoid re-allocs */
		rehash_store_len_new *= 2;
		uuidwalk->cache.rehash_store =
		        MEM_reallocN(uuidwalk->cache.rehash_store,
		                     rehash_store_len_new * sizeof(*uuidwalk->cache.rehash_store));
		uuidwalk->cache.rehash_store_len = rehash_store_len_new;
	}
}

/**
 * Re-hash all elements, delay updating so as not to create feedback loop.
 */
static void bm_uuidwalk_rehash(
        UUIDWalk *uuidwalk)
{
	GHashIterator gh_iter;
	UUID_Int *uuid_store;
	unsigned int i;

	unsigned int rehash_store_len_new = (unsigned int)MAX2(BLI_ghash_size(uuidwalk->verts_uuid),
	                                                       BLI_ghash_size(uuidwalk->faces_uuid));

	bm_uuidwalk_rehash_reserve(uuidwalk, rehash_store_len_new);
	uuid_store = uuidwalk->cache.rehash_store;

	/* verts */
	i = 0;
	GHASH_ITER (gh_iter, uuidwalk->verts_uuid) {
		BMVert *v = BLI_ghashIterator_getKey(&gh_iter);
		uuid_store[i++] = bm_uuidwalk_calc_vert_uuid(uuidwalk, v);
	}
	i = 0;
	GHASH_ITER (gh_iter, uuidwalk->verts_uuid) {
		void **uuid_p = BLI_ghashIterator_getValue_p(&gh_iter);
		*((UUID_Int *)uuid_p) = uuid_store[i++];
	}

	/* faces */
	i = 0;
	GHASH_ITER (gh_iter, uuidwalk->faces_uuid) {
		BMFace *f = BLI_ghashIterator_getKey(&gh_iter);
		uuid_store[i++] = bm_uuidwalk_calc_face_uuid(uuidwalk, f);
	}
	i = 0;
	GHASH_ITER (gh_iter, uuidwalk->faces_uuid) {
		void **uuid_p = BLI_ghashIterator_getValue_p(&gh_iter);
		*((UUID_Int *)uuid_p) = uuid_store[i++];
	}
}

static void bm_uuidwalk_rehash_facelinks(
        UUIDWalk *uuidwalk,
        LinkNode *faces, const unsigned int faces_len,
        const bool is_init)
{
	UUID_Int *uuid_store;
	LinkNode *f_link;
	unsigned int i;

	bm_uuidwalk_rehash_reserve(uuidwalk, faces_len);
	uuid_store = uuidwalk->cache.rehash_store;

	i = 0;
	for (f_link = faces; f_link; f_link = f_link->next) {
		BMFace *f = f_link->link;
		uuid_store[i++] = bm_uuidwalk_calc_face_uuid(uuidwalk, f);
	}

	i = 0;
	if (is_init) {
		for (f_link = faces; f_link; f_link = f_link->next) {
			BMFace *f = f_link->link;
			BLI_ghash_insert(uuidwalk->faces_uuid, f, (void *)uuid_store[i++]);
		}
	}
	else {
		for (f_link = faces; f_link; f_link = f_link->next) {
			BMFace *f = f_link->link;
			void **uuid_p = BLI_ghash_lookup_p(uuidwalk->faces_uuid, f);
			*((UUID_Int *)uuid_p) = uuid_store[i++];
		}
	}
}

static bool bm_vert_is_uuid_connect(
        UUIDWalk *uuidwalk, BMVert *v)
{
	BMIter eiter;
	BMEdge *e;

	BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
		BMVert *v_other = BM_edge_other_vert(e, v);
		if (BLI_ghash_haskey(uuidwalk->verts_uuid, v_other)) {
			return true;
		}
	}
	return false;
}

static void bm_uuidwalk_pass_add(
        UUIDWalk *uuidwalk, LinkNode *faces_pass, const unsigned int faces_pass_len)
{
	GHashIterator gh_iter;
	GHash *verts_uuid_pass;
	GSet  *faces_step_next;
	LinkNode *f_link;

	UUIDFaceStep *fstep;

	BLI_assert(faces_pass_len == (unsigned int)BLI_linklist_length(faces_pass));

	/* rehash faces now all their verts have been added */
	bm_uuidwalk_rehash_facelinks(uuidwalk, faces_pass, faces_pass_len, true);

	/* create verts_new */
	verts_uuid_pass = uuidwalk->cache.verts_uuid;
	faces_step_next = uuidwalk->cache.faces_step;

	BLI_assert(BLI_ghash_size(verts_uuid_pass) == 0);
	BLI_assert(BLI_gset_size(faces_step_next) == 0);

	/* Add the face_step data from connected faces, creating new passes */
	fstep = BLI_mempool_alloc(uuidwalk->step_pool);
	BLI_addhead(&uuidwalk->faces_step, fstep);
	fstep->faces = NULL;
	BLI_listbase_clear(&fstep->items);

	for (f_link = faces_pass; f_link; f_link = f_link->next) {
		BMFace *f = f_link->link;
		BMLoop *l_iter, *l_first;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			/* fill verts_new */
			if (!BLI_ghash_haskey(uuidwalk->verts_uuid, l_iter->v) &&
			    !BLI_ghash_haskey(verts_uuid_pass,      l_iter->v) &&
			    (bm_vert_is_uuid_connect(uuidwalk, l_iter->v) == true))
			{
				const UUID_Int uuid = bm_uuidwalk_calc_vert_uuid(uuidwalk, l_iter->v);
				BLI_ghash_insert(verts_uuid_pass, l_iter->v, (void *)uuid);
			}

			/* fill faces_step_next */
			if (l_iter->radial_next != l_iter) {
				BMLoop *l_iter_radial = l_iter->radial_next;
				do {
					if (!BLI_ghash_haskey(uuidwalk->faces_uuid, l_iter_radial->f) &&
					    !BLI_gset_haskey(faces_step_next,       l_iter_radial->f) &&
					    (bm_uuidwalk_face_test(uuidwalk,        l_iter_radial->f)))
					{
						BLI_gset_insert(faces_step_next, l_iter_radial->f);

						/* add to fstep */
						BLI_linklist_prepend_pool(&fstep->faces, l_iter_radial->f, uuidwalk->link_pool);
					}
				} while ((l_iter_radial = l_iter_radial->radial_next) != l_iter);
			}
		} while ((l_iter = l_iter->next) != l_first);
	}

	/* faces_uuid.update(verts_new) */
	GHASH_ITER (gh_iter, verts_uuid_pass) {
		BMVert *v = BLI_ghashIterator_getKey(&gh_iter);
		void *uuid_p = BLI_ghashIterator_getValue(&gh_iter);
		BLI_ghash_insert(uuidwalk->verts_uuid, v, uuid_p);
	}

	/* rehash faces now all their verts have been added */
	bm_uuidwalk_rehash_facelinks(uuidwalk, faces_pass, faces_pass_len, false);

	uuidwalk->pass += 1;

	BLI_ghash_clear(uuidwalk->cache.verts_uuid, NULL, NULL);
	BLI_gset_clear(uuidwalk->cache.faces_step, NULL);
}

static int bm_face_len_cmp(const void *v1, const void *v2)
{
	const BMFace *f1 = v1, *f2 = v2;

	if      (f1->len > f2->len) return  1;
	else if (f1->len < f2->len) return -1;
	else                        return  0;
}

static unsigned int bm_uuidwalk_init_from_edge(
        UUIDWalk *uuidwalk, BMEdge *e)
{
	BMLoop *l_iter = e->l;
	unsigned int f_arr_len = (unsigned int)BM_edge_face_count(e);
	BMFace **f_arr = BLI_array_alloca(f_arr, f_arr_len);
	unsigned int fstep_num = 0, i = 0;

	do {
		BMFace *f = l_iter->f;
		if (bm_uuidwalk_face_test(uuidwalk, f)) {
			f_arr[i++] = f;
		}
	} while ((l_iter = l_iter->radial_next) != e->l);
	BLI_assert(i <= f_arr_len);
	f_arr_len = i;

	qsort(f_arr, f_arr_len, sizeof(*f_arr), bm_face_len_cmp);

	/* start us off! */
	{
		const UUID_Int uuid = PRIME_VERT_INIT;
		BLI_ghash_insert(uuidwalk->verts_uuid, e->v1, (void *)uuid);
		BLI_ghash_insert(uuidwalk->verts_uuid, e->v2, (void *)uuid);
	}

	/* turning an array into LinkNode's seems odd,
	 * but this is just for initialization,
	 * elsewhere using LinkNode's makes more sense */
	for (i = 0; i < f_arr_len; i++) {
		LinkNode *faces_pass = NULL;
		const int f_len = f_arr[i]->len;

		do {
			BLI_linklist_prepend_pool(&faces_pass, f_arr[i++], uuidwalk->link_pool);
		} while (i < f_arr_len && (f_len == f_arr[i]->len));

		bm_uuidwalk_pass_add(uuidwalk, faces_pass, i);
		BLI_linklist_free_pool(faces_pass, NULL, uuidwalk->link_pool);
		fstep_num += 1;
	}

	return fstep_num;
}

#undef PRIME_VERT_INIT

/** \} */


/** \name Internal UUIDFaceStep API
 * \{ */

static int facestep_sort(const void *a, const void *b)
{
	const UUIDFaceStepItem *fstep_a = a;
	const UUIDFaceStepItem *fstep_b = b;
	return (fstep_a->uuid > fstep_b->uuid) ? 1 : 0;
}

/**
 * Put faces in lists based on their uuid's,
 * re-run for each pass since rehashing may differentiate face-groups.
 */
static bool bm_uuidwalk_facestep_begin(
        UUIDWalk *uuidwalk, UUIDFaceStep *fstep)
{
	LinkNode *f_link, *f_link_next, **f_link_prev_p;
	bool ok = false;

	BLI_assert(BLI_ghash_size(uuidwalk->cache.faces_from_uuid) == 0);
	BLI_assert(BLI_countlist(&fstep->items) == 0);

	f_link_prev_p = &fstep->faces;
	for (f_link = fstep->faces; f_link; f_link = f_link_next) {
		BMFace *f = f_link->link;
		f_link_next = f_link->next;

		/* possible another pass added this face already, free in that case */
		if (!BLI_ghash_haskey(uuidwalk->faces_uuid, f)) {
			const UUID_Int uuid = bm_uuidwalk_calc_face_uuid(uuidwalk, f);
			UUIDFaceStepItem *fstep_item;

			ok = true;

			fstep_item = BLI_ghash_lookup(uuidwalk->cache.faces_from_uuid, (void *)uuid);
			if (UNLIKELY(fstep_item == NULL)) {
				fstep_item = BLI_mempool_alloc(uuidwalk->step_pool_items);
				BLI_ghash_insert(uuidwalk->cache.faces_from_uuid, (void *)uuid, fstep_item);

				/* add to start, so its handled on the next round of passes */
				BLI_addhead(&fstep->items, fstep_item);
				fstep_item->uuid = uuid;
				fstep_item->list = NULL;
				fstep_item->list_len = 0;
			}

			BLI_linklist_prepend_pool(&fstep_item->list, f, uuidwalk->link_pool);
			fstep_item->list_len += 1;

			f_link_prev_p = &f_link->next;
		}
		else {
			*f_link_prev_p = f_link->next;
			BLI_mempool_free(uuidwalk->link_pool, f_link);
		}
	}

	BLI_ghash_clear(uuidwalk->cache.faces_from_uuid, NULL, NULL);

	BLI_sortlist(&fstep->items, facestep_sort);

	return ok;
}

/**
 * Cleans up temp data from #bm_uuidwalk_facestep_begin
 */
static void bm_uuidwalk_facestep_end(
        UUIDWalk *uuidwalk, UUIDFaceStep *fstep)
{
	UUIDFaceStepItem *fstep_item;

	while ((fstep_item = BLI_pophead(&fstep->items))) {
		BLI_mempool_free(uuidwalk->step_pool_items, fstep_item);
	}
}

static void bm_uuidwalk_facestep_free(
        UUIDWalk *uuidwalk, UUIDFaceStep *fstep)
{
	LinkNode *f_link, *f_link_next;

	BLI_assert(BLI_listbase_is_empty(&fstep->items));

	for (f_link = fstep->faces; f_link; f_link = f_link_next) {
		f_link_next = f_link->next;
		BLI_mempool_free(uuidwalk->link_pool, f_link);
	}

	BLI_remlink(&uuidwalk->faces_step, fstep);
	BLI_mempool_free(uuidwalk->step_pool, fstep);
}

/** \} */


/* -------------------------------------------------------------------- */
/* Main Loop to match up regions */

/**
 * Given a face region and 2 candidate verts to begin mapping.
 * return the matching region or NULL.
 */
static BMFace **bm_mesh_region_match_pair(
#ifdef USE_WALKER_REUSE
        UUIDWalk *w_src, UUIDWalk *w_dst,
#endif
        BMEdge *e_src, BMEdge *e_dst,
        const unsigned int faces_src_region_len,
        const unsigned int verts_src_region_len,
        unsigned int *r_faces_result_len)
{
#ifndef USE_WALKER_REUSE
	UUIDWalk w_src_, w_dst_;
	UUIDWalk *w_src = &w_src_, *w_dst = &w_dst_;
#endif
	BMFace **faces_result = NULL;
	bool found = false;

	BLI_assert(e_src != e_dst);

#ifndef USE_WALKER_REUSE
	bm_uuidwalk_init(w_src, faces_src_region_len, verts_src_region_len);
	bm_uuidwalk_init(w_dst, faces_src_region_len, verts_src_region_len);
#endif

	w_src->use_face_isolate = true;

	/* setup the initial state */
	if (UNLIKELY(bm_uuidwalk_init_from_edge(w_src, e_src) !=
	             bm_uuidwalk_init_from_edge(w_dst, e_dst)))
	{
		/* should never happen, if verts passed are compatible, but to be safe... */
		goto finally;
	}

	bm_uuidwalk_rehash_reserve(w_src, MAX2(faces_src_region_len, verts_src_region_len));
	bm_uuidwalk_rehash_reserve(w_dst, MAX2(faces_src_region_len, verts_src_region_len));

	while (true) {
		bool ok = false;

		UUIDFaceStep *fstep_src = w_src->faces_step.first;
		UUIDFaceStep *fstep_dst = w_dst->faces_step.first;

		BLI_assert(BLI_countlist(&w_src->faces_step) == BLI_countlist(&w_dst->faces_step));

		while (fstep_src) {

			/* even if the destination has faces,
			 * it's not important, since the source doesn't, free and move-on. */
			if (fstep_src->faces == NULL) {
				UUIDFaceStep *fstep_src_next = fstep_src->next;
				UUIDFaceStep *fstep_dst_next = fstep_dst->next;
				bm_uuidwalk_facestep_free(w_src, fstep_src);
				bm_uuidwalk_facestep_free(w_dst, fstep_dst);
				fstep_src = fstep_src_next;
				fstep_dst = fstep_dst_next;
				continue;
			}

			if (bm_uuidwalk_facestep_begin(w_src, fstep_src) &&
			    bm_uuidwalk_facestep_begin(w_dst, fstep_dst))
			{
				/* Step over face-lists with matching UUID's
				 * both lists are sorted, so no need for lookups.
				 * The data is created on 'begin' and cleared on 'end' */
				UUIDFaceStepItem *fstep_item_src;
				UUIDFaceStepItem *fstep_item_dst;
				for (fstep_item_src = fstep_src->items.first,
				     fstep_item_dst = fstep_dst->items.first;
				     fstep_item_src && fstep_item_dst;
				     fstep_item_src = fstep_item_src->next,
				     fstep_item_dst = fstep_item_dst->next)
				{
					while ((fstep_item_dst != NULL) &&
					       (fstep_item_dst->uuid < fstep_item_src->uuid))
					{
						fstep_item_dst = fstep_item_dst->next;
					}

					if ((fstep_item_dst == NULL) ||
					    (fstep_item_src->uuid  != fstep_item_dst->uuid) ||
					    (fstep_item_src->list_len > fstep_item_dst->list_len))
					{
						/* if the target walker has less than the source
						 * then the islands don't match, bail early */
						ok = false;
						break;
					}

					if (fstep_item_src->list_len == fstep_item_dst->list_len) {
						/* found a match */
						bm_uuidwalk_pass_add(w_src, fstep_item_src->list, fstep_item_src->list_len);
						bm_uuidwalk_pass_add(w_dst, fstep_item_dst->list, fstep_item_dst->list_len);

						BLI_linklist_free_pool(fstep_item_src->list, NULL, w_src->link_pool);
						BLI_linklist_free_pool(fstep_item_dst->list, NULL, w_dst->link_pool);

						fstep_item_src->list = NULL;
						fstep_item_src->list_len = 0;

						fstep_item_dst->list = NULL;
						fstep_item_dst->list_len = 0;

						ok = true;
					}
				}
			}

			bm_uuidwalk_facestep_end(w_src, fstep_src);
			bm_uuidwalk_facestep_end(w_dst, fstep_dst);

			/* lock-step */
			fstep_src = fstep_src->next;
			fstep_dst = fstep_dst->next;
		}

		if (!ok) {
			break;
		}

		found = ((unsigned int)BLI_ghash_size(w_dst->faces_uuid) == faces_src_region_len);
		if (found) {
			break;
		}

		/* Expensive! but some cases fails without.
		 * (also faster in other cases since it can rule-out invalid regions) */
		bm_uuidwalk_rehash(w_src);
		bm_uuidwalk_rehash(w_dst);
	}

	if (found) {
		GHashIterator gh_iter;
		const unsigned int faces_result_len = (unsigned int)BLI_ghash_size(w_dst->faces_uuid);
		unsigned int i;

		faces_result = MEM_mallocN(sizeof(faces_result) * (faces_result_len + 1), __func__);
		GHASH_ITER_INDEX (gh_iter, w_dst->faces_uuid, i) {
			BMFace *f = BLI_ghashIterator_getKey(&gh_iter);
			faces_result[i] = f;
		}
		faces_result[faces_result_len] = NULL;
		*r_faces_result_len = faces_result_len;
	}
	else {
		*r_faces_result_len = 0;
	}

finally:

#ifdef USE_WALKER_REUSE
	bm_uuidwalk_clear(w_src);
	bm_uuidwalk_clear(w_dst);
#else
	bm_uuidwalk_free(w_src);
	bm_uuidwalk_free(w_dst);
#endif

	return faces_result;
}

/**
 * Tag as visited, avoid re-use.
 */
static void bm_face_array_visit(
        BMFace **faces, const unsigned int faces_len,
        unsigned int *r_verts_len,
        bool visit_faces)
{
	unsigned int verts_len = 0;
	unsigned int i;
	for (i = 0; i < faces_len; i++) {
		BMFace *f = faces[i];
		BMLoop *l_iter, *l_first;
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			if (r_verts_len) {
				if (!BM_elem_flag_test(l_iter->v, BM_ELEM_TAG)) {
					verts_len += 1;
				}
			}

			BM_elem_flag_enable(l_iter->e, BM_ELEM_TAG);
			BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);
		} while ((l_iter = l_iter->next) != l_first);

		if (visit_faces) {
			BM_elem_flag_enable(f, BM_ELEM_TAG);
		}
	}

	if (r_verts_len) {
		*r_verts_len = verts_len;
	}
}

#ifdef USE_PIVOT_SEARCH

/** \name Internal UUIDWalk API
 * \{ */

/* signed user id */
typedef intptr_t SUID_Int;

static bool bm_edge_is_region_boundary(BMEdge *e)
{
	if (e->l->radial_next != e->l) {
		BMLoop *l_iter = e->l;
		do {
			if (!BM_elem_flag_test(l_iter->f, BM_ELEM_TAG)) {
				return true;
			}
		} while ((l_iter = l_iter->radial_next) != e->l);
		return false;
	}
	else {
		/* boundary */
		return true;
	}
}

static void bm_face_region_pivot_edge_use_best(
        GHash *gh, BMEdge *e_test,
        BMEdge **r_e_pivot_best,
        SUID_Int e_pivot_best_id[2])
{
	SUID_Int e_pivot_test_id[2];

	e_pivot_test_id[0] = (SUID_Int)BLI_ghash_lookup(gh, e_test->v1);
	e_pivot_test_id[1] = (SUID_Int)BLI_ghash_lookup(gh, e_test->v2);
	if (e_pivot_test_id[0] > e_pivot_test_id[1]) {
		SWAP(SUID_Int, e_pivot_test_id[0], e_pivot_test_id[1]);
	}

	if ((*r_e_pivot_best == NULL) ||
	    ((e_pivot_best_id[0] != e_pivot_test_id[0]) ?
	     (e_pivot_best_id[0] <  e_pivot_test_id[0]) :
	     (e_pivot_best_id[1] <  e_pivot_test_id[1])))
	{
		e_pivot_best_id[0] = e_pivot_test_id[0];
		e_pivot_best_id[1] = e_pivot_test_id[1];

		/* both verts are from the same pass, record this! */
		*r_e_pivot_best = e_test;
	}
}

/* quick id from a boundary vertex */
static SUID_Int bm_face_region_vert_boundary_id(BMVert *v)
{
#define PRIME_VERT_SMALL_A  7
#define PRIME_VERT_SMALL_B  13
#define PRIME_VERT_MID_A    103
#define PRIME_VERT_MID_B    131

	unsigned int tot = 0;
	BMIter iter;
	BMLoop *l;
	SUID_Int id = PRIME_VERT_MID_A;

	BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
		const bool is_boundary_vert = (bm_edge_is_region_boundary(l->e) || bm_edge_is_region_boundary(l->prev->e));
		id ^= (unsigned int)l->f->len * (is_boundary_vert ? PRIME_VERT_SMALL_A : PRIME_VERT_SMALL_B);
		tot += 1;
	}

	id ^= (tot * PRIME_VERT_MID_B);

	return id ? ABS(id) : 1;

#undef PRIME_VERT_SMALL_A
#undef PRIME_VERT_SMALL_B
#undef PRIME_VERT_MID_A
#undef PRIME_VERT_MID_B
}

/**
 * Accumulate id's from a previous pass (swap sign each pass)
 */
static SUID_Int bm_face_region_vert_pass_id(GHash *gh, BMVert *v)
{
	BMIter eiter;
	BMEdge *e;
	SUID_Int tot = 0;
	SUID_Int v_sum_face_len = 0;
	SUID_Int v_sum_id = 0;
	SUID_Int id;
	SUID_Int id_min = INTPTR_MIN + 1;

#define PRIME_VERT_MID_A  23
#define PRIME_VERT_MID_B  31

	BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			BMVert *v_other = BM_edge_other_vert(e, v);
			if (BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
				/* non-zero values aren't allowed... so no need to check haskey */
				SUID_Int v_other_id = (SUID_Int)BLI_ghash_lookup(gh, v_other);
				if (v_other_id > 0) {
					v_sum_id += v_other_id;
					tot += 1;

					/* face-count */
					{
						BMLoop *l_iter = e->l;
						do {
							if (BM_elem_flag_test(l_iter->f, BM_ELEM_TAG)) {
								v_sum_face_len += l_iter->f->len;
							}
						} while ((l_iter = l_iter->radial_next) != e->l);
					}
				}
			}
		}
	}

	id  = (tot * PRIME_VERT_MID_A);
	id ^= (v_sum_face_len * PRIME_VERT_MID_B);
	id ^= v_sum_id;

	/* disallow 0 & min (since it can't be flipped) */
	id = (UNLIKELY(id == 0) ? 1 : UNLIKELY(id < id_min) ? id_min : id);

	return ABS(id);

#undef PRIME_VERT_MID_A
#undef PRIME_VERT_MID_B
}

/**
 * Take a face region and find the inner-most vertex.
 * also calculate the number of connections to the boundary,
 * and the total number unique of verts used by this face region.
 *
 * This is only called once on the source region (no need to be highly optimized).
 */
static BMEdge *bm_face_region_pivot_edge_find(
        BMFace **faces_region, unsigned int faces_region_len,
        unsigned int verts_region_len,
        unsigned int *r_depth)
{
	/* note, keep deterministic where possible (geometry order independent)
	 * this function assumed all visit faces & edges are tagged */

	BLI_LINKSTACK_DECLARE(vert_queue_prev, BMVert *);
	BLI_LINKSTACK_DECLARE(vert_queue_next, BMVert *);

	GHash *gh = BLI_ghash_ptr_new(__func__);
	unsigned int i;

	BMEdge *e_pivot = NULL;
	/* pick any non-boundary edge (not ideal) */
	BMEdge *e_pivot_fallback = NULL;

	SUID_Int pass = 0;

	/* total verts in 'gs' we have visited - aka - not v_init_none */
	unsigned int vert_queue_used = 0;

	BLI_LINKSTACK_INIT(vert_queue_prev);
	BLI_LINKSTACK_INIT(vert_queue_next);

	/* face-verts */
	for (i = 0; i < faces_region_len; i++) {
		BMFace *f = faces_region[i];

		BMLoop *l_iter, *l_first;
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			BMEdge *e = l_iter->e;
			if (bm_edge_is_region_boundary(e)) {
				unsigned int j;
				for (j = 0; j < 2; j++) {
					if (!BLI_ghash_haskey(gh, (&e->v1)[j])) {
						SUID_Int v_id = bm_face_region_vert_boundary_id((&e->v1)[j]);
						BLI_ghash_insert(gh, (&e->v1)[j], (void *)v_id);
						BLI_LINKSTACK_PUSH(vert_queue_prev, (&e->v1)[j]);
						vert_queue_used += 1;
					}
				}
			}
			else {
				/* use incase (depth == 0), no interior verts */
				e_pivot_fallback = e;
			}
		} while ((l_iter = l_iter->next) != l_first);
	}

	while (BLI_LINKSTACK_SIZE(vert_queue_prev)) {
		BMVert *v;
		while ((v = BLI_LINKSTACK_POP(vert_queue_prev))) {
			BMIter eiter;
			BMEdge *e;
			BLI_assert(BLI_ghash_haskey(gh, v));
			BLI_assert((SUID_Int)BLI_ghash_lookup(gh, v) > 0);
			BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
				if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
					BMVert *v_other = BM_edge_other_vert(e, v);
					if (BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
						if (!BLI_ghash_haskey(gh, v_other)) {
							/* add as negative, so we know not to read from them this pass */
							const SUID_Int v_id_other = -bm_face_region_vert_pass_id(gh, v_other);
							BLI_ghash_insert(gh, v_other, (void *)v_id_other);
							BLI_LINKSTACK_PUSH(vert_queue_next, v_other);
							vert_queue_used += 1;
						}
					}
				}
			}
		}

		/* flip all the newly added hashes to positive */
		{
			LinkNode *v_link;
			for (v_link = vert_queue_next; v_link; v_link = v_link->next) {
				SUID_Int *v_id_p = (SUID_Int *)BLI_ghash_lookup_p(gh, v_link->link);
				*v_id_p = -(*v_id_p);
				BLI_assert(*v_id_p > 0);
			}
		}

		BLI_LINKSTACK_SWAP(vert_queue_prev, vert_queue_next);
		pass += 1;

		if (vert_queue_used == verts_region_len) {
			break;
		}
	}

	if (BLI_LINKSTACK_SIZE(vert_queue_prev) >= 2) {
		/* common case - we managed to find some interior verts */
		LinkNode *v_link;
		BMEdge *e_pivot_best = NULL;
		SUID_Int e_pivot_best_id[2] = {0, 0};

		/* temp untag, so we can quickly know what other verts are in this last pass */
		for (v_link = vert_queue_prev; v_link; v_link = v_link->next) {
			BMVert *v = v_link->link;
			BM_elem_flag_disable(v, BM_ELEM_TAG);
		}

		/* restore correct tagging */
		for (v_link = vert_queue_prev; v_link; v_link = v_link->next) {
			BMIter eiter;
			BMEdge *e_test;

			BMVert *v = v_link->link;
			BM_elem_flag_enable(v, BM_ELEM_TAG);

			BM_ITER_ELEM (e_test, &eiter, v, BM_EDGES_OF_VERT) {
				if (BM_elem_flag_test(e_test, BM_ELEM_TAG)) {
					BMVert *v_other = BM_edge_other_vert(e_test, v);
					if (BM_elem_flag_test(v_other, BM_ELEM_TAG) == false) {
						bm_face_region_pivot_edge_use_best(gh, e_test, &e_pivot_best, e_pivot_best_id);
					}
				}
			}
		}

		e_pivot = e_pivot_best;
	}

	if ((e_pivot == NULL) && BLI_LINKSTACK_SIZE(vert_queue_prev)) {
		/* find the best single edge */
		BMEdge *e_pivot_best = NULL;
		SUID_Int e_pivot_best_id[2] = {0, 0};

		LinkNode *v_link;

		/* reduce a pass since we're having to step into a previous passes vert,
		 * and will be closer to the boundary */
		BLI_assert(pass != 0);
		pass -= 1;

		for (v_link = vert_queue_prev; v_link; v_link = v_link->next) {
			BMVert *v = v_link->link;

			BMIter eiter;
			BMEdge *e_test;
			BM_ITER_ELEM (e_test, &eiter, v, BM_EDGES_OF_VERT) {
				if (BM_elem_flag_test(e_test, BM_ELEM_TAG)) {
					BMVert *v_other = BM_edge_other_vert(e_test, v);
					if (BM_elem_flag_test(v_other, BM_ELEM_TAG)) {
						bm_face_region_pivot_edge_use_best(gh, e_test, &e_pivot_best, e_pivot_best_id);
					}
				}
			}
		}

		e_pivot = e_pivot_best;
	}

	BLI_LINKSTACK_FREE(vert_queue_prev);
	BLI_LINKSTACK_FREE(vert_queue_next);

	BLI_ghash_free(gh, NULL, NULL);

	if (e_pivot == NULL) {
#ifdef DEBUG_PRINT
		printf("%s: using fallback edge!\n", __func__);
#endif
		e_pivot = e_pivot_fallback;
		pass = 0;
	}

	*r_depth = (unsigned int)pass;

	return e_pivot;
}
/** \} */

#endif  /* USE_PIVOT_SEARCH */


/* -------------------------------------------------------------------- */
/* Quick UUID pass - identify candidates */

#ifdef USE_PIVOT_FASTMATCH

/** \name Fast Match
 * \{ */

typedef uintptr_t UUIDFashMatch;

static UUIDFashMatch bm_vert_fasthash_single(BMVert *v)
{
	BMIter eiter;
	BMEdge *e;
	UUIDFashMatch e_num = 0, f_num = 0, l_num = 0;

#define PRIME_EDGE 7
#define PRIME_FACE 31
#define PRIME_LOOP 61

	BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
		if (!BM_edge_is_wire(e)) {
			BMLoop *l_iter = e->l;
			e_num += 1;
			do {
				f_num += 1;
				l_num += (unsigned int)l_iter->f->len;
			} while ((l_iter = l_iter->radial_next) != e->l);
		}
	}

	return ((e_num * PRIME_EDGE) ^
	        (f_num * PRIME_FACE) *
	        (l_num * PRIME_LOOP));

#undef PRIME_EDGE
#undef PRIME_FACE
#undef PRIME_LOOP
}

static UUIDFashMatch *bm_vert_fasthash_create(
        BMesh *bm, const unsigned int depth)
{
	UUIDFashMatch *id_prev;
	UUIDFashMatch *id_curr;
	unsigned int pass, i;
	BMVert *v;
	BMIter iter;

	id_prev = MEM_mallocN(sizeof(*id_prev) * (unsigned int)bm->totvert, __func__);
	id_curr = MEM_mallocN(sizeof(*id_curr) * (unsigned int)bm->totvert, __func__);

	BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
		id_prev[i] = bm_vert_fasthash_single(v);
	}

	for (pass = 0; pass < depth; pass++) {
		BMEdge *e;

		memcpy(id_curr, id_prev, sizeof(*id_prev) * (unsigned int)bm->totvert);

		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_edge_is_wire(e) == false) {
				const int i1 = BM_elem_index_get(e->v1);
				const int i2 = BM_elem_index_get(e->v2);

				id_curr[i1] += id_prev[i2];
				id_curr[i2] += id_prev[i1];
			}
		}
	}
	MEM_freeN(id_prev);

	return id_curr;
}

static void bm_vert_fasthash_edge_order(
        UUIDFashMatch *fm, const BMEdge *e, UUIDFashMatch e_fm[2])
{
	e_fm[0] = fm[BM_elem_index_get(e->v1)];
	e_fm[1] = fm[BM_elem_index_get(e->v2)];

	if (e_fm[0] > e_fm[1]) {
		SWAP(UUIDFashMatch, e_fm[0], e_fm[1]);
	}
}

static bool bm_vert_fasthash_edge_is_match(
        UUIDFashMatch *fm, const BMEdge *e_a, const BMEdge *e_b)
{
	UUIDFashMatch e_a_fm[2];
	UUIDFashMatch e_b_fm[2];

	bm_vert_fasthash_edge_order(fm, e_a, e_a_fm);
	bm_vert_fasthash_edge_order(fm, e_b, e_b_fm);

	return ((e_a_fm[0] == e_b_fm[0]) &&
	        (e_a_fm[1] == e_b_fm[1]));
}

static void bm_vert_fasthash_destroy(
        UUIDFashMatch *fm)
{
	MEM_freeN(fm);
}

/** \} */

#endif  /* USE_PIVOT_FASTMATCH */


/**
 * Take a face-region and return a list of matching face-regions.
 *
 * \param faces_region  A single, contiguous face-region.
 * \return  A list of matching null-terminated face-region arrays.
 */
int BM_mesh_region_match(
        BMesh *bm,
        BMFace **faces_region, unsigned int faces_region_len,
        ListBase *r_face_regions)
{
	BMEdge *e_src;
	BMEdge *e_dst;
	BMIter iter;
	unsigned int verts_region_len = 0;
	unsigned int faces_result_len = 0;
	/* number of steps from e_src to a boundary vert */
	unsigned int depth;


#ifdef USE_WALKER_REUSE
	UUIDWalk w_src, w_dst;
#endif

#ifdef USE_PIVOT_FASTMATCH
	UUIDFashMatch *fm;
#endif

#ifdef DEBUG_PRINT
	int search_num = 0;
#endif

#ifdef DEBUG_TIME
	TIMEIT_START(region_match);
#endif

	/* initialize visited verts */
	BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
	bm_face_array_visit(faces_region, faces_region_len, &verts_region_len, true);

	/* needed for 'ghashutil_bmelem_indexhash' */
	BM_mesh_elem_index_ensure(bm, BM_VERT | BM_FACE);

#ifdef USE_PIVOT_SEARCH
	e_src = bm_face_region_pivot_edge_find(
	        faces_region, faces_region_len,
	        verts_region_len, &depth);

	/* see which edge is added */
#if 0
	BM_select_history_clear(bm);
	if (e_src) {
		BM_select_history_store(bm, e_src);
	}
#endif

#else
	/* quick test only! */
	e_src = BM_mesh_active_edge_get(bm);
#endif

	if (e_src == NULL) {
#ifdef DEBUG_PRINT
		printf("Couldn't find 'e_src'");
#endif
		return 0;
	}

	BLI_listbase_clear(r_face_regions);

#ifdef USE_PIVOT_FASTMATCH
	if (depth > 0) {
		fm = bm_vert_fasthash_create(bm, depth);
	}
	else {
		fm = NULL;
	}
#endif

#ifdef USE_WALKER_REUSE
	bm_uuidwalk_init(&w_src, faces_region_len, verts_region_len);
	bm_uuidwalk_init(&w_dst, faces_region_len, verts_region_len);
#endif

	BM_ITER_MESH (e_dst, &iter, bm, BM_EDGES_OF_MESH) {
		BMFace **faces_result;
		unsigned int faces_result_len_out;

		if (BM_elem_flag_test(e_dst, BM_ELEM_TAG)) {
			continue;
		}

#ifdef USE_PIVOT_FASTMATCH
		if (fm && !bm_vert_fasthash_edge_is_match(fm, e_src, e_dst)) {
			continue;
		}
#endif

#ifdef DEBUG_PRINT
		search_num += 1;
#endif

		faces_result = bm_mesh_region_match_pair(
#ifdef USE_WALKER_REUSE
		        &w_src, &w_dst,
#endif
		        e_src, e_dst,
		        faces_region_len,
		        verts_region_len,
		        &faces_result_len_out);

		/* tag verts as visited */
		if (faces_result) {
			LinkData *link;

			bm_face_array_visit(faces_result, faces_result_len_out, NULL, false);

			link = BLI_genericNodeN(faces_result);
			BLI_addtail(r_face_regions, link);
			faces_result_len += 1;
		}
	}

#ifdef USE_WALKER_REUSE
	bm_uuidwalk_free(&w_src);
	bm_uuidwalk_free(&w_dst);
#else
	(void)bm_uuidwalk_clear;
#endif

#ifdef USE_PIVOT_FASTMATCH
	if (fm) {
		bm_vert_fasthash_destroy(fm);
	}
#endif

#ifdef DEBUG_PRINT
	printf("%s: search: %d, found %d\n", __func__, search_num, faces_result_len);
#endif

#ifdef DEBUG_TIME
	TIMEIT_END(region_match);
#endif

	return (int)faces_result_len;
}
