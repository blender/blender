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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the Sculpt Mode tools
 *
 */

/** \file blender/editors/sculpt_paint/sculpt_undo.c
 *  \ingroup edsculpt
 */


#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_mesh_types.h"

#include "BKE_ccg.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_subsurf.h"

#include "WM_api.h"
#include "WM_types.h"

#include "GPU_buffers.h"

#include "ED_sculpt.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

/************************** Undo *************************/

static void update_cb(PBVHNode *node, void *rebuild)
{
	BLI_pbvh_node_mark_update(node);
	if (*((int *)rebuild))
		BLI_pbvh_node_mark_rebuild_draw(node);
	BLI_pbvh_node_fully_hidden_set(node, 0);
}

static void sculpt_undo_restore_deformed(const SculptSession *ss,
                                         SculptUndoNode *unode,
                                         int uindex, int oindex,
                                         float coord[3])
{
	if (unode->orig_co) {
		swap_v3_v3(coord, unode->orig_co[uindex]);
		copy_v3_v3(unode->co[uindex], ss->deform_cos[oindex]);
	}
	else {
		swap_v3_v3(coord, unode->co[uindex]);
	}
}

static int sculpt_undo_restore_coords(bContext *C, DerivedMesh *dm, SculptUndoNode *unode)
{
	Scene *scene = CTX_data_scene(C);
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	MVert *mvert;
	int *index, i, j;	
	
	if (unode->maxvert) {
		/* regular mesh restore */

		if (ss->kb && strcmp(ss->kb->name, unode->shapeName)) {
			/* shape key has been changed before calling undo operator */

			Key *key = ob_get_key(ob);
			KeyBlock *kb = key_get_named_keyblock(key, unode->shapeName);

			if (kb) {
				ob->shapenr = BLI_findindex(&key->block, kb) + 1;

				sculpt_update_mesh_elements(scene, sd, ob, 0);
				WM_event_add_notifier(C, NC_OBJECT | ND_DATA, ob);
			}
			else {
				/* key has been removed -- skip this undo node */
				return 0;
			}
		}

		index = unode->index;
		mvert = ss->mvert;

		if (ss->kb) {
			float (*vertCos)[3];
			vertCos = key_to_vertcos(ob, ss->kb);

			for (i = 0; i < unode->totvert; i++) {
				if (ss->modifiers_active) sculpt_undo_restore_deformed(ss, unode, i, index[i], vertCos[index[i]]);
				else {
					if (unode->orig_co) swap_v3_v3(vertCos[index[i]], unode->orig_co[i]);
					else swap_v3_v3(vertCos[index[i]], unode->co[i]);
				}
			}

			/* propagate new coords to keyblock */
			sculpt_vertcos_to_key(ob, ss->kb, vertCos);

			/* pbvh uses it's own mvert array, so coords should be */
			/* propagated to pbvh here */
			BLI_pbvh_apply_vertCos(ss->pbvh, vertCos);

			MEM_freeN(vertCos);
		}
		else {
			for (i = 0; i < unode->totvert; i++) {
				if (ss->modifiers_active) sculpt_undo_restore_deformed(ss, unode, i, index[i], mvert[index[i]].co);
				else {
					if (unode->orig_co) swap_v3_v3(mvert[index[i]].co, unode->orig_co[i]);
					else swap_v3_v3(mvert[index[i]].co, unode->co[i]);
				}
				mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
			}
		}
	}
	else if (unode->maxgrid && dm->getGridData) {
		/* multires restore */
		CCGElem **grids, *grid;
		CCGKey key;
		float (*co)[3];
		int gridsize;

		grids = dm->getGridData(dm);
		gridsize = dm->getGridSize(dm);
		dm->getGridKey(dm, &key);

		co = unode->co;
		for (j = 0; j < unode->totgrid; j++) {
			grid = grids[unode->grids[j]];

			for (i = 0; i < gridsize * gridsize; i++, co++)
				swap_v3_v3(CCG_elem_offset_co(&key, grid, i), co[0]);
		}
	}

	return 1;
}

