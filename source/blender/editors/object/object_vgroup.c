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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Ove M Henriksen.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_vgroup.c
 *  \ingroup edobj
 */

#include <string.h>
#include <stddef.h>
#include <math.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"
#include "DNA_particle_types.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_linklist_stack.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_mesh_mapping.h"
#include "BKE_editmesh.h"
#include "BKE_modifier.h"
#include "BKE_report.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object_deform.h"
#include "BKE_object.h"
#include "BKE_lattice.h"

#include "DNA_armature_types.h"
#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_mesh.h"

#include "UI_resources.h"

#include "object_intern.h"

/************************ Exported Functions **********************/
static void vgroup_remap_update_users(Object *ob, int *map);
static void vgroup_delete_edit_mode(Object *ob, bDeformGroup *defgroup);
static void vgroup_delete_object_mode(Object *ob, bDeformGroup *dg);
static void vgroup_delete_all(Object *ob);

static bool vertex_group_use_vert_sel(Object *ob)
{
	if (ob->mode == OB_MODE_EDIT) {
		return true;
	}
	else if (ob->type == OB_MESH && ((Mesh *)ob->data)->editflag & ME_EDIT_PAINT_VERT_SEL) {
		return true;
	}
	else {
		return false;
	}
}

static Lattice *vgroup_edit_lattice(Object *ob)
{
	Lattice *lt = ob->data;
	BLI_assert(ob->type == OB_LATTICE);
	return (lt->editlatt) ? lt->editlatt->latt : lt;
}

bool ED_vgroup_sync_from_pose(Object *ob)
{
	Object *armobj = BKE_object_pose_armature_get(ob);
	if (armobj && (armobj->mode & OB_MODE_POSE)) {
		struct bArmature *arm = armobj->data;
		if (arm->act_bone) {
			int def_num = defgroup_name_index(ob, arm->act_bone->name);
			if (def_num != -1) {
				ob->actdef = def_num + 1;
				return true;
			}
		}
	}
	return false;
}

bool ED_vgroup_object_is_edit_mode(Object *ob)
{
	if (ob->type == OB_MESH)
		return (BKE_editmesh_from_object(ob) != NULL);
	else if (ob->type == OB_LATTICE)
		return (((Lattice *)ob->data)->editlatt != NULL);

	return false;
}

bDeformGroup *ED_vgroup_add_name(Object *ob, const char *name)
{
	bDeformGroup *defgroup;

	if (!ob || !OB_TYPE_SUPPORT_VGROUP(ob->type))
		return NULL;

	defgroup = BKE_defgroup_new(ob, name);

	ob->actdef = BLI_countlist(&ob->defbase);

	return defgroup;
}

bDeformGroup *ED_vgroup_add(Object *ob) 
{
	return ED_vgroup_add_name(ob, DATA_("Group"));
}

void ED_vgroup_delete(Object *ob, bDeformGroup *defgroup) 
{
	BLI_assert(BLI_findindex(&ob->defbase, defgroup) != -1);

	if (ED_vgroup_object_is_edit_mode(ob))
		vgroup_delete_edit_mode(ob, defgroup);
	else
		vgroup_delete_object_mode(ob, defgroup);
}

void ED_vgroup_clear(Object *ob)
{
	bDeformGroup *dg = (bDeformGroup *)ob->defbase.first;
	int edit_mode = ED_vgroup_object_is_edit_mode(ob);

	while (dg) {
		bDeformGroup *next_dg = dg->next;

		if (edit_mode)
			vgroup_delete_edit_mode(ob, dg);
		else
			vgroup_delete_object_mode(ob, dg);

		dg = next_dg;
	}
}

bool ED_vgroup_data_create(ID *id)
{
	/* create deform verts */

	if (GS(id->name) == ID_ME) {
		Mesh *me = (Mesh *)id;
		me->dvert = CustomData_add_layer(&me->vdata, CD_MDEFORMVERT, CD_CALLOC, NULL, me->totvert);
		return true;
	}
	else if (GS(id->name) == ID_LT) {
		Lattice *lt = (Lattice *)id;
		lt->dvert = MEM_callocN(sizeof(MDeformVert) * lt->pntsu * lt->pntsv * lt->pntsw, "lattice deformVert");
		return true;
	}
	else {
		return false;
	}
}

/**
 * Removes out of range MDeformWeights
 */
void ED_vgroup_data_clamp_range(ID *id, const int total)
{
	MDeformVert **dvert_arr;
	int dvert_tot;

	if (ED_vgroup_parray_alloc(id, &dvert_arr, &dvert_tot, false)) {
		int i;
		for (i = 0; i < dvert_tot; i++) {
			MDeformVert *dv = dvert_arr[i];
			int j;
			for (j = 0; j < dv->totweight; j++) {
				if (dv->dw[j].def_nr >= total) {
					defvert_remove_group(dv, &dv->dw[j]);
					j--;
				}
			}
		}
	}
}

bool ED_vgroup_parray_alloc(ID *id, MDeformVert ***dvert_arr, int *dvert_tot, const bool use_vert_sel)
{
	*dvert_tot = 0;
	*dvert_arr = NULL;

	if (id) {
		switch (GS(id->name)) {
			case ID_ME:
			{
				Mesh *me = (Mesh *)id;

				if (me->edit_btmesh) {
					BMEditMesh *em = me->edit_btmesh;
					BMesh *bm = em->bm;
					const int cd_dvert_offset  = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
					BMIter iter;
					BMVert *eve;
					int i;

					if (cd_dvert_offset == -1) {
						return false;
					}

					i = em->bm->totvert;

					*dvert_arr = MEM_mallocN(sizeof(void *) * i, "vgroup parray from me");
					*dvert_tot = i;

					i = 0;
					if (use_vert_sel) {
						BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
							(*dvert_arr)[i] = BM_elem_flag_test(eve, BM_ELEM_SELECT) ?
							                  BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset) : NULL;
							i++;
						}
					}
					else {
						BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
							(*dvert_arr)[i] = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
							i++;
						}
					}

					return true;
				}
				else if (me->dvert) {
					MVert *mvert = me->mvert;
					MDeformVert *dvert = me->dvert;
					int i;

					*dvert_tot = me->totvert;
					*dvert_arr = MEM_mallocN(sizeof(void *) * me->totvert, "vgroup parray from me");

					if (use_vert_sel) {
						for (i = 0; i < me->totvert; i++) {
							(*dvert_arr)[i] = (mvert[i].flag & SELECT) ?
							                  &dvert[i] : NULL;
						}
					}
					else {
						for (i = 0; i < me->totvert; i++) {
							(*dvert_arr)[i] = me->dvert + i;
						}
					}

					return true;
				}
				return false;
			}
			case ID_LT:
			{
				int i = 0;

				Lattice *lt = (Lattice *)id;
				lt = (lt->editlatt) ? lt->editlatt->latt : lt;

				if (lt->dvert) {
					BPoint *def = lt->def;
					*dvert_tot = lt->pntsu * lt->pntsv * lt->pntsw;
					*dvert_arr = MEM_mallocN(sizeof(void *) * (*dvert_tot), "vgroup parray from me");

					if (use_vert_sel) {
						for (i = 0; i < *dvert_tot; i++) {
							(*dvert_arr)[i] = (def->f1 & SELECT) ?
							                   &lt->dvert[i] : NULL;
						}
					}
					else {
						for (i = 0; i < *dvert_tot; i++) {
							(*dvert_arr)[i] = lt->dvert + i;
						}
					}

					return true;
				}
				return false;
			}
		}
	}

	return false;
}

static bool ed_vgroup_dm_parray_alloc(DerivedMesh *dm, MDeformVert ***dvert_arr, int *dvert_tot)
{
	*dvert_tot = 0;
	*dvert_arr = NULL;

	if (dm) {
		MDeformVert *dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);

		if (dvert) {
			int i, totvert = dm->getNumVerts(dm);

			*dvert_tot = totvert;
			*dvert_arr = MEM_mallocN(sizeof(void *) * totvert, "vgroup parray from me");

			for (i = 0; i < totvert; i++) {
				(*dvert_arr)[i] = dvert + i;
			}

			return true;
		}
	}

	return false;
}

/**
 * For use with tools that use ED_vgroup_parray_alloc with \a use_vert_sel == true.
 * This finds the unselected mirror deform verts and copys the weights to them from the selected.
 *
 * \note \a dvert_array has mirrored weights filled in, incase cleanup operations are needed on both.
 */
void ED_vgroup_parray_mirror_sync(Object *ob,
                                  MDeformVert **dvert_array, const int dvert_tot,
                                  const bool *vgroup_validmap, const int vgroup_tot)
{
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	MDeformVert **dvert_array_all = NULL;
	int dvert_tot_all;
	int i;

	/* get an array of all verts, not only selected */
	if (ED_vgroup_parray_alloc(ob->data, &dvert_array_all, &dvert_tot_all, false) == false) {
		BLI_assert(0);
		return;
	}
	if (em) {
		BM_mesh_elem_table_ensure(em->bm, BM_VERT);
	}

	for (i = 0; i < dvert_tot; i++) {
		if (dvert_array[i] == NULL) {
			/* its unselected, check if its mirror is */
			int i_sel = ED_mesh_mirror_get_vert(ob, i);
			if ((i_sel != -1) && (i_sel != i) && (dvert_array[i_sel])) {
				/* we found a match! */
				MDeformVert *dv_src = dvert_array[i_sel];
				MDeformVert *dv_dst = dvert_array_all[i];

				defvert_copy_subset(dv_dst, dv_src, vgroup_validmap, vgroup_tot);

				dvert_array[i] = dvert_array_all[i];
			}
		}
	}

	MEM_freeN(dvert_array_all);
}

/**
 * Fill in the pointers for mirror verts (as if all mirror verts were selected too).
 *
 * similar to #ED_vgroup_parray_mirror_sync but only fill in mirror points.
 */
void ED_vgroup_parray_mirror_assign(Object *ob,
                                    MDeformVert **dvert_array, const int dvert_tot)
{
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	MDeformVert **dvert_array_all = NULL;
	int dvert_tot_all;
	int i;

	/* get an array of all verts, not only selected */
	if (ED_vgroup_parray_alloc(ob->data, &dvert_array_all, &dvert_tot_all, false) == false) {
		BLI_assert(0);
		return;
	}
	BLI_assert(dvert_tot == dvert_tot_all);
	if (em) {
		BM_mesh_elem_table_ensure(em->bm, BM_VERT);
	}

	for (i = 0; i < dvert_tot; i++) {
		if (dvert_array[i] == NULL) {
			/* its unselected, check if its mirror is */
			int i_sel = ED_mesh_mirror_get_vert(ob, i);
			if ((i_sel != -1) && (i_sel != i) && (dvert_array[i_sel])) {
				/* we found a match! */
				dvert_array[i] = dvert_array_all[i];
			}
		}
	}

	MEM_freeN(dvert_array_all);
}

void ED_vgroup_parray_remove_zero(MDeformVert **dvert_array, const int dvert_tot,
                                  const bool *vgroup_validmap, const int vgroup_tot,
                                  const float epsilon, const bool keep_single)
{
	MDeformVert *dv;
	int i;

	for (i = 0; i < dvert_tot; i++) {
		int j;

		/* in case its not selected */
		if (!(dv = dvert_array[i])) {
			continue;
		}

		j = dv->totweight;

		while (j--) {
			MDeformWeight *dw;

			if (keep_single && dv->totweight == 1)
				break;

			dw = dv->dw + j;
			if ((dw->def_nr < vgroup_tot) && vgroup_validmap[dw->def_nr]) {
				if (dw->weight <= epsilon) {
					defvert_remove_group(dv, dw);
				}
			}
		}
	}
}

/* returns true if the id type supports weights */
bool ED_vgroup_array_get(ID *id, MDeformVert **dvert_arr, int *dvert_tot)
{
	if (id) {
		switch (GS(id->name)) {
			case ID_ME:
			{
				Mesh *me = (Mesh *)id;
				*dvert_arr = me->dvert;
				*dvert_tot = me->totvert;
				return true;
			}
			case ID_LT:
			{
				Lattice *lt = (Lattice *)id;
				lt = (lt->editlatt) ? lt->editlatt->latt : lt;
				*dvert_arr = lt->dvert;
				*dvert_tot = lt->pntsu * lt->pntsv * lt->pntsw;
				return true;
			}
		}
	}

	*dvert_arr = NULL;
	*dvert_tot = 0;
	return false;
}

/* matching index only */
bool ED_vgroup_array_copy(Object *ob, Object *ob_from)
{
	MDeformVert **dvert_array_from, **dvf;
	MDeformVert **dvert_array, **dv;
	int dvert_tot_from;
	int dvert_tot;
	int i;
	int defbase_tot_from = BLI_countlist(&ob_from->defbase);
	int defbase_tot = BLI_countlist(&ob->defbase);
	bool new_vgroup = false;

	if (ob == ob_from)
		return true;

	ED_vgroup_parray_alloc(ob_from->data, &dvert_array_from, &dvert_tot_from, false);
	ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, false);

	if ((dvert_array == NULL) && (dvert_array_from != NULL) && ED_vgroup_data_create(ob->data)) {
		ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, false);
		new_vgroup = true;
	}

	if (dvert_tot == 0 || (dvert_tot != dvert_tot_from) || dvert_array_from == NULL || dvert_array == NULL) {

		if (dvert_array) MEM_freeN(dvert_array);
		if (dvert_array_from) MEM_freeN(dvert_array_from);

		if (new_vgroup == true) {
			/* free the newly added vgroup since it wasn't compatible */
			vgroup_delete_all(ob);
		}

		/* if true: both are 0 and nothing needs changing, consider this a success */
		return (dvert_tot == dvert_tot_from);
	}

	/* do the copy */
	BLI_freelistN(&ob->defbase);
	BLI_duplicatelist(&ob->defbase, &ob_from->defbase);
	ob->actdef = ob_from->actdef;

	if (defbase_tot_from < defbase_tot) {
		/* correct vgroup indices because the number of vgroups is being reduced. */
		int *remap = MEM_mallocN(sizeof(int) * (defbase_tot + 1), __func__);
		for (i = 0; i <= defbase_tot_from; i++) remap[i] = i;
		for (; i <= defbase_tot; i++) remap[i] = 0;  /* can't use these, so disable */

		vgroup_remap_update_users(ob, remap);
		MEM_freeN(remap);
	}

	dvf = dvert_array_from;
	dv = dvert_array;

	for (i = 0; i < dvert_tot; i++, dvf++, dv++) {
		if ((*dv)->dw)
			MEM_freeN((*dv)->dw);

		*(*dv) = *(*dvf);

		if ((*dv)->dw)
			(*dv)->dw = MEM_dupallocN((*dv)->dw);
	}

	MEM_freeN(dvert_array);
	MEM_freeN(dvert_array_from);

	return true;
}

/* TODO, cache flip data to speedup calls within a loop. */
static void mesh_defvert_mirror_update_internal(Object *ob,
                                                MDeformVert *dvert_dst, MDeformVert *dvert_src,
                                                const int def_nr)
{
	if (def_nr == -1) {
		/* all vgroups, add groups where neded  */
		int flip_map_len;
		int *flip_map = defgroup_flip_map(ob, &flip_map_len, true);
		defvert_sync_mapped(dvert_dst, dvert_src, flip_map, flip_map_len, true);
		MEM_freeN(flip_map);
	}
	else {
		/* single vgroup */
		MDeformWeight *dw = defvert_verify_index(dvert_dst, defgroup_flip_index(ob, def_nr, 1));
		if (dw) {
			dw->weight = defvert_find_weight(dvert_src, def_nr);
		}
	}
}

static void ED_mesh_defvert_mirror_update_em(Object *ob, BMVert *eve, int def_nr, int vidx,
                                             const int cd_dvert_offset)
{
	Mesh *me = ob->data;
	BMEditMesh *em = me->edit_btmesh;
	BMVert *eve_mirr;
	bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

	eve_mirr = editbmesh_get_x_mirror_vert(ob, em, eve, eve->co, vidx, use_topology);

	if (eve_mirr && eve_mirr != eve) {
		MDeformVert *dvert_src = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
		MDeformVert *dvert_dst = BM_ELEM_CD_GET_VOID_P(eve_mirr, cd_dvert_offset);
		mesh_defvert_mirror_update_internal(ob, dvert_dst, dvert_src, def_nr);
	}
}

static void ED_mesh_defvert_mirror_update_ob(Object *ob, int def_nr, int vidx)
{
	int vidx_mirr;
	Mesh *me = ob->data;
	bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

	if (vidx == -1)
		return;

	vidx_mirr = mesh_get_x_mirror_vert(ob, vidx, use_topology);

	if ((vidx_mirr) >= 0 && (vidx_mirr != vidx)) {
		MDeformVert *dvert_src = &me->dvert[vidx];
		MDeformVert *dvert_dst = &me->dvert[vidx_mirr];
		mesh_defvert_mirror_update_internal(ob, dvert_dst, dvert_src, def_nr);
	}
}

/**
 * Use when adjusting the active vertex weight and apply to mirror vertices.
 */
