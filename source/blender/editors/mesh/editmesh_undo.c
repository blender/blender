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

/** \file blender/editors/mesh/editmesh_undo.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_key_types.h"

#include "BLI_listbase.h"

#include "BKE_DerivedMesh.h"
#include "BKE_context.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_editmesh.h"

#include "ED_mesh.h"
#include "ED_util.h"

#define USE_ARRAY_STORE

#ifdef USE_ARRAY_STORE
// #  define DEBUG_PRINT
// #  define DEBUG_TIME
#  ifdef DEBUG_TIME
#    include "PIL_time_utildefines.h"
#  endif

#  include "BLI_array_store.h"
#  include "BLI_array_store_utils.h"
   /* check on best size later... */
#  define ARRAY_CHUNK_SIZE 256

#  define USE_ARRAY_STORE_THREAD
#endif

#ifdef USE_ARRAY_STORE_THREAD
#  include "BLI_task.h"
#endif


#ifdef USE_ARRAY_STORE

/* Single linked list of layers stored per type */
typedef struct BArrayCustomData {
	struct BArrayCustomData *next;
	CustomDataType type;
	int states_len;  /* number of layers for each type */
	BArrayState *states[0];
} BArrayCustomData;

#endif

typedef struct UndoMesh {
	Mesh me;
	int selectmode;

	/** \note
	 * this isn't a prefect solution, if you edit keys and change shapes this works well (fixing [#32442]),
	 * but editing shape keys, going into object mode, removing or changing their order,
	 * then go back into editmode and undo will give issues - where the old index will be out of sync
	 * with the new object index.
	 *
	 * There are a few ways this could be made to work but for now its a known limitation with mixing
	 * object and editmode operations - Campbell */
	int shapenr;

#ifdef USE_ARRAY_STORE
	/* NULL arrays are considered empty */
	struct { /* most data is stored as 'custom' data */
		BArrayCustomData *vdata, *edata, *ldata, *pdata;
		BArrayState **keyblocks;
		BArrayState *mselect;
	} store;
#endif  /* USE_ARRAY_STORE */
} UndoMesh;


#ifdef USE_ARRAY_STORE

/** \name Array Store
 * \{ */

static struct {
	struct BArrayStore_AtSize bs_stride;
	int users;

	/* We could have the undo API pass in the previous state, for now store a local list */
	ListBase local_links;

#ifdef USE_ARRAY_STORE_THREAD
		TaskPool *task_pool;
#endif

} um_arraystore = {{NULL}};

static void um_arraystore_cd_compact(
        struct CustomData *cdata, const size_t data_len,
        bool create,
        const BArrayCustomData *bcd_reference,
        BArrayCustomData **r_bcd_first)
{
	if (data_len == 0) {
		if (create) {
			*r_bcd_first = NULL;
		}
	}

	const BArrayCustomData *bcd_reference_current = bcd_reference;
	BArrayCustomData *bcd = NULL, *bcd_first = NULL, *bcd_prev = NULL;
	for (int layer_start = 0, layer_end; layer_start < cdata->totlayer; layer_start = layer_end) {
		const CustomDataType type = cdata->layers[layer_start].type;

		layer_end = layer_start + 1;
		while ((layer_end < cdata->totlayer) &&
		       (type == cdata->layers[layer_end].type))
		{
			layer_end++;
		}

		const int stride = CustomData_sizeof(type);
		BArrayStore *bs = create ? BLI_array_store_at_size_ensure(&um_arraystore.bs_stride, stride, ARRAY_CHUNK_SIZE) : NULL;
		const int layer_len = layer_end - layer_start;

		if (create) {
			if (bcd_reference_current && (bcd_reference_current->type == type)) {
				/* common case, the reference is aligned */
			}
			else {
				bcd_reference_current = NULL;

				/* do a full lookup when un-alligned */
				if (bcd_reference) {
					const BArrayCustomData *bcd_iter = bcd_reference;
					while (bcd_iter) {
						if (bcd_iter->type == type) {
							bcd_reference_current = bcd_iter;
							break;
						}
						bcd_iter = bcd_iter->next;
					}
				}
			}
		}

		if (create) {
			bcd = MEM_callocN(sizeof(BArrayCustomData) + (layer_len * sizeof(BArrayState *)), __func__);
			bcd->next = NULL;
			bcd->type = type;
			bcd->states_len = layer_end - layer_start;

			if (bcd_prev) {
				bcd_prev->next = bcd;
				bcd_prev = bcd;
			}
			else {
				bcd_first = bcd;
				bcd_prev  = bcd;
			}
		}

		CustomDataLayer *layer = &cdata->layers[layer_start];
		for (int i = 0; i < layer_len; i++, layer++) {
			if (create) {
				if (layer->data) {
					BArrayState *state_reference =
					        (bcd_reference_current && i < bcd_reference_current->states_len) ?
					         bcd_reference_current->states[i] : NULL;
					bcd->states[i] = BLI_array_store_state_add(
					        bs, layer->data, (size_t)data_len * stride, state_reference);
				}
				else {
					bcd->states[i] = NULL;
				}
			}

			if (layer->data) {
				MEM_freeN(layer->data);
				layer->data = NULL;
			}
		}

		if (create) {
			if (bcd_reference_current) {
				bcd_reference_current = bcd_reference_current->next;
			}
		}
	}

	if (create) {
		*r_bcd_first = bcd_first;
	}
}