static int sculpt_undo_restore_hidden(bContext *C, DerivedMesh *dm,
                                      SculptUndoNode *unode)
{
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	int i;

	if (unode->maxvert) {
		MVert *mvert = ss->mvert;
		
		for (i = 0; i < unode->totvert; i++) {
			MVert *v = &mvert[unode->index[i]];
			int uval = BLI_BITMAP_GET(unode->vert_hidden, i);

			BLI_BITMAP_MODIFY(unode->vert_hidden, i,
			                  v->flag & ME_HIDE);
			if (uval)
				v->flag |= ME_HIDE;
			else
				v->flag &= ~ME_HIDE;
			
			v->flag |= ME_VERT_PBVH_UPDATE;
		}
	}
	else if (unode->maxgrid && dm->getGridData) {
		BLI_bitmap *grid_hidden = dm->getGridHidden(dm);
		
		for (i = 0; i < unode->totgrid; i++) {
			SWAP(BLI_bitmap,
			     unode->grid_hidden[i],
			     grid_hidden[unode->grids[i]]);
			
		}
	}

	return 1;
}

static int sculpt_undo_restore_mask(bContext *C, DerivedMesh *dm, SculptUndoNode *unode)
{
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	MVert *mvert;
	float *vmask;
	int *index, i, j;	
	
	if (unode->maxvert) {
		/* regular mesh restore */

		index = unode->index;
		mvert = ss->mvert;
		vmask = ss->vmask;

		for (i = 0; i < unode->totvert; i++) {
			SWAP(float, vmask[index[i]], unode->mask[i]);
			mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
		}
	}
	else if (unode->maxgrid && dm->getGridData) {
		/* multires restore */
		CCGElem **grids, *grid;
		CCGKey key;
		float *mask;
		int gridsize;

		grids = dm->getGridData(dm);
		gridsize = dm->getGridSize(dm);
		dm->getGridKey(dm, &key);

		mask = unode->mask;
		for (j = 0; j < unode->totgrid; j++) {
			grid = grids[unode->grids[j]];

			for (i = 0; i < gridsize * gridsize; i++, mask++)
				SWAP(float, *CCG_elem_offset_mask(&key, grid, i), *mask);
		}
	}

	return 1;
}

static void sculpt_undo_restore(bContext *C, ListBase *lb)
{
	Scene *scene = CTX_data_scene(C);
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	Object *ob = CTX_data_active_object(C);
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, 0);
	SculptSession *ss = ob->sculpt;
	SculptUndoNode *unode;
	MultiresModifierData *mmd;
	int update = FALSE, rebuild = FALSE;

	sculpt_update_mesh_elements(scene, sd, ob, 0);

	for (unode = lb->first; unode; unode = unode->next) {
		if (!(strcmp(unode->idname, ob->id.name) == 0))
			continue;

		/* check if undo data matches current data well enough to
		 * continue */
		if (unode->maxvert) {
			if (ss->totvert != unode->maxvert)
				continue;
		}
		else if (unode->maxgrid && dm->getGridData) {
			if ((dm->getNumGrids(dm) != unode->maxgrid) ||
			    (dm->getGridSize(dm) != unode->gridsize))
			{
				continue;
			}
		}
		else {
			continue;
		}

		switch (unode->type) {
			case SCULPT_UNDO_COORDS:
				if (sculpt_undo_restore_coords(C, dm, unode))
					update = TRUE;
				break;
			case SCULPT_UNDO_HIDDEN:
				if (sculpt_undo_restore_hidden(C, dm, unode))
					rebuild = TRUE;
				break;
			case SCULPT_UNDO_MASK:
				if (sculpt_undo_restore_mask(C, dm, unode))
					update = TRUE;
				break;
		}
	}

	if (update || rebuild) {
		int tag_update = 0;
		/* we update all nodes still, should be more clever, but also
		 * needs to work correct when exiting/entering sculpt mode and
		 * the nodes get recreated, though in that case it could do all */
		BLI_pbvh_search_callback(ss->pbvh, NULL, NULL, update_cb, &rebuild);
		BLI_pbvh_update(ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw, NULL);

		if ((mmd = sculpt_multires_active(scene, ob))) {
			if (rebuild)
				multires_mark_as_modified(ob, MULTIRES_HIDDEN_MODIFIED);
			else
				multires_mark_as_modified(ob, MULTIRES_COORDS_MODIFIED);
		}

		tag_update = ((Mesh *)ob->data)->id.us > 1;

		if (ss->modifiers_active) {
			Mesh *mesh = ob->data;
			BKE_mesh_calc_normals_tessface(mesh->mvert, mesh->totvert,
			                               mesh->mface, mesh->totface, NULL);

			free_sculptsession_deformMats(ss);
			tag_update |= 1;
		}

		if (tag_update)
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

		/* for non-PBVH drawing, need to recreate VBOs */
		GPU_drawobject_free(ob->derivedFinal);
	}
}

