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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_snap.c
 *  \ingroup spview3d
 */


#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_tessmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_scene.h"
#include "BKE_tracking.h"

#include "WM_api.h"
#include "WM_types.h"



#include "ED_armature.h"
#include "ED_mesh.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_curve.h" /* for curve_editnurbs */

#include "view3d_intern.h"

extern float originmat[3][3];   /* XXX object.c */

/* ************************************************** */
/* ********************* old transform stuff ******** */
/* *********** will get replaced with new transform * */
/* ************************************************** */

typedef struct TransVert {
	float *loc;
	float oldloc[3], maploc[3], fac;
	float *val, oldval;
	int flag;
	float *nor;
} TransVert;

              /* SELECT == (1 << 0) */
#define TX_VERT_USE_MAPLOC (1 << 1)

static TransVert *transvmain = NULL;
static int tottrans = 0;

/* copied from editobject.c, now uses (almost) proper depgraph */
static void special_transvert_update(Object *obedit)
{
	if (obedit) {
		DAG_id_tag_update(obedit->data, 0);
		
		if (obedit->type == OB_MESH) {
			Mesh *me = obedit->data;
			BM_mesh_normals_update(me->edit_btmesh->bm, TRUE);  // does face centers too
		}
		else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
			Curve *cu = obedit->data;
			ListBase *nurbs = BKE_curve_editNurbs_get(cu);
			Nurb *nu = nurbs->first;

			while (nu) {
				/* keep handles' vectors unchanged */
				if (nu->bezt) {
					int a = nu->pntsu;
					TransVert *tv = transvmain;
					BezTriple *bezt = nu->bezt;

					while (a--) {
						if (bezt->f1 & SELECT) tv++;

						if (bezt->f2 & SELECT) {
							float v[3];

							if (bezt->f1 & SELECT) {
								sub_v3_v3v3(v, (tv - 1)->oldloc, tv->oldloc);
								add_v3_v3v3(bezt->vec[0], bezt->vec[1], v);
							}

							if (bezt->f3 & SELECT) {
								sub_v3_v3v3(v, (tv + 1)->oldloc, tv->oldloc);
								add_v3_v3v3(bezt->vec[2], bezt->vec[1], v);
							}

							tv++;
						}

						if (bezt->f3 & SELECT) tv++;

						bezt++;
					}
				}

				BKE_nurb_test2D(nu);
				BKE_nurb_handles_test(nu); /* test for bezier too */
				nu = nu->next;
			}
		}
		else if (obedit->type == OB_ARMATURE) {
			bArmature *arm = obedit->data;
			EditBone *ebo;
			TransVert *tv = transvmain;
			int a = 0;
			
			/* Ensure all bone tails are correctly adjusted */
			for (ebo = arm->edbo->first; ebo; ebo = ebo->next) {
				/* adjust tip if both ends selected */
				if ((ebo->flag & BONE_ROOTSEL) && (ebo->flag & BONE_TIPSEL)) {
					if (tv) {
						float diffvec[3];
						
						sub_v3_v3v3(diffvec, tv->loc, tv->oldloc);
						add_v3_v3(ebo->tail, diffvec);
						
						a++;
						if (a < tottrans) tv++;
					}
				}
			}
			
			/* Ensure all bones are correctly adjusted */
			for (ebo = arm->edbo->first; ebo; ebo = ebo->next) {
				if ((ebo->flag & BONE_CONNECTED) && ebo->parent) {
					/* If this bone has a parent tip that has been moved */
					if (ebo->parent->flag & BONE_TIPSEL) {
						copy_v3_v3(ebo->head, ebo->parent->tail);
					}
					/* If this bone has a parent tip that has NOT been moved */
					else {
						copy_v3_v3(ebo->parent->tail, ebo->head);
					}
				}
			}
			if (arm->flag & ARM_MIRROR_EDIT)
				transform_armature_mirror_update(obedit);
		}
		else if (obedit->type == OB_LATTICE) {
			Lattice *lt = obedit->data;
			
			if (lt->editlatt->latt->flag & LT_OUTSIDE)
				outside_lattice(lt->editlatt->latt);
		}
	}
}

