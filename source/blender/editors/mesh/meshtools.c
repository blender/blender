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
 * The Original Code is Copyright (C) 2004 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/meshtools.c
 *  \ingroup edmesh
 *
 * meshtools.c: no editmode (violated already :), mirror & join),
 * tools operating on meshes
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"


#include "BLI_kdtree.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"
#include "BKE_multires.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

/* * ********************** no editmode!!! *********** */

/*********************** JOIN ***************************/

/* join selected meshes into the active mesh, context sensitive
 * return 0 if no join is made (error) and 1 if the join is done */

int join_mesh_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Material **matar, *ma;
	Mesh *me;
	MVert *mvert, *mv;
	MEdge *medge = NULL;
	MPoly *mpoly = NULL;
	MLoop *mloop = NULL;
	Key *key, *nkey = NULL;
	KeyBlock *kb, *okb, *kbn;
	float imat[4][4], cmat[4][4], *fp1, *fp2;
	int a, b, totcol, totmat = 0, totedge = 0, totvert = 0;
	int totloop = 0, totpoly = 0, vertofs, *matmap = NULL;
	int i, j, index, haskey = 0, edgeofs, loopofs, polyofs;
	bool ok = false;
	bDeformGroup *dg, *odg;
	MDeformVert *dvert;
	CustomData vdata, edata, fdata, ldata, pdata;

	if (scene->obedit) {
		BKE_report(op->reports, RPT_WARNING, "Cannot join while in edit mode");
		return OPERATOR_CANCELLED;
	}
	
	/* ob is the object we are adding geometry to */
	if (!ob || ob->type != OB_MESH) {
		BKE_report(op->reports, RPT_WARNING, "Active object is not a mesh");
		return OPERATOR_CANCELLED;
	}
	
	/* count & check */
	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		if (base->object->type == OB_MESH) {
			me = base->object->data;

			totvert += me->totvert;
			totedge += me->totedge;
			totloop += me->totloop;
			totpoly += me->totpoly;
			totmat += base->object->totcol;
			
			if (base->object == ob)
				ok = true;
			
			/* check for shapekeys */
			if (me->key)
				haskey++;
		}
	}
	CTX_DATA_END;
	
	/* that way the active object is always selected */ 
	if (ok == false) {
		BKE_report(op->reports, RPT_WARNING, "Active object is not a selected mesh");
		return OPERATOR_CANCELLED;
	}
	
	/* only join meshes if there are verts to join, there aren't too many, and we only had one mesh selected */
	me = (Mesh *)ob->data;
	key = me->key;

	if (totvert == 0 || totvert == me->totvert) {
		BKE_report(op->reports, RPT_WARNING, "No mesh data to join");
		return OPERATOR_CANCELLED;
	}
	
	if (totvert > MESH_MAX_VERTS) {
		BKE_reportf(op->reports, RPT_WARNING, "Joining results in %d vertices, limit is %ld", totvert, MESH_MAX_VERTS);
		return OPERATOR_CANCELLED;
	}

	/* remove tessface to ensure we don't hold references to invalid faces */
	BKE_mesh_tessface_clear(me);

	/* new material indices and material array */
	matar = MEM_callocN(sizeof(void *) * totmat, "join_mesh matar");
	if (totmat) matmap = MEM_callocN(sizeof(int) * totmat, "join_mesh matmap");
	totcol = ob->totcol;
	
	/* obact materials in new main array, is nicer start! */
	for (a = 0; a < ob->totcol; a++) {
		matar[a] = give_current_material(ob, a + 1);
		id_us_plus((ID *)matar[a]);
		/* increase id->us : will be lowered later */
	}
	
	/* - if destination mesh had shapekeys, move them somewhere safe, and set up placeholders
	 *  with arrays that are large enough to hold shapekey data for all meshes
	 * -	if destination mesh didn't have shapekeys, but we encountered some in the meshes we're 
	 *	joining, set up a new keyblock and assign to the mesh
	 */
	if (key) {
		/* make a duplicate copy that will only be used here... (must remember to free it!) */
		nkey = BKE_key_copy(key);
		
		/* for all keys in old block, clear data-arrays */
		for (kb = key->block.first; kb; kb = kb->next) {
			if (kb->data) MEM_freeN(kb->data);
			kb->data = MEM_callocN(sizeof(float) * 3 * totvert, "join_shapekey");
			kb->totelem = totvert;
		}
	}
	else if (haskey) {
		/* add a new key-block and add to the mesh */
		key = me->key = BKE_key_add((ID *)me);
		key->type = KEY_RELATIVE;
	}
	
	/* first pass over objects - copying materials and vertexgroups across */
	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		/* only act if a mesh, and not the one we're joining to */
		if ((ob != base->object) && (base->object->type == OB_MESH)) {
			me = base->object->data;
			
			/* Join this object's vertex groups to the base one's */
			for (dg = base->object->defbase.first; dg; dg = dg->next) {
				/* See if this group exists in the object (if it doesn't, add it to the end) */
				if (!defgroup_find_name(ob, dg->name)) {
					odg = MEM_callocN(sizeof(bDeformGroup), "join deformGroup");
					memcpy(odg, dg, sizeof(bDeformGroup));
					BLI_addtail(&ob->defbase, odg);
				}
			}
			if (ob->defbase.first && ob->actdef == 0)
				ob->actdef = 1;
			
			
			if (me->totvert) {
				/* Add this object's materials to the base one's if they don't exist already (but only if limits not exceeded yet) */
				if (totcol < MAXMAT) {
					for (a = 1; a <= base->object->totcol; a++) {
						ma = give_current_material(base->object, a);

						for (b = 0; b < totcol; b++) {
							if (ma == matar[b]) break;
						}
						if (b == totcol) {
							matar[b] = ma;
							if (ma) {
								id_us_plus(&ma->id);
							}
							totcol++;
						}
						if (totcol >= MAXMAT)
							break;
					}
				}
				
				/* if this mesh has shapekeys, check if destination mesh already has matching entries too */
				if (me->key && key) {
					/* for remapping KeyBlock.relative */
					int      *index_map = MEM_mallocN(sizeof(int)        * me->key->totkey, __func__);
					KeyBlock **kb_map   = MEM_mallocN(sizeof(KeyBlock *) * me->key->totkey, __func__);

					for (kb = me->key->block.first, i = 0; kb; kb = kb->next, i++) {
						BLI_assert(i < me->key->totkey);

						kbn = BKE_keyblock_find_name(key, kb->name);
						/* if key doesn't exist in destination mesh, add it */
						if (kbn) {
							index_map[i] = BLI_findindex(&key->block, kbn);
						}
						else {
							index_map[i] = key->totkey;

							kbn = BKE_keyblock_add(key, kb->name);

							BKE_keyblock_copy_settings(kbn, kb);

							/* adjust settings to fit (allocate a new data-array) */
							kbn->data = MEM_callocN(sizeof(float) * 3 * totvert, "joined_shapekey");
							kbn->totelem = totvert;
		
							/* XXX 2.5 Animato */
#if 0
							/* also, copy corresponding ipo-curve to ipo-block if applicable */
							if (me->key->ipo && key->ipo) {
								/* FIXME... this is a luxury item! */
								puts("FIXME: ignoring IPO's when joining shapekeys on Meshes for now...");
							}
#endif
						}

						kb_map[i] = kbn;
					}

					/* remap relative index values */
					for (kb = me->key->block.first, i = 0; kb; kb = kb->next, i++) {
						if (LIKELY(kb->relative < me->key->totkey)) {  /* sanity check, should always be true */
							kb_map[i]->relative = index_map[kb->relative];
						}
					}

					MEM_freeN(index_map);
					MEM_freeN(kb_map);
				}
			}
		}
	}
	CTX_DATA_END;


	/* setup new data for destination mesh */
	CustomData_reset(&vdata);
	CustomData_reset(&edata);
	CustomData_reset(&fdata);
	CustomData_reset(&ldata);
	CustomData_reset(&pdata);

	mvert = CustomData_add_layer(&vdata, CD_MVERT, CD_CALLOC, NULL, totvert);
	medge = CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);
	mloop = CustomData_add_layer(&ldata, CD_MLOOP, CD_CALLOC, NULL, totloop);
	mpoly = CustomData_add_layer(&pdata, CD_MPOLY, CD_CALLOC, NULL, totpoly);

	vertofs = 0;
	edgeofs = 0;
	loopofs = 0;
	polyofs = 0;
	
	/* inverse transform for all selected meshes in this object */
	invert_m4_m4(imat, ob->obmat);
	
	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		/* only join if this is a mesh */
		if (base->object->type == OB_MESH) {
			me = base->object->data;
			
			if (me->totvert) {

				/* merge customdata flag */
				((Mesh *)ob->data)->cd_flag |= me->cd_flag;

				/* standard data */
				CustomData_merge(&me->vdata, &vdata, CD_MASK_MESH, CD_DEFAULT, totvert);
				CustomData_copy_data_named(&me->vdata, &vdata, 0, vertofs, me->totvert);
				
				/* vertex groups */
				dvert = CustomData_get(&vdata, vertofs, CD_MDEFORMVERT);
				
				/* NB: vertex groups here are new version */
				if (dvert) {
					for (i = 0; i < me->totvert; i++) {
						for (j = 0; j < dvert[i].totweight; j++) {
							/*	Find the old vertex group */
							odg = BLI_findlink(&base->object->defbase, dvert[i].dw[j].def_nr);
							if (odg) {
								/*	Search for a match in the new object, and set new index */
								for (dg = ob->defbase.first, index = 0; dg; dg = dg->next, index++) {
									if (!strcmp(dg->name, odg->name)) {
										dvert[i].dw[j].def_nr = index;
										break;
									}
								}
							}
						}
					}
				}
				
				/* if this is the object we're merging into, no need to do anything */
				if (base->object != ob) {
					/* watch this: switch matmul order really goes wrong */
					mul_m4_m4m4(cmat, imat, base->object->obmat);
					
					/* transform vertex coordinates into new space */
					for (a = 0, mv = mvert; a < me->totvert; a++, mv++) {
						mul_m4_v3(cmat, mv->co);
					}
					
					/* for each shapekey in destination mesh:
					 *	- if there's a matching one, copy it across (will need to transform vertices into new space...)
					 *	- otherwise, just copy own coordinates of mesh (no need to transform vertex coordinates into new space)
					 */
					if (key) {
						/* if this mesh has any shapekeys, check first, otherwise just copy coordinates */
						for (kb = key->block.first; kb; kb = kb->next) {
							/* get pointer to where to write data for this mesh in shapekey's data array */
							fp1 = ((float *)kb->data) + (vertofs * 3);
							
							/* check if this mesh has such a shapekey */
							okb = me->key ? BKE_keyblock_find_name(me->key, kb->name) : NULL;

							if (okb) {
								/* copy this mesh's shapekey to the destination shapekey (need to transform first) */
								fp2 = ((float *)(okb->data));
								for (a = 0; a < me->totvert; a++, fp1 += 3, fp2 += 3) {
									copy_v3_v3(fp1, fp2);
									mul_m4_v3(cmat, fp1);
								}
							}
							else {
								/* copy this mesh's vertex coordinates to the destination shapekey */
								mv = mvert;
								for (a = 0; a < me->totvert; a++, fp1 += 3, mv++) {
									copy_v3_v3(fp1, mv->co);
								}
							}
						}
					}
				}
				else {
					/* for each shapekey in destination mesh:
					 *	- if it was an 'original', copy the appropriate data from nkey
					 *	- otherwise, copy across plain coordinates (no need to transform coordinates)
					 */
					if (key) {
						for (kb = key->block.first; kb; kb = kb->next) {
							/* get pointer to where to write data for this mesh in shapekey's data array */
							fp1 = ((float *)kb->data) + (vertofs * 3);
							
							/* check if this was one of the original shapekeys */
							okb = nkey ? BKE_keyblock_find_name(nkey, kb->name) : NULL;
							if (okb) {
								/* copy this mesh's shapekey to the destination shapekey */
								fp2 = ((float *)(okb->data));
								for (a = 0; a < me->totvert; a++, fp1 += 3, fp2 += 3) {
									copy_v3_v3(fp1, fp2);
								}
							}
							else {
								/* copy base-coordinates to the destination shapekey */
								mv = mvert;
								for (a = 0; a < me->totvert; a++, fp1 += 3, mv++) {
									copy_v3_v3(fp1, mv->co);
								}
							}
						}
					}
				}
				
				/* advance mvert pointer to end of base mesh's data */
				mvert += me->totvert;
			}
			
			if (me->totedge) {
				CustomData_merge(&me->edata, &edata, CD_MASK_MESH, CD_DEFAULT, totedge);
				CustomData_copy_data_named(&me->edata, &edata, 0, edgeofs, me->totedge);
				
				for (a = 0; a < me->totedge; a++, medge++) {
					medge->v1 += vertofs;
					medge->v2 += vertofs;
				}
			}

			if (me->totloop) {
				if (base->object != ob) {
					MultiresModifierData *mmd;

					multiresModifier_prepare_join(scene, base->object, ob);

					if ((mmd = get_multires_modifier(scene, base->object, true))) {
						ED_object_iter_other(bmain, base->object, true,
						                     ED_object_multires_update_totlevels_cb,
						                     &mmd->totlvl);
					}
				}
				
				CustomData_merge(&me->ldata, &ldata, CD_MASK_MESH, CD_DEFAULT, totloop);
				CustomData_copy_data_named(&me->ldata, &ldata, 0, loopofs, me->totloop);
				
				for (a = 0; a < me->totloop; a++, mloop++) {
					mloop->v += vertofs;
					mloop->e += edgeofs;
				}
			}
			
			if (me->totpoly) {
				if (totmat) {
					/* make mapping for materials */
					for (a = 1; a <= base->object->totcol; a++) {
						ma = give_current_material(base->object, a);

						for (b = 0; b < totcol; b++) {
							if (ma == matar[b]) {
								matmap[a - 1] = b;
								break;
							}
						}
					}
				}

				CustomData_merge(&me->pdata, &pdata, CD_MASK_MESH, CD_DEFAULT, totpoly);
				CustomData_copy_data_named(&me->pdata, &pdata, 0, polyofs, me->totpoly);
				
				for (a = 0; a < me->totpoly; a++, mpoly++) {
					mpoly->loopstart += loopofs;
					mpoly->mat_nr = matmap ? matmap[(int)mpoly->mat_nr] : 0;
				}
				
				polyofs += me->totpoly;
			}

			/* these are used for relinking (cannot be set earlier, 
			 * or else reattaching goes wrong)
			 */
			vertofs += me->totvert;
			edgeofs += me->totedge;
			loopofs += me->totloop;
			
			/* free base, now that data is merged */
			if (base->object != ob)
				ED_base_object_free_and_unlink(bmain, scene, base);
		}
	}
	CTX_DATA_END;
	
	/* return to mesh we're merging to */
	me = ob->data;
	
	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->ldata, me->totloop);
	CustomData_free(&me->pdata, me->totpoly);

	me->totvert = totvert;
	me->totedge = totedge;
	me->totloop = totloop;
	me->totpoly = totpoly;

	me->vdata = vdata;
	me->edata = edata;
	me->ldata = ldata;
	me->pdata = pdata;

	/* tessface data removed above, no need to update */
	BKE_mesh_update_customdata_pointers(me, false);

	/* update normals in case objects with non-uniform scale are joined */
	BKE_mesh_calc_normals(me);
	
	/* old material array */
	for (a = 1; a <= ob->totcol; a++) {
		ma = ob->mat[a - 1];
		if (ma) ma->id.us--;
	}
	for (a = 1; a <= me->totcol; a++) {
		ma = me->mat[a - 1];
		if (ma) ma->id.us--;
	}
	if (ob->mat) MEM_freeN(ob->mat);
	if (ob->matbits) MEM_freeN(ob->matbits);
	if (me->mat) MEM_freeN(me->mat);
	ob->mat = me->mat = NULL;
	ob->matbits = NULL;
	
	if (totcol) {
		me->mat = matar;
		ob->mat = MEM_callocN(sizeof(void *) * totcol, "join obmatar");
		ob->matbits = MEM_callocN(sizeof(char) * totcol, "join obmatbits");
	}
	else
		MEM_freeN(matar);
	
	ob->totcol = me->totcol = totcol;

	if (matmap) MEM_freeN(matmap);
	
	/* other mesh users */
	test_object_materials(bmain, (ID *)me);
	
	/* free temp copy of destination shapekeys (if applicable) */
	if (nkey) {
		/* XXX 2.5 Animato */
#if 0
		/* free it's ipo too - both are not actually freed from memory yet as ID-blocks */
		if (nkey->ipo) {
			BKE_ipo_free(nkey->ipo);
			BLI_remlink(&bmain->ipo, nkey->ipo);
			MEM_freeN(nkey->ipo);
		}
#endif
		
		BKE_key_free(nkey);
		BLI_remlink(&bmain->key, nkey);
		MEM_freeN(nkey);
	}
	
	/* ensure newly inserted keys are time sorted */
	if (key && (key->type != KEY_RELATIVE)) {
		BKE_key_sort(key);
	}


	DAG_relations_tag_update(bmain);   // removed objects, need to rebuild dag