void ED_vgroup_vert_active_mirror(Object *ob, int def_nr)
{
	Mesh *me = ob->data;
	BMEditMesh *em = me->edit_btmesh;
	MDeformVert *dvert_act;

	if (me->editflag & ME_EDIT_MIRROR_X) {
		if (em) {
			BMVert *eve_act;
			dvert_act = ED_mesh_active_dvert_get_em(ob, &eve_act);
			if (dvert_act) {
				const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
				ED_mesh_defvert_mirror_update_em(ob, eve_act, def_nr, -1, cd_dvert_offset);
			}
		}
		else {
			int v_act;
			dvert_act = ED_mesh_active_dvert_get_ob(ob, &v_act);
			if (dvert_act) {
				ED_mesh_defvert_mirror_update_ob(ob, def_nr, v_act);
			}
		}
	}
}

static void vgroup_remove_weight(Object *ob, const int def_nr)
{
	MDeformVert *dvert_act;
	MDeformWeight *dw;

	dvert_act = ED_mesh_active_dvert_get_only(ob);

	dw = defvert_find_index(dvert_act, def_nr);
	defvert_remove_group(dvert_act, dw);

}

static void vgroup_normalize_active(Object *ob, eVGroupSelect subset_type)
{
	Mesh *me = ob->data;
	BMEditMesh *em = me->edit_btmesh;
	BMVert *eve_act;
	int v_act;
	MDeformVert *dvert_act;
	int subset_count, vgroup_tot;
	const bool *vgroup_validmap;


	if (em) {
		dvert_act = ED_mesh_active_dvert_get_em(ob, &eve_act);
	}
	else {
		dvert_act = ED_mesh_active_dvert_get_ob(ob, &v_act);
	}

	if (dvert_act == NULL) {
		return;
	}

	vgroup_validmap = ED_vgroup_subset_from_select_type(ob, subset_type, &vgroup_tot, &subset_count);
	defvert_normalize_subset(dvert_act, vgroup_validmap, vgroup_tot);
	MEM_freeN((void *)vgroup_validmap);

	if (me->editflag & ME_EDIT_MIRROR_X) {
		if (em) {
			const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
			ED_mesh_defvert_mirror_update_em(ob, eve_act, -1, -1, cd_dvert_offset);
		}
		else {
			ED_mesh_defvert_mirror_update_ob(ob, -1, v_act);
		}
	}
}

static void vgroup_copy_active_to_sel(Object *ob, eVGroupSelect subset_type)
{
	Mesh *me = ob->data;
	BMEditMesh *em = me->edit_btmesh;
	MDeformVert *dvert_act;
	int i, vgroup_tot, subset_count;
	const bool *vgroup_validmap = ED_vgroup_subset_from_select_type(ob, subset_type, &vgroup_tot, &subset_count);


	if (em) {
		BMIter iter;
		BMVert *eve, *eve_act;
		const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

		dvert_act = ED_mesh_active_dvert_get_em(ob, &eve_act);
		if (dvert_act) {
			BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
				if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && eve != eve_act) {
					MDeformVert *dv = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
					defvert_copy_subset(dv, dvert_act, vgroup_validmap, vgroup_tot);
					if (me->editflag & ME_EDIT_MIRROR_X) {
						ED_mesh_defvert_mirror_update_em(ob, eve, -1, i, cd_dvert_offset);
					}
				}
			}
		}
	}
	else {
		MDeformVert *dv;
		int v_act;

		dvert_act = ED_mesh_active_dvert_get_ob(ob, &v_act);
		if (dvert_act) {
			dv  = me->dvert;
			for (i = 0; i < me->totvert; i++, dv++) {
				if ((me->mvert[i].flag & SELECT) && dv != dvert_act) {
					defvert_copy_subset(dv, dvert_act, vgroup_validmap, vgroup_tot);
					if (me->editflag & ME_EDIT_MIRROR_X) {
						ED_mesh_defvert_mirror_update_ob(ob, -1, i);
					}
				}
			}
		}
	}

	MEM_freeN((void *)vgroup_validmap);
}

/***********************Start weight transfer (WT)*********************************/

typedef enum WT_VertexGroupMode {
	WT_REPLACE_ACTIVE_VERTEX_GROUP = 1,
	WT_REPLACE_ALL_VERTEX_GROUPS = 2
} WT_VertexGroupMode;

typedef enum WT_Method {
	WT_BY_INDEX = 1,
	WT_BY_NEAREST_VERTEX = 2,
	WT_BY_NEAREST_FACE = 3,
	WT_BY_NEAREST_VERTEX_IN_FACE = 4
} WT_Method;

typedef enum WT_ReplaceMode {
	WT_REPLACE_ALL_WEIGHTS = 1,
	WT_REPLACE_EMPTY_WEIGHTS = 2
} WT_ReplaceMode;