static void set_mapped_co(void *vuserdata, int index, const float co[3],
                          const float UNUSED(no[3]), const short UNUSED(no_s[3]))
{
	void **userdata = vuserdata;
	BMEditMesh *em = userdata[0];
	TransVert *tv = userdata[1];
	BMVert *eve = EDBM_vert_at_index(em, index);
	
	if (BM_elem_index_get(eve) != -1 && !(tv[BM_elem_index_get(eve)].flag & TX_VERT_USE_MAPLOC)) {
		copy_v3_v3(tv[BM_elem_index_get(eve)].maploc, co);
		tv[BM_elem_index_get(eve)].flag |= TX_VERT_USE_MAPLOC;
	}
}

/* copied from editobject.c, needs to be replaced with new transform code still */
/* mode flags: */
#define TM_ALL_JOINTS       1 /* all joints (for bones only) */
#define TM_SKIP_HANDLES     2 /* skip handles when control point is selected (for curves only) */
static void make_trans_verts(Object *obedit, float *min, float *max, int mode)
{
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	TransVert *tv = NULL;
	MetaElem *ml;
	BMVert *eve;
	EditBone    *ebo;
	float total, center[3], centroid[3];
	int a;

	tottrans = 0; // global!
	
	INIT_MINMAX(min, max);
	zero_v3(centroid);
	
	if (obedit->type == OB_MESH) {
		Mesh *me = obedit->data;
		BMEditMesh *em = me->edit_btmesh;
		BMesh *bm = em->bm;
		BMIter iter;
		void *userdata[2] = {em, NULL};
		/*int proptrans= 0; */ /*UNUSED*/
		
		/* abuses vertex index all over, set, just set dirty here,
		 * perhaps this could use its own array instead? - campbell */

		// transform now requires awareness for select mode, so we tag the f1 flags in verts
		tottrans = 0;
		if (em->selectmode & SCE_SELECT_VERTEX) {
			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN) && BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
					BM_elem_index_set(eve, 1); /* set_dirty! */
					tottrans++;
				}
				else BM_elem_index_set(eve, 0);  /* set_dirty! */
			}
		}
		else if (em->selectmode & SCE_SELECT_EDGE) {
			BMEdge *eed;

			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				BM_elem_index_set(eve, 0);     /* set_dirty! */
			}

			BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
				if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) && BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
					BM_elem_index_set(eed->v1, 1); /* set_dirty! */
					BM_elem_index_set(eed->v2, 1); /* set_dirty! */
				}
			}

			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				if (BM_elem_index_get(eve)) tottrans++;
			}
		}
		else {
			BMFace *efa;

			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				BM_elem_index_set(eve, 0); /* set_dirty! */
			}

			BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
				if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
					BMIter liter;
					BMLoop *l;
					
					BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
						BM_elem_index_set(l->v, 1); /* set_dirty! */
					}
				}
			}

			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				if (BM_elem_index_get(eve)) tottrans++;
			}
		}
		/* for any of the 3 loops above which all dirty the indices */
		bm->elem_index_dirty |= BM_VERT;
		
		/* and now make transverts */
		if (tottrans) {
			tv = transvmain = MEM_callocN(tottrans * sizeof(TransVert), "maketransverts");
		
			a = 0;
			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				if (BM_elem_index_get(eve)) {
					BM_elem_index_set(eve, a); /* set_dirty! */
					copy_v3_v3(tv->oldloc, eve->co);
					tv->loc = eve->co;
					if (eve->no[0] != 0.0f || eve->no[1] != 0.0f || eve->no[2] != 0.0f)
						tv->nor = eve->no;  // note this is a hackish signal (ton)
					tv->flag = BM_elem_index_get(eve) & SELECT;
					tv++;
					a++;
				}
				else {
					BM_elem_index_set(eve, -1); /* set_dirty! */
				}
			}
			/* set dirty already, above */

			userdata[1] = transvmain;
		}
		
		if (transvmain && em->derivedCage) {
			EDBM_index_arrays_init(em, 1, 0, 0);
			em->derivedCage->foreachMappedVert(em->derivedCage, set_mapped_co, userdata);
			EDBM_index_arrays_free(em);
		}
	}
	else if (obedit->type == OB_ARMATURE) {
		bArmature *arm = obedit->data;
		int totmalloc = BLI_countlist(arm->edbo);

		totmalloc *= 2;  /* probably overkill but bones can have 2 trans verts each */

		tv = transvmain = MEM_callocN(totmalloc * sizeof(TransVert), "maketransverts armature");
		
		for (ebo = arm->edbo->first; ebo; ebo = ebo->next) {
			if (ebo->layer & arm->layer) {
				short tipsel = (ebo->flag & BONE_TIPSEL);
				short rootsel = (ebo->flag & BONE_ROOTSEL);
				short rootok = (!(ebo->parent && (ebo->flag & BONE_CONNECTED) && (ebo->parent->flag & BONE_TIPSEL)));
				
				if ((tipsel && rootsel) || (rootsel)) {
					/* Don't add the tip (unless mode & TM_ALL_JOINTS, for getting all joints),
					 * otherwise we get zero-length bones as tips will snap to the same
					 * location as heads.
					 */
					if (rootok) {
						copy_v3_v3(tv->oldloc, ebo->head);
						tv->loc = ebo->head;
						tv->nor = NULL;
						tv->flag = 1;
						tv++;
						tottrans++;
					}	
					
					if ((mode & TM_ALL_JOINTS) && (tipsel)) {
						copy_v3_v3(tv->oldloc, ebo->tail);
						tv->loc = ebo->tail;
						tv->nor = NULL;
						tv->flag = 1;
						tv++;
						tottrans++;
					}
				}
				else if (tipsel) {
					copy_v3_v3(tv->oldloc, ebo->tail);
					tv->loc = ebo->tail;
					tv->nor = NULL;
					tv->flag = 1;
					tv++;
					tottrans++;
				}
			}
		}
	}
	else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
		Curve *cu = obedit->data;
		int totmalloc = 0;
		ListBase *nurbs = BKE_curve_editNurbs_get(cu);

		for (nu = nurbs->first; nu; nu = nu->next) {
			if (nu->type == CU_BEZIER)
				totmalloc += 3 * nu->pntsu;
			else
				totmalloc += nu->pntsu * nu->pntsv;
		}
		tv = transvmain = MEM_callocN(totmalloc * sizeof(TransVert), "maketransverts curve");

		nu = nurbs->first;
		while (nu) {
			if (nu->type == CU_BEZIER) {
				a = nu->pntsu;
				bezt = nu->bezt;
				while (a--) {
					if (bezt->hide == 0) {
						int skip_handle = 0;
						if (bezt->f2 & SELECT)
							skip_handle = mode & TM_SKIP_HANDLES;

						if ((bezt->f1 & SELECT) && !skip_handle) {
							copy_v3_v3(tv->oldloc, bezt->vec[0]);
							tv->loc = bezt->vec[0];
							tv->flag = bezt->f1 & SELECT;
							tv++;
							tottrans++;
						}
						if (bezt->f2 & SELECT) {
							copy_v3_v3(tv->oldloc, bezt->vec[1]);
							tv->loc = bezt->vec[1];
							tv->val = &(bezt->alfa);
							tv->oldval = bezt->alfa;
							tv->flag = bezt->f2 & SELECT;
							tv++;
							tottrans++;
						}
						if ((bezt->f3 & SELECT) && !skip_handle) {
							copy_v3_v3(tv->oldloc, bezt->vec[2]);
							tv->loc = bezt->vec[2];
							tv->flag = bezt->f3 & SELECT;
							tv++;
							tottrans++;
						}
					}
					bezt++;
				}
			}
			else {
				a = nu->pntsu * nu->pntsv;
				bp = nu->bp;
				while (a--) {
					if (bp->hide == 0) {
						if (bp->f1 & SELECT) {
							copy_v3_v3(tv->oldloc, bp->vec);
							tv->loc = bp->vec;
							tv->val = &(bp->alfa);
							tv->oldval = bp->alfa;
							tv->flag = bp->f1 & SELECT;
							tv++;
							tottrans++;
						}
					}
					bp++;
				}
			}
			nu = nu->next;
		}
	}
	else if (obedit->type == OB_MBALL) {
		MetaBall *mb = obedit->data;
		int totmalloc = BLI_countlist(mb->editelems);
		
		tv = transvmain = MEM_callocN(totmalloc * sizeof(TransVert), "maketransverts mball");
		
		ml = mb->editelems->first;
		while (ml) {
			if (ml->flag & SELECT) {
				tv->loc = &ml->x;
				copy_v3_v3(tv->oldloc, tv->loc);
				tv->val = &(ml->rad);
				tv->oldval = ml->rad;
				tv->flag = 1;
				tv++;
				tottrans++;
			}
			ml = ml->next;
		}
	}
	else if (obedit->type == OB_LATTICE) {
		Lattice *lt = obedit->data;
		
		bp = lt->editlatt->latt->def;
		
		a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
		
		tv = transvmain = MEM_callocN(a * sizeof(TransVert), "maketransverts latt");
		
		while (a--) {
			if (bp->f1 & SELECT) {
				if (bp->hide == 0) {
					copy_v3_v3(tv->oldloc, bp->vec);
					tv->loc = bp->vec;
					tv->flag = bp->f1 & SELECT;
					tv++;
					tottrans++;
				}
			}
			bp++;
		}
	}
	
	if (!tottrans && transvmain) {
		/* prevent memory leak. happens for curves/latticies due to */
		/* difficult condition of adding points to trans data */
		MEM_freeN(transvmain);
		transvmain = NULL;
	}

	/* cent etc */
	tv = transvmain;
	total = 0.0;
	for (a = 0; a < tottrans; a++, tv++) {
		if (tv->flag & SELECT) {
			add_v3_v3(centroid, tv->oldloc);
			total += 1.0f;
			DO_MINMAX(tv->oldloc, min, max);
		}
	}
	if (total != 0.0f) {
		mul_v3_fl(centroid, 1.0f / total);
	}

	mid_v3_v3v3(center, min, max);
}