#if 0
	ED_object_editmode_enter(C, EM_WAITCURSOR);
	ED_object_editmode_exit(C, EM_FREEDATA | EM_WAITCURSOR | EM_DO_UNDO);
#else
	/* toggle editmode using lower level functions so this can be called from python */
	EDBM_mesh_make(scene->toolsettings, ob);
	EDBM_mesh_load(ob);
	EDBM_mesh_free(me->edit_btmesh);
	MEM_freeN(me->edit_btmesh);
	me->edit_btmesh = NULL;
	DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA);
#endif
	WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

	return OPERATOR_FINISHED;
}

/*********************** JOIN AS SHAPES ***************************/

/* Append selected meshes vertex locations as shapes of the active mesh, 
 * return 0 if no join is made (error) and 1 of the join is done */

int join_mesh_shapes_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Mesh *me = (Mesh *)ob->data;
	Mesh *selme = NULL;
	DerivedMesh *dm = NULL;
	Key *key = me->key;
	KeyBlock *kb;
	bool ok = false, nonequal_verts = false;
	
	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		if (base->object == ob) continue;
		
		if (base->object->type == OB_MESH) {
			selme = (Mesh *)base->object->data;
			
			if (selme->totvert == me->totvert)
				ok = true;
			else
				nonequal_verts = 1;
		}
	}
	CTX_DATA_END;
	
	if (!ok) {
		if (nonequal_verts)
			BKE_report(op->reports, RPT_WARNING, "Selected meshes must have equal numbers of vertices");
		else
			BKE_report(op->reports, RPT_WARNING, "No additional selected meshes with equal vertex count to join");
		return OPERATOR_CANCELLED;
	}
	
	if (key == NULL) {
		key = me->key = BKE_key_add((ID *)me);
		key->type = KEY_RELATIVE;

		/* first key added, so it was the basis. initialize it with the existing mesh */
		kb = BKE_keyblock_add(key, NULL);
		BKE_keyblock_convert_from_mesh(me, kb);
	}
	
	/* now ready to add new keys from selected meshes */
	CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases)
	{
		if (base->object == ob) continue;
		
		if (base->object->type == OB_MESH) {
			selme = (Mesh *)base->object->data;
			
			if (selme->totvert == me->totvert) {
				dm = mesh_get_derived_deform(scene, base->object, CD_MASK_BAREMESH);
				
				if (!dm) continue;
					
				kb = BKE_keyblock_add(key, base->object->id.name + 2);
				
				DM_to_meshkey(dm, me, kb);
				
				dm->release(dm);
			}
		}
	}
	CTX_DATA_END;
	
	WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
	
	return OPERATOR_FINISHED;
}

