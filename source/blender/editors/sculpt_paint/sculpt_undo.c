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

#include <stddef.h>

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
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_subsurf.h"

#include "WM_api.h"
#include "WM_types.h"

#include "GPU_buffers.h"

#include "ED_paint.h"

#include "bmesh.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

/************************** Undo *************************/

static void update_cb(PBVHNode *node, void *rebuild)
{
	BKE_pbvh_node_mark_update(node);
	if (*((bool *)rebuild))
		BKE_pbvh_node_mark_rebuild_draw(node);
	BKE_pbvh_node_fully_hidden_set(node, 0);
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

			Key *key = BKE_key_from_object(ob);
			KeyBlock *kb = key ? BKE_keyblock_find_name(key, unode->shapeName) : NULL;

			if (kb) {
				ob->shapenr = BLI_findindex(&key->block, kb) + 1;

				BKE_sculpt_update_mesh_elements(scene, sd, ob, 0, false);
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
			vertCos = BKE_key_convert_to_vertcos(ob, ss->kb);

			for (i = 0; i < unode->totvert; i++) {
				if (ss->modifiers_active) {
					sculpt_undo_restore_deformed(ss, unode, i, index[i], vertCos[index[i]]);
				}
				else {
					if (unode->orig_co) swap_v3_v3(vertCos[index[i]], unode->orig_co[i]);
					else swap_v3_v3(vertCos[index[i]], unode->co[i]);
				}
			}

			/* propagate new coords to keyblock */
			sculpt_vertcos_to_key(ob, ss->kb, vertCos);

			/* pbvh uses it's own mvert array, so coords should be */
			/* propagated to pbvh here */
			BKE_pbvh_apply_vertCos(ss->pbvh, vertCos);

			MEM_freeN(vertCos);
		}
		else {
			for (i = 0; i < unode->totvert; i++) {
				if (ss->modifiers_active) {
					sculpt_undo_restore_deformed(ss, unode, i, index[i], mvert[index[i]].co);
				}
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
			int uval = BLI_BITMAP_TEST(unode->vert_hidden, i);

			BLI_BITMAP_SET(unode->vert_hidden, i,
			                  v->flag & ME_HIDE);
			if (uval)
				v->flag |= ME_HIDE;
			else
				v->flag &= ~ME_HIDE;
			
			v->flag |= ME_VERT_PBVH_UPDATE;
		}
	}
	else if (unode->maxgrid && dm->getGridData) {
		BLI_bitmap **grid_hidden = dm->getGridHidden(dm);
		
		for (i = 0; i < unode->totgrid; i++) {
			SWAP(BLI_bitmap *,
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

static void sculpt_undo_bmesh_restore_generic(bContext *C,
                                              SculptUndoNode *unode,
                                              Object *ob,
                                              SculptSession *ss)
{
	if (unode->applied) {
		BM_log_undo(ss->bm, ss->bm_log);
		unode->applied = false;
	}
	else {
		BM_log_redo(ss->bm, ss->bm_log);
		unode->applied = true;
	}

	if (ELEM(unode->type, SCULPT_UNDO_MASK, SCULPT_UNDO_MASK)) {
		int i, totnode;
		PBVHNode **nodes;

#ifdef _OPENMP
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
#else
		(void)C;
#endif

		BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
		for (i = 0; i < totnode; i++) {
			BKE_pbvh_node_mark_redraw(nodes[i]);
		}

		if (nodes)
			MEM_freeN(nodes);
	}
	else {
		sculpt_pbvh_clear(ob);
	}
}

/* Create empty sculpt BMesh and enable logging */
static void sculpt_undo_bmesh_enable(Object *ob,
                                     SculptUndoNode *unode)
{
	SculptSession *ss = ob->sculpt;
	Mesh *me = ob->data;

	sculpt_pbvh_clear(ob);

	/* Create empty BMesh and enable logging */
	ss->bm = BM_mesh_create(&bm_mesh_allocsize_default);
	BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);
	sculpt_dyntopo_node_layers_add(ss);
	me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

	/* Restore the BMLog using saved entries */
	ss->bm_log = BM_log_from_existing_entries_create(ss->bm,
	                                                 unode->bm_entry);
}

static void sculpt_undo_bmesh_restore_begin(bContext *C,
                                            SculptUndoNode *unode,
                                            Object *ob,
                                            SculptSession *ss)
{
	if (unode->applied) {
		sculpt_dynamic_topology_disable(C, unode);
		unode->applied = false;
	}
	else {
		sculpt_undo_bmesh_enable(ob, unode);

		/* Restore the mesh from the first log entry */
		BM_log_redo(ss->bm, ss->bm_log);

		unode->applied = true;
	}
}

static void sculpt_undo_bmesh_restore_end(bContext *C,
                                          SculptUndoNode *unode,
                                          Object *ob,
                                          SculptSession *ss)
{
	if (unode->applied) {
		sculpt_undo_bmesh_enable(ob, unode);

		/* Restore the mesh from the last log entry */
		BM_log_undo(ss->bm, ss->bm_log);

		unode->applied = false;
	}
	else {
		/* Disable dynamic topology sculpting */
		sculpt_dynamic_topology_disable(C, NULL);
		unode->applied = true;
	}
}

/* Handle all dynamic-topology updates
 *
 * Returns true if this was a dynamic-topology undo step, otherwise
 * returns false to indicate the non-dyntopo code should run. */
static int sculpt_undo_bmesh_restore(bContext *C,
                                     SculptUndoNode *unode,
                                     Object *ob,
                                     SculptSession *ss)
{
	switch (unode->type) {
		case SCULPT_UNDO_DYNTOPO_BEGIN:
			sculpt_undo_bmesh_restore_begin(C, unode, ob, ss);
			return true;

		case SCULPT_UNDO_DYNTOPO_END:
			sculpt_undo_bmesh_restore_end(C, unode, ob, ss);
			return true;

		default:
			if (ss->bm_log) {
				sculpt_undo_bmesh_restore_generic(C, unode, ob, ss);
				return true;
			}
			break;
	}

	return false;
}

static void sculpt_undo_restore(bContext *C, ListBase *lb)
{
	Scene *scene = CTX_data_scene(C);
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	Object *ob = CTX_data_active_object(C);
	DerivedMesh *dm;
	SculptSession *ss = ob->sculpt;
	SculptUndoNode *unode;
	bool update = false, rebuild = false;
	bool need_mask = false;

	for (unode = lb->first; unode; unode = unode->next) {
		if (strcmp(unode->idname, ob->id.name) == 0) {
			if (unode->type == SCULPT_UNDO_MASK) {
				/* is possible that we can't do the mask undo (below)
				 * because of the vertex count */
				need_mask = true;
				break;
			}
		}
	}

	BKE_sculpt_update_mesh_elements(scene, sd, ob, 0, need_mask);

	/* call _after_ sculpt_update_mesh_elements() which may update 'ob->derivedFinal' */
	dm = mesh_get_derived_final(scene, ob, 0);

	if (lb->first && sculpt_undo_bmesh_restore(C, lb->first, ob, ss))
		return;

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

		switch (unode->type) {
			case SCULPT_UNDO_COORDS:
				if (sculpt_undo_restore_coords(C, dm, unode))
					update = true;
				break;
			case SCULPT_UNDO_HIDDEN:
				if (sculpt_undo_restore_hidden(C, dm, unode))
					rebuild = true;
				break;
			case SCULPT_UNDO_MASK:
				if (sculpt_undo_restore_mask(C, dm, unode))
					update = true;
				break;

			case SCULPT_UNDO_DYNTOPO_BEGIN:
			case SCULPT_UNDO_DYNTOPO_END:
			case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
				BLI_assert(!"Dynamic topology should've already been handled");
				break;
		}
	}

	if (update || rebuild) {
		bool tag_update = false;
		/* we update all nodes still, should be more clever, but also
		 * needs to work correct when exiting/entering sculpt mode and
		 * the nodes get recreated, though in that case it could do all */
		BKE_pbvh_search_callback(ss->pbvh, NULL, NULL, update_cb, &rebuild);
		BKE_pbvh_update(ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw, NULL);

		if (BKE_sculpt_multires_active(scene, ob)) {
			if (rebuild)
				multires_mark_as_modified(ob, MULTIRES_HIDDEN_MODIFIED);
			else
				multires_mark_as_modified(ob, MULTIRES_COORDS_MODIFIED);
		}

		tag_update |= ((Mesh *)ob->data)->id.us > 1;

		if (ss->kb || ss->modifiers_active) {
			Mesh *mesh = ob->data;
			BKE_mesh_calc_normals_tessface(mesh->mvert, mesh->totvert,
			                               mesh->mface, mesh->totface, NULL);

			BKE_free_sculptsession_deformMats(ss);
			tag_update |= true;
		}

		if (tag_update) {
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		}
		else {
			sculpt_update_object_bounding_box(ob);
		}

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

		if (unode->bm_entry) {
			BM_log_entry_drop(unode->bm_entry);
		}

		if (unode->bm_enter_totvert)
			CustomData_free(&unode->bm_enter_vdata, unode->bm_enter_totvert);
		if (unode->bm_enter_totedge)
			CustomData_free(&unode->bm_enter_edata, unode->bm_enter_totedge);
		if (unode->bm_enter_totloop)
			CustomData_free(&unode->bm_enter_ldata, unode->bm_enter_totloop);
		if (unode->bm_enter_totpoly)
			CustomData_free(&unode->bm_enter_pdata, unode->bm_enter_totpoly);
	}
}

static bool sculpt_undo_cleanup(bContext *C, ListBase *lb)
{
	Object *ob = CTX_data_active_object(C);
	SculptUndoNode *unode;

	unode = lb->first;

	if (unode && strcmp(unode->idname, ob->id.name) != 0) {
		if (unode->bm_entry)
			BM_log_cleanup_entry(unode->bm_entry);

		return true;
	}

	return false;
}

SculptUndoNode *sculpt_undo_get_node(PBVHNode *node)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_MESH);

	if (!lb) {
		return NULL;
	}

	return BLI_findptr(lb, node, offsetof(SculptUndoNode, node));
}