/**
 * \note There is no room for data going out of sync here.
 * The layers and the states are stored together so this can be kept working.
 */
static void um_arraystore_cd_expand(
        const BArrayCustomData *bcd, struct CustomData *cdata, const size_t data_len)
{
	CustomDataLayer *layer = cdata->layers;
	while (bcd) {
		const int stride = CustomData_sizeof(bcd->type);
		for (int i = 0; i < bcd->states_len; i++) {
			BLI_assert(bcd->type == layer->type);
			if (bcd->states[i]) {
				size_t state_len;
				layer->data = BLI_array_store_state_data_get_alloc(bcd->states[i], &state_len);
				BLI_assert(stride * data_len == state_len);
				UNUSED_VARS_NDEBUG(stride, data_len);
			}
			else {
				layer->data = NULL;
			}
			layer++;
		}
		bcd = bcd->next;
	}
}

static void um_arraystore_cd_free(BArrayCustomData *bcd)
{
	while (bcd) {
		BArrayCustomData *bcd_next = bcd->next;
		const int stride = CustomData_sizeof(bcd->type);
		BArrayStore *bs = BLI_array_store_at_size_get(&um_arraystore.bs_stride, stride);
		for (int i = 0; i <		bcd->states_len; i++) {
			if (bcd->states[i]) {
				BLI_array_store_state_remove(bs, bcd->states[i]);
			}
		}
		MEM_freeN(bcd);
		bcd = bcd_next;
	}
}

/**
 * \param create: When false, only free the arrays.
 * This is done since when reading from an undo state, they must be temporarily expanded.
 * then discarded afterwards, having this argument avoids having 2x code paths.
 */
static void um_arraystore_compact_ex(
        UndoMesh *um, const UndoMesh *um_ref,
        bool create)
{
	Mesh *me = &um->me;

	um_arraystore_cd_compact(&me->vdata, me->totvert, create, um_ref ? um_ref->store.vdata : NULL, &um->store.vdata);
	um_arraystore_cd_compact(&me->edata, me->totedge, create, um_ref ? um_ref->store.edata : NULL, &um->store.edata);
	um_arraystore_cd_compact(&me->ldata, me->totloop, create, um_ref ? um_ref->store.ldata : NULL, &um->store.ldata);
	um_arraystore_cd_compact(&me->pdata, me->totpoly, create, um_ref ? um_ref->store.pdata : NULL, &um->store.pdata);

	if (me->key && me->key->totkey) {
		const size_t stride = me->key->elemsize;
		BArrayStore *bs = create ? BLI_array_store_at_size_ensure(&um_arraystore.bs_stride, stride, ARRAY_CHUNK_SIZE) : NULL;
		if (create) {
			um->store.keyblocks = MEM_mallocN(me->key->totkey * sizeof(*um->store.keyblocks), __func__);
		}
		KeyBlock *keyblock = me->key->block.first;
		for (int i = 0; i < me->key->totkey; i++, keyblock = keyblock->next) {
			if (create) {
				BArrayState *state_reference =
				        (um_ref && um_ref->me.key && (i < um_ref->me.key->totkey)) ?
				         um_ref->store.keyblocks[i] : NULL;
				um->store.keyblocks[i] = BLI_array_store_state_add(
				        bs, keyblock->data, (size_t)keyblock->totelem * stride,
				        state_reference);
			}

			if (keyblock->data) {
				MEM_freeN(keyblock->data);
				keyblock->data = NULL;
			}
		}
	}

	if (me->mselect && me->totselect) {
		BLI_assert(create == (um->store.mselect == NULL));
		if (create) {
			BArrayState *state_reference = um_ref ? um_ref->store.mselect : NULL;
			const size_t stride = sizeof(*me->mselect);
			BArrayStore *bs = BLI_array_store_at_size_ensure(&um_arraystore.bs_stride, stride, ARRAY_CHUNK_SIZE);
			um->store.mselect = BLI_array_store_state_add(
			        bs, me->mselect, (size_t)me->totselect * stride, state_reference);
		}

		/* keep me->totselect for validation */
		MEM_freeN(me->mselect);
		me->mselect = NULL;
	}

	if (create) {
		um_arraystore.users += 1;
	}

	BKE_mesh_update_customdata_pointers(me, false);
}