/* -------------------------------------------------------------------- */
/* Mesh Mirror (Spatial) */

/** \name Mesh Spatial Mirror API
 * \{ */

#define KD_THRESH      0.00002f

static struct { void *tree; } MirrKdStore = {NULL};

/* mode is 's' start, or 'e' end, or 'u' use */
/* if end, ob can be NULL */
int ED_mesh_mirror_spatial_table(Object *ob, BMEditMesh *em, const float co[3], char mode)
{
	if (mode == 'u') {        /* use table */
		if (MirrKdStore.tree == NULL)
			ED_mesh_mirror_spatial_table(ob, em, NULL, 's');

		if (MirrKdStore.tree) {
			KDTreeNearest nearest;

			int i;

			i = BLI_kdtree_find_nearest(MirrKdStore.tree, co, &nearest);

			if (i != -1) {
				if (nearest.dist < KD_THRESH) {
					return i;
				}
			}
		}
		return -1;
	}
	else if (mode == 's') {   /* start table */
		Mesh *me = ob->data;
		int totvert;

		if (MirrKdStore.tree) /* happens when entering this call without ending it */
			ED_mesh_mirror_spatial_table(ob, em, co, 'e');

		if (em && me->edit_btmesh == em) {
			totvert = em->bm->totvert;
		}
		else {
			totvert = me->totvert;
		}

		MirrKdStore.tree = BLI_kdtree_new(totvert);

		if (em && me->edit_btmesh == em) {

			BMVert *eve;
			BMIter iter;
			int i;

			/* this needs to be valid for index lookups later (callers need) */
			BM_mesh_elem_table_ensure(em->bm, BM_VERT);

			BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
				BLI_kdtree_insert(MirrKdStore.tree, i, eve->co);
			}
		}
		else {
			MVert *mvert;
			int i;
			
			for (i = 0, mvert = me->mvert; i < me->totvert; i++, mvert++) {
				BLI_kdtree_insert(MirrKdStore.tree, i, mvert->co);
			}
		}

		BLI_kdtree_balance(MirrKdStore.tree);
	}
	else if (mode == 'e') { /* end table */
		if (MirrKdStore.tree) {
			BLI_kdtree_free(MirrKdStore.tree);
			MirrKdStore.tree = NULL;
		}
	}
	else {
		BLI_assert(0);
	}

	return 0;
}