static EnumPropertyItem WT_vertex_group_mode_item[] = {
	{WT_REPLACE_ACTIVE_VERTEX_GROUP,
	 "WT_REPLACE_ACTIVE_VERTEX_GROUP", 0, "Active", "Transfer active vertex group from selected to active mesh"},
	{WT_REPLACE_ALL_VERTEX_GROUPS,
	 "WT_REPLACE_ALL_VERTEX_GROUPS", 0, "All", "Transfer all vertex groups from selected to active mesh"},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem WT_method_item[] = {
	{WT_BY_INDEX,
	 "WT_BY_INDEX", 0, "Vertex index", "Copy for identical meshes"},
	{WT_BY_NEAREST_VERTEX,
	 "WT_BY_NEAREST_VERTEX", 0, "Nearest vertex", "Copy weight from closest vertex"},
	{WT_BY_NEAREST_FACE,
	 "WT_BY_NEAREST_FACE", 0, "Nearest face", "Barycentric interpolation from nearest face"},
	{WT_BY_NEAREST_VERTEX_IN_FACE,
	 "WT_BY_NEAREST_VERTEX_IN_FACE", 0, "Nearest vertex in face", "Copy weight from closest vertex in nearest face"},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem WT_replace_mode_item[] = {
	{WT_REPLACE_ALL_WEIGHTS,
	 "WT_REPLACE_ALL_WEIGHTS", 0, "All", "Overwrite all weights"},
	{WT_REPLACE_EMPTY_WEIGHTS,
	 "WT_REPLACE_EMPTY_WEIGHTS", 0, "Empty", "Add weights to vertices with no weight"},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem WT_vertex_group_select_item[] = {
	{WT_VGROUP_ACTIVE,
	 "ACTIVE", 0, "Active Group", "The active Vertex Group"},
	{WT_VGROUP_BONE_SELECT,
	 "BONE_SELECT", 0, "Selected Pose Bones", "All Vertex Groups assigned to Selection"},
	{WT_VGROUP_BONE_DEFORM,
	 "BONE_DEFORM", 0, "Deform Pose Bones", "All Vertex Groups assigned to Deform Bones"},
	{WT_VGROUP_ALL,
	 "ALL", 0, "All Groups", "All Vertex Groups"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem *ED_object_vgroup_selection_itemf_helper(
        const bContext *C, PointerRNA *UNUSED(ptr),
        PropertyRNA *UNUSED(prop), bool *r_free, const unsigned int selection_mask)
{
	Object *ob;
	EnumPropertyItem *item = NULL;
	int totitem = 0;


	if (!C) /* needed for docs and i18n tools */
		return WT_vertex_group_select_item;

	ob = CTX_data_active_object(C);
	if (selection_mask & (1 << WT_VGROUP_ACTIVE))
		RNA_enum_items_add_value(&item, &totitem, WT_vertex_group_select_item, WT_VGROUP_ACTIVE);

	if (BKE_object_pose_armature_get(ob)) {
		if (selection_mask & (1 << WT_VGROUP_BONE_SELECT))
			RNA_enum_items_add_value(&item, &totitem, WT_vertex_group_select_item, WT_VGROUP_BONE_SELECT);
		if (selection_mask & (1 << WT_VGROUP_BONE_DEFORM))
			RNA_enum_items_add_value(&item, &totitem, WT_vertex_group_select_item, WT_VGROUP_BONE_DEFORM);
	}

	if (selection_mask & (1 << WT_VGROUP_ALL))
		RNA_enum_items_add_value(&item, &totitem, WT_vertex_group_select_item, WT_VGROUP_ALL);

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static EnumPropertyItem *rna_vertex_group_with_single_itemf(bContext *C, PointerRNA *ptr,
                                                            PropertyRNA *prop, bool *r_free)
{
	return ED_object_vgroup_selection_itemf_helper(C, ptr, prop, r_free, WT_VGROUP_MASK_ALL);
}

static EnumPropertyItem *rna_vertex_group_select_itemf(bContext *C, PointerRNA *ptr,
                                                       PropertyRNA *prop, bool *r_free)
{
	return ED_object_vgroup_selection_itemf_helper(C, ptr, prop, r_free, WT_VGROUP_MASK_ALL & ~(1 << WT_VGROUP_ACTIVE));
}

static void vgroup_operator_subset_select_props(wmOperatorType *ot, bool use_active)
{
	PropertyRNA *prop;

	prop = RNA_def_enum(ot->srna,
	                    "group_select_mode", DummyRNA_NULL_items,
	                    use_active ? WT_VGROUP_ACTIVE : WT_VGROUP_ALL, "Subset",
	                    "Define which subset of Groups shall be used");

	if (use_active) {
		RNA_def_enum_funcs(prop, rna_vertex_group_with_single_itemf);
	}
	else {
		RNA_def_enum_funcs(prop, rna_vertex_group_select_itemf);
	}
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

/* Copy weight.*/
static void vgroup_transfer_weight(float *r_weight_dst, const float weight_src, const WT_ReplaceMode replace_mode)
{
	switch (replace_mode) {
		case WT_REPLACE_ALL_WEIGHTS:
			*r_weight_dst = weight_src;
			break;

		case WT_REPLACE_EMPTY_WEIGHTS:
			if (*r_weight_dst == 0.0f) {
				*r_weight_dst = weight_src;
			}
			break;

		default:
			BLI_assert(0);
			break;
	}
}

/* Could be exposed externally by implementing it in header with the rest.
 * Simple refactoring will break something.
 * For now, naming is ed_ instead of ED_*/
static bool ed_vgroup_transfer_weight(Object *ob_dst, Object *ob_src, bDeformGroup *dg_src, Scene *scene,
                                      WT_Method method, WT_ReplaceMode replace_mode, wmOperator *op)
{
	bDeformGroup *dg_dst;
	Mesh *me_dst;
	DerivedMesh *dmesh_src;
	BVHTreeFromMesh tree_mesh_vertices_src, tree_mesh_faces_src = {NULL};
	MDeformVert **dv_array_src, **dv_array_dst, **dv_src, **dv_dst;
	MVert *mv_dst, *mv_src;
	MFace *mface_src, *mf;
	BVHTreeNearest nearest;
	MDeformWeight *dw_dst, *dw_src;
	int dv_tot_src, dv_tot_dst, i, v_index, index_dst, index_src, index_nearest, index_nearest_vertex;
	unsigned int f_index;
	float weight, tmp_weight[4], tmp_co[3], normal[3], tmp_mat[4][4], dist_v1, dist_v2, dist_v3, dist_v4;
	const int use_vert_sel = vertex_group_use_vert_sel(ob_dst);
	bool is_dg_dst_new = false;

	/* Ensure vertex group on target.*/
	if ((dg_dst = defgroup_find_name(ob_dst, dg_src->name)) == NULL) {
		dg_dst = BKE_defgroup_new(ob_dst, dg_src->name);
		is_dg_dst_new = true;
	}

	/* Get meshes.*/
	dmesh_src = mesh_get_derived_final(scene, ob_src, CD_MASK_BAREMESH | CD_MASK_MDEFORMVERT);
	me_dst = ob_dst->data;

	/* Get vertex group array from source mesh */
	if (!ed_vgroup_dm_parray_alloc(dmesh_src, &dv_array_src, &dv_tot_src)) {
		BKE_report(op->reports, RPT_ERROR, "Transfer failed (source mesh does not have any vertex groups)");
		return false;
	}

	/* Create data in memory when nothing there.*/
	if (!me_dst->dvert) ED_vgroup_data_create(&me_dst->id);

	/* Get vertex group for destination mesh */
	ED_vgroup_parray_alloc(&me_dst->id, &dv_array_dst, &dv_tot_dst, use_vert_sel);

	/* Get indexes of vertex groups.*/
	index_src = BLI_findindex(&ob_src->defbase, dg_src);
	index_dst = BLI_findindex(&ob_dst->defbase, dg_dst);

	/* Get vertices.*/
	mv_dst = me_dst->mvert;
	mv_src = dmesh_src->getVertArray(dmesh_src);

	/* Prepare transformation matrix.*/
	invert_m4_m4(ob_src->imat, ob_src->obmat);
	mul_m4_m4m4(tmp_mat, ob_src->imat, ob_dst->obmat);

	/* Clear weights.*/
	if (replace_mode == WT_REPLACE_ALL_WEIGHTS) {
		for (i = 0, dv_dst = dv_array_dst; i < me_dst->totvert; i++, dv_dst++) {

			if (*dv_dst == NULL) continue;

			dw_dst = defvert_find_index(*dv_dst, index_dst);
			/* Remove vertex from group.*/
			if (dw_dst) defvert_remove_group(*dv_dst, dw_dst);
		}
	}

	switch (method) {

		case WT_BY_INDEX:
			/* Check if indices are matching, delete and return if not.*/
			if (ob_dst == ob_src || dv_tot_dst == 0 || dv_tot_dst != dv_tot_src ||
			    dv_array_src == NULL || dv_array_dst == NULL)
			{
				if (is_dg_dst_new) {
					ED_vgroup_delete(ob_dst, dg_dst);
				}

				if (dv_array_src) MEM_freeN(dv_array_src);
				if (dv_array_dst) MEM_freeN(dv_array_dst);
				dmesh_src->release(dmesh_src);
				BKE_report(op->reports, RPT_ERROR, "Transfer failed (indices are not matching)");
				return false;
			}

			/* Loop through the vertices.*/
			for (i = 0, dv_src = dv_array_src, dv_dst = dv_array_dst;
			     i < me_dst->totvert;
			     i++, dv_dst++, dv_src++, mv_src++, mv_dst++)
			{

				if (*dv_dst == NULL) {
					continue;
				}

				/* Copy weight.*/
				dw_src = defvert_find_index(*dv_src, index_src);
				if (dw_src && dw_src->weight) {
					dw_dst = defvert_verify_index(*dv_dst, index_dst);
					vgroup_transfer_weight(&dw_dst->weight, dw_src->weight, replace_mode);
				}
			}
			break;

		case WT_BY_NEAREST_VERTEX:
			/* Make node tree.*/
			bvhtree_from_mesh_verts(&tree_mesh_vertices_src, dmesh_src, FLT_EPSILON, 2, 6);

			/* Loop trough vertices.*/
			for (i = 0, dv_dst = dv_array_dst; i < me_dst->totvert; i++, dv_dst++, mv_dst++) {

				if (*dv_dst == NULL) {
					continue;
				}

				/* Reset nearest.*/
				nearest.dist_sq = FLT_MAX;
				/* It is faster to start searching at the top of the tree instead of previous search result.*/
				nearest.index = -1;

				/* Transform into target space.*/
				mul_v3_m4v3(tmp_co, tmp_mat, mv_dst->co);

				/* Node tree accelerated search for closest vetex.*/
				BLI_bvhtree_find_nearest(tree_mesh_vertices_src.tree, tmp_co,
				                         &nearest, tree_mesh_vertices_src.nearest_callback, &tree_mesh_vertices_src);

				/* Copy weight that are not NULL including weight value 0. In relevant cases, existing weights are
				 * overwritten prior to this. See the "Clear weights." step above.*/
				dw_src = defvert_find_index(dv_array_src[nearest.index], index_src);
				if (dw_src && dw_src->weight) {
					dw_dst = defvert_verify_index(*dv_dst, index_dst);
					vgroup_transfer_weight(&dw_dst->weight, dw_src->weight, replace_mode);
				}
			}

			/* Free memory.*/
			free_bvhtree_from_mesh(&tree_mesh_vertices_src);
			break;

		case WT_BY_NEAREST_FACE:
			/* Get faces.*/
			DM_ensure_tessface(dmesh_src);
			mface_src = dmesh_src->getTessFaceArray(dmesh_src);

			/* Make node tree.*/
			bvhtree_from_mesh_faces(&tree_mesh_faces_src, dmesh_src, FLT_EPSILON, 2, 6);

			/* Loop through the vertices.*/
			for (i = 0, dv_dst = dv_array_dst; i < me_dst->totvert; i++, dv_dst++, mv_dst++) {

				if (*dv_dst == NULL) {
					continue;
				}

				/* Reset nearest.*/
				nearest.dist_sq = FLT_MAX;
				/* It is faster to start searching at the top of the tree instead of previous search result.*/
				nearest.index = -1;

				/* Transform into target space.*/
				mul_v3_m4v3(tmp_co, tmp_mat, mv_dst->co);

				/* Node tree accelerated search for closest face.*/
				BLI_bvhtree_find_nearest(tree_mesh_faces_src.tree, tmp_co,
				                         &nearest, tree_mesh_faces_src.nearest_callback, &tree_mesh_faces_src);
				index_nearest = nearest.index;

				/* Project onto face.*/
				mf = &mface_src[index_nearest];
				normal_tri_v3(normal, mv_src[mf->v1].co, mv_src[mf->v2].co, mv_src[mf->v3].co);
				project_v3_plane(tmp_co, normal, mv_src[mf->v1].co);

				/* Interpolate weights over face.*/
				interp_weights_face_v3(tmp_weight,
				                       mv_src[mf->v1].co,
				                       mv_src[mf->v2].co,
				                       mv_src[mf->v3].co,
				                       mf->v4 ? mv_src[mf->v4].co : NULL,
				                       tmp_co);

				/* Get weights from face.*/
				f_index = mf->v4 ? 3 : 2;
				weight = 0.0f;
				do {
					v_index = (&mf->v1)[f_index];
					weight += tmp_weight[f_index] * defvert_find_weight(dv_array_src[v_index], index_src);
				} while (f_index--);

				/* Copy weight that are not NULL including weight value 0. In relevant cases, existing weights are
				 * overwritten prior to this. See the "Clear weights." step above.*/
				if (weight > 0.0f) {
					dw_dst = defvert_verify_index(*dv_dst, index_dst);
					vgroup_transfer_weight(&dw_dst->weight, weight, replace_mode);
				}
			}

			/* Free memory.*/
			free_bvhtree_from_mesh(&tree_mesh_faces_src);
			break;

		case WT_BY_NEAREST_VERTEX_IN_FACE:
			/* Get faces.*/
			DM_ensure_tessface(dmesh_src);
			mface_src = dmesh_src->getTessFaceArray(dmesh_src);

			/* Make node tree.*/
			bvhtree_from_mesh_faces(&tree_mesh_faces_src, dmesh_src, FLT_EPSILON, 2, 6);

			/* Loop through the vertices.*/
			for (i = 0, dv_dst = dv_array_dst; i < me_dst->totvert; i++, dv_dst++, mv_dst++) {

				if (*dv_dst == NULL) {
					continue;
				}

				/* Reset nearest.*/
				nearest.dist_sq = FLT_MAX;
				/* It is faster to start searching at the top of the tree instead of previous search result.*/
				nearest.index = -1;

				/* Transform into target space.*/
				mul_v3_m4v3(tmp_co, tmp_mat, mv_dst->co);

				/* Node tree accelerated search for closest face.*/
				BLI_bvhtree_find_nearest(tree_mesh_faces_src.tree, tmp_co,
				                         &nearest, tree_mesh_faces_src.nearest_callback, &tree_mesh_faces_src);
				index_nearest = nearest.index;

				/* Get distances.*/
				mf = &mface_src[index_nearest];
				dist_v1 = len_squared_v3v3(tmp_co, mv_src[mf->v1].co);
				dist_v2 = len_squared_v3v3(tmp_co, mv_src[mf->v2].co);
				dist_v3 = len_squared_v3v3(tmp_co, mv_src[mf->v3].co);

				/* Get closest vertex.*/
				f_index = mf->v4 ? 3 : 2;
				if (dist_v1 < dist_v2 && dist_v1 < dist_v3) index_nearest_vertex = mf->v1;
				else if (dist_v2 < dist_v3) index_nearest_vertex = mf->v2;
				else index_nearest_vertex = mf->v3;
				if (f_index == 3) {
					dist_v4 = len_squared_v3v3(tmp_co, mv_src[mf->v4].co);
					if (dist_v4 < dist_v1 && dist_v4 < dist_v2 && dist_v4 < dist_v3) {
						index_nearest_vertex = mf->v4;
					}
				}

				/* Copy weight that are not NULL including weight value 0. In relevant cases, existing weights are
				 * overwritten prior to this. See the "Clear weights." step above.*/
				dw_src = defvert_find_index(dv_array_src[index_nearest_vertex], index_src);
				if (dw_src && dw_src->weight) {
					dw_dst = defvert_verify_index(*dv_dst, index_dst);
					vgroup_transfer_weight(&dw_dst->weight, dw_src->weight, replace_mode);
				}
			}

			/* Free memory.*/
			free_bvhtree_from_mesh(&tree_mesh_faces_src);
			break;

		default:
			BLI_assert(0);
			break;
	}

	/* Free memory.*/
	if (dv_array_src) MEM_freeN(dv_array_src);
	if (dv_array_dst) MEM_freeN(dv_array_dst);
	dmesh_src->release(dmesh_src);

	return true;
}

/***********************End weight transfer (WT)***********************************/

/* for Mesh in Object mode */
/* allows editmode for Lattice */
static void ED_vgroup_nr_vert_add(Object *ob,
                                  const int def_nr, const int vertnum,
                                  const float weight, const int assignmode)
{
	/* add the vert to the deform group with the
	 * specified number
	 */
	MDeformVert *dvert = NULL;
	int tot;

	/* get the vert */
	ED_vgroup_array_get(ob->data, &dvert, &tot);
	
	if (dvert == NULL)
		return;

	/* check that vertnum is valid before trying to get the relevant dvert */
	if ((vertnum < 0) || (vertnum >= tot))
		return;


	if (dvert) {
		MDeformVert *dv = &dvert[vertnum];
		MDeformWeight *dw;

		/* Lets first check to see if this vert is
		 * already in the weight group -- if so
		 * lets update it
		 */

		dw = defvert_find_index(dv, def_nr);

		if (dw) {
			switch (assignmode) {
				case WEIGHT_REPLACE:
					dw->weight = weight;
					break;
				case WEIGHT_ADD:
					dw->weight += weight;
					if (dw->weight >= 1.0f)
						dw->weight = 1.0f;
					break;
				case WEIGHT_SUBTRACT:
					dw->weight -= weight;
					/* if the weight is zero or less then
					 * remove the vert from the deform group
					 */
					if (dw->weight <= 0.0f) {
						defvert_remove_group(dv, dw);
					}
					break;
			}
		}
		else {
			/* if the vert wasn't in the deform group then
			 * we must take a different form of action ...
			 */

			switch (assignmode) {
				case WEIGHT_SUBTRACT:
					/* if we are subtracting then we don't
					 * need to do anything
					 */
					return;

				case WEIGHT_REPLACE:
				case WEIGHT_ADD:
					/* if we are doing an additive assignment, then
					 * we need to create the deform weight
					 */

					/* we checked if the vertex was added before so no need to test again, simply add */
					defvert_add_index_notest(dv, def_nr, weight);
					break;
			}
		}
	}
}

/* called while not in editmode */
void ED_vgroup_vert_add(Object *ob, bDeformGroup *dg, int vertnum, float weight, int assignmode)
{
	/* add the vert to the deform group with the
	 * specified assign mode
	 */
	const int def_nr = BLI_findindex(&ob->defbase, dg);

	MDeformVert *dv = NULL;
	int tot;

	/* get the deform group number, exit if
	 * it can't be found
	 */
	if (def_nr != -1) {

		/* if there's no deform verts then create some,
		 */
		if (ED_vgroup_array_get(ob->data, &dv, &tot) && dv == NULL)
			ED_vgroup_data_create(ob->data);

		/* call another function to do the work
		 */
		ED_vgroup_nr_vert_add(ob, def_nr, vertnum, weight, assignmode);
	}
}

/* mesh object mode, lattice can be in editmode */
void ED_vgroup_vert_remove(Object *ob, bDeformGroup *dg, int vertnum)
{
	/* This routine removes the vertex from the specified
	 * deform group.
	 */

	/* TODO, this is slow in a loop, better pass def_nr directly, but leave for later... - campbell */
	const int def_nr = BLI_findindex(&ob->defbase, dg);

	if (def_nr != -1) {
		MDeformVert *dvert = NULL;
		int tot;

		/* get the deform vertices corresponding to the
		 * vertnum
		 */
		ED_vgroup_array_get(ob->data, &dvert, &tot);

		if (dvert) {
			MDeformVert *dv = &dvert[vertnum];
			MDeformWeight *dw;

			dw = defvert_find_index(dv, def_nr);
			defvert_remove_group(dv, dw); /* dw can be NULL */
		}
	}
}

static float get_vert_def_nr(Object *ob, const int def_nr, const int vertnum)
{
	MDeformVert *dv = NULL;

	/* get the deform vertices corresponding to the vertnum */
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (me->edit_btmesh) {
			BMEditMesh *em = me->edit_btmesh;
			const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
			/* warning, this lookup is _not_ fast */

			if (cd_dvert_offset != -1) {
				BMVert *eve;
				BM_mesh_elem_table_ensure(em->bm, BM_VERT);
				eve = BM_vert_at_index(em->bm, vertnum);
				dv = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
			}
			else {
				return 0.0f;
			}
		}
		else {
			if (me->dvert) {
				if (vertnum >= me->totvert) {
					return 0.0f;
				}
				dv = &me->dvert[vertnum];
			}
		}
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = vgroup_edit_lattice(ob);

		if (lt->dvert) {
			if (vertnum >= lt->pntsu * lt->pntsv * lt->pntsw) {
				return 0.0f;
			}
			dv = &lt->dvert[vertnum];
		}
	}
	
	if (dv) {
		MDeformWeight *dw = defvert_find_index(dv, def_nr);
		if (dw) {
			return dw->weight;
		}
	}

	return -1;
}

float ED_vgroup_vert_weight(Object *ob, bDeformGroup *dg, int vertnum)
{
	const int def_nr = BLI_findindex(&ob->defbase, dg);

	if (def_nr == -1) {
		return -1;
	}

	return get_vert_def_nr(ob, def_nr, vertnum);
}

void ED_vgroup_select_by_name(Object *ob, const char *name)
{   /* note: ob->actdef==0 signals on painting to create a new one, if a bone in posemode is selected */
	ob->actdef = defgroup_name_index(ob, name) + 1;
}

/********************** Operator Implementations *********************/

/* only in editmode */
static void vgroup_select_verts(Object *ob, int select)
{
	const int def_nr = ob->actdef - 1;

	if (!BLI_findlink(&ob->defbase, def_nr)) {
		return;
	}

	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (me->edit_btmesh) {
			BMEditMesh *em = me->edit_btmesh;
			const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

			if (cd_dvert_offset != -1) {
				BMIter iter;
				BMVert *eve;

				BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
					if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
						MDeformVert *dv = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
						if (defvert_find_index(dv, def_nr)) {
							BM_vert_select_set(em->bm, eve, select);
						}
					}
				}

				/* this has to be called, because this function operates on vertices only */
				if (select) EDBM_select_flush(em);  /* vertices to edges/faces */
				else EDBM_deselect_flush(em);
			}
		}
		else {
			if (me->dvert) {
				MVert *mv;
				MDeformVert *dv;
				int i;

				mv = me->mvert;
				dv = me->dvert;

				for (i = 0; i < me->totvert; i++, mv++, dv++) {
					if (!(mv->flag & ME_HIDE)) {
						if (defvert_find_index(dv, def_nr)) {
							if (select) mv->flag |=  SELECT;
							else mv->flag &= ~SELECT;
						}
					}
				}

				paintvert_flush_flags(ob);
			}
		}
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = vgroup_edit_lattice(ob);
		
		if (lt->dvert) {
			MDeformVert *dv;
			BPoint *bp, *actbp = BKE_lattice_active_point_get(lt);
			int a, tot;
			
			dv = lt->dvert;

			tot = lt->pntsu * lt->pntsv * lt->pntsw;
			for (a = 0, bp = lt->def; a < tot; a++, bp++, dv++) {
				if (defvert_find_index(dv, def_nr)) {
					if (select) bp->f1 |=  SELECT;
					else {
						bp->f1 &= ~SELECT;
						if (actbp && bp == actbp) lt->actbp = LT_ACTBP_NONE;
					}
				}
			}
		}
	}
}

static void vgroup_duplicate(Object *ob)
{
	bDeformGroup *dg, *cdg;
	char name[sizeof(dg->name)];
	MDeformWeight *dw_org, *dw_cpy;
	MDeformVert **dvert_array = NULL;
	int i, idg, icdg, dvert_tot = 0;

	dg = BLI_findlink(&ob->defbase, (ob->actdef - 1));
	if (!dg)
		return;
	
	if (!strstr(dg->name, "_copy")) {
		BLI_snprintf(name, sizeof(name), "%s_copy", dg->name);
	}
	else {
		BLI_strncpy(name, dg->name, sizeof(name));
	}

	cdg = defgroup_duplicate(dg);
	BLI_strncpy(cdg->name, name, sizeof(cdg->name));
	defgroup_unique_name(cdg, ob);

	BLI_addtail(&ob->defbase, cdg);

	idg = (ob->actdef - 1);
	ob->actdef = BLI_countlist(&ob->defbase);
	icdg = (ob->actdef - 1);

	/* TODO, we might want to allow only copy selected verts here? - campbell */
	ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, false);

	if (dvert_array) {
		for (i = 0; i < dvert_tot; i++) {
			MDeformVert *dv = dvert_array[i];
			dw_org = defvert_find_index(dv, idg);
			if (dw_org) {
				/* defvert_verify_index re-allocs org so need to store the weight first */
				const float weight = dw_org->weight;
				dw_cpy = defvert_verify_index(dv, icdg);
				dw_cpy->weight = weight;
			}
		}

		MEM_freeN(dvert_array);
	}
}

/**
 * Return the subset type of the Vertex Group Selection
 */
bool *ED_vgroup_subset_from_select_type(Object *ob, eVGroupSelect subset_type, int *r_vgroup_tot, int *r_subset_count)
{
	bool *vgroup_validmap = NULL;
	*r_vgroup_tot = BLI_countlist(&ob->defbase);

	switch (subset_type) {
		case WT_VGROUP_ACTIVE:
		{
			const int def_nr_active = ob->actdef - 1;
			vgroup_validmap = MEM_mallocN(*r_vgroup_tot * sizeof(*vgroup_validmap), __func__);
			memset(vgroup_validmap, false, *r_vgroup_tot * sizeof(*vgroup_validmap));
			if ((def_nr_active >= 0) && (def_nr_active < *r_vgroup_tot)) {
				*r_subset_count = 1;
				vgroup_validmap[def_nr_active] = true;
			}
			else {
				*r_subset_count = 0;
			}
			break;
		}
		case WT_VGROUP_BONE_SELECT:
		{
			vgroup_validmap = BKE_objdef_selected_get(ob, *r_vgroup_tot, r_subset_count);
			break;
		}
		case WT_VGROUP_BONE_DEFORM:
		{
			int i;
			vgroup_validmap = BKE_objdef_validmap_get(ob, *r_vgroup_tot);
			*r_subset_count = 0;
			for (i = 0; i < *r_vgroup_tot; i++) {
				if (vgroup_validmap[i] == true) {
					*r_subset_count += 1;
				}
			}
			break;
		}
		case WT_VGROUP_BONE_DEFORM_OFF:
		{
			int i;
			vgroup_validmap = BKE_objdef_validmap_get(ob, *r_vgroup_tot);
			*r_subset_count = 0;
			for (i = 0; i < *r_vgroup_tot; i++) {
				vgroup_validmap[i] = !vgroup_validmap[i];
				if (vgroup_validmap[i] == true) {
					*r_subset_count += 1;
				}
			}
			break;
		}
		case WT_VGROUP_ALL:
		default:
		{
			vgroup_validmap = MEM_mallocN(*r_vgroup_tot * sizeof(*vgroup_validmap), __func__);
			memset(vgroup_validmap, true, *r_vgroup_tot * sizeof(*vgroup_validmap));
			*r_subset_count = *r_vgroup_tot;
			break;
		}
	}

	return vgroup_validmap;
}

/**
 * store indices from the vgroup_validmap (faster lookups in some cases)
 */
void ED_vgroup_subset_to_index_array(const bool *vgroup_validmap, const int vgroup_tot,
                                     int *r_vgroup_subset_map)
{
	int i, j = 0;
	for (i = 0; i < vgroup_tot; i++) {
		if (vgroup_validmap[i]) {
			r_vgroup_subset_map[j++] = i;
		}
	}
}

static void vgroup_normalize(Object *ob)
{
	MDeformWeight *dw;
	MDeformVert *dv, **dvert_array = NULL;
	int i, dvert_tot = 0;
	const int def_nr = ob->actdef - 1;

	const int use_vert_sel = vertex_group_use_vert_sel(ob);

	if (!BLI_findlink(&ob->defbase, def_nr)) {
		return;
	}

	ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, use_vert_sel);

	if (dvert_array) {
		float weight_max = 0.0f;

		for (i = 0; i < dvert_tot; i++) {

			/* in case its not selected */
			if (!(dv = dvert_array[i])) {
				continue;
			}

			dw = defvert_find_index(dv, def_nr);
			if (dw) {
				weight_max = max_ff(dw->weight, weight_max);
			}
		}

		if (weight_max > 0.0f) {
			for (i = 0; i < dvert_tot; i++) {
				
				/* in case its not selected */
				if (!(dv = dvert_array[i])) {
					continue;
				}

				dw = defvert_find_index(dv, def_nr);
				if (dw) {
					dw->weight /= weight_max;
					
					/* in case of division errors with very low weights */
					CLAMP(dw->weight, 0.0f, 1.0f);
				}
			}
		}

		MEM_freeN(dvert_array);
	}
}

/* This finds all of the vertices face-connected to vert by an edge and returns a
 * MEM_allocated array of indices of size count.
 * count is an int passed by reference so it can be assigned the value of the length here. */
static int *getSurroundingVerts(Mesh *me, int vert, int *count)
{
	MPoly *mp = me->mpoly;
	int i = me->totpoly;
	/* Instead of looping twice on all polys and loops, and use a temp array, let's rather
	 * use a BLI_array, with a reasonable starting/reserved size (typically, there are not
	 * many vertices face-linked to another one, even 8 might be too high...). */
	int *verts = NULL;
	BLI_array_declare(verts);

	BLI_array_reserve(verts, 8);
	while (i--) {
		int j = mp->totloop;
		int first_l = mp->totloop - 1;
		MLoop *ml = &me->mloop[mp->loopstart];
		while (j--) {
			/* XXX This assume a vert can only be once in a poly, even though
			 *     it seems logical to me, not totally sure of that. */
			if (ml->v == vert) {
				int a, b, k;
				if (j == first_l) {
					/* We are on the first corner. */
					a = ml[1].v;
					b = ml[j].v;
				}
				else if (!j) {
					/* We are on the last corner. */
					a = (ml - 1)->v;
					b = me->mloop[mp->loopstart].v;
				}
				else {
					a = (ml - 1)->v;
					b = (ml + 1)->v;
				}

				/* Append a and b verts to array, if not yet present. */
				k = BLI_array_count(verts);
				/* XXX Maybe a == b is enough? */
				while (k-- && !(a == b && a == -1)) {
					if (verts[k] == a)
						a = -1;
					else if (verts[k] == b)
						b = -1;
				}
				if (a != -1)
					BLI_array_append(verts, a);
				if (b != -1)
					BLI_array_append(verts, b);

				/* Vert found in this poly, we can go to next one! */
				break;
			}
			ml++;
		}
		mp++;
	}

	/* Do not free the array! */
	*count = BLI_array_count(verts);
	return verts;
}

/* get a single point in space by averaging a point cloud (vectors of size 3)
 * coord is the place the average is stored, points is the point cloud, count is the number of points in the cloud
 */
static void getSingleCoordinate(MVert *points, int count, float coord[3])
{
	int i;
	zero_v3(coord);
	for (i = 0; i < count; i++) {
		add_v3_v3(coord, points[i].co);
	}
	mul_v3_fl(coord, 1.0f / count);
}

/* given a plane and a start and end position,
 * compute the amount of vertical distance relative to the plane and store it in dists,
 * then get the horizontal and vertical change and store them in changes
 */
static void getVerticalAndHorizontalChange(const float norm[3], float d, const float coord[3],
                                           const float start[3], float distToStart,
                                           float *end, float (*changes)[2], float *dists, int index)
{
	/* A = Q - ((Q - P).N)N
	 * D = (a * x0 + b * y0 +c * z0 + d) */
	float projA[3], projB[3];
	float plane[4];

	plane_from_point_normal_v3(plane, coord, norm);

	closest_to_plane_v3(projA, plane, start);
	closest_to_plane_v3(projB, plane, end);
	/* (vertical and horizontal refer to the plane's y and xz respectively)
	 * vertical distance */
	dists[index] = dot_v3v3(norm, end) + d;
	/* vertical change */
	changes[index][0] = dists[index] - distToStart;
	//printf("vc %f %f\n", distance(end, projB, 3)-distance(start, projA, 3), changes[index][0]);
	/* horizontal change */
	changes[index][1] = len_v3v3(projA, projB);
}

/* I need the derived mesh to be forgotten so the positions are recalculated
 * with weight changes (see dm_deform_recalc) */
static void dm_deform_clear(DerivedMesh *dm, Object *ob)
{
	if (ob->derivedDeform && (ob->derivedDeform) == dm) {
		ob->derivedDeform->needsFree = 1;
		ob->derivedDeform->release(ob->derivedDeform);
		ob->derivedDeform = NULL;
	}
	else if (dm) {
		dm->needsFree = 1;
		dm->release(dm);
	}
}

/* recalculate the deformation */
static DerivedMesh *dm_deform_recalc(Scene *scene, Object *ob)
{
	return mesh_get_derived_deform(scene, ob, CD_MASK_BAREMESH);
}

/* by changing nonzero weights, try to move a vertex in me->mverts with index 'index' to
 * distToBe distance away from the provided plane strength can change distToBe so that it moves
 * towards distToBe by that percentage cp changes how much the weights are adjusted
 * to check the distance
 *
 * index is the index of the vertex being moved
 * norm and d are the plane's properties for the equation: ax + by + cz + d = 0
 * coord is a point on the plane
 */
static void moveCloserToDistanceFromPlane(Scene *scene, Object *ob, Mesh *me, int index, float norm[3],
                                          float coord[3], float d, float distToBe, float strength, float cp)
{
	DerivedMesh *dm;
	MDeformWeight *dw;
	MVert m;
	MDeformVert *dvert = me->dvert + index;
	int totweight = dvert->totweight;
	float oldw = 0;
	float oldPos[3] = {0};
	float vc, hc, dist = 0.0f;
	int i, k;
	float (*changes)[2] = MEM_mallocN(sizeof(float *) * totweight * 2, "vertHorzChange");
	float *dists = MEM_mallocN(sizeof(float) * totweight, "distance");

	/* track if up or down moved it closer for each bone */
	int *upDown = MEM_callocN(sizeof(int) * totweight, "upDownTracker");

	int *dwIndices = MEM_callocN(sizeof(int) * totweight, "dwIndexTracker");
	float distToStart;
	int bestIndex = 0;
	bool wasChange;
	char wasUp;
	int lastIndex = -1;
	float originalDistToBe = distToBe;
	do {
		wasChange = false;
		dm = dm_deform_recalc(scene, ob);
		dm->getVert(dm, index, &m);
		copy_v3_v3(oldPos, m.co);
		distToStart = dot_v3v3(norm, oldPos) + d;

		if (distToBe == originalDistToBe) {
			distToBe += distToStart - distToStart * strength;
		}
		for (i = 0; i < totweight; i++) {
			dwIndices[i] = i;
			dw = (dvert->dw + i);
			vc = hc = 0;
			if (!dw->weight) {
				changes[i][0] = 0;
				changes[i][1] = 0;
				dists[i] = distToStart;
				continue;
			}
			for (k = 0; k < 2; k++) {
				if (dm) {
					dm_deform_clear(dm, ob); dm = NULL;
				}
				oldw = dw->weight;
				if (k) {
					dw->weight *= 1 + cp;
				}
				else {
					dw->weight /= 1 + cp;
				}
				if (dw->weight == oldw) {
					changes[i][0] = 0;
					changes[i][1] = 0;
					dists[i] = distToStart;
					break;
				}
				if (dw->weight > 1) {
					dw->weight = 1;
				}
				dm = dm_deform_recalc(scene, ob);
				dm->getVert(dm, index, &m);
				getVerticalAndHorizontalChange(norm, d, coord, oldPos, distToStart, m.co, changes, dists, i);
				dw->weight = oldw;
				if (!k) {
					vc = changes[i][0];
					hc = changes[i][1];
					dist = dists[i];
				}
				else {
					if (fabsf(dist - distToBe) < fabsf(dists[i] - distToBe)) {
						upDown[i] = 0;
						changes[i][0] = vc;
						changes[i][1] = hc;
						dists[i] = dist;
					}
					else {
						upDown[i] = 1;
					}
					if (fabsf(dists[i] - distToBe) > fabsf(distToStart - distToBe)) {
						changes[i][0] = 0;
						changes[i][1] = 0;
						dists[i] = distToStart;
					}
				}
			}
		}
		/* sort the changes by the vertical change */
		for (k = 0; k < totweight; k++) {
			float tf;
			int ti;
			bestIndex = k;
			for (i = k + 1; i < totweight; i++) {
				dist = dists[i];

				if (fabsf(dist) > fabsf(dists[i])) {
					bestIndex = i;
				}
			}
			/* switch with k */
			if (bestIndex != k) {
				ti = upDown[k];
				upDown[k] = upDown[bestIndex];
				upDown[bestIndex] = ti;

				ti = dwIndices[k];
				dwIndices[k] = dwIndices[bestIndex];
				dwIndices[bestIndex] = ti;

				tf = changes[k][0];
				changes[k][0] = changes[bestIndex][0];
				changes[bestIndex][0] = tf;

				tf = changes[k][1];
				changes[k][1] = changes[bestIndex][1];
				changes[bestIndex][1] = tf;

				tf = dists[k];
				dists[k] = dists[bestIndex];
				dists[bestIndex] = tf;
			}
		}
		bestIndex = -1;
		/* find the best change with an acceptable horizontal change */
		for (i = 0; i < totweight; i++) {
			if (fabsf(changes[i][0]) > fabsf(changes[i][1] * 2.0f)) {
				bestIndex = i;
				break;
			}
		}
		if (bestIndex != -1) {
			wasChange = true;
			/* it is a good place to stop if it tries to move the opposite direction
			 * (relative to the plane) of last time */
			if (lastIndex != -1) {
				if (wasUp != upDown[bestIndex]) {
					wasChange = false;
				}
			}
			lastIndex = bestIndex;
			wasUp = upDown[bestIndex];
			dw = (dvert->dw + dwIndices[bestIndex]);
			oldw = dw->weight;
			if (upDown[bestIndex]) {
				dw->weight *= 1 + cp;
			}
			else {
				dw->weight /= 1 + cp;
			}
			if (dw->weight > 1) {
				dw->weight = 1;
			}
			if (oldw == dw->weight) {
				wasChange = false;
			}
			if (dm) {
				dm_deform_clear(dm, ob); dm = NULL;
			}
		}
	} while (wasChange && ((distToStart - distToBe) / fabsf(distToStart - distToBe) ==
	                       (dists[bestIndex] - distToBe) / fabsf(dists[bestIndex] - distToBe)));

	MEM_freeN(upDown);
	MEM_freeN(changes);
	MEM_freeN(dists);
	MEM_freeN(dwIndices);
}

/* this is used to try to smooth a surface by only adjusting the nonzero weights of a vertex 
 * but it could be used to raise or lower an existing 'bump.' */
static void vgroup_fix(Scene *scene, Object *ob, float distToBe, float strength, float cp)
{
	int i;

	Mesh *me = ob->data;
	MVert *mvert = me->mvert;
	int *verts = NULL;
	if (!(me->editflag & ME_EDIT_PAINT_VERT_SEL))
		return;
	for (i = 0; i < me->totvert && mvert; i++, mvert++) {
		if (mvert->flag & SELECT) {
			int count = 0;
			if ((verts = getSurroundingVerts(me, i, &count))) {
				MVert m;
				MVert *p = MEM_callocN(sizeof(MVert) * (count), "deformedPoints");
				int k;

				DerivedMesh *dm = mesh_get_derived_deform(scene, ob, CD_MASK_BAREMESH);
				k = count;
				while (k--) {
					dm->getVert(dm, verts[k], &m);
					p[k] = m;
				}
				
				if (count >= 3) {
					float d /*, dist */ /* UNUSED */, mag;
					float coord[3];
					float norm[3];
					getSingleCoordinate(p, count, coord);
					dm->getVert(dm, i, &m);
					sub_v3_v3v3(norm, m.co, coord);
					mag = normalize_v3(norm);
					if (mag) { /* zeros fix */
						d = -dot_v3v3(norm, coord);
						/* dist = (dot_v3v3(norm, m.co) + d); */ /* UNUSED */
						moveCloserToDistanceFromPlane(scene, ob, me, i, norm, coord, d, distToBe, strength, cp);
					}
				}

				MEM_freeN(verts);
				MEM_freeN(p);
			}
		}
	}
}

static void vgroup_levels_subset(Object *ob, const bool *vgroup_validmap, const int vgroup_tot,
                                 const int UNUSED(subset_count),
                                 const float offset, const float gain)
{
	MDeformWeight *dw;
	MDeformVert *dv, **dvert_array = NULL;
	int i, dvert_tot = 0;

	const bool use_vert_sel = vertex_group_use_vert_sel(ob);
	const bool use_mirror = (ob->type == OB_MESH) ? (((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X) != 0 : false;

	ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, use_vert_sel);

	if (dvert_array) {

		for (i = 0; i < dvert_tot; i++) {
			int j;

			/* in case its not selected */
			if (!(dv = dvert_array[i])) {
				continue;
			}

			j = vgroup_tot;
			while (j--) {
				if (vgroup_validmap[j]) {
					dw = defvert_find_index(dv, j);
					if (dw) {
						dw->weight = gain * (dw->weight + offset);

						CLAMP(dw->weight, 0.0f, 1.0f);
					}
				}
			}
		}

		if (use_mirror && use_vert_sel) {
			ED_vgroup_parray_mirror_sync(ob, dvert_array, dvert_tot,
			                             vgroup_validmap, vgroup_tot);
		}

		MEM_freeN(dvert_array);
	}
}

static void vgroup_normalize_all(Object *ob,
                                 const bool *vgroup_validmap,
                                 const int vgroup_tot,
                                 const int subset_count,
                                 const bool lock_active)
{
	MDeformVert *dv, **dvert_array = NULL;
	int i, dvert_tot = 0;
	const int def_nr = ob->actdef - 1;

	const int use_vert_sel = vertex_group_use_vert_sel(ob);

	if ((lock_active && !BLI_findlink(&ob->defbase, def_nr)) || subset_count == 0) {
		return;
	}

	ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, use_vert_sel);

	if (dvert_array) {
		const int defbase_tot = BLI_countlist(&ob->defbase);
		bool *lock_flags = BKE_objdef_lock_flags_get(ob, defbase_tot);

		if ((lock_active == true) &&
		    (lock_flags != NULL) &&
		    (def_nr < defbase_tot))
		{
			lock_flags[def_nr] = true;
		}

		for (i = 0; i < dvert_tot; i++) {
			/* in case its not selected */
			if ((dv = dvert_array[i])) {
				if (lock_flags) {
					defvert_normalize_lock_map(dv, vgroup_validmap, vgroup_tot,
					                           lock_flags, defbase_tot);
				}
				else if (lock_active) {
					defvert_normalize_lock_single(dv, vgroup_validmap, vgroup_tot,
					                              def_nr);
				}
				else {
					defvert_normalize_subset(dv, vgroup_validmap, vgroup_tot);
				}
			}
		}

		if (lock_flags) {
			MEM_freeN(lock_flags);
		}

		MEM_freeN(dvert_array);
	}
}

enum {
	VGROUP_TOGGLE,
	VGROUP_LOCK,
	VGROUP_UNLOCK,
	VGROUP_INVERT
};

static EnumPropertyItem vgroup_lock_actions[] = {
	{VGROUP_TOGGLE, "TOGGLE", 0, "Toggle", "Unlock all vertex groups if there is at least one locked group, lock all in other case"},
	{VGROUP_LOCK, "LOCK", 0, "Lock", "Lock all vertex groups"},
	{VGROUP_UNLOCK, "UNLOCK", 0, "Unlock", "Unlock all vertex groups"},
	{VGROUP_INVERT, "INVERT", 0, "Invert", "Invert the lock state of all vertex groups"},
	{0, NULL, 0, NULL, NULL}
};

static void vgroup_lock_all(Object *ob, int action)
{
	bDeformGroup *dg;

	if (action == VGROUP_TOGGLE) {
		action = VGROUP_LOCK;
		for (dg = ob->defbase.first; dg; dg = dg->next) {
			if (dg->flag & DG_LOCK_WEIGHT) {
				action = VGROUP_UNLOCK;
				break;
			}
		}
	}

	for (dg = ob->defbase.first; dg; dg = dg->next) {
		switch (action) {
			case VGROUP_LOCK:
				dg->flag |= DG_LOCK_WEIGHT;
				break;
			case VGROUP_UNLOCK:
				dg->flag &= ~DG_LOCK_WEIGHT;
				break;
			case VGROUP_INVERT:
				dg->flag ^= DG_LOCK_WEIGHT;
				break;
		}
	}
}

static void vgroup_invert_subset(Object *ob,
                                 const bool *vgroup_validmap, const int vgroup_tot,
                                 const int UNUSED(subset_count), const bool auto_assign, const bool auto_remove)
{
	MDeformWeight *dw;
	MDeformVert *dv, **dvert_array = NULL;
	int i, dvert_tot = 0;
	const bool use_vert_sel = vertex_group_use_vert_sel(ob);
	const bool use_mirror = (ob->type == OB_MESH) ? (((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X) != 0 : false;

	ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, use_vert_sel);

	if (dvert_array) {
		for (i = 0; i < dvert_tot; i++) {
			int j;

			/* in case its not selected */
			if (!(dv = dvert_array[i])) {
				continue;
			}

			j = vgroup_tot;
			while (j--) {

				if (vgroup_validmap[j]) {
					if (auto_assign) {
						dw = defvert_verify_index(dv, j);
					}
					else {
						dw = defvert_find_index(dv, j);
					}

					if (dw) {
						dw->weight = 1.0f - dw->weight;
						CLAMP(dw->weight, 0.0f, 1.0f);
					}
				}
			}
		}

		if (use_mirror && use_vert_sel) {
			ED_vgroup_parray_mirror_sync(ob, dvert_array, dvert_tot,
			                             vgroup_validmap, vgroup_tot);
		}

		if (auto_remove) {
			ED_vgroup_parray_remove_zero(dvert_array, dvert_tot,
			                             vgroup_validmap, vgroup_tot,
			                             0.0f, false);
		}

		MEM_freeN(dvert_array);
	}
}

static void vgroup_blend_subset(Object *ob, const bool *vgroup_validmap, const int vgroup_tot,
                                const int subset_count,
                                const float fac)
{
	const float ifac = 1.0f - fac;
	MDeformVert **dvert_array = NULL;
	int i, dvert_tot = 0;
	int *vgroup_subset_map = BLI_array_alloca(vgroup_subset_map, subset_count);
	float *vgroup_subset_weights = BLI_array_alloca(vgroup_subset_weights, subset_count);
	const bool use_mirror = (ob->type == OB_MESH) ? (((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X) != 0 : false;

	BMEditMesh *em = BKE_editmesh_from_object(ob);
	BMesh *bm = em ? em->bm : NULL;
	Mesh  *me = em ? NULL   : ob->data;

	MeshElemMap *emap ;
	int *emap_mem;

	BLI_SMALLSTACK_DECLARE(dv_stack, MDeformVert *);

	ED_vgroup_subset_to_index_array(vgroup_validmap, vgroup_tot, vgroup_subset_map);
	ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, false);
	memset(vgroup_subset_weights, 0, sizeof(*vgroup_subset_weights) * subset_count);

	if (bm) {
		BM_mesh_elem_table_ensure(bm, BM_VERT);

		emap = NULL;
		emap_mem = NULL;
	}
	else {
		BKE_mesh_vert_edge_map_create(&emap, &emap_mem,
		                              me->medge, me->totvert, me->totedge);
	}


	for (i = 0; i < dvert_tot; i++) {
		MDeformVert *dv;
		int dv_stack_tot = 0;
		int j;
		/* in case its not selected */

		if (bm) {
			BMVert *v = BM_vert_at_index(bm, i);
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				BMIter eiter;
				BMEdge *e;
				BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
					BMVert *v_other = BM_edge_other_vert(e, v);
					const int i_other = BM_elem_index_get(v_other);

					if (BM_elem_flag_test(v_other, BM_ELEM_SELECT) == 0) {
						dv = dvert_array[i_other];
						BLI_SMALLSTACK_PUSH(dv_stack, dv);
						dv_stack_tot++;
					}
				}
			}
		}
		else {
			MVert *v = &me->mvert[i];
			if (v->flag & SELECT) {
				for (j = 0; j < emap[i].count; j++) {
					MEdge *e = &me->medge[emap[i].indices[j]];
					const int i_other = (e->v1 == i ? e->v2 : e->v1);
					MVert *v_other = &me->mvert[i_other];

					if ((v_other->flag & SELECT) == 0) {
						dv = dvert_array[i_other];
						BLI_SMALLSTACK_PUSH(dv_stack, dv);
						dv_stack_tot++;
					}
				}
			}
		}

		if (dv_stack_tot) {
			const float dv_mul = 1.0f / (float)dv_stack_tot;

			/* vgroup_subset_weights is zero'd at this point */
			while ((dv = BLI_SMALLSTACK_POP(dv_stack))) {
				for (j = 0; j < subset_count; j++) {
					vgroup_subset_weights[j] += dv_mul * defvert_find_weight(dv, vgroup_subset_map[j]);
				}
			}

			dv = dvert_array[i];
			for (j = 0; j < subset_count; j++) {
				MDeformWeight *dw;
				if (vgroup_subset_weights[j] > 0.0f) {
					dw = defvert_verify_index(dv, vgroup_subset_map[j]);
				}
				else {
					dw = defvert_find_index(dv, vgroup_subset_map[j]);
				}

				if (dw) {
					dw->weight = (fac * vgroup_subset_weights[j]) + (ifac * dw->weight);
					CLAMP(dw->weight, 0.0f, 1.0f);
				}

				/* zero for next iteration */
				vgroup_subset_weights[j] = 0.0f;
			}
		}
	}

	if (bm) {
		/* pass */
	}
	else {
		MEM_freeN(emap);
		MEM_freeN(emap_mem);
	}

	if (dvert_array)
		MEM_freeN(dvert_array);

	/* not so efficient to get 'dvert_array' again just so unselected verts are NULL'd */
	if (use_mirror) {
		ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, true);
		ED_vgroup_parray_mirror_sync(ob, dvert_array, dvert_tot,
		                             vgroup_validmap, vgroup_tot);
		if (dvert_array)
			MEM_freeN(dvert_array);
	}
}