/**
 * Move data from allocated arrays to de-duplicated states and clear arrays.
 */
static void um_arraystore_compact(UndoMesh *um, const UndoMesh *um_ref)
{
	um_arraystore_compact_ex(um, um_ref, true);
}

static void um_arraystore_compact_with_info(UndoMesh *um, const UndoMesh *um_ref)
{
#ifdef DEBUG_PRINT
	size_t size_expanded_prev, size_compacted_prev;
	BLI_array_store_at_size_calc_memory_usage(&um_arraystore.bs_stride, &size_expanded_prev, &size_compacted_prev);
#endif

#ifdef DEBUG_TIME
	TIMEIT_START(mesh_undo_compact);
#endif

	um_arraystore_compact(um, um_ref);

#ifdef DEBUG_TIME
	TIMEIT_END(mesh_undo_compact);
#endif

#ifdef DEBUG_PRINT
	{
		size_t size_expanded, size_compacted;
		BLI_array_store_at_size_calc_memory_usage(&um_arraystore.bs_stride, &size_expanded, &size_compacted);

		const double percent_total = size_expanded ?
		        (((double)size_compacted / (double)size_expanded) * 100.0) : -1.0;

		size_t size_expanded_step = size_expanded - size_expanded_prev;
		size_t size_compacted_step = size_compacted - size_compacted_prev;
		const double percent_step = size_expanded_step ?
		        (((double)size_compacted_step / (double)size_expanded_step) * 100.0) : -1.0;

		printf("overall memory use: %.8f%% of expanded size\n", percent_total);
		printf("step memory use:    %.8f%% of expanded size\n", percent_step);
	}
#endif
}

#ifdef USE_ARRAY_STORE_THREAD

struct UMArrayData {
	UndoMesh *um;
	const UndoMesh *um_ref;  /* can be NULL */
};
static void um_arraystore_compact_cb(TaskPool *__restrict UNUSED(pool),
                                     void *taskdata,
                                     int UNUSED(threadid))
{
	struct UMArrayData *um_data = taskdata;
	um_arraystore_compact_with_info(um_data->um, um_data->um_ref);
}

#endif  /* USE_ARRAY_STORE_THREAD */

/**
 * Remove data we only expanded for temporary use.
 */
static void um_arraystore_expand_clear(UndoMesh *um)
{
	um_arraystore_compact_ex(um, NULL, false);
}

static void um_arraystore_expand(UndoMesh *um)
{
	Mesh *me = &um->me;

	um_arraystore_cd_expand(um->store.vdata, &me->vdata, me->totvert);
	um_arraystore_cd_expand(um->store.edata, &me->edata, me->totedge);
	um_arraystore_cd_expand(um->store.ldata, &me->ldata, me->totloop);
	um_arraystore_cd_expand(um->store.pdata, &me->pdata, me->totpoly);

	if (um->store.keyblocks) {
		const size_t stride = me->key->elemsize;
		KeyBlock *keyblock = me->key->block.first;
		for (int i = 0; i < me->key->totkey; i++, keyblock = keyblock->next) {
			BArrayState *state = um->store.keyblocks[i];
			size_t state_len;
			keyblock->data = BLI_array_store_state_data_get_alloc(state, &state_len);
			BLI_assert(keyblock->totelem == (state_len / stride));
			UNUSED_VARS_NDEBUG(stride);
		}
	}

	if (um->store.mselect) {
		const size_t stride = sizeof(*me->mselect);
		BArrayState *state = um->store.mselect;
		size_t state_len;
		me->mselect = BLI_array_store_state_data_get_alloc(state, &state_len);
		BLI_assert(me->totselect == (state_len / stride));
		UNUSED_VARS_NDEBUG(stride);
	}

	/* not essential, but prevents accidental dangling pointer access */
	BKE_mesh_update_customdata_pointers(me, false);
}