/** \} */


/* -------------------------------------------------------------------- */
/* Mesh Mirror (Topology) */

/** \name Mesh Topology Mirror API
 * \{ */

static MirrTopoStore_t mesh_topo_store = {NULL, -1. - 1, -1};

/* mode is 's' start, or 'e' end, or 'u' use */
/* if end, ob can be NULL */
/* note, is supposed return -1 on error, which callers are currently checking for, but is not used so far */
int ED_mesh_mirror_topo_table(Object *ob, char mode)
{
	if (mode == 'u') {        /* use table */
		if (ED_mesh_mirrtopo_recalc_check(ob->data, ob->mode, &mesh_topo_store)) {
			ED_mesh_mirror_topo_table(ob, 's');
		}
	}
	else if (mode == 's') { /* start table */
		ED_mesh_mirrtopo_init(ob->data, ob->mode, &mesh_topo_store, false);
	}
	else if (mode == 'e') { /* end table */
		ED_mesh_mirrtopo_free(&mesh_topo_store);
	}
	else {
		BLI_assert(0);
	}

	return 0;
}

/** \} */


static int mesh_get_x_mirror_vert_spatial(Object *ob, int index)
{
	Mesh *me = ob->data;
	MVert *mvert;
	float vec[3];
	
	mvert = me->mvert + index;
	vec[0] = -mvert->co[0];
	vec[1] = mvert->co[1];
	vec[2] = mvert->co[2];
	
	return ED_mesh_mirror_spatial_table(ob, NULL, vec, 'u');
}