static void sculpt_undo_alloc_and_store_hidden(PBVH *pbvh,
                                               SculptUndoNode *unode)
{
	PBVHNode *node = unode->node;
	BLI_bitmap **grid_hidden;
	int i, *grid_indices, totgrid;

	grid_hidden = BKE_pbvh_grid_hidden(pbvh);

	BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid,
	                        NULL, NULL, NULL, NULL);
			
	unode->grid_hidden = MEM_mapallocN(sizeof(*unode->grid_hidden) * totgrid,
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

	if (node) {
		BKE_pbvh_node_num_verts(ss->pbvh, node, &totvert, &allvert);
		BKE_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid,
		                        &maxgrid, &gridsize, NULL, NULL);

		unode->totvert = totvert;
	}
	else
		maxgrid = 0;
	
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
		case SCULPT_UNDO_DYNTOPO_BEGIN:
		case SCULPT_UNDO_DYNTOPO_END:
		case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
			BLI_assert(!"Dynamic topology should've already been handled");
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

	BKE_pbvh_vertex_iter_begin(ss->pbvh, unode->node, vd, PBVH_ITER_ALL)
	{
		copy_v3_v3(unode->co[vd.i], vd.co);
		if (vd.no) copy_v3_v3_short(unode->no[vd.i], vd.no);
		else normal_float_to_short_v3(unode->no[vd.i], vd.fno);

		if (ss->modifiers_active)
			copy_v3_v3(unode->orig_co[vd.i], ss->orig_cos[unode->index[vd.i]]);
	}
	BKE_pbvh_vertex_iter_end;
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
		
		BKE_pbvh_node_num_verts(pbvh, node, NULL, &allvert);
		BKE_pbvh_node_get_verts(pbvh, node, &vert_indices, &mvert);
		for (i = 0; i < allvert; i++) {
			BLI_BITMAP_SET(unode->vert_hidden, i,
			                  mvert[vert_indices[i]].flag & ME_HIDE);
		}
	}
}