static void um_arraystore_free(UndoMesh *um)
{
	Mesh *me = &um->me;

	um_arraystore_cd_free(um->store.vdata);
	um_arraystore_cd_free(um->store.edata);
	um_arraystore_cd_free(um->store.ldata);
	um_arraystore_cd_free(um->store.pdata);

	if (um->store.keyblocks) {
		const size_t stride = me->key->elemsize;
		BArrayStore *bs = BLI_array_store_at_size_get(&um_arraystore.bs_stride, stride);
		for (int i = 0; i < me->key->totkey; i++) {
			BArrayState *state = um->store.keyblocks[i];
			BLI_array_store_state_remove(bs, state);
		}
		MEM_freeN(um->store.keyblocks);
		um->store.keyblocks = NULL;
	}

	if (um->store.mselect) {
		const size_t stride = sizeof(*me->mselect);
		BArrayStore *bs = BLI_array_store_at_size_get(&um_arraystore.bs_stride, stride);
		BArrayState *state = um->store.mselect;
		BLI_array_store_state_remove(bs, state);
		um->store.mselect = NULL;
	}

	um_arraystore.users -= 1;

	BLI_assert(um_arraystore.users >= 0);

	if (um_arraystore.users == 0) {
#ifdef DEBUG_PRINT
		printf("mesh undo store: freeing all data!\n");
#endif
		BLI_array_store_at_size_clear(&um_arraystore.bs_stride);

#ifdef USE_ARRAY_STORE_THREAD
		BLI_task_pool_free(um_arraystore.task_pool);
		um_arraystore.task_pool = NULL;
#endif
	}

}

/** \} */

#endif  /* USE_ARRAY_STORE */


/* for callbacks */
/* undo simply makes copies of a bmesh */
static void *editbtMesh_to_undoMesh(void *emv, void *obdata)
{

#ifdef USE_ARRAY_STORE_THREAD
	/* changes this waits is low, but must have finished */
	if (um_arraystore.task_pool) {
		BLI_task_pool_work_and_wait(um_arraystore.task_pool);
	}
#endif

	BMEditMesh *em = emv;
	Mesh *obme = obdata;

	UndoMesh *um = MEM_callocN(sizeof(UndoMesh), "undo Mesh");

	/* make sure shape keys work */
	um->me.key = obme->key ? BKE_key_copy_nolib(obme->key) : NULL;

	/* BM_mesh_validate(em->bm); */ /* for troubleshooting */

	BM_mesh_bm_to_me(
	        em->bm, &um->me, (&(struct BMeshToMeshParams){
	            .cd_mask_extra = CD_MASK_SHAPE_KEYINDEX,
	        }));

	um->selectmode = em->selectmode;
	um->shapenr = em->bm->shapenr;

#ifdef USE_ARRAY_STORE
	{
		/* We could be more clever here,
		 * the previous undo state may be from a separate mesh. */
		const UndoMesh *um_ref = um_arraystore.local_links.last ?
		                         ((LinkData *)um_arraystore.local_links.last)->data : NULL;

		/* add oursrlves */
		BLI_addtail(&um_arraystore.local_links, BLI_genericNodeN(um));

#ifdef USE_ARRAY_STORE_THREAD
		if (um_arraystore.task_pool == NULL) {
			TaskScheduler *scheduler = BLI_task_scheduler_get();
			um_arraystore.task_pool = BLI_task_pool_create_background(scheduler, NULL);
		}

		struct UMArrayData *um_data = MEM_mallocN(sizeof(*um_data), __func__);
		um_data->um = um;
		um_data->um_ref = um_ref;

		BLI_task_pool_push(
		        um_arraystore.task_pool,
		        um_arraystore_compact_cb, um_data, true, TASK_PRIORITY_LOW);
#else
		um_arraystore_compact_with_info(um, um_ref);
#endif
	}
#endif

	return um;
}