static int mesh_get_x_mirror_vert_topo(Object *ob, int index)
{
	if (ED_mesh_mirror_topo_table(ob, 'u') == -1)
		return -1;

	return mesh_topo_store.index_lookup[index];
}

int mesh_get_x_mirror_vert(Object *ob, int index, const bool use_topology)
{
	if (use_topology) {
		return mesh_get_x_mirror_vert_topo(ob, index);
	}
	else {
		return mesh_get_x_mirror_vert_spatial(ob, index);
	}
}

static BMVert *editbmesh_get_x_mirror_vert_spatial(Object *ob, BMEditMesh *em, const float co[3])
{
	float vec[3];
	int i;
	
	/* ignore nan verts */
	if ((finite(co[0]) == false) ||
	    (finite(co[1]) == false) ||
	    (finite(co[2]) == false))
	{
		return NULL;
	}
	
	vec[0] = -co[0];
	vec[1] = co[1];
	vec[2] = co[2];
	
	i = ED_mesh_mirror_spatial_table(ob, em, vec, 'u');
	if (i != -1) {
		return BM_vert_at_index(em->bm, i);
	}
	return NULL;
}

static BMVert *editbmesh_get_x_mirror_vert_topo(Object *ob, struct BMEditMesh *em, BMVert *eve, int index)
{
	intptr_t poinval;
	if (ED_mesh_mirror_topo_table(ob, 'u') == -1)
		return NULL;

	if (index == -1) {
		BMIter iter;
		BMVert *v;
		
		index = 0;
		BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (v == eve)
				break;
			index++;
		}
		
		if (index == em->bm->totvert) {
			return NULL;
		}
	}

	poinval = mesh_topo_store.index_lookup[index];

	if (poinval != -1)
		return (BMVert *)(poinval);
	return NULL;
}	