static void sculpt_undo_free(ListBase *lb)
{
	SculptUndoNode *unode;
	int i;

	for (unode = lb->first; unode; unode = unode->next) {
		if (unode->co)
			MEM_freeN(unode->co);
		if (unode->no)
			MEM_freeN(unode->no);
		if (unode->index)
			MEM_freeN(unode->index);
		if (unode->grids)
			MEM_freeN(unode->grids);
		if (unode->layer_disp)
			MEM_freeN(unode->layer_disp);
		if (unode->orig_co)
			MEM_freeN(unode->orig_co);
		if (unode->vert_hidden)
			MEM_freeN(unode->vert_hidden);
		if (unode->grid_hidden) {
			for (i = 0; i < unode->totgrid; i++) {
				if (unode->grid_hidden[i])
					MEM_freeN(unode->grid_hidden[i]);
			}
			MEM_freeN(unode->grid_hidden);
		}
		if (unode->mask)
			MEM_freeN(unode->mask);
	}
}

SculptUndoNode *sculpt_undo_get_node(PBVHNode *node)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_MESH);
	SculptUndoNode *unode;

	if (!lb)
		return NULL;

	for (unode = lb->first; unode; unode = unode->next)
		if (unode->node == node)
			return unode;

	return NULL;
}

static void sculpt_undo_alloc_and_store_hidden(PBVH *pbvh,
                                               SculptUndoNode *unode)
{
	PBVHNode *node = unode->node;
	BLI_bitmap *grid_hidden;
	int i, *grid_indices, totgrid;

	grid_hidden = BLI_pbvh_grid_hidden(pbvh);

	BLI_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid,
	                        NULL, NULL, NULL, NULL);
			
	unode->grid_hidden = MEM_mapallocN(sizeof(BLI_bitmap) * totgrid,
	                                   "unode->grid_hidden");
		
	for (i = 0; i < totgrid; i++) {
		if (grid_hidden[grid_indices[i]])
			unode->grid_hidden[i] = MEM_dupallocN(grid_hidden[grid_indices[i]]);
		else
			unode->grid_hidden[i] = NULL;
	}
}

static SculptUndoNode *sculpt_undo_alloc_node(Object *ob, PBVHNode *node,
                                              SculptUndoType type)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_MESH);
	SculptUndoNode *unode;
	SculptSession *ss = ob->sculpt;
	int totvert, allvert, totgrid, maxgrid, gridsize, *grids;
	
	unode = MEM_callocN(sizeof(SculptUndoNode), "SculptUndoNode");
	BLI_strncpy(unode->idname, ob->id.name, sizeof(unode->idname));
	unode->type = type;
	unode->node = node;

	BLI_pbvh_node_num_verts(ss->pbvh, node, &totvert, &allvert);
	BLI_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid,
	                        &maxgrid, &gridsize, NULL, NULL);

	unode->totvert = totvert;
	
	/* we will use this while sculpting, is mapalloc slow to access then? */

	/* general TODO, fix count_alloc */
	switch (type) {
		case SCULPT_UNDO_COORDS:
			unode->co = MEM_mapallocN(sizeof(float) * 3 * allvert, "SculptUndoNode.co");
			unode->no = MEM_mapallocN(sizeof(short) * 3 * allvert, "SculptUndoNode.no");
			undo_paint_push_count_alloc(UNDO_PAINT_MESH,
			                            (sizeof(float) * 3 +
			                             sizeof(short) * 3 +
			                             sizeof(int)) * allvert);
			break;
		case SCULPT_UNDO_HIDDEN:
			if (maxgrid)
				sculpt_undo_alloc_and_store_hidden(ss->pbvh, unode);
			else
				unode->vert_hidden = BLI_BITMAP_NEW(allvert, "SculptUndoNode.vert_hidden");
		
			break;
		case SCULPT_UNDO_MASK:
			unode->mask = MEM_mapallocN(sizeof(float) * allvert, "SculptUndoNode.mask");
			undo_paint_push_count_alloc(UNDO_PAINT_MESH, (sizeof(float) * sizeof(int)) * allvert);
			break;
	}
	
	BLI_addtail(lb, unode);

	if (maxgrid) {
		/* multires */
		unode->maxgrid = maxgrid;
		unode->totgrid = totgrid;
		unode->gridsize = gridsize;
		unode->grids = MEM_mapallocN(sizeof(int) * totgrid, "SculptUndoNode.grids");
	}
	else {
		/* regular mesh */
		unode->maxvert = ss->totvert;
		unode->index = MEM_mapallocN(sizeof(int) * allvert, "SculptUndoNode.index");
	}

	if (ss->modifiers_active)
		unode->orig_co = MEM_callocN(allvert * sizeof(*unode->orig_co), "undoSculpt orig_cos");

	return unode;
}

