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


/* for callbacks */

static void *getEditMesh(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_MESH) {
		Mesh *me = obedit->data;
		return me->edit_btmesh;
	}
	return NULL;
}

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
} UndoMesh;

/* undo simply makes copies of a bmesh */
static void *editbtMesh_to_undoMesh(void *emv, void *obdata)
{
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

	return um;
}

static void undoMesh_to_editbtMesh(void *umv, void *em_v, void *obdata)
{
	BMEditMesh *em = em_v, *em_tmp;
	Object *ob = em->ob;
	UndoMesh *um = umv;
	BMesh *bm;
	Key *key = ((Mesh *) obdata)->key;

	const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(&um->me);

	em->bm->shapenr = um->shapenr;

	EDBM_mesh_free(em);

	bm = BM_mesh_create(&allocsize);

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
}

static void free_undo(void *me_v)
{
	Mesh *me = me_v;
	if (me->key) {
		BKE_key_free(me->key);
		MEM_freeN(me->key);
	}

	BKE_mesh_free(me, false);
	MEM_freeN(me);
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