BMVert *editbmesh_get_x_mirror_vert(Object *ob, struct BMEditMesh *em, BMVert *eve, const float co[3], int index, const bool use_topology)
{
	if (use_topology) {
		return editbmesh_get_x_mirror_vert_topo(ob, em, eve, index);
	}
	else {
		return editbmesh_get_x_mirror_vert_spatial(ob, em, co);
	}
}

/**
 * Wrapper for objectmode/editmode.
 *
 * call #BM_mesh_elem_table_ensure first for editmesh.
 */
int ED_mesh_mirror_get_vert(Object *ob, int index)
{
	Mesh *me = ob->data;
	BMEditMesh *em = me->edit_btmesh;
	bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;
	int index_mirr;

	if (em) {
		BMVert *eve, *eve_mirr;
		eve = BM_vert_at_index(em->bm, index);
		eve_mirr = editbmesh_get_x_mirror_vert(ob, em, eve, eve->co, index, use_topology);
		index_mirr = eve_mirr ? BM_elem_index_get(eve_mirr) : -1;
	}
	else {
		 index_mirr = mesh_get_x_mirror_vert(ob, index, use_topology);
	}

	return index_mirr;
}

#if 0

static float *editmesh_get_mirror_uv(BMEditMesh *em, int axis, float *uv, float *mirrCent, float *face_cent)
{
	float vec[2];
	float cent_vec[2];
	float cent[2];

	/* ignore nan verts */
	if (isnan(uv[0]) || !finite(uv[0]) ||
	    isnan(uv[1]) || !finite(uv[1])
	    )
		return NULL;

	if (axis) {
		vec[0] = uv[0];
		vec[1] = -((uv[1]) - mirrCent[1]) + mirrCent[1];

		cent_vec[0] = face_cent[0];
		cent_vec[1] = -((face_cent[1]) - mirrCent[1]) + mirrCent[1];
	}
	else {
		vec[0] = -((uv[0]) - mirrCent[0]) + mirrCent[0];
		vec[1] = uv[1];

		cent_vec[0] = -((face_cent[0]) - mirrCent[0]) + mirrCent[0];
		cent_vec[1] = face_cent[1];
	}

	/* TODO - Optimize */
	{
		BMIter iter;
		BMFace *efa;
		
		BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
			uv_poly_center(efa, cent, cd_loop_uv_offset);
			
			if ( (fabsf(cent[0] - cent_vec[0]) < 0.001f) && (fabsf(cent[1] - cent_vec[1]) < 0.001f) ) {
				BMIter liter;
				BMLoop *l;
				
				BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
					MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					if ( (fabsf(luv->uv[0] - vec[0]) < 0.001f) && (fabsf(luv->uv[1] - vec[1]) < 0.001f) ) {
						return luv->uv;
								
					}
				}
			}
		}
	}

	return NULL;
}

#endif

static unsigned int mirror_facehash(const void *ptr)
{
	const MFace *mf = ptr;
	unsigned int v0, v1;

	if (mf->v4) {
		v0 = MIN4(mf->v1, mf->v2, mf->v3, mf->v4);
		v1 = MAX4(mf->v1, mf->v2, mf->v3, mf->v4);
	}
	else {
		v0 = MIN3(mf->v1, mf->v2, mf->v3);
		v1 = MAX3(mf->v1, mf->v2, mf->v3);
	}

	return ((v0 * 39) ^ (v1 * 31));
}

static int mirror_facerotation(MFace *a, MFace *b)
{
	if (b->v4) {
		if (a->v1 == b->v1 && a->v2 == b->v2 && a->v3 == b->v3 && a->v4 == b->v4)
			return 0;
		else if (a->v4 == b->v1 && a->v1 == b->v2 && a->v2 == b->v3 && a->v3 == b->v4)
			return 1;
		else if (a->v3 == b->v1 && a->v4 == b->v2 && a->v1 == b->v3 && a->v2 == b->v4)
			return 2;
		else if (a->v2 == b->v1 && a->v3 == b->v2 && a->v4 == b->v3 && a->v1 == b->v4)
			return 3;
	}
	else {
		if (a->v1 == b->v1 && a->v2 == b->v2 && a->v3 == b->v3)
			return 0;
		else if (a->v3 == b->v1 && a->v1 == b->v2 && a->v2 == b->v3)
			return 1;
		else if (a->v2 == b->v1 && a->v3 == b->v2 && a->v1 == b->v3)
			return 2;
	}
	
	return -1;
}

static bool mirror_facecmp(const void *a, const void *b)
{
	return (mirror_facerotation((MFace *)a, (MFace *)b) == -1);
}