static int inv_cmp_mdef_vert_weights(const void *a1, const void *a2)
{
	/* qsort sorts in ascending order.  We want descending order to save a memcopy
	 * so this compare function is inverted from the standard greater than comparison qsort needs.
	 * A normal compare function is called with two pointer arguments and should return an integer less than, equal to,
	 * or greater than zero corresponding to whether its first argument is considered less than, equal to,
	 * or greater than its second argument.  This does the opposite. */
	const struct MDeformWeight *dw1 = a1, *dw2 = a2;

	if      (dw1->weight < dw2->weight) return  1;
	else if (dw1->weight > dw2->weight) return -1;
	else if (&dw1 < &dw2)               return  1; /* compare addresses so we have a stable sort algorithm */
	else                                return -1;
}

/* Used for limiting the number of influencing bones per vertex when exporting
 * skinned meshes.  if all_deform_weights is True, limit all deform modifiers
 * to max_weights regardless of type, otherwise, only limit the number of influencing bones per vertex*/
static int vgroup_limit_total_subset(Object *ob,
                                     const bool *vgroup_validmap,
                                     const int vgroup_tot,
                                     const int subset_count,
                                     const int max_weights)
{
	MDeformVert *dv, **dvert_array = NULL;
	int i, dvert_tot = 0;
	const int use_vert_sel = vertex_group_use_vert_sel(ob);
	int remove_tot = 0;

	ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, use_vert_sel);

	if (dvert_array) {
		int num_to_drop = 0;

		for (i = 0; i < dvert_tot; i++) {

			MDeformWeight *dw_temp;
			int bone_count = 0, non_bone_count = 0;
			int j;

			/* in case its not selected */
			if (!(dv = dvert_array[i])) {
				continue;
			}

			num_to_drop = subset_count - max_weights;

			/* first check if we even need to test further */
			if (num_to_drop > 0) {
				/* re-pack dw array so that non-bone weights are first, bone-weighted verts at end
				 * sort the tail, then copy only the truncated array back to dv->dw */
				dw_temp = MEM_mallocN(sizeof(MDeformWeight) * dv->totweight, __func__);
				bone_count = 0; non_bone_count = 0;
				for (j = 0; j < dv->totweight; j++) {
					if (LIKELY(dv->dw[j].def_nr < vgroup_tot) &&
					    vgroup_validmap[dv->dw[j].def_nr])
					{
						dw_temp[dv->totweight - 1 - bone_count] = dv->dw[j];
						bone_count += 1;
					}
					else {
						dw_temp[non_bone_count] = dv->dw[j];
						non_bone_count += 1;
					}
				}
				BLI_assert(bone_count + non_bone_count == dv->totweight);
				num_to_drop = bone_count - max_weights;
				if (num_to_drop > 0) {
					qsort(&dw_temp[non_bone_count], bone_count, sizeof(MDeformWeight), inv_cmp_mdef_vert_weights);
					dv->totweight -= num_to_drop;
					/* Do we want to clean/normalize here? */
					MEM_freeN(dv->dw);
					dv->dw = MEM_reallocN(dw_temp, sizeof(MDeformWeight) * dv->totweight);
					remove_tot += num_to_drop;
				}
				else {
					MEM_freeN(dw_temp);
				}
			}

		}
		MEM_freeN(dvert_array);

	}

	return remove_tot;
}