static void sculpt_undo_store_mask(Object *ob, SculptUndoNode *unode)
{
	SculptSession *ss = ob->sculpt;
	PBVHVertexIter vd;

	BKE_pbvh_vertex_iter_begin(ss->pbvh, unode->node, vd, PBVH_ITER_ALL)
	{
		unode->mask[vd.i] = *vd.mask;
	}
	BKE_pbvh_vertex_iter_end;
}

static SculptUndoNode *sculpt_undo_bmesh_push(Object *ob,
                                              PBVHNode *node,
                                              SculptUndoType type)
{
	ListBase *lb = undo_paint_push_get_list(UNDO_PAINT_MESH);
	SculptUndoNode *unode = lb->first;
	SculptSession *ss = ob->sculpt;
	PBVHVertexIter vd;

	if (!lb->first) {
		unode = MEM_callocN(sizeof(*unode), __func__);

		BLI_strncpy(unode->idname, ob->id.name, sizeof(unode->idname));
		unode->type = type;
		unode->applied = true;

		if (type == SCULPT_UNDO_DYNTOPO_END) {
			unode->bm_entry = BM_log_entry_add(ss->bm_log);
			BM_log_before_all_removed(ss->bm, ss->bm_log);
		}
		else if (type == SCULPT_UNDO_DYNTOPO_BEGIN) {
			Mesh *me = ob->data;

			/* Store a copy of the mesh's current vertices, loops, and
			 * polys. A full copy like this is needed because entering
			 * dynamic-topology immediately does topological edits
			 * (converting polys to triangles) that the BMLog can't
			 * fully restore from */
			CustomData_copy(&me->vdata, &unode->bm_enter_vdata, CD_MASK_MESH,
			                CD_DUPLICATE, me->totvert);
			CustomData_copy(&me->edata, &unode->bm_enter_edata, CD_MASK_MESH,
			                CD_DUPLICATE, me->totedge);
			CustomData_copy(&me->ldata, &unode->bm_enter_ldata, CD_MASK_MESH,
			                CD_DUPLICATE, me->totloop);
			CustomData_copy(&me->pdata, &unode->bm_enter_pdata, CD_MASK_MESH,
			                CD_DUPLICATE, me->totpoly);
			unode->bm_enter_totvert = me->totvert;
			unode->bm_enter_totedge = me->totedge;
			unode->bm_enter_totloop = me->totloop;
			unode->bm_enter_totpoly = me->totpoly;

			unode->bm_entry = BM_log_entry_add(ss->bm_log);
			BM_log_all_added(ss->bm, ss->bm_log);
		}
		else {
			unode->bm_entry = BM_log_entry_add(ss->bm_log);
		}

		BLI_addtail(lb, unode);
	}

	if (node) {
		switch (type) {
			case SCULPT_UNDO_COORDS:
			case SCULPT_UNDO_MASK:
				/* Before any vertex values get modified, ensure their
				 * original positions are logged */
				BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_ALL) {
					BM_log_vert_before_modified(ss->bm_log, vd.bm_vert, vd.cd_vert_mask_offset);
				}
				BKE_pbvh_vertex_iter_end;
				break;

			case SCULPT_UNDO_HIDDEN:
			{
				GSetIterator gs_iter;
				GSet *faces = BKE_pbvh_bmesh_node_faces(node);
				BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_ALL) {
					BM_log_vert_before_modified(ss->bm_log, vd.bm_vert, vd.cd_vert_mask_offset);
				}
				BKE_pbvh_vertex_iter_end;

				GSET_ITER (gs_iter, faces) {
					BMFace *f = BLI_gsetIterator_getKey(&gs_iter);
					BM_log_face_modified(ss->bm_log, f);
				}
				break;
			}

			case SCULPT_UNDO_DYNTOPO_BEGIN:
			case SCULPT_UNDO_DYNTOPO_END:
			case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
				break;
		}
	}

	return unode;
}