/* BMESH_TODO, convert to MPoly (functions above also) */
int *mesh_get_x_mirror_faces(Object *ob, BMEditMesh *em)
{
	Mesh *me = ob->data;
	MVert *mv, *mvert = me->mvert;
	MFace mirrormf, *mf, *hashmf, *mface = me->mface;
	GHash *fhash;
	const bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;
	int *mirrorverts, *mirrorfaces;
	int a;

	mirrorverts = MEM_callocN(sizeof(int) * me->totvert, "MirrorVerts");
	mirrorfaces = MEM_callocN(sizeof(int) * 2 * me->totface, "MirrorFaces");

	ED_mesh_mirror_spatial_table(ob, em, NULL, 's');

	for (a = 0, mv = mvert; a < me->totvert; a++, mv++)
		mirrorverts[a] = mesh_get_x_mirror_vert(ob, a, use_topology);

	ED_mesh_mirror_spatial_table(ob, em, NULL, 'e');

	fhash = BLI_ghash_new_ex(mirror_facehash, mirror_facecmp, "mirror_facehash gh", me->totface);
	for (a = 0, mf = mface; a < me->totface; a++, mf++)
		BLI_ghash_insert(fhash, mf, mf);

	for (a = 0, mf = mface; a < me->totface; a++, mf++) {
		mirrormf.v1 = mirrorverts[mf->v3];
		mirrormf.v2 = mirrorverts[mf->v2];
		mirrormf.v3 = mirrorverts[mf->v1];
		mirrormf.v4 = (mf->v4) ? mirrorverts[mf->v4] : 0;

		/* make sure v4 is not 0 if a quad */
		if (mf->v4 && mirrormf.v4 == 0) {
			SWAP(unsigned int, mirrormf.v1, mirrormf.v3);
			SWAP(unsigned int, mirrormf.v2, mirrormf.v4);
		}

		hashmf = BLI_ghash_lookup(fhash, &mirrormf);
		if (hashmf) {
			mirrorfaces[a * 2] = hashmf - mface;
			mirrorfaces[a * 2 + 1] = mirror_facerotation(&mirrormf, hashmf);
		}
		else
			mirrorfaces[a * 2] = -1;
	}

	BLI_ghash_free(fhash, NULL, NULL);
	MEM_freeN(mirrorverts);
	
	return mirrorfaces;
}

/* selection, vertex and face */
/* returns 0 if not found, otherwise 1 */

/**
 * Face selection in object mode,
 * currently only weight-paint and vertex-paint use this.
 *
 * \return boolean true == Found
 */
bool ED_mesh_pick_face(bContext *C, Object *ob, const int mval[2], unsigned int *index, int size)
{
	ViewContext vc;
	Mesh *me = ob->data;

	BLI_assert(me && GS(me->id.name) == ID_ME);

	if (!me || me->totpoly == 0)
		return false;

	view3d_set_viewcontext(C, &vc);

	if (size) {
		/* sample rect to increase chances of selecting, so that when clicking
		 * on an edge in the backbuf, we can still select a face */

		float dummy_dist;
		*index = view3d_sample_backbuf_rect(&vc, mval, size, 1, me->totpoly + 1, &dummy_dist, 0, NULL, NULL);
	}
	else {
		/* sample only on the exact position */
		*index = view3d_sample_backbuf(&vc, mval[0], mval[1]);
	}

	if ((*index) == 0 || (*index) > (unsigned int)me->totpoly)
		return false;

	(*index)--;

	return true;
}
static void ed_mesh_pick_face_vert__mpoly_find(
        /* context */
        struct ARegion *ar, const float mval[2],
        /* mesh data */
        DerivedMesh *dm, MPoly *mp, MLoop *mloop,
        /* return values */
        float *r_len_best, int *r_v_idx_best)
{
	const MLoop *ml;
	int j = mp->totloop;
	for (ml = &mloop[mp->loopstart]; j--; ml++) {
		float co[3], sco[2], len;
		const int v_idx = ml->v;
		dm->getVertCo(dm, v_idx, co);
		if (ED_view3d_project_float_object(ar, co, sco, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
			len = len_manhattan_v2v2(mval, sco);
			if (len < *r_len_best) {
				*r_len_best = len;
				*r_v_idx_best = v_idx;
			}
		}
	}
}
/**
 * Use when the back buffer stores face index values. but we want a vert.
 * This gets the face then finds the closest vertex to mval.
 */
bool ED_mesh_pick_face_vert(bContext *C, Object *ob, const int mval[2], unsigned int *index, int size)
{
	unsigned int poly_index;
	Mesh *me = ob->data;

	BLI_assert(me && GS(me->id.name) == ID_ME);

	if (ED_mesh_pick_face(C, ob, mval, &poly_index, size)) {
		Scene *scene = CTX_data_scene(C);
		struct ARegion *ar = CTX_wm_region(C);

		/* derived mesh to find deformed locations */
		DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH | CD_MASK_ORIGINDEX);

		int v_idx_best = ORIGINDEX_NONE;

		/* find the vert closest to 'mval' */
		const float mval_f[2] = {UNPACK2(mval)};
		float len_best = FLT_MAX;

		MPoly *dm_mpoly;
		MLoop *dm_mloop;
		unsigned int dm_mpoly_tot;
		const int *index_mp_to_orig;

		dm_mpoly = dm->getPolyArray(dm);
		dm_mloop = dm->getLoopArray(dm);

		dm_mpoly_tot = dm->getNumPolys(dm);

		index_mp_to_orig = dm->getPolyDataArray(dm, CD_ORIGINDEX);

		/* tag all verts using this face */
		if (index_mp_to_orig) {
			unsigned int i;

			for (i = 0; i < dm_mpoly_tot; i++) {
				if (index_mp_to_orig[i] == poly_index) {
					ed_mesh_pick_face_vert__mpoly_find(
					        ar, mval_f,
					        dm, &dm_mpoly[i], dm_mloop,
					        &len_best, &v_idx_best);
				}
			}
		}
		else {
			if (poly_index < dm_mpoly_tot) {
				ed_mesh_pick_face_vert__mpoly_find(
				        ar, mval_f,
				        dm, &dm_mpoly[poly_index], dm_mloop,
				        &len_best, &v_idx_best);
			}
		}

		/* map 'dm -> me' index if possible */
		if (v_idx_best != ORIGINDEX_NONE) {
			const int *index_mv_to_orig;

			index_mv_to_orig = dm->getVertDataArray(dm, CD_ORIGINDEX);
			if (index_mv_to_orig) {
				v_idx_best = index_mv_to_orig[v_idx_best];
			}
		}

		dm->release(dm);

		if ((v_idx_best != ORIGINDEX_NONE) && (v_idx_best < me->totvert)) {
			*index = v_idx_best;
			return true;
		}
	}

	return false;
}