static void vgroup_clean_subset(Object *ob, const bool *vgroup_validmap, const int vgroup_tot, const int UNUSED(subset_count),
                                const float epsilon, const bool keep_single)
{
	MDeformVert **dvert_array = NULL;
	int dvert_tot = 0;
	const bool use_vert_sel = vertex_group_use_vert_sel(ob);
	const bool use_mirror = (ob->type == OB_MESH) ? (((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X) != 0 : false;

	ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, use_vert_sel);

	if (dvert_array) {
		if (use_mirror && use_vert_sel) {
			/* correct behavior in this case isn't well defined
			 * for now assume both sides are mirrored correctly,
			 * so cleaning one side also cleans the other */
			ED_vgroup_parray_mirror_assign(ob, dvert_array, dvert_tot);
		}

		ED_vgroup_parray_remove_zero(dvert_array, dvert_tot,
		                             vgroup_validmap, vgroup_tot,
		                             epsilon, keep_single);

		MEM_freeN(dvert_array);
	}
}

static void vgroup_quantize_subset(Object *ob, const bool *vgroup_validmap, const int vgroup_tot, const int UNUSED(subset_count),
                                   const int steps)
{
	MDeformVert **dvert_array = NULL;
	int dvert_tot = 0;
	const bool use_vert_sel = vertex_group_use_vert_sel(ob);
	const bool use_mirror = (ob->type == OB_MESH) ? (((Mesh *)ob->data)->editflag & ME_EDIT_MIRROR_X) != 0 : false;
	ED_vgroup_parray_alloc(ob->data, &dvert_array, &dvert_tot, use_vert_sel);

	if (dvert_array) {
		const float steps_fl = steps;
		MDeformVert *dv;
		int i;

		if (use_mirror && use_vert_sel) {
			ED_vgroup_parray_mirror_assign(ob, dvert_array, dvert_tot);
		}

		for (i = 0; i < dvert_tot; i++) {
			MDeformWeight *dw;
			int j;

			/* in case its not selected */
			if (!(dv = dvert_array[i])) {
				continue;
			}

			for (j = 0, dw = dv->dw; j < dv->totweight; j++, dw++) {
				if ((dw->def_nr < vgroup_tot) && vgroup_validmap[dw->def_nr]) {
					dw->weight = floorf((dw->weight * steps_fl) + 0.5f) / steps_fl;
					CLAMP(dw->weight, 0.0f, 1.0f);
				}
			}
		}

		MEM_freeN(dvert_array);
	}
}

static void dvert_mirror_op(MDeformVert *dvert, MDeformVert *dvert_mirr,
                            const char sel, const char sel_mirr,
                            const int *flip_map, const int flip_map_len,
                            const bool mirror_weights, const bool flip_vgroups, const bool all_vgroups,
                            const int act_vgroup)
{
	BLI_assert(sel || sel_mirr);

	if (sel_mirr && sel) {
		/* swap */
		if (mirror_weights) {
			if (all_vgroups) {
				SWAP(MDeformVert, *dvert, *dvert_mirr);
			}
			else {
				MDeformWeight *dw =      defvert_find_index(dvert, act_vgroup);
				MDeformWeight *dw_mirr = defvert_find_index(dvert_mirr, act_vgroup);

				if (dw || dw_mirr) {
					if (dw_mirr == NULL)
						dw_mirr = defvert_verify_index(dvert_mirr, act_vgroup);
					if (dw == NULL)
						dw = defvert_verify_index(dvert, act_vgroup);

					SWAP(float, dw->weight, dw_mirr->weight);
				}
			}
		}

		if (flip_vgroups) {
			defvert_flip(dvert, flip_map, flip_map_len);
			defvert_flip(dvert_mirr, flip_map, flip_map_len);
		}
	}
	else {
		/* dvert should always be the target, only swaps pointer */
		if (sel_mirr) {
			SWAP(MDeformVert *, dvert, dvert_mirr);
		}

		if (mirror_weights) {
			if (all_vgroups) {
				defvert_copy(dvert, dvert_mirr);
			}
			else {
				defvert_copy_index(dvert, dvert_mirr, act_vgroup);
			}
		}

		/* flip map already modified for 'all_vgroups' */
		if (flip_vgroups) {
			defvert_flip(dvert, flip_map, flip_map_len);
		}
	}
}

/* TODO, vgroup locking */
/* TODO, face masking */
void ED_vgroup_mirror(Object *ob,
                      const bool mirror_weights, const bool flip_vgroups,
                      const bool all_vgroups, const bool use_topology,
                      int *r_totmirr, int *r_totfail)
{

#define VGROUP_MIRR_OP                                                        \
        dvert_mirror_op(dvert, dvert_mirr,                                    \
                        sel, sel_mirr,                                        \
                        flip_map, flip_map_len,                               \
                        mirror_weights, flip_vgroups,                         \
                        all_vgroups, def_nr                                   \
                        )

	BMVert *eve, *eve_mirr;
	MDeformVert *dvert, *dvert_mirr;
	char sel, sel_mirr;
	int *flip_map = NULL, flip_map_len;
	const int def_nr = ob->actdef - 1;
	int totmirr = 0, totfail = 0;

	*r_totmirr = *r_totfail = 0;

	if ((mirror_weights == false && flip_vgroups == false) ||
	    (BLI_findlink(&ob->defbase, def_nr) == NULL))
	{
		return;
	}

	if (flip_vgroups) {
		flip_map = all_vgroups ?
		           defgroup_flip_map(ob, &flip_map_len, false) :
		           defgroup_flip_map_single(ob, &flip_map_len, false, def_nr);

		BLI_assert(flip_map != NULL);

		if (flip_map == NULL) {
			/* something went wrong!, possibly no groups */
			return;
		}
	}
	else {
		flip_map = NULL;
		flip_map_len = 0;
	}

	/* only the active group */
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		BMEditMesh *em = me->edit_btmesh;

		if (em) {
			const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
			BMIter iter;

			if (cd_dvert_offset == -1) {
				goto cleanup;
			}

			EDBM_verts_mirror_cache_begin(em, 0, true, false, use_topology);

			/* Go through the list of editverts and assign them */
			BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
				if ((eve_mirr = EDBM_verts_mirror_get(em, eve))) {
					if (eve_mirr != eve) {
						sel = BM_elem_flag_test(eve, BM_ELEM_SELECT);
						sel_mirr = BM_elem_flag_test(eve_mirr, BM_ELEM_SELECT);

						if ((sel || sel_mirr) && (eve != eve_mirr)) {
							dvert      = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
							dvert_mirr = BM_ELEM_CD_GET_VOID_P(eve_mirr, cd_dvert_offset);

							VGROUP_MIRR_OP;
							totmirr++;
						}
					}

					/* don't use these again */
					EDBM_verts_mirror_cache_clear(em, eve);
					EDBM_verts_mirror_cache_clear(em, eve_mirr);
				}
				else {
					totfail++;
				}
			}
			EDBM_verts_mirror_cache_end(em);
		}
		else {
			/* object mode / weight paint */
			MVert *mv, *mv_mirr;
			int vidx, vidx_mirr;
			const int use_vert_sel = (me->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;

			if (me->dvert == NULL) {
				goto cleanup;
			}

			if (!use_vert_sel) {
				sel = sel_mirr = true;
			}

			/* tag verts we have used */
			for (vidx = 0, mv = me->mvert; vidx < me->totvert; vidx++, mv++) {
				mv->flag &= ~ME_VERT_TMP_TAG;
			}

			for (vidx = 0, mv = me->mvert; vidx < me->totvert; vidx++, mv++) {
				if ((mv->flag & ME_VERT_TMP_TAG) == 0) {
					if ((vidx_mirr = mesh_get_x_mirror_vert(ob, vidx, use_topology)) != -1) {
						if (vidx != vidx_mirr) {
							mv_mirr = &me->mvert[vidx_mirr];
							if ((mv_mirr->flag & ME_VERT_TMP_TAG) == 0) {

								if (use_vert_sel) {
									sel = mv->flag & SELECT;
									sel_mirr = mv_mirr->flag & SELECT;
								}

								if (sel || sel_mirr) {
									dvert = &me->dvert[vidx];
									dvert_mirr = &me->dvert[vidx_mirr];

									VGROUP_MIRR_OP;
									totmirr++;
								}

								mv->flag |= ME_VERT_TMP_TAG;
								mv_mirr->flag |= ME_VERT_TMP_TAG;
							}
						}
					}
					else {
						totfail++;
					}
				}
			}
		}
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = vgroup_edit_lattice(ob);
		int i1, i2;
		int u, v, w;
		int pntsu_half;
		/* half but found up odd value */

		if (lt->pntsu == 1 || lt->dvert == NULL) {
			goto cleanup;
		}

		/* unlike editmesh we know that by only looping over the first half of
		 * the 'u' indices it will cover all points except the middle which is
		 * ok in this case */
		pntsu_half = lt->pntsu / 2;

		for (w = 0; w < lt->pntsw; w++) {
			for (v = 0; v < lt->pntsv; v++) {
				for (u = 0; u < pntsu_half; u++) {
					int u_inv = (lt->pntsu - 1) - u;
					if (u != u_inv) {
						BPoint *bp, *bp_mirr;

						i1 = BKE_lattice_index_from_uvw(lt, u, v, w);
						i2 = BKE_lattice_index_from_uvw(lt, u_inv, v, w);

						bp = &lt->def[i1];
						bp_mirr = &lt->def[i2];

						sel = bp->f1 & SELECT;
						sel_mirr = bp_mirr->f1 & SELECT;

						if (sel || sel_mirr) {
							dvert = &lt->dvert[i1];
							dvert_mirr = &lt->dvert[i2];

							VGROUP_MIRR_OP;
							totmirr++;
						}
					}
				}
			}
		}
	}

	/* disabled, confusing when you have an active pose bone */
#if 0
	/* flip active group index */
	if (flip_vgroups && flip_map[def_nr] >= 0)
		ob->actdef = flip_map[def_nr] + 1;
#endif

cleanup:
	*r_totmirr = totmirr;
	*r_totfail = totfail;

	if (flip_map) MEM_freeN(flip_map);

#undef VGROUP_MIRR_OP

}