SculptUndoNode *sculpt_undo_push_node(Object *ob, PBVHNode *node,
                                      SculptUndoType type)
{
	SculptSession *ss = ob->sculpt;
	SculptUndoNode *unode;

	/* list is manipulated by multiple threads, so we lock */
	BLI_lock_thread(LOCK_CUSTOM1);

	if (ss->bm ||
	    ELEM(type,
	         SCULPT_UNDO_DYNTOPO_BEGIN,
	         SCULPT_UNDO_DYNTOPO_END))
	{
		/* Dynamic topology stores only one undo node per stroke,
		 * regardless of the number of PBVH nodes modified */
		unode = sculpt_undo_bmesh_push(ob, node, type);
		BLI_unlock_thread(LOCK_CUSTOM1);
		return unode;
	}
	else if ((unode = sculpt_undo_get_node(node))) {
		BLI_unlock_thread(LOCK_CUSTOM1);
		return unode;
	}

	unode = sculpt_undo_alloc_node(ob, node, type);
	
	BLI_unlock_thread(LOCK_CUSTOM1);

	/* copy threaded, hopefully this is the performance critical part */

	if (unode->grids) {
		int totgrid, *grids;
		BKE_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid,
		                        NULL, NULL, NULL, NULL);
		memcpy(unode->grids, grids, sizeof(int) * totgrid);
	}
	else {
		int *vert_indices, allvert;
		BKE_pbvh_node_num_verts(ss->pbvh, node, NULL, &allvert);
		BKE_pbvh_node_get_verts(ss->pbvh, node, &vert_indices, NULL);
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
		case SCULPT_UNDO_DYNTOPO_BEGIN:
		case SCULPT_UNDO_DYNTOPO_END:
		case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
			BLI_assert(!"Dynamic topology should've already been handled");
			break;
	}

	/* store active shape key */
	if (ss->kb) BLI_strncpy(unode->shapeName, ss->kb->name, sizeof(ss->kb->name));
	else unode->shapeName[0] = '\0';

	return unode;
}

void sculpt_undo_push_begin(const char *name)
{
	ED_undo_paint_push_begin(UNDO_PAINT_MESH, name,
	                         sculpt_undo_restore, sculpt_undo_free, sculpt_undo_cleanup);
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

		if (unode->node)
			BKE_pbvh_node_layer_disp_free(unode->node);
	}

	ED_undo_paint_push_end(UNDO_PAINT_MESH);
}