/**
 * Vertex selection in object mode,
 * currently only weight paint uses this.
 *
 * \return boolean true == Found
 */
typedef struct VertPickData {
	const MVert *mvert;
	const float *mval_f;  /* [2] */
	ARegion *ar;

	/* runtime */
	float len_best;
	int v_idx_best;
} VertPickData;

static void ed_mesh_pick_vert__mapFunc(void *userData, int index, const float co[3],
                                       const float UNUSED(no_f[3]), const short UNUSED(no_s[3]))
{
	VertPickData *data = userData;
	if ((data->mvert[index].flag & ME_HIDE) == 0) {
		float sco[2];

		if (ED_view3d_project_float_object(data->ar, co, sco, V3D_PROJ_TEST_CLIP_DEFAULT) == V3D_PROJ_RET_OK) {
			const float len = len_manhattan_v2v2(data->mval_f, sco);
			if (len < data->len_best) {
				data->len_best = len;
				data->v_idx_best = index;
			}
		}
	}
}
bool ED_mesh_pick_vert(bContext *C, Object *ob, const int mval[2], unsigned int *index, int size, bool use_zbuf)
{
	ViewContext vc;
	Mesh *me = ob->data;

	BLI_assert(me && GS(me->id.name) == ID_ME);

	if (!me || me->totvert == 0)
		return false;

	view3d_set_viewcontext(C, &vc);

	if (use_zbuf) {
		if (size > 0) {
			/* sample rect to increase chances of selecting, so that when clicking
			 * on an face in the backbuf, we can still select a vert */

			float dummy_dist;
			*index = view3d_sample_backbuf_rect(&vc, mval, size, 1, me->totvert + 1, &dummy_dist, 0, NULL, NULL);
		}
		else {
			/* sample only on the exact position */
			*index = view3d_sample_backbuf(&vc, mval[0], mval[1]);
		}

		if ((*index) == 0 || (*index) > (unsigned int)me->totvert)
			return false;

		(*index)--;
	}
	else {
		/* derived mesh to find deformed locations */
		DerivedMesh *dm = mesh_get_derived_final(vc.scene, ob, CD_MASK_BAREMESH);
		ARegion *ar = vc.ar;
		RegionView3D *rv3d = ar->regiondata;

		/* find the vert closest to 'mval' */
		const float mval_f[2] = {(float)mval[0],
		                         (float)mval[1]};

		VertPickData data = {NULL};

		ED_view3d_init_mats_rv3d(ob, rv3d);

		if (dm == NULL) {
			return false;
		}

		/* setup data */
		data.mvert = me->mvert;
		data.ar = ar;
		data.mval_f = mval_f;
		data.len_best = FLT_MAX;
		data.v_idx_best = -1;

		dm->foreachMappedVert(dm, ed_mesh_pick_vert__mapFunc, &data, DM_FOREACH_NOP);

		dm->release(dm);

		if (data.v_idx_best == -1) {
			return false;
		}

		*index = data.v_idx_best;
	}

	return true;
}


MDeformVert *ED_mesh_active_dvert_get_em(Object *ob, BMVert **r_eve)
{
	if (ob->mode & OB_MODE_EDIT && ob->type == OB_MESH && ob->defbase.first) {
		Mesh *me = ob->data;
		BMesh *bm = me->edit_btmesh->bm;
		const int cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);

		if (cd_dvert_offset != -1) {
			BMVert *eve = BM_mesh_active_vert_get(bm);

			if (eve) {
				if (r_eve) *r_eve = eve;
				return BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
			}
		}
	}

	if (r_eve) *r_eve = NULL;
	return NULL;
}

MDeformVert *ED_mesh_active_dvert_get_ob(Object *ob, int *r_index)
{
	Mesh *me = ob->data;
	int index = BKE_mesh_mselect_active_get(me, ME_VSEL);
	if (r_index) *r_index = index;
	if (index == -1 || me->dvert == NULL) {
		return NULL;
	}
	else {
		return me->dvert + index;
	}
}

MDeformVert *ED_mesh_active_dvert_get_only(Object *ob)
{
	if (ob->type == OB_MESH) {
		if (ob->mode & OB_MODE_EDIT) {
			return ED_mesh_active_dvert_get_em(ob, NULL);
		}
		else {
			return ED_mesh_active_dvert_get_ob(ob, NULL);
		}
	}
	else {
		return NULL;
	}
}