static void vgroup_remap_update_users(Object *ob, int *map)
{
	ExplodeModifierData *emd;
	ModifierData *md;
	ParticleSystem *psys;
	ClothModifierData *clmd;
	ClothSimSettings *clsim;
	int a;

	/* these cases don't use names to refer to vertex groups, so when
	 * they get deleted the numbers get out of sync, this corrects that */

	if (ob->soft)
		ob->soft->vertgroup = map[ob->soft->vertgroup];

	for (md = ob->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_Explode) {
			emd = (ExplodeModifierData *)md;
			emd->vgroup = map[emd->vgroup];
		}
		else if (md->type == eModifierType_Cloth) {
			clmd = (ClothModifierData *)md;
			clsim = clmd->sim_parms;

			if (clsim) {
				clsim->vgroup_mass = map[clsim->vgroup_mass];
				clsim->vgroup_bend = map[clsim->vgroup_bend];
				clsim->vgroup_struct = map[clsim->vgroup_struct];
			}
		}
	}

	for (psys = ob->particlesystem.first; psys; psys = psys->next) {
		for (a = 0; a < PSYS_TOT_VG; a++)
			psys->vgroup[a] = map[psys->vgroup[a]];
	}
}


static void vgroup_delete_update_users(Object *ob, int id)
{
	int i, defbase_tot = BLI_countlist(&ob->defbase) + 1;
	int *map = MEM_mallocN(sizeof(int) * defbase_tot, "vgroup del");

	map[id] = map[0] = 0;
	for (i = 1; i < id; i++) map[i] = i;
	for (i = id + 1; i < defbase_tot; i++) map[i] = i - 1;

	vgroup_remap_update_users(ob, map);
	MEM_freeN(map);
}


static void vgroup_delete_object_mode(Object *ob, bDeformGroup *dg)
{
	MDeformVert *dvert_array = NULL;
	int dvert_tot = 0;
	const int def_nr = BLI_findindex(&ob->defbase, dg);

	BLI_assert(def_nr != -1);

	ED_vgroup_array_get(ob->data, &dvert_array, &dvert_tot);

	if (dvert_array) {
		int i, j;
		MDeformVert *dv;
		for (i = 0, dv = dvert_array; i < dvert_tot; i++, dv++) {
			MDeformWeight *dw;

			dw = defvert_find_index(dv, def_nr);
			defvert_remove_group(dv, dw); /* dw can be NULL */

			/* inline, make into a function if anything else needs to do this */
			for (j = 0; j < dv->totweight; j++) {
				if (dv->dw[j].def_nr > def_nr) {
					dv->dw[j].def_nr--;
				}
			}
			/* done */
		}
	}

	vgroup_delete_update_users(ob, def_nr + 1);

	/* Remove the group */
	BLI_freelinkN(&ob->defbase, dg);

	/* Update the active deform index if necessary */
	if (ob->actdef > def_nr)
		ob->actdef--;
	if (ob->actdef < 1 && ob->defbase.first)
		ob->actdef = 1;

	/* remove all dverts */
	if (BLI_listbase_is_empty(&ob->defbase)) {
		if (ob->type == OB_MESH) {
			Mesh *me = ob->data;
			CustomData_free_layer_active(&me->vdata, CD_MDEFORMVERT, me->totvert);
			me->dvert = NULL;
		}
		else if (ob->type == OB_LATTICE) {
			Lattice *lt = ob->data;
			if (lt->dvert) {
				MEM_freeN(lt->dvert);
				lt->dvert = NULL;
			}
		}
	}
}

/* only in editmode */
/* removes from active defgroup, if allverts==0 only selected vertices */
static bool vgroup_active_remove_verts(Object *ob, const bool allverts, bDeformGroup *dg)
{
	MDeformVert *dv;
	const int def_nr = BLI_findindex(&ob->defbase, dg);
	bool changed = false;

	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (me->edit_btmesh) {
			BMEditMesh *em = me->edit_btmesh;
			const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

			if (cd_dvert_offset != -1) {
				BMVert *eve;
				BMIter iter;

				BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
					dv = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);

					if (dv && dv->dw && (allverts || BM_elem_flag_test(eve, BM_ELEM_SELECT))) {
						MDeformWeight *dw = defvert_find_index(dv, def_nr);
						defvert_remove_group(dv, dw); /* dw can be NULL */
						changed = true;
					}
				}
			}
		}
		else {
			if (me->dvert) {
				MVert *mv;
				int i;

				mv = me->mvert;
				dv = me->dvert;

				for (i = 0; i < me->totvert; i++, mv++, dv++) {
					if (mv->flag & SELECT) {
						if (dv->dw && (allverts || (mv->flag & SELECT))) {
							MDeformWeight *dw = defvert_find_index(dv, def_nr);
							defvert_remove_group(dv, dw); /* dw can be NULL */
							changed = true;
						}
					}
				}
			}
		}
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = vgroup_edit_lattice(ob);
		
		if (lt->dvert) {
			BPoint *bp;
			int i, tot = lt->pntsu * lt->pntsv * lt->pntsw;
				
			for (i = 0, bp = lt->def; i < tot; i++, bp++) {
				if (allverts || (bp->f1 & SELECT)) {
					MDeformWeight *dw;

					dv = &lt->dvert[i];

					dw = defvert_find_index(dv, def_nr);
					defvert_remove_group(dv, dw); /* dw can be NULL */
					changed = true;
				}
			}
		}
	}

	return changed;
}

static void vgroup_delete_edit_mode(Object *ob, bDeformGroup *dg)
{
	int i;
	const int dg_index = BLI_findindex(&ob->defbase, dg);

	BLI_assert(dg_index != -1);

	/* Make sure that no verts are using this group */
	if (vgroup_active_remove_verts(ob, true, dg) == false) {
		/* do nothing */
	}
	/* Make sure that any verts with higher indices are adjusted accordingly */
	else if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		BMEditMesh *em = me->edit_btmesh;
		const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

		BMIter iter;
		BMVert *eve;
		MDeformVert *dvert;

		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			dvert = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);

			if (dvert)
				for (i = 0; i < dvert->totweight; i++)
					if (dvert->dw[i].def_nr > dg_index)
						dvert->dw[i].def_nr--;
		}
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = vgroup_edit_lattice(ob);
		BPoint *bp;
		MDeformVert *dvert = lt->dvert;
		int a, tot;

		if (dvert) {
			tot = lt->pntsu * lt->pntsv * lt->pntsw;
			for (a = 0, bp = lt->def; a < tot; a++, bp++, dvert++) {
				for (i = 0; i < dvert->totweight; i++) {
					if (dvert->dw[i].def_nr > dg_index)
						dvert->dw[i].def_nr--;
				}
			}
		}
	}

	vgroup_delete_update_users(ob, dg_index + 1);

	/* Remove the group */
	BLI_freelinkN(&ob->defbase, dg);

	/* Update the active deform index if necessary */
	if (ob->actdef > dg_index)
		ob->actdef--;
	if (ob->actdef < 1 && ob->defbase.first)
		ob->actdef = 1;

	/* remove all dverts */
	if (BLI_listbase_is_empty(&ob->defbase)) {
		if (ob->type == OB_MESH) {
			Mesh *me = ob->data;
			CustomData_free_layer_active(&me->vdata, CD_MDEFORMVERT, me->totvert);
			me->dvert = NULL;
		}
		else if (ob->type == OB_LATTICE) {
			Lattice *lt = vgroup_edit_lattice(ob);
			if (lt->dvert) {
				MEM_freeN(lt->dvert);
				lt->dvert = NULL;
			}
		}
	}
}

static void vgroup_delete(Object *ob)
{
	bDeformGroup *dg = BLI_findlink(&ob->defbase, ob->actdef - 1);
	if (!dg)
		return;

	if (BKE_object_is_in_editmode_vgroup(ob))
		vgroup_delete_edit_mode(ob, dg);
	else
		vgroup_delete_object_mode(ob, dg);
}

static void vgroup_delete_all(Object *ob)
{
	/* Remove all DVerts */
	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;
		CustomData_free_layer_active(&me->vdata, CD_MDEFORMVERT, me->totvert);
		me->dvert = NULL;
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = vgroup_edit_lattice(ob);
		if (lt->dvert) {
			MEM_freeN(lt->dvert);
			lt->dvert = NULL;
		}
	}
	
	/* Remove all DefGroups */
	BLI_freelistN(&ob->defbase);
	
	/* Fix counters/indices */
	ob->actdef = 0;
}

/* only in editmode */
static void vgroup_assign_verts(Object *ob, const float weight)
{
	const int def_nr = ob->actdef - 1;

	if (!BLI_findlink(&ob->defbase, def_nr))
		return;

	if (ob->type == OB_MESH) {
		Mesh *me = ob->data;

		if (me->edit_btmesh) {
			BMEditMesh *em = me->edit_btmesh;
			int cd_dvert_offset;

			BMIter iter;
			BMVert *eve;

			if (!CustomData_has_layer(&em->bm->vdata, CD_MDEFORMVERT))
				BM_data_layer_add(em->bm, &em->bm->vdata, CD_MDEFORMVERT);

			cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

			/* Go through the list of editverts and assign them */
			BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
				if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
					MDeformVert *dv;
					MDeformWeight *dw;
					dv = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset); /* can be NULL */
					dw = defvert_verify_index(dv, def_nr);
					if (dw) {
						dw->weight = weight;
					}
				}
			}
		}
		else {
			MVert *mv;
			MDeformVert *dv;
			int i;

			if (!me->dvert) {
				ED_vgroup_data_create(&me->id);
			}

			mv = me->mvert;
			dv = me->dvert;

			for (i = 0; i < me->totvert; i++, mv++, dv++) {
				if (mv->flag & SELECT) {
					MDeformWeight *dw;
					dw = defvert_verify_index(dv, def_nr);
					if (dw) {
						dw->weight = weight;
					}
				}
			}
		}
	}
	else if (ob->type == OB_LATTICE) {
		Lattice *lt = vgroup_edit_lattice(ob);
		MDeformVert *dv;
		BPoint *bp;
		int a, tot;

		if (lt->dvert == NULL)
			ED_vgroup_data_create(&lt->id);

		dv = lt->dvert;

		tot = lt->pntsu * lt->pntsv * lt->pntsw;
		for (a = 0, bp = lt->def; a < tot; a++, bp++, dv++) {
			if (bp->f1 & SELECT) {
				MDeformWeight *dw;

				dw = defvert_verify_index(dv, def_nr);
				if (dw) {
					dw->weight = weight;
				}
			}
		}
	}
}

/* only in editmode */
/* removes from all defgroup, if allverts==0 only selected vertices */
static bool vgroup_remove_verts(Object *ob, int allverts)
{
	bool changed = false;
	/* To prevent code redundancy, we just use vgroup_active_remove_verts, but that
	 * only operates on the active vgroup. So we iterate through all groups, by changing
	 * active group index
	 */
	bDeformGroup *dg;
	for (dg = ob->defbase.first; dg; dg = dg->next) {
		changed |= vgroup_active_remove_verts(ob, allverts, dg);
	}
	return changed;
}

/********************** vertex group operators *********************/

static int vertex_group_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;

	return (ob && !ob->id.lib &&
	        data && !data->lib &&
	        OB_TYPE_SUPPORT_VGROUP(ob->type) &&
	        ob->defbase.first);
}

static int vertex_group_supported_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;
	return (ob && !ob->id.lib && OB_TYPE_SUPPORT_VGROUP(ob->type) && data && !data->lib);
}

static int vertex_group_mesh_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;

	return (ob && !ob->id.lib &&
	        data && !data->lib &&
	        ob->type == OB_MESH &&
	        ob->defbase.first);
}

static int vertex_group_mesh_supported_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;
	return (ob && !ob->id.lib && ob->type == OB_MESH && data && !data->lib);
}


static int UNUSED_FUNCTION(vertex_group_poll_edit) (bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;

	if (!(ob && !ob->id.lib && data && !data->lib))
		return 0;

	return BKE_object_is_in_editmode_vgroup(ob);
}

/* editmode _or_ weight paint vertex sel */
static int vertex_group_vert_select_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;

	if (!(ob && !ob->id.lib && data && !data->lib))
		return 0;

	return (BKE_object_is_in_editmode_vgroup(ob) ||
	        BKE_object_is_in_wpaint_select_vert(ob));
}

/* editmode _or_ weight paint vertex sel and active group unlocked */
static int vertex_group_vert_select_unlocked_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;

	if (!(ob && !ob->id.lib && data && !data->lib))
		return 0;

	if (!(BKE_object_is_in_editmode_vgroup(ob) ||
	    BKE_object_is_in_wpaint_select_vert(ob)))
	{
		return 0;
	}

	if (ob->actdef != 0) {
		bDeformGroup *dg = BLI_findlink(&ob->defbase, ob->actdef - 1);
		if (dg) {
			return !(dg->flag & DG_LOCK_WEIGHT);
		}
	}
	return 1;
}

static int vertex_group_vert_select_mesh_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;

	if (!(ob && !ob->id.lib && data && !data->lib))
		return 0;

	/* only difference to #vertex_group_vert_select_poll */
	if (ob->type != OB_MESH)
		return 0;

	return (BKE_object_is_in_editmode_vgroup(ob) ||
	        BKE_object_is_in_wpaint_select_vert(ob));
}

static int vertex_group_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);

	ED_vgroup_add(ob);
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob->data);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_add";
	ot->description = "Add a new vertex group to the active object";
	
	/* api callbacks */
	ot->poll = vertex_group_supported_poll;
	ot->exec = vertex_group_add_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_group_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);

	if (RNA_boolean_get(op->ptr, "all"))
		vgroup_delete_all(ob);
	else
		vgroup_delete(ob);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob->data);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_remove";
	ot->description = "Delete the active or all vertex groups from the active object";
	
	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_remove_exec;

	/* flags */
	/* redo operator will fail in this case because vertex groups aren't stored
	 * in local edit mode stack and toggling "all" property will lead to
	 * all groups deleted without way to restore them (see [#29527], sergey) */
	ot->flag = /*OPTYPE_REGISTER|*/ OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 0, "All", "Remove all vertex groups");
}

static int vertex_group_assign_exec(bContext *C, wmOperator *UNUSED(op))
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = ED_object_context(C);
	
	vgroup_assign_verts(ob, ts->vgroup_weight);
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_assign(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Assign to Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_assign";
	ot->description = "Assign the selected vertices to the active vertex group";
	
	/* api callbacks */
	ot->poll = vertex_group_vert_select_unlocked_poll;
	ot->exec = vertex_group_assign_exec;

	/* flags */
	/* redo operator will fail in this case because vertex group assignment
	 * isn't stored in local edit mode stack and toggling "new" property will
	 * lead to creating plenty of new vertex groups (see [#29527], sergey) */
	ot->flag = /*OPTYPE_REGISTER|*/ OPTYPE_UNDO;
}

/* NOTE: just a wrapper around vertex_group_assign_exec(), except we add these to a new group */
static int vertex_group_assign_new_exec(bContext *C, wmOperator *op)
{
	/* create new group... */
	Object *ob = ED_object_context(C);
	ED_vgroup_add(ob);
	
	/* assign selection to new group */
	return vertex_group_assign_exec(C, op);
}

void OBJECT_OT_vertex_group_assign_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Assign to New Group";
	ot->idname = "OBJECT_OT_vertex_group_assign_new";
	ot->description = "Assign the selected vertices to a new vertex group";
	
	/* api callbacks */
	ot->poll = vertex_group_vert_select_poll;
	ot->exec = vertex_group_assign_new_exec;
	
	/* flags */
	/* redo operator will fail in this case because vertex group assignment
	 * isn't stored in local edit mode stack and toggling "new" property will
	 * lead to creating plenty of new vertex groups (see [#29527], sergey) */
	ot->flag = /*OPTYPE_REGISTER|*/ OPTYPE_UNDO;
}