static void sculpt_undo_store_coords(Object *ob, SculptUndoNode *unode)
{
	SculptSession *ss = ob->sculpt;
	PBVHVertexIter vd;

	BLI_pbvh_vertex_iter_begin(ss->pbvh, unode->node, vd, PBVH_ITER_ALL)
	{
		copy_v3_v3(unode->co[vd.i], vd.co);
		if (vd.no) copy_v3_v3_short(unode->no[vd.i], vd.no);
		else normal_float_to_short_v3(unode->no[vd.i], vd.fno);

		if (ss->modifiers_active)
			copy_v3_v3(unode->orig_co[vd.i], ss->orig_cos[unode->index[vd.i]]);
	}
	BLI_pbvh_vertex_iter_end;
}

static void sculpt_undo_store_hidden(Object *ob, SculptUndoNode *unode)
{
	PBVH *pbvh = ob->sculpt->pbvh;
	PBVHNode *node = unode->node;

	if (unode->grids) {
		/* already stored during allocation */
	}
	else {
		MVert *mvert;
		int *vert_indices, allvert;
		int i;
		
		BLI_pbvh_node_num_verts(pbvh, node, NULL, &allvert);
		BLI_pbvh_node_get_verts(pbvh, node, &vert_indices, &mvert);
		for (i = 0; i < allvert; i++) {
			BLI_BITMAP_MODIFY(unode->vert_hidden, i,
			                  mvert[vert_indices[i]].flag & ME_HIDE);
		}
	}
}

static void sculpt_undo_store_mask(Object *ob, SculptUndoNode *unode)
{
	SculptSession *ss = ob->sculpt;
	PBVHVertexIter vd;

	BLI_pbvh_vertex_iter_begin(ss->pbvh, unode->node, vd, PBVH_ITER_ALL)
	{
		unode->mask[vd.i] = *vd.mask;
	}
	BLI_pbvh_vertex_iter_end;
}

SculptUndoNode *sculpt_undo_push_node(Object *ob, PBVHNode *node,
                                      SculptUndoType type)
{
	SculptSession *ss = ob->sculpt;
	SculptUndoNode *unode;

	/* list is manipulated by multiple threads, so we lock */
	BLI_lock_thread(LOCK_CUSTOM1);

	if ((unode = sculpt_undo_get_node(node))) {
		BLI_unlock_thread(LOCK_CUSTOM1);
		return unode;
	}

	unode = sculpt_undo_alloc_node(ob, node, type);
	
	BLI_unlock_thread(LOCK_CUSTOM1);

	/* copy threaded, hopefully this is the performance critical part */

	if (unode->grids) {
		int totgrid, *grids;
		BLI_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid,
		                        NULL, NULL, NULL, NULL);
		memcpy(unode->grids, grids, sizeof(int) * totgrid);
	}
	else {
		int *vert_indices, allvert;
		BLI_pbvh_node_num_verts(ss->pbvh, node, NULL, &allvert);
		BLI_pbvh_node_get_verts(ss->pbvh, node, &vert_indices, NULL);
		memcpy(unode->index, vert_indices, sizeof(int) * unode->totvert);
	}

	switch (type) {
		case SCULPT_UNDO_COORDS:
			sculpt_undo_store_coords(ob, unode);
			break;
		case SCULPT_UNDO_HIDDEN:
			sculpt_undo_store_hidden(ob, unode);
			break;
		case SCULPT_UNDO_MASK:
			sculpt_undo_store_mask(ob, unode);
			break;
	}

	/* store active shape key */
	if (ss->kb) BLI_strncpy(unode->shapeName, ss->kb->name, sizeof(ss->kb->name));
	else unode->shapeName[0] = '\0';

	return unode;
}

void sculpt_undo_push_begin(const char *name)
{
	undo_paint_push_begin(UNDO_PAINT_MESH, name,
	                      sculpt_undo_restore, sculpt_undo_free);
}

void sculpt_undo_push_end(void)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_MESH);
	SculptUndoNode *unode;

	/* we don't need normals in the undo stack */
	for (unode = lb->first; unode; unode = unode->next) {
		if (unode->no) {
			MEM_freeN(unode->no);
			unode->no = NULL;
		}

		if (unode->layer_disp) {
			MEM_freeN(unode->layer_disp);
			unode->layer_disp = NULL;
		}
	}

	undo_paint_push_end(UNDO_PAINT_MESH);
}