static void undoMesh_to_editbtMesh(void *um_v, void *em_v, void *obdata)
{
	BMEditMesh *em = em_v, *em_tmp;
	Object *ob = em->ob;
	UndoMesh *um = um_v;
	BMesh *bm;
	Key *key = ((Mesh *) obdata)->key;

#ifdef USE_ARRAY_STORE
#ifdef USE_ARRAY_STORE_THREAD
	/* changes this waits is low, but must have finished */
	BLI_task_pool_work_and_wait(um_arraystore.task_pool);
#endif

#ifdef DEBUG_TIME
	TIMEIT_START(mesh_undo_expand);
#endif

	um_arraystore_expand(um);

#ifdef DEBUG_TIME
	TIMEIT_END(mesh_undo_expand);
#endif
#endif  /* USE_ARRAY_STORE */

	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(&um->me);

	em->bm->shapenr = um->shapenr;

	EDBM_mesh_free(em);

	bm = BM_mesh_create(
	        &allocsize,
	        &((struct BMeshCreateParams){.use_toolflags = true,}));

	BM_mesh_bm_from_me(
	        bm, &um->me, (&(struct BMeshFromMeshParams){
	            .calc_face_normal = true, .active_shapekey = um->shapenr,
	        }));

	em_tmp = BKE_editmesh_create(bm, true);
	*em = *em_tmp;

	em->selectmode = um->selectmode;
	bm->selectmode = um->selectmode;
	em->ob = ob;

	/* T35170: Restore the active key on the RealMesh. Otherwise 'fake' offset propagation happens
	 *         if the active is a basis for any other. */
	if (key && (key->type == KEY_RELATIVE)) {
		/* Since we can't add, remove or reorder keyblocks in editmode, it's safe to assume
		 * shapenr from restored bmesh and keyblock indices are in sync. */
		const int kb_act_idx = ob->shapenr - 1;

		/* If it is, let's patch the current mesh key block to its restored value.
		 * Else, the offsets won't be computed and it won't matter. */
		if (BKE_keyblock_is_basis(key, kb_act_idx)) {
			KeyBlock *kb_act = BLI_findlink(&key->block, kb_act_idx);

			if (kb_act->totelem != um->me.totvert) {
				/* The current mesh has some extra/missing verts compared to the undo, adjust. */
				MEM_SAFE_FREE(kb_act->data);
				kb_act->data = MEM_mallocN((size_t)(key->elemsize * bm->totvert), __func__);
				kb_act->totelem = um->me.totvert;
			}

			BKE_keyblock_update_from_mesh(&um->me, kb_act);
		}
	}

	ob->shapenr = um->shapenr;

	MEM_freeN(em_tmp);

#ifdef USE_ARRAY_STORE
	um_arraystore_expand_clear(um);
#endif
}

static void free_undo(void *um_v)
{
	UndoMesh *um = um_v;
	Mesh *me = &um->me;

#ifdef USE_ARRAY_STORE

#ifdef USE_ARRAY_STORE_THREAD
	/* changes this waits is low, but must have finished */
	BLI_task_pool_work_and_wait(um_arraystore.task_pool);
#endif

	/* we need to expand so any allocations in custom-data are freed with the mesh */
	um_arraystore_expand(um);

	{
		LinkData *link = BLI_findptr(&um_arraystore.local_links, um, offsetof(LinkData, data));
		BLI_remlink(&um_arraystore.local_links, link);
		MEM_freeN(link);
	}
	um_arraystore_free(um);
#endif

	if (me->key) {
		BKE_key_free(me->key);
		MEM_freeN(me->key);
	}

	BKE_mesh_free(me);
	MEM_freeN(me);
}

static void *getEditMesh(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_MESH) {
		Mesh *me = obedit->data;
		return me->edit_btmesh;
	}
	return NULL;
}

/* and this is all the undo system needs to know */
void undo_push_mesh(bContext *C, const char *name)
{
	/* em->ob gets out of date and crashes on mesh undo,
	 * this is an easy way to ensure its OK
	 * though we could investigate the matter further. */
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	em->ob = obedit;

	undo_editmode_push(C, name, getEditMesh, free_undo, undoMesh_to_editbtMesh, editbtMesh_to_undoMesh, NULL);
}