static int vertex_group_remove_from_exec(bContext *C, wmOperator *op)
{
	const bool use_all_groups = RNA_boolean_get(op->ptr, "use_all_groups");
	const bool use_all_verts = RNA_boolean_get(op->ptr, "use_all_verts");

	Object *ob = ED_object_context(C);

	if (use_all_groups) {
		if (vgroup_remove_verts(ob, 0) == false) {
			return OPERATOR_CANCELLED;
		}
	}
	else {
		bDeformGroup *dg = BLI_findlink(&ob->defbase, ob->actdef - 1);

		if ((dg == NULL) || (vgroup_active_remove_verts(ob, use_all_verts, dg) == false)) {
			return OPERATOR_CANCELLED;
		}
	}

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_remove_from(wmOperatorType *ot)
{
	PropertyRNA *prop;
	/* identifiers */
	ot->name = "Remove from Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_remove_from";
	ot->description = "Remove the selected vertices from active or all vertex group(s)";

	/* api callbacks */
	ot->poll = vertex_group_vert_select_unlocked_poll;
	ot->exec = vertex_group_remove_from_exec;

	/* flags */
	/* redo operator will fail in this case because vertex groups assignment
	 * isn't stored in local edit mode stack and toggling "all" property will lead to
	 * removing vertices from all groups (see [#29527], sergey) */
	ot->flag = /*OPTYPE_REGISTER|*/ OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_boolean(ot->srna, "use_all_groups", 0, "All Groups", "Remove from all groups");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "use_all_verts", 0, "All Verts", "Clear the active group");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static int vertex_group_select_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);

	if (!ob || ob->id.lib)
		return OPERATOR_CANCELLED;

	vgroup_select_verts(ob, 1);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_select";
	ot->description = "Select all the vertices assigned to the active vertex group";

	/* api callbacks */
	ot->poll = vertex_group_vert_select_poll;
	ot->exec = vertex_group_select_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_group_deselect_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);

	vgroup_select_verts(ob, 0);
	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_deselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Deselect Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_deselect";
	ot->description = "Deselect all selected vertices assigned to the active vertex group";

	/* api callbacks */
	ot->poll = vertex_group_vert_select_poll;
	ot->exec = vertex_group_deselect_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_group_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);

	vgroup_duplicate(ob);
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_copy";
	ot->description = "Make a copy of the active vertex group";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_copy_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_group_levels_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	
	float offset = RNA_float_get(op->ptr, "offset");
	float gain = RNA_float_get(op->ptr, "gain");
	eVGroupSelect subset_type  = RNA_enum_get(op->ptr, "group_select_mode");

	int subset_count, vgroup_tot;

	const bool *vgroup_validmap = ED_vgroup_subset_from_select_type(ob, subset_type, &vgroup_tot, &subset_count);
	vgroup_levels_subset(ob, vgroup_validmap, vgroup_tot, subset_count, offset, gain);
	MEM_freeN((void *)vgroup_validmap);
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_levels(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Vertex Group Levels";
	ot->idname = "OBJECT_OT_vertex_group_levels";
	ot->description = "Add some offset and multiply with some gain the weights of the active vertex group";
	
	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_levels_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	vgroup_operator_subset_select_props(ot, true);
	RNA_def_float(ot->srna, "offset", 0.f, -1.0, 1.0, "Offset", "Value to add to weights", -1.0f, 1.f);
	RNA_def_float(ot->srna, "gain", 1.f, 0.f, FLT_MAX, "Gain", "Value to multiply weights by", 0.0f, 10.f);
}

static int vertex_group_normalize_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);

	vgroup_normalize(ob);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_normalize(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Normalize Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_normalize";
	ot->description = "Normalize weights of the active vertex group, so that the highest ones are now 1.0";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_normalize_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_group_normalize_all_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	bool lock_active = RNA_boolean_get(op->ptr, "lock_active");
	eVGroupSelect subset_type  = RNA_enum_get(op->ptr, "group_select_mode");

	int subset_count, vgroup_tot;

	const bool *vgroup_validmap = ED_vgroup_subset_from_select_type(ob, subset_type, &vgroup_tot, &subset_count);
	vgroup_normalize_all(ob, vgroup_validmap, vgroup_tot, subset_count, lock_active);
	MEM_freeN((void *)vgroup_validmap);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_normalize_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Normalize All Vertex Groups";
	ot->idname = "OBJECT_OT_vertex_group_normalize_all";
	ot->description = "Normalize all weights of all vertex groups, "
	                  "so that for each vertex, the sum of all weights is 1.0";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_normalize_all_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	vgroup_operator_subset_select_props(ot, false);
	RNA_def_boolean(ot->srna, "lock_active", true, "Lock Active",
	                "Keep the values of the active group while normalizing others");
}

static int vertex_group_fix_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	Scene *scene = CTX_data_scene(C);
	
	float distToBe = RNA_float_get(op->ptr, "dist");
	float strength = RNA_float_get(op->ptr, "strength");
	float cp = RNA_float_get(op->ptr, "accuracy");
	ModifierData *md = ob->modifiers.first;

	while (md) {
		if (md->type == eModifierType_Mirror && (md->mode & eModifierMode_Realtime)) {
			break;
		}
		md = md->next;
	}
	
	if (md && md->type == eModifierType_Mirror) {
		BKE_report(op->reports, RPT_ERROR_INVALID_CONTEXT, "This operator does not support an active mirror modifier");
		return OPERATOR_CANCELLED;
	}
	vgroup_fix(scene, ob, distToBe, strength, cp);
	
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_fix(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Fix Vertex Group Deform";
	ot->idname = "OBJECT_OT_vertex_group_fix";
	ot->description = "Modify the position of selected vertices by changing only their respective "
	                  "groups' weights (this tool may be slow for many vertices)";
	
	/* api callbacks */
	ot->poll = vertex_group_mesh_poll;
	ot->exec = vertex_group_fix_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	RNA_def_float(ot->srna, "dist", 0.0f, -FLT_MAX, FLT_MAX, "Distance", "The distance to move to", -10.0f, 10.0f);
	RNA_def_float(ot->srna, "strength", 1.f, -2.0f, FLT_MAX, "Strength",
	              "The distance moved can be changed by this multiplier", -2.0f, 2.0f);
	RNA_def_float(ot->srna, "accuracy", 1.0f, 0.05f, FLT_MAX, "Change Sensitivity",
	              "Change the amount weights are altered with each iteration: lower values are slower", 0.05f, 1.f);
}


static int vertex_group_lock_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);

	int action = RNA_enum_get(op->ptr, "action");

	vgroup_lock_all(ob, action);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_lock(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change the Lock On Vertex Groups";
	ot->idname = "OBJECT_OT_vertex_group_lock";
	ot->description = "Change the lock state of all vertex groups of active object";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_lock_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "action", vgroup_lock_actions, VGROUP_TOGGLE, "Action",
	             "Lock action to execute on vertex groups");
}

static int vertex_group_invert_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	bool auto_assign = RNA_boolean_get(op->ptr, "auto_assign");
	bool auto_remove = RNA_boolean_get(op->ptr, "auto_remove");

	eVGroupSelect subset_type  = RNA_enum_get(op->ptr, "group_select_mode");

	int subset_count, vgroup_tot;

	const bool *vgroup_validmap = ED_vgroup_subset_from_select_type(ob, subset_type, &vgroup_tot, &subset_count);
	vgroup_invert_subset(ob, vgroup_validmap, vgroup_tot, subset_count, auto_assign, auto_remove);
	MEM_freeN((void *)vgroup_validmap);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_invert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Invert Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_invert";
	ot->description = "Invert active vertex group's weights";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_invert_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	vgroup_operator_subset_select_props(ot, true);
	RNA_def_boolean(ot->srna, "auto_assign", true, "Add Weights",
	                "Add verts from groups that have zero weight before inverting");
	RNA_def_boolean(ot->srna, "auto_remove", true, "Remove Weights",
	                "Remove verts from groups that have zero weight after inverting");
}


static int vertex_group_blend_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	float fac = RNA_float_get(op->ptr, "factor");
	eVGroupSelect subset_type  = RNA_enum_get(op->ptr, "group_select_mode");

	int subset_count, vgroup_tot;

	const bool *vgroup_validmap = ED_vgroup_subset_from_select_type(ob, subset_type, &vgroup_tot, &subset_count);
	vgroup_blend_subset(ob, vgroup_validmap, vgroup_tot, subset_count, fac);
	MEM_freeN((void *)vgroup_validmap);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

/* check we have a vertex selection, either in weight paint or editmode */
static int vertex_group_blend_poll(bContext *C)
{
	Object *ob = ED_object_context(C);
	ID *data = (ob) ? ob->data : NULL;

	if (!(ob && !ob->id.lib && data && !data->lib))
		return false;

	if (ob->type != OB_MESH) {
		return false;
	}

	if (BKE_object_is_in_editmode_vgroup(ob)) {
		return true;
	}
	else if (ob->mode & OB_MODE_WEIGHT_PAINT) {
		if (ME_EDIT_PAINT_SEL_MODE(((Mesh *)data)) == SCE_SELECT_VERTEX) {
			return true;
		}
		else {
			CTX_wm_operator_poll_msg_set(C, "Vertex select needs to be enabled in weight paint mode");
			return false;
		}

	}
	else {
		return false;
	}
}

void OBJECT_OT_vertex_group_blend(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Blend Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_blend";
	ot->description = "Blend selected vertex weights with unselected for the active group";

	/* api callbacks */
	ot->poll = vertex_group_blend_poll;
	ot->exec = vertex_group_blend_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	vgroup_operator_subset_select_props(ot, true);
	prop = RNA_def_property(ot->srna, "factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_ui_text(prop, "Factor", "");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_float_default(prop, 1.0f);
}


static int vertex_group_clean_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);

	float limit = RNA_float_get(op->ptr, "limit");
	bool keep_single = RNA_boolean_get(op->ptr, "keep_single");
	eVGroupSelect subset_type  = RNA_enum_get(op->ptr, "group_select_mode");

	int subset_count, vgroup_tot;

	const bool *vgroup_validmap = ED_vgroup_subset_from_select_type(ob, subset_type, &vgroup_tot, &subset_count);
	vgroup_clean_subset(ob, vgroup_validmap, vgroup_tot, subset_count, limit, keep_single);
	MEM_freeN((void *)vgroup_validmap);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_clean(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clean Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_clean";
	ot->description = "Remove vertex group assignments which are not required";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_clean_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	vgroup_operator_subset_select_props(ot, true);
	RNA_def_float(ot->srna, "limit", 0.0f, 0.0f, 1.0, "Limit",
	              "Remove vertices which weight is below or equal to this limit", 0.0f, 0.99f);
	RNA_def_boolean(ot->srna, "keep_single", false, "Keep Single",
	                "Keep verts assigned to at least one group when cleaning");
}

static int vertex_group_quantize_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);

	const int steps = RNA_int_get(op->ptr, "steps");
	eVGroupSelect subset_type  = RNA_enum_get(op->ptr, "group_select_mode");

	int subset_count, vgroup_tot;

	const bool *vgroup_validmap = ED_vgroup_subset_from_select_type(ob, subset_type, &vgroup_tot, &subset_count);
	vgroup_quantize_subset(ob, vgroup_validmap, vgroup_tot, subset_count, steps);
	MEM_freeN((void *)vgroup_validmap);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_quantize(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Quantize Vertex Weights";
	ot->idname = "OBJECT_OT_vertex_group_quantize";
	ot->description = "Set weights to a fixed number of steps";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_quantize_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	vgroup_operator_subset_select_props(ot, true);
	RNA_def_int(ot->srna, "steps", 4, 1, 1000, "Steps", "Number of steps between 0 and 1", 1, 100);
}

static int vertex_group_limit_total_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);

	const int limit = RNA_int_get(op->ptr, "limit");
	eVGroupSelect subset_type  = RNA_enum_get(op->ptr, "group_select_mode");

	int subset_count, vgroup_tot;

	const bool *vgroup_validmap = ED_vgroup_subset_from_select_type(ob, subset_type, &vgroup_tot, &subset_count);
	int remove_tot = vgroup_limit_total_subset(ob, vgroup_validmap, vgroup_tot, subset_count, limit);
	MEM_freeN((void *)vgroup_validmap);

	BKE_reportf(op->reports, remove_tot ? RPT_INFO : RPT_WARNING, "%d vertex weights limited", remove_tot);

	if (remove_tot) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
		WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

		return OPERATOR_FINISHED;
	}
	else {
		/* note, would normally return canceled, except we want the redo
		 * UI to show up for users to change */
		return OPERATOR_FINISHED;
	}
}

void OBJECT_OT_vertex_group_limit_total(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Limit Number of Weights per Vertex";
	ot->idname = "OBJECT_OT_vertex_group_limit_total";
	ot->description = "Limit deform weights associated with a vertex to a specified number by removing lowest weights";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_limit_total_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	vgroup_operator_subset_select_props(ot, false);
	RNA_def_int(ot->srna, "limit", 4, 1, 32, "Limit", "Maximum number of deform weights", 1, 32);
}

static int vertex_group_mirror_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	int totmirr = 0, totfail = 0;

	ED_vgroup_mirror(ob,
	                 RNA_boolean_get(op->ptr, "mirror_weights"),
	                 RNA_boolean_get(op->ptr, "flip_group_names"),
	                 RNA_boolean_get(op->ptr, "all_groups"),
	                 RNA_boolean_get(op->ptr, "use_topology"),
	                 &totmirr, &totfail);

	ED_mesh_report_mirror(op, totmirr, totfail);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mirror Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_mirror";
	ot->description = "Mirror all vertex groups, flip weights and/or names, editing only selected vertices, "
	                  "flipping when both sides are selected otherwise copy from unselected";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_mirror_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "mirror_weights", true, "Mirror Weights", "Mirror weights");
	RNA_def_boolean(ot->srna, "flip_group_names", true, "Flip Group Names", "Flip vertex group names");
	RNA_def_boolean(ot->srna, "all_groups", false, "All Groups", "Mirror all vertex groups weights");
	RNA_def_boolean(ot->srna, "use_topology", 0, "Topology Mirror",
	                "Use topology based mirroring (for when both sides of mesh have matching, unique topology)");
}

static int vertex_group_copy_to_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_context(C);
	Base *base;
	int retval = OPERATOR_CANCELLED;

	for (base = scene->base.first; base; base = base->next) {
		if (base->object->type == ob->type) {
			if (base->object != ob && base->object->data == ob->data) {
				BLI_freelistN(&base->object->defbase);
				BLI_duplicatelist(&base->object->defbase, &ob->defbase);
				base->object->actdef = ob->actdef;

				DAG_id_tag_update(&base->object->id, OB_RECALC_DATA);
				WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, base->object);
				WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, base->object->data);

				retval = OPERATOR_FINISHED;
			}
		}
	}

	return retval;
}

void OBJECT_OT_vertex_group_copy_to_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Vertex Groups to Linked";
	ot->idname = "OBJECT_OT_vertex_group_copy_to_linked";
	ot->description = "Copy vertex groups to all users of the same geometry data";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_copy_to_linked_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_group_copy_to_selected_exec(bContext *C, wmOperator *op)
{
	Object *obact = ED_object_context(C);
	int changed_tot = 0;
	int fail = 0;

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (obact != ob) {
			if (ED_vgroup_array_copy(ob, obact)) changed_tot++;
			else fail++;
		}
	}
	CTX_DATA_END;

	if ((changed_tot == 0 && fail == 0) || fail) {
		BKE_reportf(op->reports, RPT_ERROR,
		            "Copy vertex groups to selected: %d done, %d failed (object data must have matching indices)",
		            changed_tot, fail);
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_group_copy_to_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Vertex Group to Selected";
	ot->idname = "OBJECT_OT_vertex_group_copy_to_selected";
	ot->description = "Copy vertex groups to other selected objects with matching indices";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_copy_to_selected_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_group_transfer_weight_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob_act = CTX_data_active_object(C);

	bDeformGroup *dg_act = BLI_findlink(&ob_act->defbase, (ob_act->actdef - 1));
	char dg_act_name[MAX_VGROUP_NAME];  /* may be freed so copy */

	int fail = 0;
	bool changed = false;

	WT_VertexGroupMode vertex_group_mode = RNA_enum_get(op->ptr, "group_select_mode");
	WT_Method method = RNA_enum_get(op->ptr, "method");
	WT_ReplaceMode replace_mode = RNA_enum_get(op->ptr, "replace_mode");

	if (vertex_group_mode == WT_REPLACE_ACTIVE_VERTEX_GROUP) {
		if (!dg_act) {
			BKE_report(op->reports, RPT_WARNING, "Failed, active object has no active groups");
			return OPERATOR_FINISHED; /* to get the chance to make changes in the redo panel*/
		}
	}

	if (dg_act) {
		BLI_strncpy(dg_act_name, dg_act->name, sizeof(dg_act_name));
	}

	/* Macro to loop through selected objects and perform operation depending on function, option and method.*/
	CTX_DATA_BEGIN (C, Object *, ob_src, selected_editable_objects)
	{
		if (ob_act != ob_src) {

			if (BLI_listbase_is_empty(&ob_src->defbase)) {
				BKE_reportf(op->reports, RPT_WARNING,
				            "Skipping object '%s' it has no vertex groups", ob_src->id.name + 2);
				continue;
			}
			else if (ob_src->type != OB_MESH) {
				/* armatures can be in pose mode so ignore them */
				if (ob_src->type != OB_ARMATURE) {
					BKE_reportf(op->reports, RPT_WARNING,
					            "Skipping object '%s' only copying from meshes is supported", ob_src->id.name + 2);
				}
				continue;
			}

			switch (vertex_group_mode) {

				case WT_REPLACE_ACTIVE_VERTEX_GROUP:
				{
					bDeformGroup *dg_src;
					dg_src = defgroup_find_name(ob_src, dg_act_name);

					if (dg_src == NULL) {
						BKE_reportf(op->reports, RPT_WARNING,
						            "Skipping object '%s' no group '%s' found", ob_src->id.name + 2, dg_act_name);
						continue;
					}

					if (ed_vgroup_transfer_weight(ob_act, ob_src, dg_src, scene, method, replace_mode, op)) {
						changed = true;
					}
					else {
						fail++;
					}
					break;
				}
				case WT_REPLACE_ALL_VERTEX_GROUPS:
				{
					bDeformGroup *dg_src;
					for (dg_src = ob_src->defbase.first; dg_src; dg_src = dg_src->next) {
						if (ed_vgroup_transfer_weight(ob_act, ob_src, dg_src, scene, method, replace_mode, op)) {
							changed = true;
						}
						else {
							fail++;
						}
					}
					break;
				}
				default:
					BLI_assert(0);
					break;
			}
		}
	}
	CTX_DATA_END;

	if (changed) {

		/* possible the active vertex group changed because of adding/removing */
		/* note!, dg_act may be realloc'd, only check its not NULL */
		if (dg_act) {
			ED_vgroup_select_by_name(ob_act, dg_act_name);
		}
		else {
			ED_vgroup_sync_from_pose(ob_act);
		}

		/* Event notifiers for correct display of data.*/


		DAG_id_tag_update(&ob_act->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob_act);  /* for buttons */
		WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob_act);
		return OPERATOR_FINISHED;
	}
	else {
		if (BLI_listbase_is_empty(&op->reports->list)) {
			BKE_report(op->reports, RPT_WARNING, "Failed, no other selected objects with vertex groups found");
		}

		return OPERATOR_FINISHED; /* to get the chance to make changes in the redo panel */
	}
}