/* *********************** operators ******************** */

static int snap_sel_to_grid(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	RegionView3D *rv3d = CTX_wm_region_data(C);
	TransVert *tv;
	float gridf, imat[3][3], bmat[3][3], vec[3];
	int a;

	gridf = rv3d->gridview;

	if (obedit) {
		tottrans = 0;
		
		if (ELEM6(obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL))
			make_trans_verts(obedit, bmat[0], bmat[1], 0);
		if (tottrans == 0) return OPERATOR_CANCELLED;
		
		copy_m3_m4(bmat, obedit->obmat);
		invert_m3_m3(imat, bmat);
		
		tv = transvmain;
		for (a = 0; a < tottrans; a++, tv++) {
			copy_v3_v3(vec, tv->loc);
			mul_m3_v3(bmat, vec);
			add_v3_v3(vec, obedit->obmat[3]);
			vec[0] = gridf * floorf(0.5f + vec[0] / gridf);
			vec[1] = gridf * floorf(0.5f + vec[1] / gridf);
			vec[2] = gridf * floorf(0.5f + vec[2] / gridf);
			sub_v3_v3(vec, obedit->obmat[3]);
			
			mul_m3_v3(imat, vec);
			copy_v3_v3(tv->loc, vec);
		}
		
		special_transvert_update(obedit);
		
		MEM_freeN(transvmain);
		transvmain = NULL;
	}
	else {
		struct KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);

		CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
		{
			if (ob->mode & OB_MODE_POSE) {
				bPoseChannel *pchan;
				bArmature *arm = ob->data;
				
				invert_m4_m4(ob->imat, ob->obmat);
				
				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					if (pchan->bone->flag & BONE_SELECTED) {
						if (pchan->bone->layer & arm->layer) {
							if ((pchan->bone->flag & BONE_CONNECTED) == 0) {
								float nLoc[3];
								
								/* get nearest grid point to snap to */
								copy_v3_v3(nLoc, pchan->pose_mat[3]);
								/* We must operate in world space! */
								mul_m4_v3(ob->obmat, nLoc);
								vec[0] = gridf * (float)(floor(0.5f + nLoc[0] / gridf));
								vec[1] = gridf * (float)(floor(0.5f + nLoc[1] / gridf));
								vec[2] = gridf * (float)(floor(0.5f + nLoc[2] / gridf));
								/* Back in object space... */
								mul_m4_v3(ob->imat, vec);
								
								/* Get location of grid point in pose space. */
								BKE_armature_loc_pose_to_bone(pchan, vec, vec);
								
								/* adjust location */
								if ((pchan->protectflag & OB_LOCK_LOCX) == 0)
									pchan->loc[0] = vec[0];
								if ((pchan->protectflag & OB_LOCK_LOCY) == 0)
									pchan->loc[1] = vec[1];
								if ((pchan->protectflag & OB_LOCK_LOCZ) == 0)
									pchan->loc[2] = vec[2];

								/* auto-keyframing */
								ED_autokeyframe_pchan(C, scene, ob, pchan, ks);
							}
							/* if the bone has a parent and is connected to the parent,
							 * don't do anything - will break chain unless we do auto-ik.
							 */
						}
					}
				}
				ob->pose->flag |= (POSE_LOCKED | POSE_DO_UNLOCK);
				
				DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			}
			else {
				ob->recalc |= OB_RECALC_OB;
				
				vec[0] = -ob->obmat[3][0] + gridf *floorf(0.5f + ob->obmat[3][0] / gridf);
				vec[1] = -ob->obmat[3][1] + gridf *floorf(0.5f + ob->obmat[3][1] / gridf);
				vec[2] = -ob->obmat[3][2] + gridf *floorf(0.5f + ob->obmat[3][2] / gridf);
				
				if (ob->parent) {
					BKE_object_where_is_calc(scene, ob);
					
					invert_m3_m3(imat, originmat);
					mul_m3_v3(imat, vec);
				}
				if ((ob->protectflag & OB_LOCK_LOCX) == 0)
					ob->loc[0] += vec[0];
				if ((ob->protectflag & OB_LOCK_LOCY) == 0)
					ob->loc[1] += vec[1];
				if ((ob->protectflag & OB_LOCK_LOCZ) == 0)
					ob->loc[2] += vec[2];
				
				/* auto-keyframing */
				ED_autokeyframe_object(C, scene, ob, ks);
			}
		}
		CTX_DATA_END;
	}

	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_selected_to_grid(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Snap Selection to Grid";
	ot->description = "Snap selected item(s) to nearest grid node";
	ot->idname = "VIEW3D_OT_snap_selected_to_grid";
	
	/* api callbacks */
	ot->exec = snap_sel_to_grid;
	ot->poll = ED_operator_region_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *************************************************** */

static int snap_sel_to_curs(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	TransVert *tv;
	float *curs, imat[3][3], bmat[3][3], vec[3];
	int a;

	curs = give_cursor(scene, v3d);

	if (obedit) {
		tottrans = 0;
		
		if (ELEM6(obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL))
			make_trans_verts(obedit, bmat[0], bmat[1], 0);
		if (tottrans == 0) return OPERATOR_CANCELLED;
		
		copy_m3_m4(bmat, obedit->obmat);
		invert_m3_m3(imat, bmat);
		
		tv = transvmain;
		for (a = 0; a < tottrans; a++, tv++) {
			sub_v3_v3v3(vec, curs, obedit->obmat[3]);
			mul_m3_v3(imat, vec);
			copy_v3_v3(tv->loc, vec);
		}
		
		special_transvert_update(obedit);
		
		MEM_freeN(transvmain);
		transvmain = NULL;
	}
	else {
		struct KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);

		CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
		{
			if (ob->mode & OB_MODE_POSE) {
				bPoseChannel *pchan;
				bArmature *arm = ob->data;
				
				invert_m4_m4(ob->imat, ob->obmat);
				copy_v3_v3(vec, curs);
				mul_m4_v3(ob->imat, vec);
				
				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					if (pchan->bone->flag & BONE_SELECTED) {
						if (pchan->bone->layer & arm->layer) {
							if ((pchan->bone->flag & BONE_CONNECTED) == 0) {
								/* Get position in pchan (pose) space. */
								BKE_armature_loc_pose_to_bone(pchan, vec, vec);

								/* copy new position */
								if ((pchan->protectflag & OB_LOCK_LOCX) == 0)
									pchan->loc[0] = vec[0];
								if ((pchan->protectflag & OB_LOCK_LOCY) == 0)
									pchan->loc[1] = vec[1];
								if ((pchan->protectflag & OB_LOCK_LOCZ) == 0)
									pchan->loc[2] = vec[2];

								/* auto-keyframing */
								ED_autokeyframe_pchan(C, scene, ob, pchan, ks);
							}
							/* if the bone has a parent and is connected to the parent,
							 * don't do anything - will break chain unless we do auto-ik.
							 */
						}
					}
				}
				ob->pose->flag |= (POSE_LOCKED | POSE_DO_UNLOCK);
				
				DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			}
			else {
				ob->recalc |= OB_RECALC_OB;
				
				vec[0] = -ob->obmat[3][0] + curs[0];
				vec[1] = -ob->obmat[3][1] + curs[1];
				vec[2] = -ob->obmat[3][2] + curs[2];
				
				if (ob->parent) {
					BKE_object_where_is_calc(scene, ob);
					
					invert_m3_m3(imat, originmat);
					mul_m3_v3(imat, vec);
				}
				if ((ob->protectflag & OB_LOCK_LOCX) == 0)
					ob->loc[0] += vec[0];
				if ((ob->protectflag & OB_LOCK_LOCY) == 0)
					ob->loc[1] += vec[1];
				if ((ob->protectflag & OB_LOCK_LOCZ) == 0)
					ob->loc[2] += vec[2];

				/* auto-keyframing */
				ED_autokeyframe_object(C, scene, ob, ks);
			}
		}
		CTX_DATA_END;
	}

	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_selected_to_cursor(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Snap Selection to Cursor";
	ot->description = "Snap selected item(s) to cursor";
	ot->idname = "VIEW3D_OT_snap_selected_to_cursor";
	
	/* api callbacks */
	ot->exec = snap_sel_to_curs;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *************************************************** */

static int snap_curs_to_grid(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	RegionView3D *rv3d = CTX_wm_region_data(C);
	View3D *v3d = CTX_wm_view3d(C);
	float gridf, *curs;

	gridf = rv3d->gridview;
	curs = give_cursor(scene, v3d);

	curs[0] = gridf * floorf(0.5f + curs[0] / gridf);
	curs[1] = gridf * floorf(0.5f + curs[1] / gridf);
	curs[2] = gridf * floorf(0.5f + curs[2] / gridf);
	
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);    // hrm
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_grid(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Snap Cursor to Grid";
	ot->description = "Snap cursor to nearest grid node";
	ot->idname = "VIEW3D_OT_snap_cursor_to_grid";
	
	/* api callbacks */
	ot->exec = snap_curs_to_grid;
	ot->poll = ED_operator_region_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* **************************************************** */

static void bundle_midpoint(Scene *scene, Object *ob, float vec[3])
{
	MovieClip *clip = BKE_object_movieclip_get(scene, ob, 0);
	MovieTracking *tracking;
	MovieTrackingObject *object;
	int ok = 0;
	float min[3], max[3], mat[4][4], pos[3], cammat[4][4] = MAT4_UNITY;

	if (!clip)
		return;

	tracking = &clip->tracking;

	copy_m4_m4(cammat, ob->obmat);

	BKE_get_tracking_mat(scene, ob, mat);

	INIT_MINMAX(min, max);

	for (object = tracking->objects.first; object; object = object->next) {
		ListBase *tracksbase = BKE_tracking_object_tracks(tracking, object);
		MovieTrackingTrack *track = tracksbase->first;
		float obmat[4][4];

		if (object->flag & TRACKING_OBJECT_CAMERA) {
			copy_m4_m4(obmat, mat);
		}
		else {
			float imat[4][4];

			BKE_tracking_get_interpolated_camera(tracking, object, scene->r.cfra, imat);
			invert_m4(imat);

			mult_m4_m4m4(obmat, cammat, imat);
		}

		while (track) {
			if ((track->flag & TRACK_HAS_BUNDLE) && TRACK_SELECTED(track)) {
				ok = 1;
				mul_v3_m4v3(pos, obmat, track->bundle_pos);
				DO_MINMAX(pos, min, max);
			}

			track = track->next;
		}
	}

	if (ok) {
		mid_v3_v3v3(vec, min, max);
	}
}

static int snap_curs_to_sel(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	TransVert *tv;
	float *curs, bmat[3][3], vec[3], min[3], max[3], centroid[3];
	int count, a;

	curs = give_cursor(scene, v3d);

	count = 0;
	INIT_MINMAX(min, max);
	zero_v3(centroid);

	if (obedit) {
		tottrans = 0;
		
		if (ELEM6(obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL))
			make_trans_verts(obedit, bmat[0], bmat[1], TM_ALL_JOINTS | TM_SKIP_HANDLES);
		if (tottrans == 0) return OPERATOR_CANCELLED;
		
		copy_m3_m4(bmat, obedit->obmat);
		
		tv = transvmain;
		for (a = 0; a < tottrans; a++, tv++) {
			copy_v3_v3(vec, tv->loc);
			mul_m3_v3(bmat, vec);
			add_v3_v3(vec, obedit->obmat[3]);
			add_v3_v3(centroid, vec);
			DO_MINMAX(vec, min, max);
		}
		
		if (v3d->around == V3D_CENTROID) {
			mul_v3_fl(centroid, 1.0f / (float)tottrans);
			copy_v3_v3(curs, centroid);
		}
		else {
			mid_v3_v3v3(curs, min, max);
		}
		MEM_freeN(transvmain);
		transvmain = NULL;
	}
	else {
		Object *obact = CTX_data_active_object(C);
		
		if (obact && (obact->mode & OB_MODE_POSE)) {
			bArmature *arm = obact->data;
			bPoseChannel *pchan;
			for (pchan = obact->pose->chanbase.first; pchan; pchan = pchan->next) {
				if (arm->layer & pchan->bone->layer) {
					if (pchan->bone->flag & BONE_SELECTED) {
						copy_v3_v3(vec, pchan->pose_head);
						mul_m4_v3(obact->obmat, vec);
						add_v3_v3(centroid, vec);
						DO_MINMAX(vec, min, max);
						count++;
					}
				}
			}
		}
		else {
			CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
			{
				copy_v3_v3(vec, ob->obmat[3]);

				/* special case for camera -- snap to bundles */
				if (ob->type == OB_CAMERA) {
					/* snap to bundles should happen only when bundles are visible */
					if (v3d->flag2 & V3D_SHOW_RECONSTRUCTION) {
						bundle_midpoint(scene, ob, vec);
					}
				}

				add_v3_v3(centroid, vec);
				DO_MINMAX(vec, min, max);
				count++;
			}
			CTX_DATA_END;
		}
		if (count) {
			if (v3d->around == V3D_CENTROID) {
				mul_v3_fl(centroid, 1.0f / (float)count);
				copy_v3_v3(curs, centroid);
			}
			else {
				mid_v3_v3v3(curs, min, max);
			}
		}
	}
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Snap Cursor to Selected";
	ot->description = "Snap cursor to center of selected item(s)";
	ot->idname = "VIEW3D_OT_snap_cursor_to_selected";
	
	/* api callbacks */
	ot->exec = snap_curs_to_sel;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */

static int snap_curs_to_active(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	Object *obact = CTX_data_active_object(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	float *curs;
	
	curs = give_cursor(scene, v3d);

	if (obedit) {
		if (obedit->type == OB_MESH) {
			/* check active */
			Mesh *me = obedit->data;
			BMEditSelection ese;
			
			if (BM_select_history_active_get(me->edit_btmesh->bm, &ese)) {
				BM_editselection_center(&ese, curs);
			}
			
			mul_m4_v3(obedit->obmat, curs);
		}
	}
	else {
		if (obact) {
			copy_v3_v3(curs, obact->obmat[3]);
		}
	}
	
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_active(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Snap Cursor to Active";
	ot->description = "Snap cursor to active item";
	ot->idname = "VIEW3D_OT_snap_cursor_to_active";
	
	/* api callbacks */
	ot->exec = snap_curs_to_active;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* **************************************************** */
/*New Code - Snap Cursor to Center -*/
static int snap_curs_to_center(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	float *curs;
	curs = give_cursor(scene, v3d);

	zero_v3(curs);
	
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_center(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Snap Cursor to Center";
	ot->description = "Snap cursor to the Center";
	ot->idname = "VIEW3D_OT_snap_cursor_to_center";
	
	/* api callbacks */
	ot->exec = snap_curs_to_center;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* **************************************************** */


int minmax_verts(Object *obedit, float *min, float *max)
{
	TransVert *tv;
	float centroid[3], vec[3], bmat[3][3];
	int a;

	tottrans = 0;
	if (ELEM5(obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE))
		make_trans_verts(obedit, bmat[0], bmat[1], TM_ALL_JOINTS);
	
	if (tottrans == 0) return 0;

	copy_m3_m4(bmat, obedit->obmat);
	
	tv = transvmain;
	for (a = 0; a < tottrans; a++, tv++) {
		copy_v3_v3(vec, (tv->flag & TX_VERT_USE_MAPLOC) ? tv->maploc : tv->loc);
		mul_m3_v3(bmat, vec);
		add_v3_v3(vec, obedit->obmat[3]);
		add_v3_v3(centroid, vec);
		DO_MINMAX(vec, min, max);
	}
	
	MEM_freeN(transvmain);
	transvmain = NULL;
	
	return 1;
}