/* transfers weight from active to selected */
void OBJECT_OT_vertex_group_transfer_weight(wmOperatorType *ot)
{
	/* Identifiers.*/
	ot->name = "Transfer Weights";
	ot->idname = "OBJECT_OT_vertex_group_transfer_weight";
	ot->description = "Transfer weight paint to active from selected mesh";

	/* API callbacks.*/
	ot->poll = vertex_group_mesh_supported_poll;
	ot->exec = vertex_group_transfer_weight_exec;

	/* Flags.*/
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* Properties.*/
	/* TODO, use vgroup_operator_subset_select_props for group_select_mode */
	ot->prop = RNA_def_enum(ot->srna, "group_select_mode", WT_vertex_group_mode_item, WT_REPLACE_ALL_VERTEX_GROUPS, "Group", "");
	ot->prop = RNA_def_enum(ot->srna, "method", WT_method_item, WT_BY_NEAREST_FACE, "Method", "");
	ot->prop = RNA_def_enum(ot->srna, "replace_mode", WT_replace_mode_item, WT_REPLACE_ALL_WEIGHTS, "Replace", "");
}

static int set_active_group_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	int nr = RNA_enum_get(op->ptr, "group");

	BLI_assert(nr + 1 >= 0);
	ob->actdef = nr + 1;

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob);

	return OPERATOR_FINISHED;
}

static EnumPropertyItem *vgroup_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{	
	Object *ob = ED_object_context(C);
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	EnumPropertyItem *item = NULL;
	bDeformGroup *def;
	int a, totitem = 0;
	
	if (!ob)
		return DummyRNA_NULL_items;
	
	for (a = 0, def = ob->defbase.first; def; def = def->next, a++) {
		tmp.value = a;
		tmp.icon = ICON_GROUP_VERTEX;
		tmp.identifier = def->name;
		tmp.name = def->name;
		RNA_enum_item_add(&item, &totitem, &tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

void OBJECT_OT_vertex_group_set_active(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Set Active Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_set_active";
	ot->description = "Set the active vertex group";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = set_active_group_exec;
	ot->invoke = WM_menu_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "group", DummyRNA_NULL_items, 0, "Group", "Vertex group to set as active");
	RNA_def_enum_funcs(prop, vgroup_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

/* creates the name_array parameter for vgroup_do_remap, call this before fiddling
 * with the order of vgroups then call vgroup_do_remap after */
static char *vgroup_init_remap(Object *ob)
{
	bDeformGroup *def;
	int defbase_tot = BLI_countlist(&ob->defbase);
	char *name_array = MEM_mallocN(MAX_VGROUP_NAME * sizeof(char) * defbase_tot, "sort vgroups");
	char *name;

	name = name_array;
	for (def = ob->defbase.first; def; def = def->next) {
		BLI_strncpy(name, def->name, MAX_VGROUP_NAME);
		name += MAX_VGROUP_NAME;
	}

	return name_array;
}

static int vgroup_do_remap(Object *ob, const char *name_array, wmOperator *op)
{
	MDeformVert *dvert = NULL;
	bDeformGroup *def;
	int defbase_tot = BLI_countlist(&ob->defbase);

	/* needs a dummy index at the start*/
	int *sort_map_update = MEM_mallocN(sizeof(int) * (defbase_tot + 1), "sort vgroups");
	int *sort_map = sort_map_update + 1;

	const char *name;
	int i;

	name = name_array;
	for (def = ob->defbase.first, i = 0; def; def = def->next, i++) {
		sort_map[i] = BLI_findstringindex(&ob->defbase, name, offsetof(bDeformGroup, name));
		name += MAX_VGROUP_NAME;

		BLI_assert(sort_map[i] != -1);
	}

	if (ob->mode == OB_MODE_EDIT) {
		if (ob->type == OB_MESH) {
			BMEditMesh *em = BKE_editmesh_from_object(ob);
			const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);

			if (cd_dvert_offset != -1) {
				BMIter iter;
				BMVert *eve;

				BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
					dvert = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
					if (dvert->totweight) {
						defvert_remap(dvert, sort_map, defbase_tot);
					}
				}
			}
		}
		else {
			BKE_report(op->reports, RPT_ERROR, "Editmode lattice is not supported yet");
			MEM_freeN(sort_map_update);
			return OPERATOR_CANCELLED;
		}
	}
	else {
		int dvert_tot = 0;

		ED_vgroup_array_get(ob->data, &dvert, &dvert_tot);

		/*create as necessary*/
		if (dvert) {
			while (dvert_tot--) {
				if (dvert->totweight)
					defvert_remap(dvert, sort_map, defbase_tot);
				dvert++;
			}
		}
	}

	/* update users */
	for (i = 0; i < defbase_tot; i++)
		sort_map[i]++;

	sort_map_update[0] = 0;
	vgroup_remap_update_users(ob, sort_map_update);

	BLI_assert(sort_map_update[ob->actdef] >= 0);
	ob->actdef = sort_map_update[ob->actdef];
	
	MEM_freeN(sort_map_update);

	return OPERATOR_FINISHED;
}

static int vgroup_sort_name(void *def_a_ptr, void *def_b_ptr)
{
	bDeformGroup *def_a = (bDeformGroup *)def_a_ptr;
	bDeformGroup *def_b = (bDeformGroup *)def_b_ptr;

	return BLI_natstrcmp(def_a->name, def_b->name);
}

/* Sorts the weight groups according to the bone hierarchy of the
   associated armature (similar to how bones are ordered in the Outliner) */
static void vgroup_sort_bone_hierarchy(Object *ob, ListBase *bonebase)
{
	if (bonebase == NULL) {
		Object *armobj = modifiers_isDeformedByArmature(ob);
		if (armobj != NULL) {
			bArmature *armature = armobj->data;
			bonebase = &armature->bonebase;
		}
	}

	if (bonebase != NULL) {
		Bone *bone;
		for (bone = bonebase->last; bone; bone = bone->prev) {
			bDeformGroup *dg = defgroup_find_name(ob, bone->name);
			vgroup_sort_bone_hierarchy(ob, &bone->childbase);

			if (dg != NULL) {
				BLI_remlink(&ob->defbase, dg);
				BLI_addhead(&ob->defbase, dg);
			}
		}
	}

	return;
}

enum {
	SORT_TYPE_NAME          = 0,
	SORT_TYPE_BONEHIERARCHY = 1
};

static int vertex_group_sort_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	char *name_array;
	int ret;
	int sort_type = RNA_enum_get(op->ptr, "sort_type");

	/*init remapping*/
	name_array = vgroup_init_remap(ob);

	/*sort vgroup names*/
	switch(sort_type) {
		case SORT_TYPE_NAME: 
			BLI_sortlist(&ob->defbase, vgroup_sort_name);
			break;
		case SORT_TYPE_BONEHIERARCHY:
			vgroup_sort_bone_hierarchy(ob, NULL);
			break;
	}
	
	/*remap vgroup data to map to correct names*/
	ret = vgroup_do_remap(ob, name_array, op);

	if (ret != OPERATOR_CANCELLED) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob);
	}

	if (name_array) MEM_freeN(name_array);

	return ret;
}

void OBJECT_OT_vertex_group_sort(wmOperatorType *ot)
{
	static EnumPropertyItem vgroup_sort_type[] = {
			{SORT_TYPE_NAME, "NAME", 0, "Name", ""},
			{SORT_TYPE_BONEHIERARCHY, "BONE_HIERARCHY", 0, "Bone Hierarchy", ""},
			{0, NULL, 0, NULL, NULL}
	};

	ot->name = "Sort Vertex Groups";
	ot->idname = "OBJECT_OT_vertex_group_sort";
	ot->description = "Sort vertex groups";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vertex_group_sort_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "sort_type", vgroup_sort_type, SORT_TYPE_NAME, "Sort type", "Sort type");
}

static int vgroup_move_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	bDeformGroup *def;
	char *name_array;
	int dir = RNA_enum_get(op->ptr, "direction"), ret;

	def = BLI_findlink(&ob->defbase, ob->actdef - 1);
	if (!def) {
		return OPERATOR_CANCELLED;
	}

	name_array = vgroup_init_remap(ob);

	if (dir == 1) { /*up*/
		void *prev = def->prev;

		BLI_remlink(&ob->defbase, def);
		BLI_insertlinkbefore(&ob->defbase, prev, def);
	}
	else { /*down*/
		void *next = def->next;

		BLI_remlink(&ob->defbase, def);
		BLI_insertlinkafter(&ob->defbase, next, def);
	}

	ret = vgroup_do_remap(ob, name_array, op);

	if (name_array) MEM_freeN(name_array);

	if (ret != OPERATOR_CANCELLED) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM | ND_VERTEX_GROUP, ob);
	}

	return ret;
}

void OBJECT_OT_vertex_group_move(wmOperatorType *ot)
{
	static EnumPropertyItem vgroup_slot_move[] = {
		{1, "UP", 0, "Up", ""},
		{-1, "DOWN", 0, "Down", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Move Vertex Group";
	ot->idname = "OBJECT_OT_vertex_group_move";
	ot->description = "Move the active vertex group up/down in the list";

	/* api callbacks */
	ot->poll = vertex_group_poll;
	ot->exec = vgroup_move_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "direction", vgroup_slot_move, 0, "Direction", "Direction to move, UP or DOWN");
}

static void vgroup_copy_active_to_sel_single(Object *ob, const int def_nr)
{
	MDeformVert *dvert_act;

	Mesh *me = ob->data;
	BMEditMesh *em = me->edit_btmesh;
	float weight_act;
	int i;

	if (em) {
		const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
		BMIter iter;
		BMVert *eve, *eve_act;

		dvert_act = ED_mesh_active_dvert_get_em(ob, &eve_act);
		if (dvert_act == NULL) {
			return;
		}
		weight_act = defvert_find_weight(dvert_act, def_nr);

		BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
			if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && (eve != eve_act)) {
				MDeformVert *dv = BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
				MDeformWeight *dw = defvert_find_index(dv, def_nr);
				if (dw) {
					dw->weight = weight_act;

					if (me->editflag & ME_EDIT_MIRROR_X) {
						ED_mesh_defvert_mirror_update_em(ob, eve, -1, i, cd_dvert_offset);
					}
				}
			}
		}

		if (me->editflag & ME_EDIT_MIRROR_X) {
			ED_mesh_defvert_mirror_update_em(ob, eve_act, -1, -1, cd_dvert_offset);
		}
	}
	else {
		MDeformVert *dv;
		int v_act;

		dvert_act = ED_mesh_active_dvert_get_ob(ob, &v_act);
		if (dvert_act == NULL) {
			return;
		}
		weight_act = defvert_find_weight(dvert_act, def_nr);

		dv  = me->dvert;
		for (i = 0; i < me->totvert; i++, dv++) {
			if ((me->mvert[i].flag & SELECT) && (dv != dvert_act)) {
				MDeformWeight *dw = defvert_find_index(dv, def_nr);
				if (dw) {
					dw->weight = weight_act;
					if (me->editflag & ME_EDIT_MIRROR_X) {
						ED_mesh_defvert_mirror_update_ob(ob, -1, i);
					}
				}
			}
		}

		if (me->editflag & ME_EDIT_MIRROR_X) {
			ED_mesh_defvert_mirror_update_ob(ob, -1, v_act);
		}
	}
}

static bool check_vertex_group_accessible(wmOperator *op, Object *ob, int def_nr)
{
	bDeformGroup *dg = BLI_findlink(&ob->defbase, def_nr);

	if (!dg) {
		BKE_report(op->reports, RPT_ERROR, "Invalid vertex group index");
		return false;
	}

	if (dg->flag & DG_LOCK_WEIGHT) {
		BKE_report(op->reports, RPT_ERROR, "Vertex group is locked");
		return false;
	}

	return true;
}

static int vertex_weight_paste_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	const int def_nr = RNA_int_get(op->ptr, "weight_group");

	if (!check_vertex_group_accessible(op, ob, def_nr)) {
		return OPERATOR_CANCELLED;
	}

	vgroup_copy_active_to_sel_single(ob, def_nr);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_weight_paste(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Paste Weight to Selected";
	ot->idname = "OBJECT_OT_vertex_weight_paste";
	ot->description = "Copy this group's weight to other selected verts (disabled if vertex group is locked)";

	/* api callbacks */
	ot->poll = vertex_group_vert_select_mesh_poll;
	ot->exec = vertex_weight_paste_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_int(ot->srna, "weight_group", -1, -1, INT_MAX, "Weight Index",
	                   "Index of source weight in active vertex group", -1, INT_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

static int vertex_weight_delete_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	const int def_nr = RNA_int_get(op->ptr, "weight_group");

	if (!check_vertex_group_accessible(op, ob, def_nr)) {
		return OPERATOR_CANCELLED;
	}

	vgroup_remove_weight(ob, def_nr);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_weight_delete(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Delete Weight";
	ot->idname = "OBJECT_OT_vertex_weight_delete";
	ot->description = "Delete this weight from the vertex (disabled if vertex group is locked)";

	/* api callbacks */
	ot->poll = vertex_group_vert_select_mesh_poll;
	ot->exec = vertex_weight_delete_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_int(ot->srna, "weight_group", -1, -1, INT_MAX, "Weight Index",
	                   "Index of source weight in active vertex group", -1, INT_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

static int vertex_weight_set_active_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_context(C);
	const int wg_index = RNA_int_get(op->ptr, "weight_group");

	if (wg_index != -1) {
		ob->actdef = wg_index + 1;
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_weight_set_active(wmOperatorType *ot)
{
	PropertyRNA *prop;

	ot->name = "Set Active Group";
	ot->idname = "OBJECT_OT_vertex_weight_set_active";
	ot->description = "Set as active vertex group";

	/* api callbacks */
	ot->poll = vertex_group_vert_select_mesh_poll;
	ot->exec = vertex_weight_set_active_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	prop = RNA_def_int(ot->srna, "weight_group", -1, -1, INT_MAX, "Weight Index",
	                   "Index of source weight in active vertex group", -1, INT_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

static int vertex_weight_normalize_active_vertex_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	eVGroupSelect subset_type = ts->vgroupsubset;

	vgroup_normalize_active(ob, subset_type);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_weight_normalize_active_vertex(wmOperatorType *ot)
{

	ot->name = "Normalize Active";
	ot->idname = "OBJECT_OT_vertex_weight_normalize_active_vertex";
	ot->description = "Normalize active vertex's weights";

	/* api callbacks */
	ot->poll = vertex_group_vert_select_mesh_poll;
	ot->exec = vertex_weight_normalize_active_vertex_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_weight_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_context(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	eVGroupSelect subset_type = ts->vgroupsubset;

	vgroup_copy_active_to_sel(ob, subset_type);

	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_weight_copy(wmOperatorType *ot)
{

	ot->name = "Copy Active";
	ot->idname = "OBJECT_OT_vertex_weight_copy";
	ot->description = "Copy weights from active to selected";

	/* api callbacks */
	ot->poll = vertex_group_vert_select_mesh_poll;
	ot->exec = vertex_weight_copy_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
